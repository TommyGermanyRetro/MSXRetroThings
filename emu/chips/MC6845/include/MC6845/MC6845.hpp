#pragma once

#include <array>
#include <cstdint>

namespace mc6845 {

// Motorola MC6845 CRT controller. Port side: an address register selecting R0-R17 and a data
// register; R14-R17 (cursor address, light pen) read back, the others read 00h. The host clocks
// it with tick() at hostClockHz; it counts character clocks to fields for the cursor blink.
//
// R0 horizontal total, R1 characters per row, R2/R3 horizontal sync, R4 vertical total (rows),
// R5 vertical adjust (lines), R6 displayed rows, R7 vertical sync, R8 interlace, R9 last raster
// line of a row, R10/R11 cursor start (with blink mode in bits 5-6) and end line, R12/R13 start
// address, R14/R15 cursor address, R16/R17 light pen.
class MC6845 final {
public:
    MC6845(std::uint32_t hostClockHz, std::uint32_t characterClockHz);

    void reset();
    void selectRegister(std::uint8_t value) noexcept;
    void writeRegister(std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t readRegister() const noexcept;
    void tick() noexcept;

    [[nodiscard]] std::uint8_t registerValue(std::uint8_t index) const noexcept;
    [[nodiscard]] unsigned charactersPerRow() const noexcept;
    [[nodiscard]] unsigned displayedRows() const noexcept;
    [[nodiscard]] unsigned rasterLinesPerRow() const noexcept;
    [[nodiscard]] std::uint16_t startAddress() const noexcept;
    [[nodiscard]] std::uint16_t cursorAddress() const noexcept;
    // Whether the cursor shows on this raster line of its row in the current field.
    [[nodiscard]] bool cursorVisible(unsigned rasterLine) const noexcept;
    // Character clocks per field and the fields counted so far.
    [[nodiscard]] std::uint32_t characterClocksPerField() const noexcept;
    [[nodiscard]] std::uint64_t fieldCount() const noexcept;

private:
    std::uint32_t hostHz;
    std::uint32_t characterHz;
    std::array<std::uint8_t, 18> registers{};
    std::uint8_t selected = 0;
    std::uint64_t phase = 0;
    std::uint32_t characterClocks = 0;
    std::uint64_t fields = 0;
};

} // namespace mc6845
