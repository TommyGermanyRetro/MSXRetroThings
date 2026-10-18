#include "INS8250/INS8250.hpp"

#include <algorithm>

namespace ins8250 {

namespace {

constexpr std::uint8_t DivisorLatchAccess = 0x80;
constexpr std::uint8_t LoopMode = 0x10;

constexpr std::uint8_t EnableReceived = 0x01;
constexpr std::uint8_t EnableTransmitter = 0x02;
constexpr std::uint8_t EnableLineStatus = 0x04;
constexpr std::uint8_t EnableModemStatus = 0x08;

constexpr std::uint8_t LineErrors = LineOverrun | LineParityError | LineFramingError | LineBreak;

} // namespace

INS8250::INS8250(std::uint32_t hostClockHz, std::uint32_t crystalHz)
    : hostHz(hostClockHz), crystal(crystalHz)
{
    reset();
}

void INS8250::reset()
{
    interruptEnable = 0;
    lineControl = 0;
    modemControl = 0;
    lineStatus = LineTransmitterHoldingEmpty | LineTransmitterEmpty;
    modemStatus = 0;
    transmitterInterrupt = false;
    shifting = false;
    receiving = false;
    outputPending = false;
    updateModemStatus();
    modemStatus &= 0xF0;
}

std::uint64_t INS8250::frameTicks() const noexcept
{
    const auto settings = lineSettings();
    const unsigned bits = 1 + settings.dataBits + (settings.parity == 'N' ? 0 : 1) + settings.stopBits;
    const std::uint64_t perBit = static_cast<std::uint64_t>(hostHz) * 16 * std::max<std::uint32_t>(divisor, 1) / crystal;
    return std::max<std::uint64_t>(1, perBit * bits);
}

LineSettings INS8250::lineSettings() const noexcept
{
    LineSettings settings;
    settings.baud = crystal / 16 / std::max<std::uint32_t>(divisor, 1);
    settings.dataBits = 5 + (lineControl & 0x03);
    settings.stopBits = (lineControl & 0x04) ? 2 : 1;
    if (!(lineControl & 0x08)) settings.parity = 'N';
    else if (lineControl & 0x20) settings.parity = (lineControl & 0x10) ? 'S' : 'M';
    else settings.parity = (lineControl & 0x10) ? 'E' : 'O';
    return settings;
}

ModemOutputs INS8250::modemOutputs() const noexcept
{
    if (loopback()) return {};
    return {(modemControl & 0x01) != 0, (modemControl & 0x02) != 0, (modemControl & 0x04) != 0, (modemControl & 0x08) != 0};
}

bool INS8250::breakActive() const noexcept
{
    return (lineControl & 0x40) != 0 && !loopback();
}

bool INS8250::loopback() const noexcept
{
    return (modemControl & LoopMode) != 0;
}

// In loopback the modem outputs feed the modem inputs: RTS to CTS, DTR to DSR, OUT1 to RI, OUT2 to DCD.
std::uint8_t INS8250::modemStatusLines() const noexcept
{
    if (loopback()) {
        return static_cast<std::uint8_t>(((modemControl & 0x02) ? 0x10 : 0) | ((modemControl & 0x01) ? 0x20 : 0) |
                                         ((modemControl & 0x04) ? 0x40 : 0) | ((modemControl & 0x08) ? 0x80 : 0));
    }
    return static_cast<std::uint8_t>((externalInputs.cts ? 0x10 : 0) | (externalInputs.dsr ? 0x20 : 0) |
                                     (externalInputs.ri ? 0x40 : 0) | (externalInputs.dcd ? 0x80 : 0));
}

void INS8250::updateModemStatus() noexcept
{
    const auto now = modemStatusLines();
    const auto before = static_cast<std::uint8_t>(modemStatus & 0xF0);
    const auto changed = static_cast<std::uint8_t>(now ^ before);
    std::uint8_t deltas = modemStatus & 0x0F;
    if (changed & 0x10) deltas |= 0x01;
    if (changed & 0x20) deltas |= 0x02;
    if ((before & 0x40) && !(now & 0x40)) deltas |= 0x04;
    if (changed & 0x80) deltas |= 0x08;
    modemStatus = static_cast<std::uint8_t>(now | deltas);
}

void INS8250::setModemInputs(const ModemInputs& inputs) noexcept
{
    externalInputs = inputs;
    updateModemStatus();
}

std::uint8_t INS8250::interruptIdentification() const noexcept
{
    if ((interruptEnable & EnableLineStatus) && (lineStatus & LineErrors)) return 0x06;
    if ((interruptEnable & EnableReceived) && (lineStatus & LineDataReady)) return 0x04;
    if ((interruptEnable & EnableTransmitter) && transmitterInterrupt) return 0x02;
    if ((interruptEnable & EnableModemStatus) && (modemStatus & 0x0F)) return 0x00;
    return 0x01;
}

bool INS8250::interruptRequest() const noexcept
{
    return (interruptIdentification() & 0x01) == 0;
}

std::uint8_t INS8250::read(std::uint8_t reg)
{
    switch (reg & 7) {
    case 0:
        if (lineControl & DivisorLatchAccess) return static_cast<std::uint8_t>(divisor & 0xFF);
        lineStatus &= static_cast<std::uint8_t>(~LineDataReady);
        return receiverBuffer;
    case 1:
        if (lineControl & DivisorLatchAccess) return static_cast<std::uint8_t>(divisor >> 8);
        return interruptEnable;
    case 2: {
        const auto id = interruptIdentification();
        if (id == 0x02) transmitterInterrupt = false;
        return id;
    }
    case 3: return lineControl;
    case 4: return modemControl;
    case 5: {
        const auto value = lineStatus;
        lineStatus &= static_cast<std::uint8_t>(~LineErrors);
        return value;
    }
    case 6: {
        updateModemStatus();
        const auto value = modemStatus;
        modemStatus &= 0xF0;
        return value;
    }
    default: return scratch;
    }
}

void INS8250::write(std::uint8_t reg, std::uint8_t value)
{
    switch (reg & 7) {
    case 0:
        if (lineControl & DivisorLatchAccess) {
            divisor = static_cast<std::uint16_t>((divisor & 0xFF00) | value);
            break;
        }
        holding = value;
        transmitterInterrupt = false;
        if (!shifting) {
            shift = holding;
            shifting = true;
            transmitTicks = frameTicks();
            lineStatus = static_cast<std::uint8_t>((lineStatus | LineTransmitterHoldingEmpty) & ~LineTransmitterEmpty);
            transmitterInterrupt = true;
        } else {
            lineStatus &= static_cast<std::uint8_t>(~LineTransmitterHoldingEmpty);
        }
        break;
    case 1:
        if (lineControl & DivisorLatchAccess) {
            divisor = static_cast<std::uint16_t>((divisor & 0x00FF) | (value << 8));
            break;
        }
        // Enabling the transmitter interrupt with an empty holding register raises it at once.
        if ((value & EnableTransmitter) && !(interruptEnable & EnableTransmitter) &&
            (lineStatus & LineTransmitterHoldingEmpty)) {
            transmitterInterrupt = true;
        }
        interruptEnable = value & 0x0F;
        break;
    case 3: lineControl = value; break;
    case 4:
        modemControl = value & 0x1F;
        updateModemStatus();
        break;
    case 5: break;
    case 6: break;
    default: scratch = value; break;
    }
}

void INS8250::deliver(std::uint8_t value) noexcept
{
    if (lineStatus & LineDataReady) lineStatus |= LineOverrun;
    receiverBuffer = value;
    lineStatus |= LineDataReady;
}

void INS8250::tick()
{
    if (shifting && --transmitTicks == 0) {
        if (loopback()) deliver(shift);
        else {
            output = shift;
            outputPending = true;
        }
        if (!(lineStatus & LineTransmitterHoldingEmpty)) {
            shift = holding;
            transmitTicks = frameTicks();
            lineStatus |= LineTransmitterHoldingEmpty;
            transmitterInterrupt = true;
        } else {
            shifting = false;
            lineStatus |= LineTransmitterEmpty;
        }
    }
    if (receiving && --receiveTicks == 0) {
        receiving = false;
        if (!loopback()) deliver(receiveValue);
    }
}

bool INS8250::takeTransmitted(std::uint8_t& value) noexcept
{
    if (!outputPending) return false;
    outputPending = false;
    value = output;
    return true;
}

bool INS8250::readyToReceive() const noexcept
{
    return !receiving;
}

void INS8250::receive(std::uint8_t value) noexcept
{
    receiving = true;
    receiveValue = value;
    receiveTicks = frameTicks();
}

} // namespace ins8250
