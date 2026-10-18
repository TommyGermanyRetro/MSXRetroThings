#include "WD2793/FloppyDrive.hpp"

#include <cstring>
#include <type_traits>

#include <utility>

namespace wd2793 {

FloppyDrive::FloppyDrive(bool doubleSided)
    : doubleSidedDrive(doubleSided)
{
}

bool FloppyDrive::insert(std::vector<std::uint8_t> image, bool writeProtected)
{
    unsigned sides = 0;
    unsigned sectors = 0;
    for (const unsigned candidateSides : {1u, 2u}) {
        for (const unsigned candidateSectors : {8u, 9u}) {
            if (image.size() == Cylinders * candidateSides * candidateSectors * SectorSize) {
                sides = candidateSides;
                sectors = candidateSectors;
            }
        }
    }
    if (sides == 0 || (sides == 2 && !doubleSidedDrive)) return false;
    data = std::move(image);
    imageSides = sides;
    imageSectors = sectors;
    present = true;
    protectedDisk = writeProtected;
    dirty = false;
    changed = true;
    return true;
}

void FloppyDrive::eject() noexcept
{
    data.clear();
    present = false;
    dirty = false;
    changed = true;
    imageSides = 0;
    imageSectors = 0;
}

bool FloppyDrive::inserted() const noexcept
{
    return present;
}

const std::vector<std::uint8_t>& FloppyDrive::image() const noexcept
{
    return data;
}

bool FloppyDrive::modified() const noexcept
{
    return dirty;
}

void FloppyDrive::clearModified() noexcept
{
    dirty = false;
}

unsigned FloppyDrive::sides() const noexcept
{
    return imageSides;
}

unsigned FloppyDrive::sectorsPerTrack() const noexcept
{
    return imageSectors;
}

void FloppyDrive::setMotor(bool on) noexcept
{
    motorOn = on;
}

bool FloppyDrive::motor() const noexcept
{
    return motorOn;
}

void FloppyDrive::setSide(unsigned side) noexcept
{
    selectedSide = side & 1;
}

unsigned FloppyDrive::side() const noexcept
{
    return selectedSide;
}

unsigned FloppyDrive::headCylinder() const noexcept
{
    return cylinder;
}

void FloppyDrive::step(bool inward) noexcept
{
    if (inward) {
        if (cylinder < MaximumHeadCylinder) ++cylinder;
    } else if (cylinder > 0) {
        --cylinder;
    }
}

bool FloppyDrive::ready() const noexcept
{
    return present && (doubleSidedDrive || selectedSide == 0);
}

bool FloppyDrive::track00() const noexcept
{
    return motorOn && cylinder == 0;
}

bool FloppyDrive::writeProtected() const noexcept
{
    return motorOn && present && protectedDisk;
}

bool FloppyDrive::trackFormatted() const noexcept
{
    return present && cylinder < Cylinders && selectedSide < imageSides;
}

std::size_t FloppyDrive::sectorOffset(unsigned number) const noexcept
{
    if (!trackFormatted() || number < 1 || number > imageSectors) return data.size();
    const std::size_t index = (static_cast<std::size_t>(cylinder) * imageSides + selectedSide) * imageSectors + (number - 1);
    return index * SectorSize;
}

const std::uint8_t* FloppyDrive::sector(unsigned number) const noexcept
{
    const std::size_t offset = sectorOffset(number);
    return offset < data.size() ? &data[offset] : nullptr;
}

std::uint8_t* FloppyDrive::writableSector(unsigned number) noexcept
{
    const std::size_t offset = sectorOffset(number);
    if (offset >= data.size()) return nullptr;
    dirty = true;
    return &data[offset];
}

bool FloppyDrive::takeDiskChanged() noexcept
{
    const bool result = changed;
    changed = false;
    return result;
}

bool FloppyDrive::diskChanged() const noexcept
{
    return changed;
}

std::vector<std::uint8_t> FloppyDrive::saveState() const
{
    return {static_cast<std::uint8_t>(motorOn ? 1 : 0), static_cast<std::uint8_t>(selectedSide),
            static_cast<std::uint8_t>(cylinder)};
}

bool FloppyDrive::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != 3 || state[1] > 1 || state[2] > 85) return false;
    motorOn = state[0] != 0;
    selectedSide = state[1];
    cylinder = state[2];
    return true;
}

} // namespace wd2793
