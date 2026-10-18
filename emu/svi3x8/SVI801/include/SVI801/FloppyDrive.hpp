#pragma once

#include "FD1793/FD1793.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace svi801 {

inline constexpr unsigned SviDiskCylinders = 40;
inline constexpr std::size_t SviDiskSingleSidedSize = 172032;
inline constexpr std::size_t SviDiskDoubleSidedSize = 346112;

// SVI-318/328 disk image (.dsk): 40 cylinders, one or two sides, stored cylinder by cylinder with
// side 0 before side 1. Track 0 on side 0 holds 18 FM sectors of 128 bytes, every other track
// 17 MFM sectors of 256 bytes; sectors are numbered from 1.
class SviDisk final {
public:
    // Accepts 172032 (single-sided) or 346112 (double-sided) bytes.
    [[nodiscard]] bool load(const std::vector<std::uint8_t>& image);
    // A formatted disk with every byte E5h.
    [[nodiscard]] static SviDisk blank(bool doubleSided);

    [[nodiscard]] bool doubleSided() const noexcept;
    [[nodiscard]] const std::vector<std::uint8_t>& image() const noexcept;
    [[nodiscard]] bool modified() const noexcept;
    void markSaved() noexcept;

    [[nodiscard]] static bool doubleDensity(unsigned cylinder, unsigned side) noexcept;
    [[nodiscard]] static unsigned sectorsPerTrack(unsigned cylinder, unsigned side) noexcept;
    [[nodiscard]] static std::size_t sectorSize(unsigned cylinder, unsigned side) noexcept;
    // Offset of a sector (numbered from 1) in the image, or the image size if it does not exist.
    [[nodiscard]] std::size_t sectorOffset(unsigned cylinder, unsigned side, unsigned sector) const noexcept;
    [[nodiscard]] bool readSector(unsigned cylinder, unsigned side, unsigned sector, std::vector<std::uint8_t>& data) const;
    [[nodiscard]] bool writeSector(unsigned cylinder, unsigned side, unsigned sector, const std::vector<std::uint8_t>& data);

private:
    std::vector<std::uint8_t> bytes;
    bool sides2 = false;
    bool changed = false;
};

// 5.25" drive of the SVI-605B: head mechanics (cylinders 0-41), spindle motor and the disk in it.
// The disk turns while the motor runs; the index hole passes once per revolution.
class FloppyDrive final : public fd1793::Drive {
public:
    explicit FloppyDrive(std::uint32_t hostClockHz);

    void insert(const SviDisk& disk);
    void eject() noexcept;
    [[nodiscard]] bool hasDisk() const noexcept;
    // Inserts a blank disk, single- or double-sided, that has no file yet (see
    // ExpanderBoard::setBlankDiskOnFormat).
    void insertBlank(bool doubleSided = false);
    // The disk in the drive was inserted by insertBlank() and has no file yet.
    [[nodiscard]] bool hasBlankDisk() const noexcept;
    [[nodiscard]] const SviDisk& disk() const noexcept;
    [[nodiscard]] SviDisk& disk() noexcept;
    void setWriteProtected(bool protect) noexcept;
    void setMotor(bool on) noexcept;
    [[nodiscard]] bool motorOn() const noexcept;
    [[nodiscard]] unsigned cylinder() const noexcept;
    // One host clock.
    void tick() noexcept;

    [[nodiscard]] bool ready() const override;
    [[nodiscard]] bool writeProtected() const override;
    [[nodiscard]] bool track0() const override;
    [[nodiscard]] bool index() const override;
    [[nodiscard]] std::uint32_t rotationUs() const override;
    void step(bool towardsCenter) override;
    [[nodiscard]] std::vector<fd1793::TrackSector> sectors(std::uint8_t side, bool doubleDensity) const override;
    [[nodiscard]] bool readSector(std::uint8_t side, bool doubleDensity, std::size_t index,
                                  std::vector<std::uint8_t>& data) const override;
    [[nodiscard]] bool writeSector(std::uint8_t side, bool doubleDensity, std::size_t index,
                                   const std::vector<std::uint8_t>& data) override;
    [[nodiscard]] bool formatTrack(std::uint8_t side, bool doubleDensity,
                                   const std::vector<fd1793::FormattedSector>& laidDown) override;

private:
    [[nodiscard]] bool trackReadable(std::uint8_t side, bool doubleDensity) const noexcept;

    std::uint32_t clockHz;
    std::uint64_t revolutionTicks;
    std::uint64_t rotation = 0;
    SviDisk inserted;
    bool diskPresent = false;
    bool blankDisk = false;
    bool protect = false;
    bool motor = false;
    unsigned head = 0;
};

} // namespace svi801
