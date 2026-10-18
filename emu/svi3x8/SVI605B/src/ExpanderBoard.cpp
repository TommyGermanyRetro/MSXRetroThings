#include "SVI605B/ExpanderBoard.hpp"

namespace svi605b {

ExpanderBoard::ExpanderBoard(std::size_t slotCount, std::size_t driveCount, bool doubleSidedBlank)
    : CardCage(slotCount),
      disks(driveCount, doubleSidedBlank)
{
}

std::size_t ExpanderBoard::driveCount() const noexcept
{
    return disks.driveCount();
}

svi801::FloppyDrive& ExpanderBoard::drive(std::size_t index) noexcept
{
    return disks.drive(index);
}

const svi801::FloppyDrive& ExpanderBoard::drive(std::size_t index) const noexcept
{
    return disks.drive(index);
}

const fd1793::FD1793& ExpanderBoard::diskController() const noexcept
{
    return disks.diskController();
}

std::uint8_t ExpanderBoard::driveControl() const noexcept
{
    return disks.driveControl();
}

void ExpanderBoard::setBlankDiskOnFormat(bool enabled) noexcept
{
    disks.setBlankDiskOnFormat(enabled);
}

printer::Printer& ExpanderBoard::printer() noexcept
{
    return centronics.printer();
}

const printer::Printer& ExpanderBoard::printer() const noexcept
{
    return centronics.printer();
}

void ExpanderBoard::reset()
{
    centronics.reset();
    disks.reset();
    CardCage::reset();
}

std::uint8_t ExpanderBoard::readIo(std::uint8_t port)
{
    return static_cast<std::uint8_t>(centronics.readIo(port) & disks.readIo(port) & CardCage::readIo(port));
}

void ExpanderBoard::writeIo(std::uint8_t port, std::uint8_t value)
{
    centronics.writeIo(port, value);
    disks.writeIo(port, value);
    CardCage::writeIo(port, value);
}

void ExpanderBoard::tick()
{
    disks.tick();
    CardCage::tick();
}

} // namespace svi605b
