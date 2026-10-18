#include "SVI806/SVI806.hpp"

#include "SVI3X8/ExpansionPort.hpp"

#include <algorithm>

namespace svi806 {

namespace {

// Limits for a picture from odd register values.
constexpr unsigned MaxColumns = 128;
constexpr unsigned MaxRows = 64;
constexpr unsigned MaxRasterLines = 16;

} // namespace

SVI806::SVI806()
    : controller(svi3x8::CpuClockHz, SVI806CharacterClockHz)
{
}

bool SVI806::loadCharacterRom(const std::vector<std::uint8_t>& image)
{
    if (image.size() != SVI806CharacterRomSize) return false;
    std::copy(image.begin(), image.end(), characterRom.begin());
    return true;
}

const mc6845::MC6845& SVI806::crtc() const noexcept
{
    return controller;
}

bool SVI806::videoRamEnabled() const noexcept
{
    return ramEnabled;
}

void SVI806::reset()
{
    controller.reset();
    ramEnabled = false;
}

svi3x8::ExpansionMemoryCycle SVI806::readMemory(std::uint16_t address, std::uint8_t) const
{
    if (address < 0xF000 || !ramEnabled) return {};
    return {videoRam[address & 0x7FF], false, true};
}

svi3x8::ExpansionMemoryCycle SVI806::writeMemory(std::uint16_t address, std::uint8_t value, std::uint8_t)
{
    if (address < 0xF000 || !ramEnabled) return {};
    videoRam[address & 0x7FF] = value;
    return {0xFF, false, true};
}

std::uint8_t SVI806::readIo(std::uint8_t port)
{
    return port == 0x51 ? controller.readRegister() : static_cast<std::uint8_t>(0xFF);
}

void SVI806::writeIo(std::uint8_t port, std::uint8_t value)
{
    if (port == 0x50) controller.selectRegister(value);
    if (port == 0x51) controller.writeRegister(value);
    if (port == 0x58) ramEnabled = value != 0;
}

void SVI806::tick()
{
    controller.tick();
}

bool SVI806::interruptRequest() const
{
    return false;
}

std::uint8_t SVI806::peekVideoRam(std::uint16_t offset) const noexcept
{
    return videoRam[offset & 0x7FF];
}

MonochromeFrame SVI806::render() const
{
    const unsigned columns = std::min(controller.charactersPerRow(), MaxColumns);
    const unsigned rows = std::min(controller.displayedRows(), MaxRows);
    const unsigned lines = std::min(controller.rasterLinesPerRow(), MaxRasterLines);
    MonochromeFrame frame;
    frame.width = columns * 8;
    frame.height = rows * lines;
    frame.pixels.assign(static_cast<std::size_t>(frame.width) * frame.height, 0);
    const auto start = controller.startAddress();
    const auto cursor = controller.cursorAddress();
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            const auto address = static_cast<std::uint16_t>(start + row * controller.charactersPerRow() + column);
            const auto character = videoRam[address & 0x7FF];
            for (unsigned line = 0; line < lines; ++line) {
                auto bits = characterRom[(static_cast<std::size_t>(character) << 4) | (line & 0x0F)];
                if ((address & 0x3FFF) == cursor && controller.cursorVisible(line)) bits = 0xFF;
                auto* out = &frame.pixels[(static_cast<std::size_t>(row) * lines + line) * frame.width + column * 8];
                for (unsigned bit = 0; bit < 8; ++bit) out[bit] = (bits >> (7 - bit)) & 1;
            }
        }
    }
    return frame;
}

} // namespace svi806
