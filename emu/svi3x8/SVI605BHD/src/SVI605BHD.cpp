#include "SVI605BHD/SVI605BHD.hpp"

namespace svi605bhd {

SVI605BHD::SVI605BHD()
    : ExpanderBoard(SlotCount, DriveCount, true)
{
}

lanhd01::LANHD01& SVI605BHD::sasi() noexcept
{
    return adapter;
}

const lanhd01::LANHD01& SVI605BHD::sasi() const noexcept
{
    return adapter;
}

void SVI605BHD::reset()
{
    ExpanderBoard::reset();
    adapter.reset();
}

std::uint8_t SVI605BHD::readIo(std::uint8_t port)
{
    return static_cast<std::uint8_t>(ExpanderBoard::readIo(port) & adapter.readIo(port));
}

void SVI605BHD::writeIo(std::uint8_t port, std::uint8_t value)
{
    ExpanderBoard::writeIo(port, value);
    adapter.writeIo(port, value);
}

void SVI605BHD::tick()
{
    ExpanderBoard::tick();
    adapter.tick();
}

bool SVI605BHD::interruptRequest() const
{
    return ExpanderBoard::interruptRequest() || adapter.interruptRequest();
}

} // namespace svi605bhd
