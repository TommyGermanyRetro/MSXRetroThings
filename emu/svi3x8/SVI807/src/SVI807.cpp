#include "SVI807/SVI807.hpp"

namespace svi807 {

namespace {

// Bank lines from PSG port B, active low.
constexpr std::uint8_t Bank21 = 0x02;
constexpr std::uint8_t Bank22 = 0x04;
constexpr std::uint8_t Bank31 = 0x08;
constexpr std::uint8_t Bank32 = 0x10;

bool active(std::uint8_t bankLines, std::uint8_t line) noexcept
{
    return (bankLines & line) == 0;
}

} // namespace

SVI807::SVI807(SVI807Switches switches) noexcept
    : settings(switches)
{
}

const SVI807Switches& SVI807::switches() const noexcept
{
    return settings;
}

bool SVI807::answers(std::uint16_t address, std::uint8_t bankLines) const noexcept
{
    if (address < 0x8000) {
        return (settings.bank21 && active(bankLines, Bank21)) || (settings.bank31 && active(bankLines, Bank31));
    }
    return (settings.bank22 && active(bankLines, Bank22)) || (settings.bank32 && active(bankLines, Bank32)) ||
           settings.bank02;
}

bool SVI807::onlyBank02(std::uint8_t bankLines) const noexcept
{
    return !(settings.bank22 && active(bankLines, Bank22)) && !(settings.bank32 && active(bankLines, Bank32));
}

void SVI807::reset()
{
}

svi3x8::ExpansionMemoryCycle SVI807::readMemory(std::uint16_t address, std::uint8_t bankLines) const
{
    if (!answers(address, bankLines)) return {};
    if (address < 0x8000) return {lowHalf[address], true, false};
    return {highHalf[address - 0x8000], false, true, onlyBank02(bankLines)};
}

svi3x8::ExpansionMemoryCycle SVI807::writeMemory(std::uint16_t address, std::uint8_t value, std::uint8_t bankLines)
{
    if (!answers(address, bankLines)) return {};
    if (address < 0x8000) {
        lowHalf[address] = value;
        return {0xFF, true, false};
    }
    highHalf[address - 0x8000] = value;
    return {0xFF, false, true, onlyBank02(bankLines)};
}

std::uint8_t SVI807::readIo(std::uint8_t)
{
    return 0xFF;
}

void SVI807::writeIo(std::uint8_t, std::uint8_t)
{
}

void SVI807::tick()
{
}

bool SVI807::interruptRequest() const
{
    return false;
}

} // namespace svi807
