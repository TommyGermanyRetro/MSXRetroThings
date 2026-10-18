#include "FD1793/FD1793.hpp"

#include <algorithm>
#include <array>

namespace fd1793 {

namespace {

// Step rates r1 r0 at 1 MHz.
constexpr std::array<std::uint32_t, 4> StepRateUs{6000, 12000, 20000, 30000};
constexpr std::uint32_t HeadSettleUs = 30000;
constexpr unsigned SearchRevolutions = 5;
constexpr unsigned HeadUnloadRevolutions = 15;
constexpr std::uint32_t MfmByteUs = 32;
constexpr std::uint32_t FmByteUs = 64;

// Bytes from the start of an ID field to the first data byte (ID, CRC, gap 2, sync, data mark).
constexpr std::uint32_t MfmIdToDataBytes = 7 + 22 + 12 + 4;
constexpr std::uint32_t FmIdToDataBytes = 7 + 11 + 6 + 1;
// Bytes from the start of an ID field to the end of its CRC.
constexpr std::uint32_t IdFieldBytes = 7;

bool isReadSector(std::uint8_t command) noexcept { return (command & 0xE0) == 0x80; }
bool isWriteSector(std::uint8_t command) noexcept { return (command & 0xE0) == 0xA0; }
bool isReadAddress(std::uint8_t command) noexcept { return (command & 0xF0) == 0xC0; }
bool isReadTrack(std::uint8_t command) noexcept { return (command & 0xF0) == 0xE0; }
bool isWriteTrack(std::uint8_t command) noexcept { return (command & 0xF0) == 0xF0; }
bool isRestore(std::uint8_t command) noexcept { return (command & 0xF0) == 0x00; }
bool isSeek(std::uint8_t command) noexcept { return (command & 0xF0) == 0x10; }

std::size_t sectorLength(const SectorId& id) noexcept
{
    return static_cast<std::size_t>(128) << (id.sizeCode & 3);
}

} // namespace

std::uint16_t crc16(const std::uint8_t* bytes, std::size_t count, std::uint16_t crc) noexcept
{
    for (std::size_t i = 0; i < count; ++i) {
        crc = static_cast<std::uint16_t>(crc ^ (bytes[i] << 8));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021) : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

FD1793::FD1793(std::uint32_t hostClockHz)
    : clockHz(hostClockHz)
{
}

void FD1793::reset()
{
    busy = false;
    phase = Phase::Idle;
    intrq = false;
    drq = false;
    immediateInterrupt = false;
    interruptConditions = 0;
    headLoaded = false;
    errors = 0;
    sector = 1;
    data = 0;
    startCommand(0x03);
}

std::uint8_t FD1793::read(std::uint8_t reg)
{
    switch (reg & 3) {
    case 0: {
        const auto value = peekStatus();
        if (!immediateInterrupt) intrq = false;
        return value;
    }
    case 1: return track;
    case 2: return sector;
    default:
        drq = false;
        return data;
    }
}

void FD1793::write(std::uint8_t reg, std::uint8_t value)
{
    switch (reg & 3) {
    case 0:
        if ((value & 0xF0) == 0xD0) {
            forceInterrupt(value);
        } else if (!busy) {
            startCommand(value);
        }
        break;
    case 1: track = value; break;
    case 2: sector = value; break;
    default:
        data = value;
        drq = false;
        break;
    }
}

void FD1793::setDrive(Drive* selected) noexcept
{
    drive = selected;
}

void FD1793::setSide(std::uint8_t side) noexcept
{
    sideSelect = side & 1;
}

void FD1793::setDoubleDensity(bool doubleDensity) noexcept
{
    mfm = doubleDensity;
}

bool FD1793::interruptRequest() const noexcept
{
    return intrq;
}

bool FD1793::dataRequest() const noexcept
{
    return drq;
}

std::uint8_t FD1793::trackRegister() const noexcept
{
    return track;
}

std::uint8_t FD1793::sectorRegister() const noexcept
{
    return sector;
}

bool FD1793::driveReady() const noexcept
{
    return drive && drive->ready();
}

std::uint8_t FD1793::peekStatus() const noexcept
{
    std::uint8_t value = busy ? StatusBusy : 0;
    if (!driveReady()) value |= StatusNotReady;
    if (typeOne) {
        if (drive && drive->index()) value |= StatusIndex;
        if (drive && drive->track0()) value |= StatusTrack0;
        if (headLoaded) value |= StatusHeadLoaded;
        if (drive && drive->writeProtected()) value |= StatusWriteProtect;
        value |= errors & (StatusSeekError | StatusCrcError);
    } else {
        if (drq) value |= StatusDataRequest;
        value |= errors & (StatusLostData | StatusCrcError | StatusNotFound | StatusWriteProtect);
    }
    return value;
}

std::uint64_t FD1793::ticksForUs(std::uint64_t microseconds) const noexcept
{
    return std::max<std::uint64_t>(1, microseconds * clockHz / 1000000);
}

std::uint64_t FD1793::byteTicks() const noexcept
{
    return ticksForUs(mfm ? MfmByteUs : FmByteUs);
}

std::uint32_t FD1793::untilPositionUs(std::uint32_t positionUs) const noexcept
{
    const auto now = drive ? drive->rotationUs() : 0;
    return (positionUs % RevolutionUs + RevolutionUs - now) % RevolutionUs;
}

void FD1793::presentByte(std::uint8_t value)
{
    if (drq) errors |= StatusLostData;
    data = value;
    drq = true;
}

void FD1793::tick()
{
    const bool index = drive && drive->index();
    if (index && !lastIndex) {
        if (interruptConditions & 0x04) intrq = true;
        if (!busy && headLoaded && ++idleRevolutions >= HeadUnloadRevolutions) headLoaded = false;
    }
    lastIndex = index;
    const bool ready = driveReady();
    if (ready != lastReady) {
        if ((interruptConditions & 0x01) && ready) intrq = true;
        if ((interruptConditions & 0x02) && !ready) intrq = true;
    }
    lastReady = ready;

    if (busy && waitTicks > 0 && --waitTicks == 0) advance();
}

void FD1793::startCommand(std::uint8_t value)
{
    command = value;
    intrq = false;
    immediateInterrupt = false;
    interruptConditions = 0;
    errors = 0;
    busy = true;
    idleRevolutions = 0;
    waitTicks = 1;

    if (command < 0x80) {
        typeOne = true;
        headLoaded = (command & 0x08) != 0;
        if (isRestore(command)) {
            track = 0xFF;
            data = 0;
            stepsLeft = 255;
        } else if (isSeek(command)) {
            stepsLeft = 256;
        } else {
            if ((command & 0x60) == 0x40) stepInward = true;
            if ((command & 0x60) == 0x60) stepInward = false;
            stepsLeft = 1;
        }
        phase = Phase::Step;
        return;
    }

    typeOne = false;
    drq = false;
    if (!driveReady()) {
        finish(0);
        return;
    }
    headLoaded = true;
    if (isWriteTrack(command) && drive->writeProtected()) {
        finish(StatusWriteProtect);
        return;
    }
    phase = Phase::HeadSettle;
    waitTicks = (command & 0x04) ? ticksForUs(HeadSettleUs) : 1;
}

void FD1793::forceInterrupt(std::uint8_t value)
{
    if (busy) {
        busy = false;
        phase = Phase::Idle;
        waitTicks = 0;
    } else {
        typeOne = true;
        errors = 0;
    }
    drq = false;
    interruptConditions = value & 0x07;
    immediateInterrupt = (value & 0x08) != 0;
    intrq = immediateInterrupt;
    lastReady = driveReady();
}

void FD1793::finish(std::uint8_t errorBits)
{
    errors |= errorBits;
    busy = false;
    phase = Phase::Idle;
    waitTicks = 0;
    intrq = true;
}

void FD1793::nextStep()
{
    const bool stepCommand = !isRestore(command) && !isSeek(command);
    if (!stepCommand) {
        if (track == data) {
            phase = Phase::StepDone;
            waitTicks = 1;
            return;
        }
        stepInward = data > track;
    }
    if (!stepInward && drive && drive->track0()) {
        track = 0;
        phase = Phase::StepDone;
        waitTicks = 1;
        return;
    }
    if (stepsLeft == 0) {
        finish(StatusSeekError);
        return;
    }
    --stepsLeft;
    if (!stepCommand || (command & 0x10)) track = static_cast<std::uint8_t>(stepInward ? track + 1 : track - 1);
    if (drive) drive->step(stepInward);
    phase = stepCommand ? Phase::StepDone : Phase::Step;
    waitTicks = ticksForUs(StepRateUs[command & 3]);
}

void FD1793::searchSector()
{
    if (!driveReady()) {
        finish(StatusNotFound);
        return;
    }
    if (isWriteSector(command) && drive->writeProtected()) {
        finish(StatusWriteProtect);
        return;
    }
    const auto onTrack = drive->sectors(sideSelect, mfm);
    const bool compareSide = (command & 0x02) != 0;
    const auto side = static_cast<std::uint8_t>((command >> 3) & 1);
    std::uint32_t bestWait = RevolutionUs + 1;
    for (std::size_t i = 0; i < onTrack.size(); ++i) {
        const auto& id = onTrack[i].id;
        if (isReadAddress(command) || (id.track == track && id.sector == sector && (!compareSide || id.side == side))) {
            const auto wait = untilPositionUs(onTrack[i].positionUs);
            if (wait < bestWait) {
                bestWait = wait;
                sectorIndex = i;
            }
        }
    }
    if (bestWait > RevolutionUs) {
        errors |= StatusNotFound;
        phase = Phase::Finish;
        waitTicks = ticksForUs(static_cast<std::uint64_t>(SearchRevolutions) * RevolutionUs);
        return;
    }
    const auto id = onTrack[sectorIndex].id;
    const auto byteUs = mfm ? MfmByteUs : FmByteUs;
    if (isReadAddress(command)) {
        std::array<std::uint8_t, 8> field{0xA1, 0xA1, 0xA1, 0xFE, id.track, id.side, id.sector, id.sizeCode};
        const auto crc = mfm ? crc16(field.data(), field.size()) : crc16(field.data() + 3, 5);
        buffer = {id.track, id.side, id.sector, id.sizeCode, static_cast<std::uint8_t>(crc >> 8),
                  static_cast<std::uint8_t>(crc & 0xFF)};
        bufferPosition = 0;
        phase = Phase::ReadAddress;
        waitTicks = ticksForUs(bestWait + byteUs);
        return;
    }
    if (isReadSector(command)) {
        buffer.clear();
        if (!drive->readSector(sideSelect, mfm, sectorIndex, buffer)) buffer.assign(sectorLength(id), 0);
        bufferPosition = 0;
        phase = Phase::ReadData;
        waitTicks = ticksForUs(bestWait + (mfm ? MfmIdToDataBytes : FmIdToDataBytes) * byteUs);
        return;
    }
    phase = Phase::WriteStart;
    waitTicks = ticksForUs(bestWait + IdFieldBytes * byteUs);
}

void FD1793::startTrackTransfer()
{
    bufferPosition = 0;
    if (isReadTrack(command)) {
        buffer = buildTrackImage();
        phase = Phase::ReadTrackData;
    } else {
        buffer.clear();
        buffer.push_back(data);
        drq = true;
        phase = Phase::WriteTrackData;
    }
    waitTicks = byteTicks();
}

std::vector<std::uint8_t> FD1793::buildTrackImage() const
{
    const std::size_t length = RevolutionUs / (mfm ? MfmByteUs : FmByteUs);
    const std::uint8_t gap = mfm ? 0x4E : 0xFF;
    std::vector<std::uint8_t> raw;
    const auto fill = [&](std::size_t count, std::uint8_t value) { raw.insert(raw.end(), count, value); };
    const auto addCrc = [&](std::size_t start) {
        const auto crc = crc16(raw.data() + start, raw.size() - start);
        raw.push_back(static_cast<std::uint8_t>(crc >> 8));
        raw.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    };
    if (mfm) {
        fill(80, gap); fill(12, 0x00); fill(3, 0xC2); fill(1, 0xFC); fill(50, gap);
    } else {
        fill(40, gap); fill(6, 0x00); fill(1, 0xFC); fill(26, gap);
    }
    if (driveReady()) {
        const auto onTrack = drive->sectors(sideSelect, mfm);
        for (std::size_t i = 0; i < onTrack.size(); ++i) {
            const auto& id = onTrack[i].id;
            fill(mfm ? 12 : 6, 0x00);
            const auto idStart = raw.size();
            if (mfm) fill(3, 0xA1);
            raw.insert(raw.end(), {0xFE, id.track, id.side, id.sector, id.sizeCode});
            addCrc(idStart);
            fill(mfm ? 22 : 11, gap);
            fill(mfm ? 12 : 6, 0x00);
            const auto dataStart = raw.size();
            if (mfm) fill(3, 0xA1);
            raw.push_back(0xFB);
            std::vector<std::uint8_t> sectorData;
            if (!drive->readSector(sideSelect, mfm, i, sectorData)) sectorData.assign(sectorLength(id), 0);
            raw.insert(raw.end(), sectorData.begin(), sectorData.end());
            addCrc(dataStart);
            fill(mfm ? 54 : 27, gap);
        }
    }
    raw.resize(length, gap);
    return raw;
}

void FD1793::finishWriteTrack()
{
    // In WRITE TRACK, F5h/F6h stand for the A1h/C2h sync marks in MFM and F7h for the two CRC
    // bytes; FEh starts an ID field, FBh or F8h a data field.
    std::vector<FormattedSector> laidDown;
    for (std::size_t i = 0; i + 4 < buffer.size(); ++i) {
        if (buffer[i] != 0xFE) continue;
        FormattedSector formatted;
        formatted.id = SectorId{buffer[i + 1], buffer[i + 2], buffer[i + 3], buffer[i + 4]};
        std::size_t j = i + 5;
        while (j < buffer.size() && buffer[j] != 0xFB && buffer[j] != 0xF8 && buffer[j] != 0xFE) ++j;
        if (j >= buffer.size() || buffer[j] == 0xFE) {
            i = j - 1;
            continue;
        }
        const auto length = sectorLength(formatted.id);
        if (j + 1 + length > buffer.size()) break;
        formatted.data.assign(buffer.begin() + static_cast<std::ptrdiff_t>(j + 1),
                              buffer.begin() + static_cast<std::ptrdiff_t>(j + 1 + length));
        laidDown.push_back(std::move(formatted));
        i = j + length;
    }
    if (drive) (void)drive->formatTrack(sideSelect, mfm, laidDown);
    finish(0);
}

void FD1793::advance()
{
    switch (phase) {
    case Phase::Step:
        nextStep();
        break;
    case Phase::StepDone:
        if (isRestore(command) && !(drive && drive->track0())) {
            finish(StatusSeekError);
        } else if (command & 0x04) {
            phase = Phase::Verify;
            waitTicks = ticksForUs(HeadSettleUs);
            headLoaded = true;
        } else {
            finish(0);
        }
        break;
    case Phase::Verify: {
        if (!driveReady()) {
            errors |= StatusSeekError;
            phase = Phase::Finish;
            waitTicks = ticksForUs(static_cast<std::uint64_t>(SearchRevolutions) * RevolutionUs);
            break;
        }
        std::uint32_t bestWait = RevolutionUs + 1;
        for (const auto& entry : drive->sectors(sideSelect, mfm)) {
            if (entry.id.track == track) bestWait = std::min(bestWait, untilPositionUs(entry.positionUs));
        }
        if (bestWait > RevolutionUs) {
            errors |= StatusSeekError;
            phase = Phase::Finish;
            waitTicks = ticksForUs(static_cast<std::uint64_t>(SearchRevolutions) * RevolutionUs);
        } else {
            phase = Phase::Finish;
            waitTicks = ticksForUs(bestWait + IdFieldBytes * (mfm ? MfmByteUs : FmByteUs));
        }
        break;
    }
    case Phase::HeadSettle:
        if (isReadTrack(command) || isWriteTrack(command)) {
            if (isWriteTrack(command)) {
                drq = true;
                phase = Phase::WriteTrackStart;
                waitTicks = 3 * byteTicks();
            } else {
                phase = Phase::TrackWaitIndex;
                waitTicks = ticksForUs(untilPositionUs(0));
            }
        } else {
            searchSector();
        }
        break;
    case Phase::Search:
        searchSector();
        break;
    case Phase::ReadData:
        if (bufferPosition < buffer.size()) {
            presentByte(buffer[bufferPosition++]);
            waitTicks = byteTicks();
        } else if (command & 0x10) {
            ++sector;
            phase = Phase::Search;
            waitTicks = 2 * byteTicks();
        } else {
            phase = Phase::Finish;
            waitTicks = 2 * byteTicks();
        }
        break;
    case Phase::WriteStart:
        drq = true;
        phase = Phase::WriteData;
        buffer.clear();
        waitTicks = (mfm ? 22 : 11) * byteTicks();
        break;
    case Phase::WriteData: {
        const auto length = sectorLength(drive->sectors(sideSelect, mfm)[sectorIndex].id);
        if (buffer.empty() && drq) {
            finish(StatusLostData);
            break;
        }
        if (buffer.size() < length) {
            if (drq) errors |= StatusLostData;
            buffer.push_back(drq ? 0 : data);
            if (buffer.size() < length) drq = true;
            waitTicks = byteTicks();
            break;
        }
        (void)drive->writeSector(sideSelect, mfm, sectorIndex, buffer);
        if (command & 0x10) {
            ++sector;
            phase = Phase::Search;
        } else {
            phase = Phase::Finish;
        }
        waitTicks = 2 * byteTicks();
        break;
    }
    case Phase::ReadAddress:
        if (bufferPosition < buffer.size()) {
            presentByte(buffer[bufferPosition++]);
            waitTicks = byteTicks();
        } else {
            sector = buffer[0];
            finish(0);
        }
        break;
    case Phase::WriteTrackStart:
        if (drq) {
            finish(StatusLostData);
            break;
        }
        phase = Phase::TrackWaitIndex;
        waitTicks = ticksForUs(untilPositionUs(0));
        break;
    case Phase::TrackWaitIndex:
        startTrackTransfer();
        break;
    case Phase::ReadTrackData:
        if (bufferPosition < buffer.size()) {
            presentByte(buffer[bufferPosition++]);
            waitTicks = byteTicks();
        } else {
            finish(0);
        }
        break;
    case Phase::WriteTrackData: {
        const std::size_t length = RevolutionUs / (mfm ? MfmByteUs : FmByteUs);
        if (buffer.size() < length) {
            if (drq) errors |= StatusLostData;
            buffer.push_back(drq ? 0 : data);
            drq = true;
            waitTicks = byteTicks();
        } else {
            drq = false;
            finishWriteTrack();
        }
        break;
    }
    case Phase::Finish:
        finish(0);
        break;
    case Phase::Idle:
        break;
    }
}

} // namespace fd1793
