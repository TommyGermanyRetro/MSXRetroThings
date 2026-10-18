#include "MC6845/MC6845.hpp"

namespace mc6845 {

namespace {

// Bits each register holds; R16 and R17 (light pen) are read-only.
constexpr std::array<std::uint8_t, 18> RegisterMasks{
    0xFF, 0xFF, 0xFF, 0x0F, 0x7F, 0x1F, 0x7F, 0x7F, 0x03, 0x1F, 0x7F, 0x1F, 0x3F, 0xFF, 0x3F, 0xFF, 0x00, 0x00};

} // namespace

MC6845::MC6845(std::uint32_t hostClockHz, std::uint32_t characterClockHz)
    : hostHz(hostClockHz), characterHz(characterClockHz)
{
}

void MC6845::reset()
{
    phase = 0;
    characterClocks = 0;
}

void MC6845::selectRegister(std::uint8_t value) noexcept
{
    selected = value & 0x1F;
}

void MC6845::writeRegister(std::uint8_t value) noexcept
{
    if (selected < registers.size()) registers[selected] = value & RegisterMasks[selected];
}

std::uint8_t MC6845::readRegister() const noexcept
{
    return selected >= 14 && selected <= 17 ? registers[selected] : static_cast<std::uint8_t>(0x00);
}

std::uint8_t MC6845::registerValue(std::uint8_t index) const noexcept
{
    return index < registers.size() ? registers[index] : static_cast<std::uint8_t>(0x00);
}

unsigned MC6845::charactersPerRow() const noexcept
{
    return registers[1];
}

unsigned MC6845::displayedRows() const noexcept
{
    return registers[6];
}

unsigned MC6845::rasterLinesPerRow() const noexcept
{
    return static_cast<unsigned>(registers[9]) + 1;
}

std::uint16_t MC6845::startAddress() const noexcept
{
    return static_cast<std::uint16_t>((registers[12] << 8) | registers[13]);
}

std::uint16_t MC6845::cursorAddress() const noexcept
{
    return static_cast<std::uint16_t>((registers[14] << 8) | registers[15]);
}

std::uint32_t MC6845::characterClocksPerField() const noexcept
{
    const std::uint32_t lines = (static_cast<std::uint32_t>(registers[4]) + 1) * rasterLinesPerRow() + registers[5];
    return (static_cast<std::uint32_t>(registers[0]) + 1) * lines;
}

std::uint64_t MC6845::fieldCount() const noexcept
{
    return fields;
}

// Blink modes in R10 bits 5-6: 00 steady, 01 no cursor, 10 every 16 fields, 11 every 32 fields.
bool MC6845::cursorVisible(unsigned rasterLine) const noexcept
{
    const unsigned first = registers[10] & 0x1F;
    const unsigned last = registers[11];
    if (rasterLine < first || rasterLine > last) return false;
    switch ((registers[10] >> 5) & 3) {
    case 0: return true;
    case 1: return false;
    case 2: return (fields / 8) % 2 == 0;
    default: return (fields / 16) % 2 == 0;
    }
}

void MC6845::tick() noexcept
{
    phase += characterHz;
    while (phase >= hostHz) {
        phase -= hostHz;
        if (++characterClocks >= characterClocksPerField()) {
            characterClocks = 0;
            ++fields;
        }
    }
}

} // namespace mc6845
