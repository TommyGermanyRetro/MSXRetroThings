#pragma once

#include "SVI608M/SVI608M.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace svi609 {

// Factory slots.
inline constexpr std::size_t VideoSlot = 0;
inline constexpr std::size_t SerialSlot = 1;
inline constexpr std::size_t RamSlot = 2;
inline constexpr std::size_t SasiSlot = 3;
inline constexpr std::size_t LanSlot = 4;

// Spectravideo SVI-609 Master Station: an SVI-328 with the Network Expander, the expander of the
// SVI-608M (main board of the SVI-605B with five slots, only drive A, double-sided, Seagate ST-212
// on the WD1002-SHD) with all five slots fitted at the factory: SVI-806 80 columns, SVI-805
// RS-232, SVI-807 64 KB RAM (S2+S3), SASI host adapter LAN-HD-01 and the LAN card (fully fitted
// LAN-IF-01 with boot EPROM and printer PIA at 60h-63h). The SCC of the LAN card is not connected.
class SVI609 final : public svi608m::SVI608M {
public:
    // Fits the five factory cards. characterRom is the character ROM of the SVI-806 (4 KiB),
    // bootRom the EPROM of the LAN card (4 KiB). Fails with a ROM of the wrong size; the slots stay
    // unchanged then.
    [[nodiscard]] bool fitFactoryCards(const std::vector<std::uint8_t>& characterRom,
                                       const std::vector<std::uint8_t>& bootRom);
};

} // namespace svi609
