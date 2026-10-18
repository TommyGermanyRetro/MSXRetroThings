#include "SVI801/FloppyDrive.hpp"

#include <algorithm>

namespace svi801 {

namespace {

constexpr unsigned HighestCylinder = 41;
constexpr std::uint32_t IndexPulseUs = 2000;
// ID fields are spread over the revolution after the index gap.
constexpr std::uint32_t FirstSectorUs = 2500;

} // namespace

bool SviDisk::load(const std::vector<std::uint8_t>& image)
{
    if (image.size() != SviDiskSingleSidedSize && image.size() != SviDiskDoubleSidedSize) return false;
    bytes = image;
    sides2 = image.size() == SviDiskDoubleSidedSize;
    changed = false;
    return true;
}

SviDisk SviDisk::blank(bool doubleSided)
{
    SviDisk disk;
    disk.bytes.assign(doubleSided ? SviDiskDoubleSidedSize : SviDiskSingleSidedSize, 0xE5);
    disk.sides2 = doubleSided;
    disk.changed = true;
    return disk;
}

bool SviDisk::doubleSided() const noexcept
{
    return sides2;
}

const std::vector<std::uint8_t>& SviDisk::image() const noexcept
{
    return bytes;
}

bool SviDisk::modified() const noexcept
{
    return changed;
}

void SviDisk::markSaved() noexcept
{
    changed = false;
}

bool SviDisk::doubleDensity(unsigned cylinder, unsigned side) noexcept
{
    return cylinder != 0 || side != 0;
}

unsigned SviDisk::sectorsPerTrack(unsigned cylinder, unsigned side) noexcept
{
    return doubleDensity(cylinder, side) ? 17 : 18;
}

std::size_t SviDisk::sectorSize(unsigned cylinder, unsigned side) noexcept
{
    return doubleDensity(cylinder, side) ? 256 : 128;
}

std::size_t SviDisk::sectorOffset(unsigned cylinder, unsigned side, unsigned sector) const noexcept
{
    const unsigned sides = sides2 ? 2 : 1;
    if (cylinder >= SviDiskCylinders || side >= sides || sector < 1 || sector > sectorsPerTrack(cylinder, side)) {
        return bytes.size();
    }
    const std::size_t fullTrack = 17 * 256;
    const std::size_t firstTrack = 18 * 128;
    const std::size_t trackIndex = static_cast<std::size_t>(cylinder) * sides + side;
    const std::size_t trackStart = trackIndex == 0 ? 0 : firstTrack + (trackIndex - 1) * fullTrack;
    return trackStart + (sector - 1) * sectorSize(cylinder, side);
}

bool SviDisk::readSector(unsigned cylinder, unsigned side, unsigned sector, std::vector<std::uint8_t>& data) const
{
    const auto offset = sectorOffset(cylinder, side, sector);
    if (offset >= bytes.size()) return false;
    const auto size = sectorSize(cylinder, side);
    data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
    return true;
}

bool SviDisk::writeSector(unsigned cylinder, unsigned side, unsigned sector, const std::vector<std::uint8_t>& data)
{
    const auto offset = sectorOffset(cylinder, side, sector);
    if (offset >= bytes.size() || data.size() != sectorSize(cylinder, side)) return false;
    std::copy(data.begin(), data.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
    changed = true;
    return true;
}

FloppyDrive::FloppyDrive(std::uint32_t hostClockHz)
    : clockHz(hostClockHz),
      revolutionTicks(static_cast<std::uint64_t>(hostClockHz) * fd1793::RevolutionUs / 1000000)
{
}

void FloppyDrive::insert(const SviDisk& disk)
{
    inserted = disk;
    diskPresent = true;
    blankDisk = false;
}

void FloppyDrive::insertBlank(bool doubleSided)
{
    insert(SviDisk::blank(doubleSided));
    blankDisk = true;
}

bool FloppyDrive::hasBlankDisk() const noexcept
{
    return diskPresent && blankDisk;
}

void FloppyDrive::eject() noexcept
{
    inserted = SviDisk{};
    diskPresent = false;
    blankDisk = false;
}

bool FloppyDrive::hasDisk() const noexcept
{
    return diskPresent;
}

const SviDisk& FloppyDrive::disk() const noexcept
{
    return inserted;
}

SviDisk& FloppyDrive::disk() noexcept
{
    return inserted;
}

void FloppyDrive::setWriteProtected(bool protectDisk) noexcept
{
    protect = protectDisk;
}

void FloppyDrive::setMotor(bool on) noexcept
{
    motor = on;
}

bool FloppyDrive::motorOn() const noexcept
{
    return motor;
}

unsigned FloppyDrive::cylinder() const noexcept
{
    return head;
}

void FloppyDrive::tick() noexcept
{
    if (motor && diskPresent && ++rotation >= revolutionTicks) rotation = 0;
}

bool FloppyDrive::ready() const
{
    return diskPresent && motor;
}

bool FloppyDrive::writeProtected() const
{
    return diskPresent && protect;
}

bool FloppyDrive::track0() const
{
    return head == 0;
}

bool FloppyDrive::index() const
{
    return diskPresent && rotationUs() < IndexPulseUs;
}

std::uint32_t FloppyDrive::rotationUs() const
{
    return static_cast<std::uint32_t>(rotation * 1000000 / clockHz);
}

void FloppyDrive::step(bool towardsCenter)
{
    if (towardsCenter && head < HighestCylinder) ++head;
    if (!towardsCenter && head > 0) --head;
}

bool FloppyDrive::trackReadable(std::uint8_t side, bool doubleDensity) const noexcept
{
    return diskPresent && head < SviDiskCylinders && (side == 0 || inserted.doubleSided()) &&
           SviDisk::doubleDensity(head, side) == doubleDensity;
}

std::vector<fd1793::TrackSector> FloppyDrive::sectors(std::uint8_t side, bool doubleDensity) const
{
    std::vector<fd1793::TrackSector> list;
    if (!trackReadable(side, doubleDensity)) return list;
    const unsigned count = SviDisk::sectorsPerTrack(head, side);
    const std::uint32_t spacing = (fd1793::RevolutionUs - FirstSectorUs) / count;
    for (unsigned i = 0; i < count; ++i) {
        fd1793::TrackSector entry;
        entry.id = {static_cast<std::uint8_t>(head), side, static_cast<std::uint8_t>(i + 1),
                    static_cast<std::uint8_t>(doubleDensity ? 1 : 0)};
        entry.positionUs = FirstSectorUs + i * spacing;
        list.push_back(entry);
    }
    return list;
}

bool FloppyDrive::readSector(std::uint8_t side, bool doubleDensity, std::size_t index, std::vector<std::uint8_t>& data) const
{
    if (!trackReadable(side, doubleDensity)) return false;
    return inserted.readSector(head, side, static_cast<unsigned>(index + 1), data);
}

bool FloppyDrive::writeSector(std::uint8_t side, bool doubleDensity, std::size_t index, const std::vector<std::uint8_t>& data)
{
    if (!trackReadable(side, doubleDensity) || protect) return false;
    return inserted.writeSector(head, side, static_cast<unsigned>(index + 1), data);
}

// The image keeps only the SVI layout; a format with that layout writes its sector data.
bool FloppyDrive::formatTrack(std::uint8_t side, bool doubleDensity, const std::vector<fd1793::FormattedSector>& laidDown)
{
    if (!trackReadable(side, doubleDensity) || protect) return false;
    const unsigned count = SviDisk::sectorsPerTrack(head, side);
    const auto size = SviDisk::sectorSize(head, side);
    std::vector<bool> seen(count, false);
    for (const auto& formatted : laidDown) {
        const unsigned number = formatted.id.sector;
        if (number < 1 || number > count || formatted.data.size() != size || seen[number - 1]) return false;
        seen[number - 1] = true;
    }
    if (std::find(seen.begin(), seen.end(), false) != seen.end()) return false;
    for (const auto& formatted : laidDown) {
        (void)inserted.writeSector(head, side, formatted.id.sector, formatted.data);
    }
    return true;
}

} // namespace svi801
