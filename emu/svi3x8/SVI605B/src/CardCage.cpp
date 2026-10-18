#include "SVI605B/CardCage.hpp"

#include <utility>

namespace svi605b {

CardCage::CardCage(std::size_t slotCount)
    : slots(slotCount)
{
}

std::size_t CardCage::slotCount() const noexcept
{
    return slots.size();
}

std::unique_ptr<svi3x8::ExpansionPort> CardCage::insertCard(std::size_t slot, std::unique_ptr<svi3x8::ExpansionPort> card)
{
    if (slot >= slots.size()) return card;
    return std::exchange(slots[slot], std::move(card));
}

std::unique_ptr<svi3x8::ExpansionPort> CardCage::removeCard(std::size_t slot)
{
    return slot < slots.size() ? std::move(slots[slot]) : nullptr;
}

svi3x8::ExpansionPort* CardCage::card(std::size_t slot) const noexcept
{
    return slot < slots.size() ? slots[slot].get() : nullptr;
}

void CardCage::reset()
{
    for (const auto& slot : slots) {
        if (slot) slot->reset();
    }
}

bool CardCage::ramDisabledByOthers(std::uint16_t address, std::uint8_t bankLines) const
{
    for (const auto& slot : slots) {
        if (!slot) continue;
        const auto cycle = slot->readMemory(address, bankLines);
        if (cycle.ramDisable && !cycle.ramBank2) return true;
    }
    return false;
}

svi3x8::ExpansionMemoryCycle CardCage::readMemory(std::uint16_t address, std::uint8_t bankLines) const
{
    const bool ramDisabled = ramDisabledByOthers(address, bankLines);
    svi3x8::ExpansionMemoryCycle bus;
    for (const auto& slot : slots) {
        if (!slot) continue;
        const auto cycle = slot->readMemory(address, bankLines);
        if (cycle.ramBank2 && ramDisabled) continue;
        bus.data &= cycle.data;
        bus.romDisable = bus.romDisable || cycle.romDisable;
        bus.ramDisable = bus.ramDisable || cycle.ramDisable;
    }
    return bus;
}

svi3x8::ExpansionMemoryCycle CardCage::writeMemory(std::uint16_t address, std::uint8_t value, std::uint8_t bankLines)
{
    const bool ramDisabled = ramDisabledByOthers(address, bankLines);
    svi3x8::ExpansionMemoryCycle bus;
    for (const auto& slot : slots) {
        if (!slot) continue;
        if (ramDisabled && slot->readMemory(address, bankLines).ramBank2) continue;
        const auto cycle = slot->writeMemory(address, value, bankLines);
        bus.romDisable = bus.romDisable || cycle.romDisable;
        bus.ramDisable = bus.ramDisable || cycle.ramDisable;
    }
    return bus;
}

std::uint8_t CardCage::readIo(std::uint8_t port)
{
    std::uint8_t data = 0xFF;
    for (const auto& slot : slots) {
        if (slot) data &= slot->readIo(port);
    }
    return data;
}

void CardCage::writeIo(std::uint8_t port, std::uint8_t value)
{
    for (const auto& slot : slots) {
        if (slot) slot->writeIo(port, value);
    }
}

void CardCage::tick()
{
    for (const auto& slot : slots) {
        if (slot) slot->tick();
    }
}

bool CardCage::interruptRequest() const
{
    for (const auto& slot : slots) {
        if (slot && slot->interruptRequest()) return true;
    }
    return false;
}

} // namespace svi605b
