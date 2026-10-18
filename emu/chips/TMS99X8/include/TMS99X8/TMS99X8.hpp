#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tms99x8 {

inline constexpr std::size_t VramSize = 0x4000;
inline constexpr std::size_t ScreenWidth = 256;
inline constexpr std::size_t ScreenHeight = 192;
inline constexpr std::uint32_t MasterClockHz = 21477270;
inline constexpr std::uint16_t DotsPerScanline = 342;
inline constexpr std::uint8_t MasterTicksPerDot = 4;
inline constexpr std::uint32_t MasterTicksPerScanline =
    DotsPerScanline * MasterTicksPerDot;
inline constexpr std::uint16_t HorizontalDisplayStart = 37;

inline constexpr std::uint16_t MaxScanlines = 313;

using FrameBuffer = std::array<std::uint8_t, ScreenWidth * ScreenHeight>;
// Backdrop colour (register 7 bits 0-3) per dot of every raster line, indexed line * dots + dot.
using BackdropRaster = std::array<std::uint8_t, DotsPerScanline * MaxScanlines>;

enum class Model : std::uint8_t {
    Ntsc,
    Pal
};

struct YpbprPixel {
    std::uint8_t y = 0;
    std::int8_t pb = 0;
    std::int8_t pr = 0;
};

using YpbprFrame = std::array<YpbprPixel, ScreenWidth * ScreenHeight>;

struct Outputs {
    bool interruptRequest = false;
    std::uint8_t status = 0;
    std::uint16_t vramAddress = 0;
    std::uint16_t scanline = 0;
    std::uint16_t dot = 0;
    std::uint16_t scanlinesPerFrame = 262;
    std::uint16_t displayStartScanline = 40;
};

class TMS99X8 {
public:
    explicit TMS99X8(Model model = Model::Pal);
    virtual ~TMS99X8();

    TMS99X8(const TMS99X8&) = delete;
    TMS99X8& operator=(const TMS99X8&) = delete;
    TMS99X8(TMS99X8&&) noexcept;
    TMS99X8& operator=(TMS99X8&&) noexcept;

    void reset();

    // Port 0 is the VRAM data port; port 1 is the control/status port.
    [[nodiscard]] std::uint8_t read(std::uint8_t port);
    void write(std::uint8_t port, std::uint8_t value);

    // Advances one VDP master clock; a dot is emitted every MasterTicksPerDot clocks.
    void advanceMasterTick() noexcept;
    // Advances one VDP dot; one raster line contains DotsPerScanline dots.
    void advanceDot() noexcept;
    // Advances to the next raster line.
    void advanceScanline() noexcept;

    // Returns palette indices (0-15), row-major, and updates sprite status flags.
    [[nodiscard]] FrameBuffer renderFrame();
    // Returns the frame assembled from visible raster lines as the beam advanced.
    [[nodiscard]] const FrameBuffer& scanoutFrame() const noexcept;
    // Returns the backdrop colour the beam saw at each dot, including the border around the
    // active display; register 7 changes take effect at the next dot.
    [[nodiscard]] const BackdropRaster& backdropRaster() const noexcept;
    [[nodiscard]] YpbprFrame renderYpbprFrame();

    [[nodiscard]] Outputs outputs() const noexcept;
    [[nodiscard]] Model model() const noexcept;
    [[nodiscard]] std::uint32_t fieldRateHz() const noexcept;
    [[nodiscard]] std::uint8_t registerValue(std::uint8_t index) const noexcept;
    [[nodiscard]] std::uint8_t peekVram(std::uint16_t address) const noexcept;
    void pokeVram(std::uint16_t address, std::uint8_t value) noexcept;

    // Complete chip state (VRAM, registers, beam position, access timing, picture) for snapshots;
    // loadState() rejects data of another size or of the other model (NTSC/PAL).
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace tms99x8