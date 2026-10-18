#pragma once

#include "SVI605B/ExpanderBoard.hpp"

#include <cstddef>

namespace svi605b {

inline constexpr std::size_t SlotCount = 4;
inline constexpr std::size_t DriveCount = 2;

// Spectravideo SVI-605B Super Expander: the expander main board (ExpanderBoard) with four card
// slots and two double-sided 5.25" drives A and B.
class SVI605B final : public ExpanderBoard {
public:
    SVI605B();
};

} // namespace svi605b
