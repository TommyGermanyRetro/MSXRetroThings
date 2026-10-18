#include "Z8530/Z8530.hpp"

namespace z8530 {

namespace {

constexpr std::uint8_t Rr0RxAvailable = 0x01;
constexpr std::uint8_t Rr0ZeroCount = 0x02;
constexpr std::uint8_t Rr0TxEmpty = 0x04;
constexpr std::uint8_t Rr0Dcd = 0x08;
constexpr std::uint8_t Rr0SyncHunt = 0x10;
constexpr std::uint8_t Rr0Cts = 0x20;
constexpr std::uint8_t Rr0TxUnderrun = 0x40;
constexpr std::uint8_t Rr0BreakAbort = 0x80;

constexpr std::uint8_t Rr1AllSent = 0x01;
constexpr std::uint8_t Rr1Residue8 = 0x06;
constexpr std::uint8_t Rr1Parity = 0x10;
constexpr std::uint8_t Rr1Overrun = 0x20;
constexpr std::uint8_t Rr1CrcFraming = 0x40;
constexpr std::uint8_t Rr1EndOfFrame = 0x80;

constexpr std::uint16_t SdlcResidue = 0xF0B8;

int rxMode(std::uint8_t wr1)
{
    return (wr1 >> 3) & 0x03;
}

std::uint8_t lowMask(int bits)
{
    return static_cast<std::uint8_t>((1u << bits) - 1u);
}

} // namespace

Z8530::Z8530()
{
    reset();
}

void Z8530::reset() noexcept
{
    resetChannel(ChannelA, true);
    resetChannel(ChannelB, true);
    wr9 = 0;
}

void Z8530::resetChannel(int channel, bool hardware) noexcept
{
    auto& c = channels[channel];
    c.wr[0] = 0;
    c.wr[1] &= 0x24;
    c.wr[3] &= static_cast<std::uint8_t>(~0x01);
    c.wr[4] |= 0x04;
    c.wr[5] &= 0x61;
    c.wr[10] = hardware ? 0x00 : static_cast<std::uint8_t>(c.wr[10] & 0x60);
    if (hardware) c.wr[11] = 0x08;
    c.wr[14] = hardware ? 0x20 : static_cast<std::uint8_t>((c.wr[14] & 0xC3) | 0x20);
    c.wr[15] = 0xF8;
    c.pointer = 0;

    c.fifo.clear();
    c.hasPending = false;
    c.lastStatus = 0;
    c.latchedErrors = 0;
    c.locked = false;
    c.firstCharArmed = false;
    c.hunting = true;
    c.inFrame = false;
    c.breakAbort = false;

    c.bufferFull = false;
    c.shifting = false;
    c.trailer.clear();
    c.trailerIsCrc = false;
    c.frameOpen = false;
    c.underrunLatch = true;

    c.brgPhase = 0;
    c.zeroCount = false;
    c.extLatched = false;
    c.rxIp = false;
    c.rxSpecial = false;
    c.txIp = false;
    c.extIp = false;
    c.ius = {};
    c.lastRr0 = liveRr0(channel);
}

bool Z8530::asyncMode(int channel) const noexcept
{
    return (channels[channel].wr[4] & 0x0C) != 0;
}

bool Z8530::sdlcMode(int channel) const noexcept
{
    return !asyncMode(channel) && (channels[channel].wr[4] & 0x30) == 0x20;
}

bool Z8530::transmitterEnabled(int channel) const noexcept
{
    const auto& c = channels[channel];
    if ((c.wr[5] & 0x08) == 0) return false;
    return (c.wr[3] & 0x20) == 0 || c.cts || (c.wr[14] & 0x18) != 0;
}

bool Z8530::receiverEnabled(int channel) const noexcept
{
    const auto& c = channels[channel];
    if ((c.wr[3] & 0x01) == 0) return false;
    return (c.wr[3] & 0x20) == 0 || c.dcd || (c.wr[14] & 0x10) != 0;
}

std::uint64_t Z8530::brgPeriod(int channel) const noexcept
{
    const auto& c = channels[channel];
    if ((c.wr[14] & 0x01) == 0) return 0;
    const std::uint64_t source = (c.wr[14] & 0x02) != 0 ? 1 : c.rtxcPeriod;
    const std::uint64_t timeConstant = c.wr[12] | (c.wr[13] << 8);
    return 2 * (timeConstant + 2) * source;
}

std::uint64_t Z8530::clockPeriod(int channel) const noexcept
{
    const auto& c = channels[channel];
    switch ((c.wr[11] >> 3) & 0x03) {
    case 0: return c.rtxcPeriod;
    case 1: return c.trxcPeriod;
    case 2: return brgPeriod(channel);
    default: return 0;
    }
}

// Time of one symbol on the line in PCLK cycles; bits is the data bit count of an asynchronous
// character.
std::uint64_t Z8530::symbolTime(int channel, const LineSymbol& symbol, int bits) const noexcept
{
    const auto& c = channels[channel];
    std::uint64_t halfBits = 16;
    std::uint64_t multiplier = 1;
    if (asyncMode(channel) && symbol.kind == LineSymbol::Kind::Data) {
        static constexpr std::uint64_t StopHalfBits[4] = {0, 2, 3, 4};
        halfBits = 2 * (1 + static_cast<std::uint64_t>(bits) + (c.wr[4] & 0x01)) + StopHalfBits[(c.wr[4] >> 2) & 0x03];
        static constexpr std::uint64_t Multipliers[4] = {1, 16, 32, 64};
        multiplier = Multipliers[c.wr[4] >> 6];
    }
    return clockPeriod(channel) * multiplier * halfBits / 2;
}

// Bits per transmitted character (WR5 bits 5-6); the five-or-less setting codes the count in the
// upper bits: 000DDDDD five, 1000DDDD four, 11000DDD three, 111000DD two, 1111000D one.
int Z8530::transmitBits(int channel, std::uint8_t value, std::uint8_t& data) const noexcept
{
    static constexpr int Bits[4] = {5, 7, 6, 8};
    int bits = Bits[(channels[channel].wr[5] >> 5) & 0x03];
    if (bits == 5) {
        int ones = 0;
        while (ones < 4 && (value & (0x80 >> ones)) != 0) ++ones;
        bits = 5 - ones;
    }
    data = static_cast<std::uint8_t>(value & lowMask(bits));
    return bits;
}

// Parity bit that makes the character even (WR4 bit 1 = 1) or odd.
bool Z8530::parityOf(int channel, std::uint8_t data, int bits) const noexcept
{
    int ones = 0;
    for (int bit = 0; bit < bits; ++bit) ones += (data >> bit) & 1;
    const bool even = (channels[channel].wr[4] & 0x02) != 0;
    return even ? (ones & 1) != 0 : (ones & 1) == 0;
}

std::uint16_t Z8530::crcCcitt(std::uint16_t crc, std::uint8_t data) noexcept
{
    crc ^= data;
    for (int bit = 0; bit < 8; ++bit) {
        crc = (crc & 1) != 0 ? static_cast<std::uint16_t>((crc >> 1) ^ 0x8408) : static_cast<std::uint16_t>(crc >> 1);
    }
    return crc;
}

// ------------------------------------------------------------------------------------------------
// CPU side

std::uint8_t Z8530::readControl(int channel) noexcept
{
    auto& c = channels[channel];
    const int reg = c.pointer;
    c.pointer = 0;
    if (reg == 8) return readData(channel);
    return readRegister(channel, reg);
}

void Z8530::writeControl(int channel, std::uint8_t value) noexcept
{
    auto& c = channels[channel];
    if (c.pointer == 0) {
        c.pointer = static_cast<std::uint8_t>((value & 0x07) | (((value >> 3) & 0x07) == 1 ? 0x08 : 0x00));
        command(channel, value);
    } else {
        const int reg = c.pointer;
        c.pointer = 0;
        writeRegister(channel, reg, value);
    }
    updateRx(ChannelA);
    updateRx(ChannelB);
    updateExtStatus(ChannelA);
    updateExtStatus(ChannelB);
}

std::uint8_t Z8530::readData(int channel) noexcept
{
    auto& c = channels[channel];
    if (c.locked) return c.fifo.front().data;
    if (c.fifo.empty()) return c.lastData;
    const auto entry = c.fifo.front();
    c.lastData = entry.data;
    c.lastStatus = entry.status;
    const int mode = rxMode(c.wr[1]);
    if ((mode == 1 || mode == 3) && special(channel, entry.status)) {
        // The character with the special condition stays at the exit of the FIFO until Error Reset.
        c.locked = true;
        c.firstCharArmed = false;
    } else {
        popReceived(channel);
    }
    updateRx(channel);
    updateExtStatus(channel);
    return entry.data;
}

void Z8530::writeData(int channel, std::uint8_t value) noexcept
{
    auto& c = channels[channel];
    c.buffer = value;
    c.bufferFull = true;
    c.txIp = false;
}

void Z8530::command(int channel, std::uint8_t value) noexcept
{
    auto& c = channels[channel];
    const std::uint16_t preset = (c.wr[10] & 0x80) != 0 ? 0xFFFF : 0x0000;
    switch ((value >> 3) & 0x07) {
    case 2: // Reset External/Status Interrupts: open the latches; a change that persists interrupts again.
        if (c.extLatched) {
            c.extLatched = false;
            c.extIp = false;
            c.lastRr0 = c.latchedRr0;
        }
        break;
    case 3: // Send Abort (SDLC)
        c.bufferFull = false;
        c.trailer.clear();
        c.trailer.push_back({LineSymbol::Kind::Abort});
        c.trailerIsCrc = false;
        c.frameOpen = false;
        c.underrunLatch = true;
        break;
    case 4: c.firstCharArmed = true; break;
    case 5: c.txIp = false; break;
    case 6: // Error Reset
        c.latchedErrors = 0;
        c.lastStatus &= static_cast<std::uint8_t>(~(Rr1EndOfFrame | Rr1CrcFraming | Rr1Overrun | Rr1Parity));
        if (c.locked) {
            c.locked = false;
            popReceived(channel);
        }
        break;
    case 7: // Reset Highest IUS
        for (int source = 0; source < SourceCount; ++source) {
            if (sourceUnderService(source)) {
                channels[source / 3].ius[source % 3] = false;
                break;
            }
        }
        break;
    default: break;
    }
    switch (value >> 6) {
    case 1: c.rxCrc = preset; break;
    case 2: c.txCrc = preset; break;
    case 3:
        if (transmitterEnabled(channel)) c.underrunLatch = false;
        break;
    default: break;
    }
}

void Z8530::writeRegister(int channel, int reg, std::uint8_t value) noexcept
{
    auto& c = channels[channel];
    switch (reg) {
    case 1: {
        const int old = rxMode(c.wr[1]);
        c.wr[1] = value;
        if (rxMode(value) == 1 && old != 1) c.firstCharArmed = true;
        break;
    }
    case 2: vector = value; break;
    case 3: {
        const std::uint8_t old = c.wr[3];
        c.wr[3] = value;
        const bool enabled = (value & 0x01) != 0;
        if ((value & 0x10) != 0 || !enabled || (old & 0x01) == 0) {
            c.hunting = true;
            c.inFrame = false;
            c.hasPending = false;
        }
        break;
    }
    case 5: {
        const std::uint8_t old = c.wr[5];
        c.wr[5] = value;
        if (((old ^ value) & 0x10) != 0) {
            emit(channel, {(value & 0x10) != 0 ? LineSymbol::Kind::BreakStart : LineSymbol::Kind::BreakEnd});
        }
        if ((value & 0x08) == 0) c.underrunLatch = true;
        break;
    }
    case 8: writeData(channel, value); break;
    case 9:
        switch (value >> 6) {
        case 1: resetChannel(ChannelB, false); break;
        case 2: resetChannel(ChannelA, false); break;
        case 3: reset(); break;
        default: break;
        }
        wr9 = static_cast<std::uint8_t>(value & 0x1F);
        break;
    default: c.wr[reg] = value; break;
    }
}

std::uint8_t Z8530::readRegister(int channel, int reg) const noexcept
{
    const auto& c = channels[channel];
    switch (reg & 0x0F) {
    case 0:
    case 4: return rr0(channel);
    case 1:
    case 5: return rr1(channel);
    case 2:
    case 6: return channel == ChannelA ? vector : modifiedVector(highestPending(false));
    case 3:
    case 7: return channel == ChannelA ? rr3() : 0x00;
    case 9:
    case 13: return c.wr[13];
    case 10:
    case 14: return 0x00;
    case 11:
    case 15: return static_cast<std::uint8_t>(c.wr[15] & 0xFA);
    case 12: return c.wr[12];
    default: return 0xFF;
    }
}

std::uint8_t Z8530::liveRr0(int channel) const noexcept
{
    const auto& c = channels[channel];
    std::uint8_t value = 0;
    if (!c.fifo.empty()) value |= Rr0RxAvailable;
    if (c.zeroCount && (c.wr[15] & 0x02) != 0) value |= Rr0ZeroCount;
    if (!c.bufferFull && !c.trailerIsCrc) value |= Rr0TxEmpty;
    if (c.dcd) value |= Rr0Dcd;
    if (sdlcMode(channel) && c.hunting) value |= Rr0SyncHunt;
    if (c.cts) value |= Rr0Cts;
    if (c.underrunLatch) value |= Rr0TxUnderrun;
    if (c.breakAbort) value |= Rr0BreakAbort;
    return value;
}

std::uint8_t Z8530::rr0(int channel) const noexcept
{
    const auto& c = channels[channel];
    const auto live = liveRr0(channel);
    if (!c.extLatched) return live;
    const auto latched = static_cast<std::uint8_t>(c.wr[15] & 0xF8);
    return static_cast<std::uint8_t>((live & ~latched) | (c.latchedRr0 & latched));
}

std::uint8_t Z8530::rr1(int channel) const noexcept
{
    const auto& c = channels[channel];
    const std::uint8_t status = c.fifo.empty() ? c.lastStatus : c.fifo.front().status;
    std::uint8_t value = static_cast<std::uint8_t>(status | c.latchedErrors | Rr1Residue8);
    if (!asyncMode(channel) || (!c.bufferFull && !c.shifting)) value |= Rr1AllSent;
    return value;
}

std::uint8_t Z8530::rr3() const noexcept
{
    const auto& a = channels[ChannelA];
    const auto& b = channels[ChannelB];
    return static_cast<std::uint8_t>((b.extIp ? 0x01 : 0) | (b.txIp ? 0x02 : 0) | (b.rxIp ? 0x04 : 0) |
                                     (a.extIp ? 0x08 : 0) | (a.txIp ? 0x10 : 0) | (a.rxIp ? 0x20 : 0));
}

// ------------------------------------------------------------------------------------------------
// Interrupts: source 0-2 Rx, Tx, Ext/Status of channel A, 3-5 those of channel B.

bool Z8530::sourcePending(int source) const noexcept
{
    const auto& c = channels[source / 3];
    switch (source % 3) {
    case 0: return c.rxIp;
    case 1: return c.txIp;
    default: return c.extIp;
    }
}

bool Z8530::sourceUnderService(int source) const noexcept
{
    return channels[source / 3].ius[source % 3];
}

int Z8530::highestPending(bool skipServiced) const noexcept
{
    for (int source = 0; source < SourceCount; ++source) {
        if (skipServiced && sourceUnderService(source)) return -1;
        if (sourcePending(source)) return source;
    }
    return -1;
}

std::uint8_t Z8530::vectorStatus(int source) const noexcept
{
    switch (source) {
    case 0: return channels[ChannelA].rxSpecial ? 7 : 6;
    case 1: return 4;
    case 2: return 5;
    case 3: return channels[ChannelB].rxSpecial ? 3 : 2;
    case 4: return 0;
    case 5: return 1;
    default: return 3;
    }
}

std::uint8_t Z8530::modifiedVector(int source) const noexcept
{
    const std::uint8_t code = vectorStatus(source);
    if ((wr9 & 0x10) != 0) {
        const auto high = static_cast<std::uint8_t>(((code & 0x04) << 2) | ((code & 0x02) << 4) | ((code & 0x01) << 6));
        return static_cast<std::uint8_t>((vector & 0x8F) | high);
    }
    return static_cast<std::uint8_t>((vector & 0xF1) | (code << 1));
}

bool Z8530::interruptRequest() const noexcept
{
    return (wr9 & 0x08) != 0 && highestPending(true) >= 0;
}

std::uint8_t Z8530::interruptAcknowledge() noexcept
{
    if ((wr9 & 0x08) == 0) return 0xFF;
    const int source = highestPending(true);
    if (source < 0) return 0xFF;
    channels[source / 3].ius[source % 3] = true;
    if ((wr9 & 0x02) != 0) return 0xFF;
    return (wr9 & 0x01) != 0 ? modifiedVector(source) : vector;
}

// ------------------------------------------------------------------------------------------------
// Receiver

bool Z8530::special(int channel, std::uint8_t status) const noexcept
{
    const auto& c = channels[channel];
    if ((status & Rr1Overrun) != 0) return true;
    if (asyncMode(channel) && (status & Rr1CrcFraming) != 0) return true;
    if (sdlcMode(channel) && (status & Rr1EndOfFrame) != 0) return true;
    return (status & Rr1Parity) != 0 && (c.wr[1] & 0x04) != 0 && (c.wr[4] & 0x01) != 0;
}

void Z8530::pushReceived(int channel, std::uint8_t data, std::uint8_t status) noexcept
{
    auto& c = channels[channel];
    if (c.fifo.size() >= FifoDepth) {
        // Overrun: the newest character overwrites the last place of the FIFO and is flagged.
        c.fifo.back() = {data, static_cast<std::uint8_t>(status | Rr1Overrun)};
    } else {
        c.fifo.push_back({data, status});
    }
}

void Z8530::popReceived(int channel) noexcept
{
    auto& c = channels[channel];
    if (c.fifo.empty()) return;
    c.latchedErrors |= static_cast<std::uint8_t>(c.fifo.front().status & (Rr1Overrun | Rr1Parity));
    c.fifo.pop_front();
    c.firstCharArmed = c.firstCharArmed && rxMode(c.wr[1]) != 1;
}

void Z8530::updateRx(int channel) noexcept
{
    auto& c = channels[channel];
    c.rxIp = false;
    c.rxSpecial = false;
    if (c.fifo.empty()) return;
    switch (rxMode(c.wr[1])) {
    case 1:
        c.rxIp = c.locked || c.firstCharArmed;
        c.rxSpecial = c.locked;
        break;
    case 2:
        c.rxIp = true;
        c.rxSpecial = special(channel, c.fifo.front().status);
        break;
    case 3:
        c.rxIp = c.locked;
        c.rxSpecial = c.locked;
        break;
    default: break;
    }
}

void Z8530::receive(int channel, const LineSymbol& symbol) noexcept
{
    auto& c = channels[channel];
    if ((c.wr[14] & 0x08) != 0) c.output.push_back(symbol);
    if ((c.wr[14] & 0x10) == 0) acceptSymbol(channel, symbol);
    updateRx(channel);
    updateExtStatus(channel);
}

void Z8530::acceptSymbol(int channel, const LineSymbol& symbol) noexcept
{
    if (!receiverEnabled(channel)) return;
    if (asyncMode(channel)) {
        acceptAsync(channel, symbol);
    } else if (sdlcMode(channel)) {
        acceptSdlc(channel, symbol);
    }
}

void Z8530::acceptAsync(int channel, const LineSymbol& symbol) noexcept
{
    auto& c = channels[channel];
    switch (symbol.kind) {
    case LineSymbol::Kind::BreakStart:
        c.breakAbort = true;
        updateExtStatus(channel);
        return;
    case LineSymbol::Kind::BreakEnd:
        c.breakAbort = false;
        updateExtStatus(channel);
        pushReceived(channel, 0x00, 0);
        return;
    case LineSymbol::Kind::Data: break;
    default: return;
    }
    static constexpr int Bits[4] = {5, 7, 6, 8};
    const int bits = Bits[c.wr[3] >> 6];
    const bool parity = (c.wr[4] & 0x01) != 0;
    const auto value = static_cast<std::uint8_t>(symbol.value & lowMask(bits));
    std::uint8_t data = value;
    int used = bits;
    if (parity && bits < 8) data |= static_cast<std::uint8_t>((symbol.parityBit ? 1 : 0) << used++);
    if (used < 8) data |= static_cast<std::uint8_t>(0xFF << used);
    std::uint8_t status = 0;
    if (parity && parityOf(channel, value, bits) != symbol.parityBit) status |= Rr1Parity;
    if (!symbol.framingOk) status |= Rr1CrcFraming;
    if ((c.wr[3] & 0x02) != 0 && data == c.wr[6]) return;
    pushReceived(channel, data, status);
}

void Z8530::acceptSdlc(int channel, const LineSymbol& symbol) noexcept
{
    auto& c = channels[channel];
    switch (symbol.kind) {
    case LineSymbol::Kind::Flag:
        c.hunting = false;
        if (c.inFrame && c.hasPending) {
            std::uint8_t status = Rr1EndOfFrame;
            if (c.rxCrc != SdlcResidue) status |= Rr1CrcFraming;
            pushReceived(channel, c.pending.data, status);
        }
        c.hasPending = false;
        c.inFrame = true;
        c.firstByte = true;
        c.frameAccepted = true;
        c.rxCrc = (c.wr[10] & 0x80) != 0 ? 0xFFFF : 0x0000;
        return;
    case LineSymbol::Kind::Abort:
        c.hasPending = false;
        c.inFrame = false;
        c.hunting = true;
        // Both edges of the abort sequence reach the Ext/Status logic.
        c.breakAbort = true;
        updateExtStatus(channel);
        c.breakAbort = false;
        updateExtStatus(channel);
        return;
    case LineSymbol::Kind::Data: break;
    default: return;
    }
    if (c.hunting || !c.inFrame) return;
    if (c.firstByte) {
        c.firstByte = false;
        if ((c.wr[3] & 0x04) != 0) {
            const std::uint8_t mask = (c.wr[3] & 0x02) != 0 ? 0xF0 : 0xFF;
            c.frameAccepted = symbol.value == 0xFF || (symbol.value & mask) == (c.wr[6] & mask);
        }
    }
    if (!c.frameAccepted) return;
    c.rxCrc = crcCcitt(c.rxCrc, symbol.value);
    // A byte goes into the FIFO when the next one follows; the last one waits for the closing flag.
    if (c.hasPending) pushReceived(channel, c.pending.data, 0);
    c.pending = {symbol.value, 0};
    c.hasPending = true;
}

// ------------------------------------------------------------------------------------------------
// Ext/Status

void Z8530::updateExtStatus(int channel) noexcept
{
    auto& c = channels[channel];
    const auto live = liveRr0(channel);
    const auto enabled = static_cast<std::uint8_t>(c.wr[15] & 0xF8);
    auto changed = static_cast<std::uint8_t>((live ^ c.lastRr0) & enabled);
    // Only the 0-to-1 edge of Tx Underrun/EOM and of Zero Count interrupts.
    if ((live & Rr0TxUnderrun) == 0) changed &= static_cast<std::uint8_t>(~Rr0TxUnderrun);
    const bool zeroCountEdge = (live & Rr0ZeroCount) != 0 && (c.lastRr0 & Rr0ZeroCount) == 0;
    if (changed != 0 || zeroCountEdge) {
        if (!c.extLatched) {
            c.extLatched = true;
            c.latchedRr0 = live;
            if ((c.wr[1] & 0x01) != 0) c.extIp = true;
        } else if ((changed & Rr0BreakAbort) != 0) {
            c.latchedRr0 = static_cast<std::uint8_t>((c.latchedRr0 & ~Rr0BreakAbort) | (live & Rr0BreakAbort));
            if ((c.wr[1] & 0x01) != 0) c.extIp = true;
        }
    }
    c.lastRr0 = live;
}

// ------------------------------------------------------------------------------------------------
// Transmitter, clocks and line

void Z8530::emit(int channel, const LineSymbol& symbol) noexcept
{
    auto& c = channels[channel];
    if ((c.wr[14] & 0x08) == 0) c.output.push_back(symbol);
    if ((c.wr[14] & 0x10) != 0) {
        acceptSymbol(channel, symbol);
        updateRx(channel);
    }
}

void Z8530::startShift(int channel, const LineSymbol& symbol, int bits) noexcept
{
    auto& c = channels[channel];
    c.shifting = true;
    c.shiftSymbol = symbol;
    c.shiftRemaining = symbolTime(channel, symbol, bits);
    if (c.shiftRemaining == 0) c.shiftRemaining = 1;
}

void Z8530::tickTransmitter(int channel) noexcept
{
    auto& c = channels[channel];
    if (c.shifting) {
        if (--c.shiftRemaining != 0) return;
        c.shifting = false;
        emit(channel, c.shiftSymbol);
    }
    if (clockPeriod(channel) == 0) return;
    if (!c.trailer.empty()) {
        const auto symbol = c.trailer.front();
        c.trailer.pop_front();
        startShift(channel, symbol);
        if (c.trailer.empty() && c.trailerIsCrc) {
            // The CRC has left the shift register and the closing flag is loaded.
            c.trailerIsCrc = false;
            if ((c.wr[1] & 0x02) != 0) c.txIp = true;
        }
        return;
    }
    if (!transmitterEnabled(channel)) return;
    const bool sdlc = sdlcMode(channel);
    if (c.bufferFull) {
        if (sdlc && !c.frameOpen) {
            c.frameOpen = true;
            startShift(channel, {LineSymbol::Kind::Flag, 0x7E});
            return;
        }
        c.bufferFull = false;
        LineSymbol symbol{LineSymbol::Kind::Data, c.buffer};
        int bits = 8;
        if (sdlc) {
            if ((c.wr[5] & 0x01) != 0) c.txCrc = crcCcitt(c.txCrc, c.buffer);
        } else if (asyncMode(channel)) {
            bits = transmitBits(channel, c.buffer, symbol.value);
            symbol.parityBit = (c.wr[4] & 0x01) != 0 && parityOf(channel, symbol.value, bits);
        }
        startShift(channel, symbol, bits);
        if ((c.wr[1] & 0x02) != 0) c.txIp = true;
        return;
    }
    if (sdlc && c.frameOpen) {
        // Transmit underrun: CRC (or abort) and flag if the latch was reset, otherwise a flag.
        c.frameOpen = false;
        if (!c.underrunLatch) {
            c.underrunLatch = true;
            if ((c.wr[10] & 0x04) != 0) {
                c.trailer.push_back({LineSymbol::Kind::Abort});
            } else if ((c.wr[5] & 0x01) != 0) {
                const auto crc = static_cast<std::uint16_t>(~c.txCrc);
                c.trailer.push_back({LineSymbol::Kind::Data, static_cast<std::uint8_t>(crc & 0xFF)});
                c.trailer.push_back({LineSymbol::Kind::Data, static_cast<std::uint8_t>(crc >> 8)});
                c.trailerIsCrc = true;
            }
        }
        c.trailer.push_back({LineSymbol::Kind::Flag, 0x7E});
    }
}

void Z8530::tickBrg(int channel) noexcept
{
    auto& c = channels[channel];
    const auto period = brgPeriod(channel);
    if (period == 0) {
        c.brgPhase = 0;
        c.zeroCount = false;
        return;
    }
    c.brgPhase = (c.brgPhase + 1) % (period / 2);
    c.zeroCount = c.brgPhase == 0;
}

void Z8530::tick() noexcept
{
    for (int channel = ChannelA; channel <= ChannelB; ++channel) {
        tickBrg(channel);
        tickTransmitter(channel);
        updateRx(channel);
        updateExtStatus(channel);
    }
}

bool Z8530::hasTransmitted(int channel) const noexcept
{
    return !channels[channel].output.empty();
}

LineSymbol Z8530::takeTransmitted(int channel) noexcept
{
    auto& output = channels[channel].output;
    if (output.empty()) return {};
    const auto symbol = output.front();
    output.pop_front();
    return symbol;
}

void Z8530::setCts(int channel, bool active) noexcept
{
    channels[channel].cts = active;
    updateExtStatus(channel);
}

void Z8530::setDcd(int channel, bool active) noexcept
{
    channels[channel].dcd = active;
    updateExtStatus(channel);
}

bool Z8530::dtr(int channel) const noexcept
{
    const auto& c = channels[channel];
    return (c.wr[5] & 0x80) != 0 && (c.wr[14] & 0x04) == 0;
}

bool Z8530::rts(int channel) const noexcept
{
    return (channels[channel].wr[5] & 0x02) != 0;
}

void Z8530::setRtxcPeriod(int channel, std::uint32_t pclkCycles) noexcept
{
    channels[channel].rtxcPeriod = pclkCycles;
}

void Z8530::setTrxcPeriod(int channel, std::uint32_t pclkCycles) noexcept
{
    channels[channel].trxcPeriod = pclkCycles;
}

} // namespace z8530
