#pragma once

#include "LANHD01/LANHD01.hpp"
#include "SVI605B/ExpanderBoard.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace svi608m {

inline constexpr std::size_t SlotCount = 5;
inline constexpr std::size_t DriveCount = 1;

// Where the system came from after the last reset: the first sector read either from floppy
// (FD1793 data with DRQ) or from the hard disk (Data In of a SASI Read command).
enum class BootSource { Unknown, Floppy, HardDisk };

// Factory slots.
inline constexpr std::size_t VideoSlot = 0;
inline constexpr std::size_t SasiSlot = 1;
inline constexpr std::size_t BootSlot = 2;

// Spectravideo SVI-608M Super Expander with hard disk: the main board of the SVI-605B
// (svi605b::ExpanderBoard) with five card slots and only drive A, double-sided (selecting drive B
// selects no drive; CP/M 2.27 uses side 0, cylinders 0-39, then side 1, cylinders 0-39, and a blank
// disk for formatting is double-sided). It has no printer PIA: CP/M 2.27 prints through the
// Centronics port. Its factory cards are the SVI-806 80-column card, the SASI host adapter
// LAN-HD-01 with the WD1002-SHD and the boot card LAN-IF-01.
class SVI608M : public svi605b::ExpanderBoard {
public:
    SVI608M();

    // Fits the factory cards SVI-806 (character ROM, 4 KiB), SASI host adapter without a hard disk
    // and boot card (EPROM, 4 KiB) into slots VideoSlot, SasiSlot and BootSlot. Fails with a ROM
    // of the wrong size; the slots stay unchanged then.
    [[nodiscard]] bool fitFactoryCards(const std::vector<std::uint8_t>& characterRom,
                                       const std::vector<std::uint8_t>& bootRom);

    // Source of the first sector read since the last reset. CP/M 2.27 started from the hard disk
    // names the hard disk A: to D: and the floppy E:.
    [[nodiscard]] BootSource bootSource() const noexcept;

    void reset() override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;

private:
    [[nodiscard]] const lanhd01::LANHD01* sasiCard() const noexcept;

    BootSource source = BootSource::Unknown;
    bool expectOpcode = false;
    bool sasiReading = false;
};

} // namespace svi608m
