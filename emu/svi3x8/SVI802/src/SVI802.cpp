#include "SVI802/SVI802.hpp"

namespace svi802 {

CentronicsPort& SVI802::port() noexcept
{
    return centronics;
}

const CentronicsPort& SVI802::port() const noexcept
{
    return centronics;
}

void SVI802::reset()
{
    centronics.reset();
}

svi3x8::ExpansionMemoryCycle SVI802::readMemory(std::uint16_t, std::uint8_t) const
{
    return {};
}

svi3x8::ExpansionMemoryCycle SVI802::writeMemory(std::uint16_t, std::uint8_t, std::uint8_t)
{
    return {};
}

std::uint8_t SVI802::readIo(std::uint8_t port)
{
    return centronics.readIo(port);
}

void SVI802::writeIo(std::uint8_t port, std::uint8_t value)
{
    centronics.writeIo(port, value);
}

void SVI802::tick()
{
}

bool SVI802::interruptRequest() const
{
    return false;
}

} // namespace svi802
