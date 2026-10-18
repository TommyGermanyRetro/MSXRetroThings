#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>

namespace v9938 {

inline constexpr std::size_t VramSize = 0x20000;
inline constexpr std::uint32_t MasterClockHz = 21477270;
inline constexpr std::uint16_t DotsPerScanline = 342;
inline constexpr std::uint8_t MasterTicksPerDot = 4;
inline constexpr std::uint32_t MasterTicksPerScanline = DotsPerScanline * MasterTicksPerDot;
inline constexpr std::uint16_t PalScanlines = 313;
inline constexpr std::uint16_t NtscScanlines = 262;
inline constexpr std::uint8_t PaletteSize = 16;
inline constexpr std::size_t ScreenWidth = 512;
inline constexpr std::size_t MaxDisplayLines = 212;

// Display area as 0x0GRB colours; 256-dot modes fill every dot twice.
using FrameBuffer = std::array<std::uint16_t, ScreenWidth * MaxDisplayLines>;
// Border colours (0x0GRB) at the start of a raster line; only GRAPHIC 5 gives even and odd dots
// different colours.
struct BorderColours {
    std::uint16_t even = 0;
    std::uint16_t odd = 0;
};
using BorderRaster = std::array<BorderColours, PalScanlines>;

struct Outputs {
    bool interruptRequest = false;
};

class V9938 final {
public:
    V9938();
    ~V9938();

    V9938(const V9938&) = delete;
    V9938& operator=(const V9938&) = delete;
    V9938(V9938&&) noexcept;
    V9938& operator=(V9938&&) noexcept;

    void reset();

    // Port 0 is VRAM data, 1 control/status, 2 palette, 3 indirect register access.
    [[nodiscard]] std::uint8_t read(std::uint8_t port);
    void write(std::uint8_t port, std::uint8_t value);

    // Advances one VDP master clock; a dot is emitted every MasterTicksPerDot clocks.
    void advanceMasterTick() noexcept;

    [[nodiscard]] Outputs outputs() const noexcept;
    [[nodiscard]] std::uint8_t peekVram(std::uint32_t address) const noexcept;
    void pokeVram(std::uint32_t address, std::uint8_t value) noexcept;

    // Control registers R#0-R#23 and R#32-R#46; nonexistent registers read as 0.
    [[nodiscard]] std::uint8_t controlRegister(std::uint8_t index) const noexcept;
    // Status register S#index without the side effects of a port read.
    [[nodiscard]] std::uint8_t peekStatus(std::uint8_t index) const noexcept;
    // Palette entry as 0x0GRB with three bits per component.
    [[nodiscard]] std::uint16_t paletteEntry(std::uint8_t index) const noexcept;

    // Timing of the current field: PAL/NTSC is latched from R#9 at the start of every field.
    [[nodiscard]] bool palTiming() const noexcept;
    [[nodiscard]] std::uint16_t linesPerFrame() const noexcept;
    [[nodiscard]] std::uint32_t frameTick() const noexcept;
    [[nodiscard]] std::uint32_t displayStartTick() const noexcept;
    // Counts started fields; a field is complete when the counter advances.
    [[nodiscard]] std::uint32_t frameCount() const noexcept;
    // R#9 IL latched for the current field, and whether it is the second field (S#2 EO).
    [[nodiscard]] bool interlaced() const noexcept;
    [[nodiscard]] bool secondField() const noexcept;

    // Display lines are rendered one by one as the beam reaches them.
    [[nodiscard]] const FrameBuffer& frame() const noexcept;
    [[nodiscard]] std::uint16_t frameDisplayLines() const noexcept;
    [[nodiscard]] const BorderRaster& borderRaster() const noexcept;

    // Complete VRAM, register, palette, timing and command state for snapshots; loadState()
    // rejects data of another size.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace v9938
