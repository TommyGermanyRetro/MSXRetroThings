#pragma once

#include "AY8910/AY8910.hpp"
#include "SVI328/Cassette.hpp"
#include "SVI3X8/ExpansionPort.hpp"
#include "TMS99X8/TMS99X8.hpp"
#include "Z80Bus/Z80Bus.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace svi328 {

inline constexpr std::size_t BankSize = 0x8000;
inline constexpr std::size_t RomSize = 0x8000;
inline constexpr std::size_t CartridgeMaxSize = 0x10000;
inline constexpr std::size_t KeyboardRowCount = 11;
inline constexpr std::uint32_t VdpMasterClockHz = 21477270;
inline constexpr std::uint32_t CpuClockHz = VdpMasterClockHz / 6;
inline constexpr std::uint8_t VdpMasterTicksPerCpuTick = 6;
inline constexpr std::uint32_t AyMasterClockHz = (CpuClockHz + 1) / 2;
inline constexpr std::uint16_t VdpScanlinesPerPalField = 313;
inline constexpr std::uint32_t VdpMasterTicksPerPalField =
    tms99x8::MasterTicksPerScanline * VdpScanlinesPerPalField;
inline constexpr std::uint32_t CpuTicksPerPalField =
    VdpMasterTicksPerPalField / VdpMasterTicksPerCpuTick;
static_assert(VdpMasterTicksPerPalField % VdpMasterTicksPerCpuTick == 0);
inline constexpr std::uint16_t VdpScanlinesPerNtscField = 262;
inline constexpr std::uint32_t VdpMasterTicksPerNtscField =
    tms99x8::MasterTicksPerScanline * VdpScanlinesPerNtscField;
inline constexpr std::uint32_t CpuTicksPerNtscField =
    VdpMasterTicksPerNtscField / VdpMasterTicksPerCpuTick;
static_assert(VdpMasterTicksPerNtscField % VdpMasterTicksPerCpuTick == 0);

// Video standard of the machine: PAL with the TMS9929A (Europe), NTSC with the TMS9918A (USA).
// Both run from the same 21.477 MHz clock; they differ in lines per field and field rate.
enum class VideoStandard : std::uint8_t { Pal, Ntsc };

// Joystick bits for setJoysticks(), active low as on the connector.
inline constexpr std::uint8_t JoystickUp = 0x01;
inline constexpr std::uint8_t JoystickDown = 0x02;
inline constexpr std::uint8_t JoystickLeft = 0x04;
inline constexpr std::uint8_t JoystickRight = 0x08;
inline constexpr std::uint8_t JoystickTrigger = 0x10;

// Spectravideo SVI-328 with 64 KiB RAM: BASIC ROM (bank 1), RAM bank 21 or cartridge bank 11 in
// 0000h-7FFFh, RAM bank 2 or cartridge bank 12 in 8000h-FFFFh, selected by PSG port B (register 15,
// active low). Banks 22, 31 and 32 come from expansion devices, which switch the internal ROM or
// RAM bank 2 off with /ROMDIS or /RAMDIS.
class SVI328 final {
public:
    explicit SVI328(VideoStandard standard = VideoStandard::Pal);
    ~SVI328();

    SVI328(const SVI328&) = delete;
    SVI328& operator=(const SVI328&) = delete;
    SVI328(SVI328&&) noexcept;
    SVI328& operator=(SVI328&&) noexcept;

    void reset();
    [[nodiscard]] bool loadRom(const std::vector<std::uint8_t>& image);

    // SVI cartridge image up to 64 KiB: bytes 0000h-7FFFh form bank 11 (/CART in 0000h-7FFFh),
    // 8000h-FFFFh bank 12 (/ROMEN0 in 8000h-BFFFh, /ROMEN1 in C000h-FFFFh). The BASIC ROM starts
    // a cartridge whose first bytes are F3h 31h by jumping to 0000h with only /CART selected.
    [[nodiscard]] bool insertCartridge(const std::vector<std::uint8_t>& image);
    void ejectCartridge() noexcept;
    [[nodiscard]] bool cartridgeInserted() const noexcept;

    // Device on the expansion connector, or nullptr. The machine does not own it; it must stay
    // alive while connected. reset() also resets the device (/RST).
    void connectExpansion(svi3x8::ExpansionPort* port) noexcept;

    // Memory as the CPU sees it with the current bank selection.
    [[nodiscard]] std::uint8_t readMemory(std::uint16_t address) const noexcept;
    void writeMemory(std::uint16_t address, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port);
    void writeIo(std::uint8_t port, std::uint8_t value);

    // Advances six shared master clocks, one Z80 T-state, and intervening VDP dots.
    [[nodiscard]] z80bus::Outputs tick();

    // SVI keyboard matrix, rows 0-10, active low.
    void setKeyboardRow(std::uint8_t row, std::uint8_t activeLowKeys) noexcept;
    void setKeyboardKey(std::uint8_t row, std::uint8_t bit, bool pressed) noexcept;
    // Joystick bits 0-3 directions, bit 4 trigger, active low.
    void setJoysticks(std::uint8_t joystick1, std::uint8_t joystick2) noexcept;

    // SVI CAS image; playback runs while PPI port C bit 4 holds the motor on.
    [[nodiscard]] bool loadCassette(const std::vector<std::uint8_t>& image);
    void rewindCassette() noexcept;
    void ejectCassette() noexcept;
    [[nodiscard]] CassetteState cassetteState() const noexcept;
    [[nodiscard]] const std::vector<std::uint8_t>& cassetteImage() const noexcept;
    // Records the write signal (PPI port C bit 5) into a new CAS image while the motor runs.
    void startCassetteRecording();
    [[nodiscard]] bool stopCassetteRecording();
    void markCassetteSaved() noexcept;

    // PSG port B as it selects the memory banks (FFh while port B is an input).
    [[nodiscard]] std::uint8_t bankRegister() const noexcept;
    // Keyboard click, PPI port C bit 7.
    [[nodiscard]] bool clickLevel() const noexcept;

    [[nodiscard]] VideoStandard videoStandard() const noexcept;
    // CPU T-states and VDP master clocks in one field (313 or 262 lines), fields per second.
    [[nodiscard]] std::uint32_t cpuTicksPerField() const noexcept;
    [[nodiscard]] std::uint32_t vdpMasterTicksPerField() const noexcept;
    [[nodiscard]] std::uint32_t fieldRateHz() const noexcept;

    // The fitted video display processor: TMS9929A (PAL) or TMS9918A (NTSC).
    [[nodiscard]] tms99x8::TMS99X8& video() noexcept;
    [[nodiscard]] const tms99x8::TMS99X8& video() const noexcept;
    [[nodiscard]] ay8910::AY8910& sound() noexcept;
    [[nodiscard]] const ay8910::AY8910& sound() const noexcept;

    // Snapshot of the running machine: CPU, RAM banks 2 and 21, VDP with VRAM, PSG and PPI. The
    // BASIC ROM, the module, the cassette, expansion devices and the host inputs (keyboard,
    // joysticks) are not part of it.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    // Restores a snapshot of the same video standard. On any mismatch it returns false and leaves
    // the machine unchanged.
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);
    // Video standard a snapshot was taken with, or nullopt when the data is no snapshot.
    [[nodiscard]] static std::optional<VideoStandard> stateVideoStandard(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace svi328
