#include "WD1002SHD/WD1002SHD.hpp"

#include <algorithm>
#include <utility>

namespace wd1002shd {

namespace {

constexpr std::uint8_t FormatPattern = 0x6C;
constexpr std::uint8_t EmptyByte = 0xE5;
constexpr std::uint8_t StatusError = 0x02;

std::uint32_t blockCount(std::uint8_t value) noexcept
{
    return value == 0 ? 256u : value;
}

} // namespace

WD1002SHD::WD1002SHD()
{
    sectorBuffer.fill(0);
}

void WD1002SHD::reset() noexcept
{
    phase = Phase::BusFree;
    commandLength = 0;
    transfer.clear();
    transferPosition = 0;
}

bool WD1002SHD::select(std::uint8_t idBits) noexcept
{
    if (phase != Phase::BusFree || !(idBits & 0x01)) return false;
    phase = Phase::Command;
    commandLength = 0;
    return true;
}

std::uint8_t WD1002SHD::signals() const noexcept
{
    switch (phase) {
    case Phase::Command: return PhaseCommand;
    case Phase::DataIn: return PhaseDataIn;
    case Phase::DataOut: return PhaseDataOut;
    case Phase::Status: return PhaseStatus;
    case Phase::Message: return PhaseMessage;
    default: return PhaseBusFree;
    }
}

void WD1002SHD::writeData(std::uint8_t value)
{
    if (phase == Phase::Command) {
        command[commandLength++] = value;
        if (commandLength == command.size()) execute();
        return;
    }
    if (phase == Phase::DataOut) {
        transfer[transferPosition++] = value;
        if (transferPosition == transfer.size()) dataOutComplete();
    }
}

std::uint8_t WD1002SHD::readData()
{
    switch (phase) {
    case Phase::DataIn: {
        const auto value = transfer[transferPosition++];
        if (transferPosition == transfer.size()) finish();
        return value;
    }
    case Phase::Status:
        phase = Phase::Message;
        return statusByte;
    case Phase::Message:
        phase = Phase::BusFree;
        return 0x00;
    default:
        return 0xFF;
    }
}

std::uint8_t WD1002SHD::lun() const noexcept
{
    return static_cast<std::uint8_t>(command[1] >> 5);
}

std::uint32_t WD1002SHD::logicalAddress() const noexcept
{
    return (static_cast<std::uint32_t>(command[1] & 0x1F) << 16) | (static_cast<std::uint32_t>(command[2]) << 8) | command[3];
}

std::uint32_t WD1002SHD::capacity() const noexcept
{
    return parameterCylinders * parameterHeads * SectorsPerTrack;
}

void WD1002SHD::finish()
{
    const auto unit = std::min<std::uint8_t>(lun(), 1);
    statusByte = static_cast<std::uint8_t>((unit << 5) | (lastError[unit] != ErrorNone ? StatusError : 0));
    transfer.clear();
    transferPosition = 0;
    phase = Phase::Status;
}

void WD1002SHD::finishWithError(std::uint8_t error)
{
    lastError[std::min<std::uint8_t>(lun(), 1)] = error;
    finish();
}

void WD1002SHD::readSectors(std::uint32_t address, std::uint32_t count)
{
    transfer.assign(static_cast<std::size_t>(count) * SectorSize, EmptyByte);
    const std::size_t offset = static_cast<std::size_t>(address) * SectorSize;
    if (offset < disk.size()) {
        const auto available = std::min(transfer.size(), disk.size() - offset);
        std::copy_n(disk.begin() + static_cast<std::ptrdiff_t>(offset), available, transfer.begin());
    }
}

void WD1002SHD::writeSectors(std::uint32_t address, const std::uint8_t* data, std::uint32_t count)
{
    const std::size_t offset = static_cast<std::size_t>(address) * SectorSize;
    const std::size_t length = static_cast<std::size_t>(count) * SectorSize;
    if (disk.size() < offset + length) disk.resize(offset + length, EmptyByte);
    std::copy_n(data, length, disk.begin() + static_cast<std::ptrdiff_t>(offset));
    changed = true;
}

void WD1002SHD::fillTracks(std::uint32_t firstTrack, std::uint32_t lastTrack)
{
    const bool useBuffer = (command[5] & 0x20) != 0;
    std::vector<std::uint8_t> track(std::size_t{SectorsPerTrack} * SectorSize, FormatPattern);
    if (useBuffer) {
        for (unsigned s = 0; s < SectorsPerTrack; ++s) {
            std::copy(sectorBuffer.begin(), sectorBuffer.end(), track.begin() + static_cast<std::ptrdiff_t>(s * SectorSize));
        }
    }
    for (auto t = firstTrack; t <= lastTrack; ++t) writeSectors(t * SectorsPerTrack, track.data(), SectorsPerTrack);
}

void WD1002SHD::execute()
{
    const auto unit = std::min<std::uint8_t>(lun(), 1);
    const std::uint8_t opcode = command[0];
    // Request Status, Set Parameters (both drives, bytes 1-5 don't care), the sector buffer and the
    // RAM and controller diagnostics do not address a drive.
    const bool driveCommand = opcode != 0x03 && opcode != 0x0C && opcode != 0x0F && opcode != 0x10 &&
                              opcode != 0xE0 && opcode != 0xE4;
    if (opcode != 0x03) {
        lastError[unit] = ErrorNone;
        addressValid[unit] = false;
    }
    if (driveCommand && (unit != 0 || !diskPresent)) {
        finishWithError(ErrorNotReady);
        return;
    }
    const auto address = logicalAddress();
    const auto track = address / SectorsPerTrack;
    switch (opcode) {
    case 0x00: // Test Drive Ready
    case 0x01: // Restore
    case 0x05: // Check Track Format
    case 0x0B: // Seek
    case 0xE0: // RAM Diagnostic
    case 0xE3: // Drive Diagnostic
    case 0xE4: // Controller Diagnostic
        if ((opcode == 0x05 || opcode == 0x0B) && address >= capacity()) {
            finishWithError(ErrorInvalidAddress);
            return;
        }
        finish();
        return;
    case 0x03: { // Request Status
        const auto error = lastError[unit];
        const auto lsa = lastAddress[unit];
        transfer = {static_cast<std::uint8_t>((addressValid[unit] ? 0x80 : 0) | error),
                    static_cast<std::uint8_t>((unit << 5) | ((lsa >> 16) & 0x1F)),
                    static_cast<std::uint8_t>(lsa >> 8), static_cast<std::uint8_t>(lsa)};
        transferPosition = 0;
        lastError[unit] = ErrorNone;
        phase = Phase::DataIn;
        return;
    }
    case 0x04: // Format Drive: from the given track to the end of the disk
    case 0x06: // Format Track
    case 0x07: // Format Bad Track
        if (address >= capacity()) {
            finishWithError(ErrorInvalidAddress);
            return;
        }
        fillTracks(track, opcode == 0x04 ? capacity() / SectorsPerTrack - 1 : track);
        lastAddress[unit] = (opcode == 0x04 ? capacity() : (track + 1) * SectorsPerTrack);
        addressValid[unit] = true;
        finish();
        return;
    case 0x08:   // Read
    case 0xE5: { // Read Long (sector plus four ECC bytes)
        const auto count = blockCount(command[4]);
        lastAddress[unit] = address;
        addressValid[unit] = true;
        if (address + count > capacity()) {
            finishWithError(ErrorInvalidAddress);
            return;
        }
        readSectors(address, count);
        if (opcode == 0xE5) {
            std::vector<std::uint8_t> withEcc;
            for (std::uint32_t s = 0; s < count; ++s) {
                withEcc.insert(withEcc.end(), transfer.begin() + static_cast<std::ptrdiff_t>(s * SectorSize),
                               transfer.begin() + static_cast<std::ptrdiff_t>((s + 1) * SectorSize));
                withEcc.insert(withEcc.end(), 4, 0x00);
            }
            transfer = std::move(withEcc);
        }
        transferPosition = 0;
        phase = Phase::DataIn;
        return;
    }
    case 0x0A:   // Write
    case 0xE6: { // Write Long
        const auto count = blockCount(command[4]);
        lastAddress[unit] = address;
        addressValid[unit] = true;
        if (address + count > capacity()) {
            finishWithError(ErrorInvalidAddress);
            return;
        }
        transfer.assign(static_cast<std::size_t>(count) * (SectorSize + (opcode == 0xE6 ? 4 : 0)), 0);
        transferPosition = 0;
        phase = Phase::DataOut;
        return;
    }
    case 0x0C: // Set Parameters
        transfer.assign(8, 0);
        transferPosition = 0;
        phase = Phase::DataOut;
        return;
    case 0x0D: // Return Burst Error Length
        transfer = {0x00};
        transferPosition = 0;
        phase = Phase::DataIn;
        return;
    case 0x0E: // Format Alternate Track: the alternate track address follows
        transfer.assign(3, 0);
        transferPosition = 0;
        phase = Phase::DataOut;
        return;
    case 0x0F: // Write Sector Buffer
        transfer.assign(SectorSize, 0);
        transferPosition = 0;
        phase = Phase::DataOut;
        return;
    case 0x10: // Read Sector Buffer
        transfer.assign(sectorBuffer.begin(), sectorBuffer.end());
        transferPosition = 0;
        phase = Phase::DataIn;
        return;
    default:
        finishWithError(ErrorInvalidCommand);
        return;
    }
}

void WD1002SHD::dataOutComplete()
{
    const auto address = logicalAddress();
    switch (command[0]) {
    case 0x0A:
        writeSectors(address, transfer.data(), blockCount(command[4]));
        break;
    case 0xE6: {
        std::vector<std::uint8_t> sectors;
        for (std::size_t s = 0; s < blockCount(command[4]); ++s) {
            const auto start = transfer.begin() + static_cast<std::ptrdiff_t>(s * (SectorSize + 4));
            sectors.insert(sectors.end(), start, start + SectorSize);
        }
        writeSectors(address, sectors.data(), blockCount(command[4]));
        break;
    }
    case 0x0C:
        parameterCylinders = std::max(1u, (static_cast<unsigned>(transfer[0]) << 8) | transfer[1]);
        parameterHeads = std::max(1u, static_cast<unsigned>(transfer[2]));
        break;
    case 0x0E:
        fillTracks(address / SectorsPerTrack, address / SectorsPerTrack);
        break;
    case 0x0F:
        std::copy(transfer.begin(), transfer.end(), sectorBuffer.begin());
        break;
    default:
        break;
    }
    finish();
}

void WD1002SHD::attachDisk(std::vector<std::uint8_t> image)
{
    disk = std::move(image);
    diskPresent = true;
    changed = false;
}

void WD1002SHD::detachDisk() noexcept
{
    disk.clear();
    diskPresent = false;
    changed = false;
}

bool WD1002SHD::hasDisk() const noexcept
{
    return diskPresent;
}

const std::vector<std::uint8_t>& WD1002SHD::image() const noexcept
{
    return disk;
}

bool WD1002SHD::modified() const noexcept
{
    return changed;
}

void WD1002SHD::markSaved() noexcept
{
    changed = false;
}

unsigned WD1002SHD::cylinders() const noexcept
{
    return parameterCylinders;
}

unsigned WD1002SHD::heads() const noexcept
{
    return parameterHeads;
}

} // namespace wd1002shd
