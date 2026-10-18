#include "SVI602/SVI602.hpp"

#include <utility>

namespace svi602 {

std::unique_ptr<svi3x8::ExpansionPort> SVI602::insertCard(std::unique_ptr<svi3x8::ExpansionPort> card)
{
    return std::exchange(slot, std::move(card));
}

std::unique_ptr<svi3x8::ExpansionPort> SVI602::removeCard()
{
    return std::move(slot);
}

svi3x8::ExpansionPort* SVI602::card() const noexcept
{
    return slot.get();
}

void SVI602::reset()
{
    if (slot) slot->reset();
}

svi3x8::ExpansionMemoryCycle SVI602::readMemory(std::uint16_t address, std::uint8_t bankLines) const
{
    return slot ? slot->readMemory(address, bankLines) : svi3x8::ExpansionMemoryCycle{};
}

svi3x8::ExpansionMemoryCycle SVI602::writeMemory(std::uint16_t address, std::uint8_t value, std::uint8_t bankLines)
{
    return slot ? slot->writeMemory(address, value, bankLines) : svi3x8::ExpansionMemoryCycle{};
}

std::uint8_t SVI602::readIo(std::uint8_t port)
{
    return slot ? slot->readIo(port) : static_cast<std::uint8_t>(0xFF);
}

void SVI602::writeIo(std::uint8_t port, std::uint8_t value)
{
    if (slot) slot->writeIo(port, value);
}

void SVI602::tick()
{
    if (slot) slot->tick();
}

bool SVI602::interruptRequest() const
{
    return slot && slot->interruptRequest();
}

} // namespace svi602
