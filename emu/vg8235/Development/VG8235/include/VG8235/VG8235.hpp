#pragma once

#include "AY8910/AY8910.hpp"
#include "CARTRIDGE/Cartridge.hpp"
#include "PRINTER/Printer.hpp"
#include "RP5C01/RP5C01.hpp"
#include "WD2793/WD2793.hpp"
#include "V9938/V9938.hpp"
#include "VG8235/Cassette.hpp"
#include "Z80Bus/Z80Bus.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace usbslot {
class BoardSlot;
} // namespace usbslot

namespace vg8235 {

inline constexpr std::size_t AddressSpaceSize = 0x10000;
inline constexpr std::size_t PageSize = 0x4000;
inline constexpr std::size_t MainRomSize = 0x8000;
inline constexpr std::size_t SubRomSize = 0x4000;
inline constexpr std::size_t DiskRomSize = 0x4000;
inline constexpr std::size_t MapperSegments = 8;
inline constexpr std::size_t MapperSize = MapperSegments * PageSize;
inline constexpr std::size_t KeyboardRowCount = 16;
inline constexpr std::uint8_t VdpMasterTicksPerCpuTick = 6;
inline constexpr std::uint32_t CpuClockHz = v9938::MasterClockHz / VdpMasterTicksPerCpuTick;
inline constexpr std::uint32_t AyMasterClockHz = (CpuClockHz + 1) / 2;
inline constexpr std::uint32_t VdpMasterTicksPerPalField =
    v9938::MasterTicksPerScanline * v9938::PalScanlines;
inline constexpr std::uint32_t CpuTicksPerPalField =
    VdpMasterTicksPerPalField / VdpMasterTicksPerCpuTick;
static_assert(VdpMasterTicksPerPalField % VdpMasterTicksPerCpuTick == 0);

// Philips VG 8235/00: slot 0 BIOS/BASIC, slots 1 and 2 cartridges, slot 3 expanded with
// 3-0 Sub-ROM, 3-1 empty, 3-2 128 KiB memory mapper and 3-3 disk ROM with the WD2793.
class VG8235 final {
public:
    VG8235();
    ~VG8235();

    VG8235(const VG8235&) = delete;
    VG8235& operator=(const VG8235&) = delete;
    VG8235(VG8235&&) noexcept;
    VG8235& operator=(VG8235&&) noexcept;

    void reset();

    [[nodiscard]] bool loadMainRom(const std::vector<std::uint8_t>& image);
    [[nodiscard]] bool loadSubRom(const std::vector<std::uint8_t>& image);
    [[nodiscard]] bool loadDiskRom(const std::vector<std::uint8_t>& image);
    // Plain ROM at a fixed address in slot 1 or 2.
    [[nodiscard]] bool loadCartridge(std::uint8_t primarySlot, std::uint16_t baseAddress,
                                     const std::vector<std::uint8_t>& image);
    // ROM cartridge with the given mapper, detected from the image when none is given.
    [[nodiscard]] bool insertCartridge(std::uint8_t primarySlot, const std::vector<std::uint8_t>& image,
                                       std::optional<cartridge::Mapper> mapper = std::nullopt);
    void ejectCartridge(std::uint8_t primarySlot) noexcept;
    [[nodiscard]] const cartridge::Cartridge& cartridgeSlot(std::uint8_t primarySlot) const noexcept;
    // Puts a slot of a USB2SLOT board into slot 1 or 2 in place of a module: the CPU then reads and
    // writes the real cartridge there, one board cycle per bus cycle, and the I/O ports the board slot
    // claims go to the board as well (a port the machine serves itself, like the mapper ports, reads the
    // AND of both). A null board slot frees the slot. The board slot must stay alive while it is connected.
    [[nodiscard]] bool connectBridgeSlot(std::uint8_t primarySlot, usbslot::BoardSlot* boardSlot);
    // Slot of the board in slot 1 or 2, 0 when none is connected.
    [[nodiscard]] std::uint8_t bridgeSlot(std::uint8_t primarySlot) const noexcept;

    // CPU view of memory and I/O, including the slot-3 secondary slot register at #FFFF.
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
    [[nodiscard]] std::uint8_t secondarySlotRegister() const noexcept;
    [[nodiscard]] std::uint8_t mapperRegister(std::uint8_t page) const noexcept;
    [[nodiscard]] const std::array<std::uint8_t, MapperSize>& mapperRam() const noexcept;
    [[nodiscard]] v9938::V9938& video() noexcept;
    [[nodiscard]] const v9938::V9938& video() const noexcept;
    [[nodiscard]] ay8910::AY8910& sound() noexcept;
    [[nodiscard]] const ay8910::AY8910& sound() const noexcept;
    // Drive A on the WD2793 of the Philips disk interface; the host inserts .dsk images.
    [[nodiscard]] wd2793::FloppyDrive& diskDrive() noexcept;
    [[nodiscard]] const wd2793::FloppyDrive& diskDrive() const noexcept;
    // Battery-backed clock; the host saves and restores its image() as the CMOS file.
    [[nodiscard]] rp5c01::RP5C01& realTimeClock() noexcept;
    [[nodiscard]] printer::Printer& printer() noexcept;
    [[nodiscard]] const printer::Printer& printer() const noexcept;
    // Sets the clock for a cold start in the 24-hour mode that the MSX2 BIOS uses.
    void setClock(const rp5c01::DateTime& value) noexcept;
    [[nodiscard]] const rp5c01::RP5C01& realTimeClock() const noexcept;

    // Snapshot of the running machine: CPU, PPI, VDP, PSG, clock, disk controller and drive mechanics,
    // mapper RAM and registers, slot selection and the bank registers of the modules. ROMs, module
    // images, the disk, the cassette and the printer are not part of it.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    // True when the data starts like a snapshot of this version.
    [[nodiscard]] static bool isState(const std::vector<std::uint8_t>& state);
    // Restores a snapshot taken with the same modules inserted; on failure the machine stays as it was.
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace vg8235
