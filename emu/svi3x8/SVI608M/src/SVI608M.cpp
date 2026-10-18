#include "SVI608M/SVI608M.hpp"

#include "LANIF01/LANIF01.hpp"
#include "SVI806/SVI806.hpp"

#include <memory>

namespace svi608m {

SVI608M::SVI608M()
    : ExpanderBoard(SlotCount, DriveCount, true)
{
}

bool SVI608M::fitFactoryCards(const std::vector<std::uint8_t>& characterRom, const std::vector<std::uint8_t>& bootRom)
{
    auto video = std::make_unique<svi806::SVI806>();
    auto boot = std::make_unique<lanif01::LANIF01>();
    if (!video->loadCharacterRom(characterRom) || !boot->loadRom(bootRom)) return false;
    (void)insertCard(VideoSlot, std::move(video));
    (void)insertCard(SasiSlot, std::make_unique<lanhd01::LANHD01>());
    (void)insertCard(BootSlot, std::move(boot));
    return true;
}

BootSource SVI608M::bootSource() const noexcept
{
    return source;
}

const lanhd01::LANHD01* SVI608M::sasiCard() const noexcept
{
    for (std::size_t slot = 0; slot < slotCount(); ++slot) {
        if (const auto* card = dynamic_cast<const lanhd01::LANHD01*>(this->card(slot))) return card;
    }
    return nullptr;
}

void SVI608M::reset()
{
    ExpanderBoard::reset();
    source = BootSource::Unknown;
    expectOpcode = false;
    sasiReading = false;
}

std::uint8_t SVI608M::readIo(std::uint8_t port)
{
    if (source == BootSource::Unknown) {
        if (port == 0x33 && diskController().dataRequest()) source = BootSource::Floppy;
        const auto* sasi = sasiCard();
        if (port == 0x42 && sasiReading && sasi && sasi->controller().signals() == wd1002shd::PhaseDataIn) {
            source = BootSource::HardDisk;
        }
    }
    return ExpanderBoard::readIo(port);
}

void SVI608M::writeIo(std::uint8_t port, std::uint8_t value)
{
    // Each SASI command starts with SEL (43h); its first byte in the Command phase is the opcode.
    if (port == 0x43) expectOpcode = true;
    if (port == 0x41 && expectOpcode) {
        const auto* sasi = sasiCard();
        if (sasi && sasi->controller().signals() == wd1002shd::PhaseCommand) {
            sasiReading = value == 0x08;
            expectOpcode = false;
        }
    }
    ExpanderBoard::writeIo(port, value);
}

} // namespace svi608m
