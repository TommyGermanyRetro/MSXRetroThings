#include "SVI803/SVI803.hpp"

namespace svi803 {

void SVI803::reset()
{
}

svi3x8::ExpansionMemoryCycle SVI803::readMemory(std::uint16_t address, std::uint8_t) const
{
    if (address < 0x8000 || address >= 0xC000) return {};
    return {ram[address - 0x8000], false, false};
}

svi3x8::ExpansionMemoryCycle SVI803::writeMemory(std::uint16_t address, std::uint8_t value, std::uint8_t)
{
    if (address >= 0x8000 && address < 0xC000) ram[address - 0x8000] = value;
    return {};
}

std::uint8_t SVI803::readIo(std::uint8_t)
{
    return 0xFF;
}

void SVI803::writeIo(std::uint8_t, std::uint8_t)
{
}

void SVI803::tick()
{
}

bool SVI803::interruptRequest() const
{
    return false;
}

} // namespace svi803
