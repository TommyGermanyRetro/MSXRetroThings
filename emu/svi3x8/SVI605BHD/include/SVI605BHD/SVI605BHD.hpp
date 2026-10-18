#pragma once

#include "LANHD01/LANHD01.hpp"
#include "SVI605B/ExpanderBoard.hpp"

#include <cstddef>
#include <cstdint>

namespace svi605bhd {

inline constexpr std::size_t SlotCount = 4;
inline constexpr std::size_t DriveCount = 1;

// Spectravideo SVI-605B with hard disk: the main board of the SVI-605B (svi605b::ExpanderBoard) with
// four card slots and only drive A, double-sided (selecting drive B selects no drive; a blank disk for
// formatting is double-sided). In place of drive B a Seagate ST-212 sits on a WD1002-SHD behind the
// built-in SASI host adapter LAN-HD-01 at 40h-46h, which takes no slot. The adapter has no ROM: the
// BASIC ROM starts from the floppy, and CP/M 2.27 uses the hard disk as drives B: to E:.
class SVI605BHD final : public svi605b::ExpanderBoard {
public:
    SVI605BHD();

    // Built-in SASI host adapter with the WD1002-SHD and its hard disk.
    [[nodiscard]] lanhd01::LANHD01& sasi() noexcept;
    [[nodiscard]] const lanhd01::LANHD01& sasi() const noexcept;

    void reset() override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    lanhd01::LANHD01 adapter;
};

} // namespace svi605bhd
