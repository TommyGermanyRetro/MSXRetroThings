#pragma once

#include "AY8910/AY8910.hpp"
#include "CARTRIDGE/Cartridge.hpp"
#include "PRINTER/Printer.hpp"
#include "SVI728/Cassette.hpp"
#include "TMS99X8/TMS99X8.hpp"
#include "Z80Bus/Z80Bus.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace svi728 {

inline constexpr std::size_t AddressSpaceSize = 0x10000;
inline constexpr std::size_t PageSize = 0x4000;
inline constexpr std::size_t RamSize = 0x10000;
inline constexpr std::size_t MainRomSize = 0x8000;
inline constexpr std::size_t KeyboardRowCount = 16;
inline constexpr std::uint32_t VdpMasterClockHz = 21477270;
inline constexpr std::uint32_t CpuClockHz = VdpMasterClockHz / 6;
inline constexpr std::uint32_t PalFieldRateHz = 50;
inline constexpr std::uint8_t VdpMasterTicksPerCpuTick = 6;
inline constexpr std::uint8_t VdpMasterTicksPerDot =
    tms99x8::MasterTicksPerDot;
inline constexpr std::uint32_t AyMasterClockHz = (CpuClockHz + 1) / 2;
inline constexpr std::uint16_t VdpScanlinesPerPalField = 313;
inline constexpr std::uint32_t VdpDotsPerPalField =
    tms99x8::DotsPerScanline * VdpScanlinesPerPalField;
inline constexpr std::uint32_t VdpMasterTicksPerScanline =
    tms99x8::MasterTicksPerScanline;
inline constexpr std::uint32_t VdpMasterTicksPerPalField =
    VdpMasterTicksPerScanline * VdpScanlinesPerPalField;
inline constexpr std::uint32_t CpuTicksPerPalField =
    VdpMasterTicksPerPalField / VdpMasterTicksPerCpuTick;
static_assert(VdpMasterTicksPerPalField % VdpMasterTicksPerCpuTick == 0);
inline constexpr std::uint32_t NtscFieldRateHz = 60;
inline constexpr std::uint16_t VdpScanlinesPerNtscField = 262;
inline constexpr std::uint32_t VdpMasterTicksPerNtscField =
    VdpMasterTicksPerScanline * VdpScanlinesPerNtscField;
inline constexpr std::uint32_t CpuTicksPerNtscField =
    VdpMasterTicksPerNtscField / VdpMasterTicksPerCpuTick;
static_assert(VdpMasterTicksPerNtscField % VdpMasterTicksPerCpuTick == 0);
static_assert(VdpMasterClockHz % VdpMasterTicksPerCpuTick == 0);

// The European machine carries the TMS9929A (PAL, 50 Hz), the NTSC machine the TMS9918A (60 Hz).
enum class VideoStandard : std::uint8_t { Pal, Ntsc };

class SVI728 final {
public:
    explicit SVI728(VideoStandard standard = VideoStandard::Pal);
    ~SVI728();

    SVI728(const SVI728&) = delete;
    SVI728& operator=(const SVI728&) = delete;
    SVI728(SVI728&&) noexcept;
    SVI728& operator=(SVI728&&) noexcept;

    void reset();

    [[nodiscard]] bool loadMainRom(const std::vector<std::uint8_t>& image);
    // Plain ROM at a fixed address in slot 2 or 3.
    [[nodiscard]] bool loadSlotRom(std::uint8_t primarySlot, std::uint16_t baseAddress,
                                   const std::vector<std::uint8_t>& image);
    // ROM cartridge in slot 2 or 3 with the given mapper, detected from the image when none is given.
    [[nodiscard]] bool insertCartridge(std::uint8_t primarySlot, const std::vector<std::uint8_t>& image,
                                       std::optional<cartridge::Mapper> mapper = std::nullopt);
    void ejectCartridge(std::uint8_t primarySlot) noexcept;
    [[nodiscard]] const cartridge::Cartridge& cartridgeSlot(std::uint8_t primarySlot) const noexcept;

    [[nodiscard]] std::uint8_t readMemory(std::uint16_t address) const noexcept;
    void writeMemory(std::uint16_t address, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port);
    void writeIo(std::uint8_t port, std::uint8_t value);

    // Advances six shared master clocks, one Z80 T-state, and intervening VDP dots.
    [[nodiscard]] z80bus::Outputs tick();

    void setKeyboardRow(std::uint8_t row, std::uint8_t activeLowKeys) noexcept;
    void setKeyboardKey(std::uint8_t row, std::uint8_t bit, bool pressed) noexcept;
    void setJoystickPorts(std::uint8_t portA, std::uint8_t portB) noexcept;
    [[nodiscard]] bool loadCassette(const std::vector<std::uint8_t>& image);
    void rewindCassette() noexcept;
    void ejectCassette() noexcept;
    [[nodiscard]] CassetteState cassetteState() const noexcept;
    void startCassetteRecording();
    [[nodiscard]] bool stopCassetteRecording();
    [[nodiscard]] const std::vector<std::uint8_t>& cassetteImage() const noexcept;
    void markCassetteSaved() noexcept;
    [[nodiscard]] bool oneBitSoundLevel() const noexcept;

    [[nodiscard]] std::uint8_t primarySlotRegister() const noexcept;
    [[nodiscard]] const std::array<std::uint8_t, RamSize>& ram() const noexcept;
    [[nodiscard]] tms99x8::TMS99X8& video() noexcept;
    [[nodiscard]] const tms99x8::TMS99X8& video() const noexcept;
    [[nodiscard]] VideoStandard videoStandard() const noexcept;
    // CPU T-states and VDP master ticks of one field, and fields per second, of the fitted VDP.
    [[nodiscard]] std::uint32_t cpuTicksPerField() const noexcept;
    [[nodiscard]] std::uint32_t vdpMasterTicksPerField() const noexcept;
    [[nodiscard]] std::uint32_t fieldRateHz() const noexcept;

    // Snapshot of the running machine: CPU, PPI, VDP, PSG, RAM, slot selection and the bank
    // registers of the modules. Main ROM, module images, cassette and printer are not part of it.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    // Video standard of a snapshot, or nothing if the data is not a snapshot of this version.
    [[nodiscard]] static std::optional<VideoStandard> stateVideoStandard(const std::vector<std::uint8_t>& state);
    // Restores a snapshot of the same video standard with the same modules inserted; on failure the
    // machine stays as it was.
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);
    [[nodiscard]] ay8910::AY8910& sound() noexcept;
    [[nodiscard]] const ay8910::AY8910& sound() const noexcept;
    [[nodiscard]] printer::Printer& printer() noexcept;
    [[nodiscard]] const printer::Printer& printer() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace svi728