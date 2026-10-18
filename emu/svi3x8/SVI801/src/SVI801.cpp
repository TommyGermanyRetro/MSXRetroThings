#include "SVI801/SVI801.hpp"

namespace svi801 {

SVI801::SVI801()
    : unit(MaxDriveCount, false)
{
}

DiskUnit& SVI801::disks() noexcept
{
    return unit;
}

const DiskUnit& SVI801::disks() const noexcept
{
    return unit;
}

void SVI801::reset()
{
    unit.reset();
}

svi3x8::ExpansionMemoryCycle SVI801::readMemory(std::uint16_t, std::uint8_t) const
{
    return {};
}

svi3x8::ExpansionMemoryCycle SVI801::writeMemory(std::uint16_t, std::uint8_t, std::uint8_t)
{
    return {};
}

std::uint8_t SVI801::readIo(std::uint8_t port)
{
    return unit.readIo(port);
}

void SVI801::writeIo(std::uint8_t port, std::uint8_t value)
{
    unit.writeIo(port, value);
}

void SVI801::tick()
{
    unit.tick();
}

bool SVI801::interruptRequest() const
{
    return false;
}

} // namespace svi801
