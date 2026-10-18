#include "LANIF01/LanCard.hpp"

namespace lanif01 {

bool LanCard::loadRom(const std::vector<std::uint8_t>& image)
{
    return boot.loadRom(image);
}

bool LanCard::romActive() const noexcept
{
    return boot.romActive();
}

PiaPrinterPort& LanCard::printerPort() noexcept
{
    return printer;
}

const PiaPrinterPort& LanCard::printerPort() const noexcept
{
    return printer;
}

void LanCard::reset()
{
    boot.reset();
    printer.reset();
}

svi3x8::ExpansionMemoryCycle LanCard::readMemory(std::uint16_t address, std::uint8_t bankLines) const
{
    return boot.readMemory(address, bankLines);
}

svi3x8::ExpansionMemoryCycle LanCard::writeMemory(std::uint16_t address, std::uint8_t value, std::uint8_t bankLines)
{
    return boot.writeMemory(address, value, bankLines);
}

std::uint8_t LanCard::readIo(std::uint8_t port)
{
    return static_cast<std::uint8_t>(boot.readIo(port) & printer.readIo(port));
}

void LanCard::writeIo(std::uint8_t port, std::uint8_t value)
{
    boot.writeIo(port, value);
    printer.writeIo(port, value);
}

void LanCard::tick()
{
    boot.tick();
    printer.tick();
}

bool LanCard::interruptRequest() const
{
    return boot.interruptRequest() || printer.interruptRequest();
}

} // namespace lanif01
