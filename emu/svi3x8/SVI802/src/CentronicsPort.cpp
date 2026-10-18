#include "SVI802/CentronicsPort.hpp"

namespace svi802 {

printer::Printer& CentronicsPort::printer() noexcept
{
    return device;
}

const printer::Printer& CentronicsPort::printer() const noexcept
{
    return device;
}

void CentronicsPort::reset()
{
    device.reset();
}

// The printer status reports BUSY in bit 1; the port passes it on in bit 0.
std::uint8_t CentronicsPort::readIo(std::uint8_t port) const
{
    if (port == 0x12) return static_cast<std::uint8_t>(0xFE | ((device.status() >> 1) & 1));
    return 0xFF;
}

void CentronicsPort::writeIo(std::uint8_t port, std::uint8_t value)
{
    if (port == 0x10) device.writeData(value);
    if (port == 0x11) device.writeStrobe((value & 0x01) != 0);
}

} // namespace svi802
