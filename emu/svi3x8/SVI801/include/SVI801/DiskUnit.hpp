#pragma once

#include "FD1793/FD1793.hpp"
#include "SVI801/FloppyDrive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace svi801 {

inline constexpr std::size_t MaxDriveCount = 2;

// Spectravideo disk unit: an FD1793 with up to two 5.25" drives, clocked with the CPU clock.
// 30h-33h FD1793 registers; 34h read: bit 7 INTRQ, bit 6 DRQ; 34h write: bit 0 select drive A,
// bit 1 select drive B (both, none or a missing drive: no drive), bit 2 motor A, bit 3 motor B;
// 38h write: bit 0 density (1 = FM), bit 1 side.
class DiskUnit {
public:
    // driveCount (1 or 2) drives; blank disks for formatting are double-sided when doubleSidedBlank is set.
    DiskUnit(std::size_t driveCount, bool doubleSidedBlank);

    [[nodiscard]] std::size_t driveCount() const noexcept;
    // Drive A (0) or B (1); without a drive B, index 1 gives a drive that is never selected.
    [[nodiscard]] FloppyDrive& drive(std::size_t index) noexcept;
    [[nodiscard]] const FloppyDrive& drive(std::size_t index) const noexcept;
    [[nodiscard]] const fd1793::FD1793& diskController() const noexcept;
    // Last value written to port 34h.
    [[nodiscard]] std::uint8_t driveControl() const noexcept;
    // When software writes to or formats an empty drive (WRITE SECTOR or WRITE TRACK), insert a
    // blank disk first, so that formatting works without a prepared image. On by default; reading
    // an empty drive still fails as on the real unit.
    void setBlankDiskOnFormat(bool enabled) noexcept;

    void reset();
    // FFh for ports the unit does not answer.
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port);
    void writeIo(std::uint8_t port, std::uint8_t value);
    void tick();

private:
    void applyDriveControl();

    std::size_t drivesFitted;
    fd1793::FD1793 fdc;
    std::array<FloppyDrive, MaxDriveCount> drives;
    std::uint8_t control = 0;
    bool blankOnFormat = true;
    bool blankDoubleSided;
};

} // namespace svi801
