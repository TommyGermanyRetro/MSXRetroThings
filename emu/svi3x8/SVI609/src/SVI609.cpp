#include "SVI609/SVI609.hpp"

#include "LANHD01/LANHD01.hpp"
#include "LANIF01/LanCard.hpp"
#include "SVI805/SVI805.hpp"
#include "SVI806/SVI806.hpp"
#include "SVI807/SVI807.hpp"

#include <memory>

namespace svi609 {

bool SVI609::fitFactoryCards(const std::vector<std::uint8_t>& characterRom, const std::vector<std::uint8_t>& bootRom)
{
    auto video = std::make_unique<svi806::SVI806>();
    auto lan = std::make_unique<lanif01::LanCard>();
    if (!video->loadCharacterRom(characterRom) || !lan->loadRom(bootRom)) return false;
    (void)insertCard(VideoSlot, std::move(video));
    (void)insertCard(SerialSlot, std::make_unique<svi805::SVI805>());
    (void)insertCard(RamSlot, std::make_unique<svi807::SVI807>());
    (void)insertCard(SasiSlot, std::make_unique<lanhd01::LANHD01>());
    (void)insertCard(LanSlot, std::move(lan));
    return true;
}

} // namespace svi609
