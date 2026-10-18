#include "SVI801/DiskUnit.hpp"

#include "SVI3X8/ExpansionPort.hpp"

#include <algorithm>

namespace svi801 {

DiskUnit::DiskUnit(std::size_t driveCount, bool doubleSidedBlank)
    : drivesFitted(std::clamp<std::size_t>(driveCount, 1, MaxDriveCount)),
      fdc(svi3x8::CpuClockHz),
      drives{FloppyDrive(svi3x8::CpuClockHz), FloppyDrive(svi3x8::CpuClockHz)},
      blankDoubleSided(doubleSidedBlank)
{
    applyDriveControl();
}

std::size_t DiskUnit::driveCount() const noexcept
{
    return drivesFitted;
}

FloppyDrive& DiskUnit::drive(std::size_t index) noexcept
{
    return drives[index < MaxDriveCount ? index : 0];
}

const FloppyDrive& DiskUnit::drive(std::size_t index) const noexcept
{
    return drives[index < MaxDriveCount ? index : 0];
}

const fd1793::FD1793& DiskUnit::diskController() const noexcept
{
    return fdc;
}

std::uint8_t DiskUnit::driveControl() const noexcept
{
    return control;
}

void DiskUnit::setBlankDiskOnFormat(bool enabled) noexcept
{
    blankOnFormat = enabled;
}

void DiskUnit::applyDriveControl()
{
    drives[0].setMotor((control & 0x04) != 0);
    drives[1].setMotor((control & 0x08) != 0);
    switch (control & 0x03) {
    case 1: fdc.setDrive(&drives[0]); break;
    case 2: fdc.setDrive(drivesFitted > 1 ? &drives[1] : nullptr); break;
    default: fdc.setDrive(nullptr); break;
    }
}

void DiskUnit::reset()
{
    control = 0;
    applyDriveControl();
    fdc.reset();
}

std::uint8_t DiskUnit::readIo(std::uint8_t port)
{
    if (port >= 0x30 && port <= 0x33) return fdc.read(port & 3);
    if (port == 0x34) {
        return static_cast<std::uint8_t>((fdc.interruptRequest() ? 0x80 : 0) | (fdc.dataRequest() ? 0x40 : 0));
    }
    return 0xFF;
}

void DiskUnit::writeIo(std::uint8_t port, std::uint8_t value)
{
    const bool writeCommand = (value & 0xE0) == 0xA0 || (value & 0xF0) == 0xF0;
    if (port == 0x30 && writeCommand && blankOnFormat) {
        const auto selected = control & 0x03;
        if (selected == 1 || (selected == 2 && drivesFitted > 1)) {
            auto& floppy = drives[selected - 1];
            if (!floppy.hasDisk()) floppy.insertBlank(blankDoubleSided);
        }
    }
    if (port >= 0x30 && port <= 0x33) fdc.write(port & 3, value);
    if (port == 0x34) {
        control = value;
        applyDriveControl();
    }
    if (port == 0x38) {
        fdc.setDoubleDensity((value & 0x01) == 0);
        fdc.setSide(static_cast<std::uint8_t>((value >> 1) & 1));
    }
}

void DiskUnit::tick()
{
    fdc.tick();
    for (auto& floppy : drives) floppy.tick();
}

} // namespace svi801
