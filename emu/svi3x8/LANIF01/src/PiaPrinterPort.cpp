#include "LANIF01/PiaPrinterPort.hpp"

namespace lanif01 {

namespace {

// BUSY (PB0) low: the printer is ready; the other status lines read high.
constexpr std::uint8_t PrinterStatus = 0xFE;

} // namespace

PiaPrinterPort::PiaPrinterPort()
{
    reset();
}

printer::Printer& PiaPrinterPort::printer() noexcept
{
    return device;
}

const printer::Printer& PiaPrinterPort::printer() const noexcept
{
    return device;
}

const mc6821::MC6821& PiaPrinterPort::pia() const noexcept
{
    return chip;
}

void PiaPrinterPort::reset()
{
    chip.reset();
    chip.setPortBInputs(PrinterStatus);
    device.reset();
    lastStrobe = !chip.ca2();
    device.writeStrobe(lastStrobe);
}

svi3x8::ExpansionMemoryCycle PiaPrinterPort::readMemory(std::uint16_t, std::uint8_t) const
{
    return {};
}

svi3x8::ExpansionMemoryCycle PiaPrinterPort::writeMemory(std::uint16_t, std::uint8_t, std::uint8_t)
{
    return {};
}

std::uint8_t PiaPrinterPort::readIo(std::uint8_t port)
{
    if (port < 0x60 || port > 0x63) return 0xFF;
    const auto value = chip.read(port & 3);
    followPins();
    return value;
}

void PiaPrinterPort::writeIo(std::uint8_t port, std::uint8_t value)
{
    if (port < 0x60 || port > 0x63) return;
    const bool dataRegister = port == 0x60 && (chip.read(1) & 0x04) != 0;
    chip.write(port & 3, value);
    if (dataRegister) device.writeData(static_cast<std::uint8_t>(~chip.portA()));
    followPins();
}

// The printer takes the byte on the rising /STROBE edge and answers with an /ACK pulse.
void PiaPrinterPort::followPins()
{
    const bool strobe = !chip.ca2();
    if (strobe != lastStrobe) {
        const auto printed = device.output().size();
        lastStrobe = strobe;
        device.writeStrobe(strobe);
        if (device.output().size() != printed) {
            chip.setCA1(false);
            chip.setCA1(true);
        }
    }
}

void PiaPrinterPort::tick()
{
}

bool PiaPrinterPort::interruptRequest() const
{
    return chip.irqA() || chip.irqB();
}

} // namespace lanif01
