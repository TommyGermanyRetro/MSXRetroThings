#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wd2793 {

inline constexpr std::size_t SectorSize = 512;
inline constexpr unsigned Cylinders = 80;
// The head can move a few cylinders beyond the last formatted one.
inline constexpr unsigned MaximumHeadCylinder = 83;

// 3.5" double-density drive holding a raw .dsk image: 80 cylinders of 8 or 9 sectors of 512 bytes
// on one or two sides, sectors stored cylinder by cylinder, side 0 before side 1.
class FloppyDrive final {
public:
    explicit FloppyDrive(bool doubleSided = true);

    // Accepts 320, 360, 640 or 720 KiB images; a single-sided drive takes only the 1-sided sizes.
    [[nodiscard]] bool insert(std::vector<std::uint8_t> image, bool writeProtected);
    void eject() noexcept;
    [[nodiscard]] bool inserted() const noexcept;
    [[nodiscard]] const std::vector<std::uint8_t>& image() const noexcept;
    // Set when a sector was written since insert() or clearModified().
    [[nodiscard]] bool modified() const noexcept;
    void clearModified() noexcept;

    [[nodiscard]] unsigned sides() const noexcept;
    [[nodiscard]] unsigned sectorsPerTrack() const noexcept;

    void setMotor(bool on) noexcept;
    [[nodiscard]] bool motor() const noexcept;
    void setSide(unsigned side) noexcept;
    [[nodiscard]] unsigned side() const noexcept;
    [[nodiscard]] unsigned headCylinder() const noexcept;
    void step(bool inward) noexcept;

    // Drive signals: READY means a disk is inserted and the selected side exists on this drive.
    // Like Philips drives, TRACK 00 and WRITE PROTECT are only active while the motor runs.
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool track00() const noexcept;
    [[nodiscard]] bool writeProtected() const noexcept;
    // True when the head is over a cylinder and side that exist in the image.
    [[nodiscard]] bool trackFormatted() const noexcept;

    // Sector at the head position, nullptr when it does not exist; writableSector() marks the
    // image as modified.
    [[nodiscard]] const std::uint8_t* sector(unsigned number) const noexcept;
    [[nodiscard]] std::uint8_t* writableSector(unsigned number) noexcept;

    // Disk change flag: set on insert and eject, cleared when read.
    [[nodiscard]] bool takeDiskChanged() noexcept;
    [[nodiscard]] bool diskChanged() const noexcept;

    // Motor, selected side and head cylinder for snapshots; the disk image is not part of it.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    // Byte offset of a sector at the head position, or data.size() when it does not exist.
    [[nodiscard]] std::size_t sectorOffset(unsigned number) const noexcept;

    std::vector<std::uint8_t> data;
    bool doubleSidedDrive = true;
    bool present = false;
    bool protectedDisk = false;
    bool dirty = false;
    bool changed = false;
    bool motorOn = false;
    unsigned imageSides = 0;
    unsigned imageSectors = 0;
    unsigned selectedSide = 0;
    unsigned cylinder = 0;
};

} // namespace wd2793
