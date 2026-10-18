#include "LANIF01/LANIF01.hpp"

#include <algorithm>

namespace lanif01 {

bool LANIF01::loadRom(const std::vector<std::uint8_t>& image)
{
    if (image.size() != BootRomSize) return false;
    std::copy(image.begin(), image.end(), rom.begin());
    return true;
}

bool LANIF01::romActive() const noexcept
{
    return active;
}

void LANIF01::reset()
{
    active = true;
}

svi3x8::ExpansionMemoryCycle LANIF01::readMemory(std::uint16_t address, std::uint8_t) const
{
    if (!active || address >= 0x8000) return {};
    return {rom[address & (BootRomSize - 1)], true, false};
}

svi3x8::ExpansionMemoryCycle LANIF01::writeMemory(std::uint16_t, std::uint8_t, std::uint8_t)
{
    return {};
}

std::uint8_t LANIF01::readIo(std::uint8_t)
{
    return 0xFF;
}

void LANIF01::writeIo(std::uint8_t port, std::uint8_t)
{
    if (port == 0x6F) active = false;
}

void LANIF01::tick()
{
}

bool LANIF01::interruptRequest() const
{
    return false;
}

} // namespace lanif01
