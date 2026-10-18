#pragma once

#include "MC6845/MC6845.hpp"
#include "SVI3X8/ExpansionPort.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace svi806 {

inline constexpr std::size_t SVI806CharacterRomSize = 4096;
inline constexpr std::uint32_t SVI806CharacterClockHz = 2000000;

// One picture of the 80-column card: width x height pixels, 1 = lit (green on the monitor).
struct MonochromeFrame {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<std::uint8_t> pixels;
};

// SVI-806 80-column card: MC6845 (address 50h, data 51h, 2 MHz character clock), 2 KiB video RAM in
// F000h-FFFFh (mirrored every 2 KiB) while port 58h holds a value other than 0, and a 4 KiB
// character ROM with 16 lines per character. The video RAM switches RAM bank 2 off there with
// /RAMDIS. The picture shows R1 characters by R6 rows of R9 + 1 lines, 8 pixels per character;
// the cursor fills its raster lines.
class SVI806 final : public svi3x8::ExpansionPort {
public:
    SVI806();

    [[nodiscard]] bool loadCharacterRom(const std::vector<std::uint8_t>& image);
    [[nodiscard]] const mc6845::MC6845& crtc() const noexcept;
    [[nodiscard]] bool videoRamEnabled() const noexcept;
    [[nodiscard]] MonochromeFrame render() const;
    // Video RAM byte at offset 0-7FFh (character code; code 0 is the space).
    [[nodiscard]] std::uint8_t peekVideoRam(std::uint16_t offset) const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    mc6845::MC6845 controller;
    std::array<std::uint8_t, 0x800> videoRam{};
    std::array<std::uint8_t, SVI806CharacterRomSize> characterRom{};
    bool ramEnabled = false;
};

} // namespace svi806
