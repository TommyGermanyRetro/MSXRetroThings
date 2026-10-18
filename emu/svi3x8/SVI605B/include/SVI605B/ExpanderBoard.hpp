#pragma once

#include "SVI605B/CardCage.hpp"
#include "SVI802/CentronicsPort.hpp"
#include "SVI801/DiskUnit.hpp"

#include <cstddef>
#include <cstdint>

namespace svi605b {

// Main board of the Spectravideo Super Expanders with built-in disk unit: card slots (CardCage), a
// built-in Centronics printer port (svi802::CentronicsPort, 10h-12h) and a built-in FD1793 disk controller
// with up to two double-sided 5.25" drives (svi801::DiskUnit, 30h-34h and 38h).
class ExpanderBoard : public CardCage {
public:
    // A board with slotCount card slots and driveCount (1 or 2) drives; blank disks for formatting
    // are double-sided when doubleSidedBlank is set.
    ExpanderBoard(std::size_t slotCount, std::size_t driveCount, bool doubleSidedBlank = false);

    [[nodiscard]] std::size_t driveCount() const noexcept;

    // Printer on the Centronics port; it latches the data byte on the rising /STROBE edge.
    [[nodiscard]] printer::Printer& printer() noexcept;
    [[nodiscard]] const printer::Printer& printer() const noexcept;

    // Drive A (0) or B (1); without a drive B, index 1 gives a drive that is never selected.
    [[nodiscard]] svi801::FloppyDrive& drive(std::size_t index) noexcept;
    [[nodiscard]] const svi801::FloppyDrive& drive(std::size_t index) const noexcept;
    [[nodiscard]] const fd1793::FD1793& diskController() const noexcept;
    // Last value written to port 34h.
    [[nodiscard]] std::uint8_t driveControl() const noexcept;
    // When software writes to or formats an empty drive (WRITE SECTOR or WRITE TRACK), insert a
    // blank disk first, so that formatting from BASIC works without a prepared image. On by
    // default; reading an empty drive still fails as on the real unit.
    void setBlankDiskOnFormat(bool enabled) noexcept;

    void reset() override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;

private:
    svi802::CentronicsPort centronics;
    svi801::DiskUnit disks;
};

} // namespace svi605b
