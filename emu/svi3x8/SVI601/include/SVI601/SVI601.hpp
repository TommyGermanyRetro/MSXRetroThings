#pragma once

#include "SVI605B/CardCage.hpp"

#include <cstddef>

namespace svi601 {

inline constexpr std::size_t SlotCount = 7;
// SK6 takes the SVI-801 disk controller cartridge; all other cartridges go into the other slots.
inline constexpr std::size_t DiskControllerSlot = 5;

// Spectravideo SVI-601 Super Expander: seven cartridge slots SK1-SK7 on the expansion connector,
// without built-in drives or printer port. Disks come with the SVI-801 controller in SK6 and its
// external SVI-902 drives, printing with the SVI-802 Centronics cartridge.
class SVI601 final : public svi605b::CardCage {
public:
    SVI601();
};

} // namespace svi601
