#include "WD2793/WD2793.hpp"

#include <cstring>
#include <type_traits>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace wd2793 {

namespace {

constexpr std::uint64_t Never = std::numeric_limits<std::uint64_t>::max();

// Double density at 250 kbit/s: 32 us per byte, 6250 bytes per revolution at 300 rpm.
constexpr std::uint32_t MicrosecondsPerByte = 32;
constexpr unsigned BytesPerTrack = 6250;
constexpr std::array<std::uint32_t, 4> StepMilliseconds = {6, 12, 20, 30};
constexpr std::uint32_t SettleMilliseconds = 30;
constexpr unsigned IndexRevolutions = 5;

// IBM System 34 track layout in bytes from the index pulse.
constexpr unsigned TrackHeaderBytes = 80 + 12 + 4 + 50;
constexpr unsigned SectorSlotBytes = 12 + 10 + 22 + 12 + 4 + SectorSize + 2 + 84;
constexpr unsigned IdMarkOffset = 12 + 3;    // the FE byte after the sync bytes and A1 A1 A1
constexpr unsigned IdFieldBytes = 7;         // FE, C, H, R, N and two CRC bytes
constexpr unsigned DataOffset = 12 + 10 + 22 + 12 + 4;

// Status bits.
constexpr std::uint8_t Busy = 0x01;
constexpr std::uint8_t IndexBit = 0x02;
constexpr std::uint8_t DataRequestBit = 0x02;
constexpr std::uint8_t Track00Bit = 0x04;
constexpr std::uint8_t LostData = 0x04;
constexpr std::uint8_t CrcError = 0x08;
constexpr std::uint8_t SeekError = 0x10;
constexpr std::uint8_t RecordNotFound = 0x10;
constexpr std::uint8_t HeadLoaded = 0x20;
constexpr std::uint8_t WriteProtect = 0x40;
constexpr std::uint8_t NotReady = 0x80;

std::uint16_t crc16(std::uint16_t crc, std::uint8_t value) noexcept
{
    crc = static_cast<std::uint16_t>(crc ^ (value << 8));
    for (int bit = 0; bit < 8; ++bit) {
        crc = static_cast<std::uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}

enum class Phase {
    Idle,
    Step,
    Settle,
    Verify,
    VerifyCheck,
    SearchSector,
    CheckId,
    ReadBytes,
    ReadEnd,
    WriteFirstCheck,
    WriteBytes,
    WriteEnd,
    SearchAddress,
    AddressFound,
    WaitIndexRead,
    WaitIndexWrite,
    WriteTrackBytes,
};

} // namespace

struct WD2793::Impl {
    std::uint64_t now = 0;
    std::uint64_t eventTime = Never;
    std::uint64_t deadline = 0;
    std::uint64_t byteTicks = 1;
    std::uint64_t rotationTicks = 1;
    std::uint64_t indexTicks = 1;
    std::uint64_t millisecondTicks = 1;
    FloppyDrive* drive = nullptr;

    std::uint8_t status = 0;
    std::uint8_t track = 0;
    std::uint8_t sector = 0;
    std::uint8_t data = 0;
    std::uint8_t command = 0;
    bool commandBusy = false;
    bool drq = false;
    bool intrq = false;
    bool immediateInterrupt = false;
    bool indexInterrupt = false;
    bool typeIStatus = true;
    bool headLoaded = false;
    bool directionIn = true;

    Phase phase = Phase::Idle;
    unsigned stepPulses = 0;
    unsigned slot = 0;
    std::vector<std::uint8_t> buffer;
    std::size_t position = 0;

    explicit Impl(std::uint32_t clockHz)
    {
        const std::uint64_t clock = clockHz ? clockHz : 1;
        byteTicks = std::max<std::uint64_t>(1, clock * MicrosecondsPerByte / 1000000);
        rotationTicks = byteTicks * BytesPerTrack;
        indexTicks = rotationTicks / 50;
        millisecondTicks = std::max<std::uint64_t>(1, clock / 1000);
    }

    [[nodiscard]] bool ready() const noexcept { return drive != nullptr && drive->ready(); }
    [[nodiscard]] bool rotating() const noexcept { return drive != nullptr && drive->motor() && drive->inserted(); }
    [[nodiscard]] std::uint64_t angle(std::uint64_t time) const noexcept { return time % rotationTicks; }
    [[nodiscard]] bool indexPulse() const noexcept { return rotating() && angle(now) < indexTicks; }
    [[nodiscard]] std::uint64_t nextIndex(std::uint64_t time) const noexcept
    {
        return time - angle(time) + rotationTicks;
    }
    [[nodiscard]] unsigned sectorCount() const noexcept { return drive != nullptr ? drive->sectorsPerTrack() : 0; }
    [[nodiscard]] std::uint64_t slotStart(unsigned index) const noexcept
    {
        return static_cast<std::uint64_t>(TrackHeaderBytes + index * SectorSlotBytes) * byteTicks;
    }

    // Time at which the FE byte of the next ID field passes the head, or Never on an unreadable track.
    [[nodiscard]] std::uint64_t nextId(std::uint64_t from, unsigned& foundSlot) const noexcept
    {
        if (!rotating() || !drive->trackFormatted()) return Never;
        std::uint64_t best = Never;
        for (unsigned index = 0; index < sectorCount(); ++index) {
            std::uint64_t time = from - angle(from) + slotStart(index) + IdMarkOffset * byteTicks;
            // The controller needs the full A1 A1 A1 sequence in front of the mark.
            if (time < from + 4 * byteTicks) time += rotationTicks;
            if (time < best) {
                best = time;
                foundSlot = index;
            }
        }
        return best;
    }

    void schedule(Phase next, std::uint64_t time) noexcept
    {
        phase = next;
        eventTime = time;
    }

    void finish() noexcept
    {
        commandBusy = false;
        intrq = true;
        phase = Phase::Idle;
        eventTime = Never;
    }

    // ---------------------------------------------------------------------------------------------
    // Type I: Restore, Seek, Step, Step-in, Step-out
    // ---------------------------------------------------------------------------------------------

    void startTypeI() noexcept
    {
        typeIStatus = true;
        commandBusy = true;
        drq = false;
        status &= static_cast<std::uint8_t>(~(SeekError | CrcError));
        if (command & 0x08) headLoaded = true;
        stepPulses = 0;
        const std::uint8_t kind = command & 0xF0;
        if (kind < 0x10) {
            track = 0xFF;
            data = 0;
        } else if (kind >= 0x40) {
            directionIn = kind < 0x60;
        }
        schedule(Phase::Step, now);
    }

    [[nodiscard]] bool seeking() const noexcept { return (command & 0xE0) == 0x00; }

    void step() noexcept
    {
        // A Restore gives up after 255 step pulses without TRACK 00.
        if ((command & 0xF0) == 0x00 && stepPulses == 255) {
            status |= SeekError;
            finish();
            return;
        }
        const bool updateTrack = seeking() || (command & 0x10);
        if (seeking()) {
            if (track == data) {
                endStepping();
                return;
            }
            directionIn = data > track;
        }
        if (updateTrack) track = static_cast<std::uint8_t>(directionIn ? track + 1 : track - 1);
        if (!directionIn && drive != nullptr && drive->track00()) {
            track = 0;
            endStepping();
            return;
        }
        if (drive != nullptr) drive->step(directionIn);
        ++stepPulses;
        schedule(seeking() ? Phase::Step : Phase::Settle, now + StepMilliseconds[command & 0x03] * millisecondTicks);
    }

    void endStepping() noexcept
    {
        if (command & 0x04) {
            headLoaded = true;
            deadline = 0;
            schedule(Phase::Verify, now + SettleMilliseconds * millisecondTicks);
            return;
        }
        if (!(command & 0x08)) headLoaded = false;
        finish();
    }

    // Waits for the next ID field within five revolutions and schedules `found` after `delay`;
    // without one the command ends with `timeoutStatus`. Without rotation the wait has no end.
    void awaitNextId(Phase waiting, Phase found, std::uint64_t delay, std::uint8_t timeoutStatus) noexcept
    {
        if (!rotating()) {
            schedule(waiting, Never);
            return;
        }
        if (deadline == 0) deadline = now + IndexRevolutions * rotationTicks;
        const std::uint64_t id = nextId(now, slot);
        if (id == Never || id > deadline) {
            if (now >= deadline) {
                status |= timeoutStatus;
                finish();
            } else {
                schedule(waiting, deadline);
            }
            return;
        }
        schedule(found, id + delay);
    }

    void verifyCheck() noexcept
    {
        if (drive->headCylinder() == track) {
            finish();
            return;
        }
        schedule(Phase::Verify, now);
    }

    // ---------------------------------------------------------------------------------------------
    // Type II and III
    // ---------------------------------------------------------------------------------------------

    // Common start of Type II and III commands; returns false when the command ends at once.
    bool startTransfer(bool writing) noexcept
    {
        typeIStatus = false;
        drq = false;
        status = 0;
        if (!ready()) {
            status = NotReady;
            intrq = true;
            return false;
        }
        commandBusy = true;
        headLoaded = true;
        if (writing && drive->writeProtected()) {
            status = WriteProtect;
            finish();
            return false;
        }
        deadline = 0;
        return true;
    }

    [[nodiscard]] std::uint64_t settleDelay() const noexcept
    {
        return (command & 0x04) ? SettleMilliseconds * millisecondTicks : 0;
    }

    void checkId() noexcept
    {
        const bool sideMatches = !(command & 0x02) || drive->side() == ((command >> 3) & 1u);
        const bool match = drive->headCylinder() == track && sideMatches && slot + 1 == sector;
        if (!match) {
            schedule(Phase::SearchSector, now);
            return;
        }
        const std::uint64_t slotBegin = now - IdFieldBytes * byteTicks - IdMarkOffset * byteTicks;
        if ((command & 0xE0) == 0x80) {
            const std::uint8_t* source = drive->sector(sector);
            buffer.assign(source, source + SectorSize);
            position = 0;
            schedule(Phase::ReadBytes, slotBegin + (DataOffset + 1) * byteTicks);
        } else {
            // DRQ two bytes after the ID field; the first byte must be there 22 bytes after it.
            drq = true;
            buffer.assign(SectorSize, 0);
            position = 0;
            schedule(Phase::WriteFirstCheck, now + 22 * byteTicks);
        }
    }

    // Hands the next buffered byte to the CPU; an unread previous byte is lost.
    void readByte() noexcept
    {
        if (drq) status |= LostData;
        data = buffer[position++];
        drq = true;
        if (position < buffer.size()) {
            schedule(Phase::ReadBytes, now + byteTicks);
        } else {
            schedule(Phase::ReadEnd, now + 2 * byteTicks);
        }
    }

    void endRead() noexcept
    {
        switch (command & 0xF0) {
        case 0xC0:
            // Read Address copies the track address of the ID field into the sector register.
            sector = buffer[0];
            finish();
            return;
        case 0xE0:
            finish();
            return;
        default:
            break;
        }
        if (command & 0x10) {
            ++sector;
            deadline = 0;
            schedule(Phase::SearchSector, now);
            return;
        }
        finish();
    }

    void writeFirstCheck() noexcept
    {
        if (drq) {
            status |= LostData;
            finish();
            return;
        }
        // Twelve zero bytes and the data address mark precede the data.
        schedule(Phase::WriteBytes, now + 16 * byteTicks);
    }

    void writeByte() noexcept
    {
        std::uint8_t value = data;
        if (position > 0 && drq) {
            status |= LostData;
            value = 0;
        }
        buffer[position++] = value;
        if (position < buffer.size()) {
            drq = true;
            schedule(Phase::WriteBytes, now + byteTicks);
            return;
        }
        if (std::uint8_t* target = drive->writableSector(sector)) std::copy(buffer.begin(), buffer.end(), target);
        schedule(Phase::WriteEnd, now + 3 * byteTicks);
    }

    void endWrite() noexcept
    {
        if (command & 0x10) {
            ++sector;
            deadline = 0;
            schedule(Phase::SearchSector, now);
            return;
        }
        finish();
    }

    void addressFound() noexcept
    {
        buffer = idField(slot);
        position = 0;
        readByte();
    }

    // C, H, R, N and the CRC of the ID field in a slot of the current track.
    [[nodiscard]] std::vector<std::uint8_t> idField(unsigned index) const
    {
        std::vector<std::uint8_t> field = {static_cast<std::uint8_t>(drive->headCylinder()),
                                           static_cast<std::uint8_t>(drive->side()),
                                           static_cast<std::uint8_t>(index + 1), 2};
        std::uint16_t crc = 0xFFFF;
        for (const std::uint8_t value : {0xA1, 0xA1, 0xA1, 0xFE}) crc = crc16(crc, value);
        for (const std::uint8_t value : field) crc = crc16(crc, value);
        field.push_back(static_cast<std::uint8_t>(crc >> 8));
        field.push_back(static_cast<std::uint8_t>(crc));
        return field;
    }

    // The bytes Read Track returns for the current track, starting at the index pulse.
    [[nodiscard]] std::vector<std::uint8_t> rawTrack() const
    {
        std::vector<std::uint8_t> raw;
        raw.reserve(BytesPerTrack);
        const auto put = [&](std::size_t count, std::uint8_t value) { raw.insert(raw.end(), count, value); };
        put(80, 0x4E);
        put(12, 0x00);
        put(3, 0xC2);
        put(1, 0xFC);
        put(50, 0x4E);
        if (drive != nullptr && drive->trackFormatted()) {
            for (unsigned index = 0; index < sectorCount(); ++index) {
                put(12, 0x00);
                put(3, 0xA1);
                put(1, 0xFE);
                for (const std::uint8_t value : idField(index)) raw.push_back(value);
                put(22, 0x4E);
                put(12, 0x00);
                put(3, 0xA1);
                put(1, 0xFB);
                const std::uint8_t* source = drive->sector(index + 1);
                std::uint16_t crc = 0xFFFF;
                for (const std::uint8_t value : {0xA1, 0xA1, 0xA1, 0xFB}) crc = crc16(crc, value);
                for (std::size_t offset = 0; offset < SectorSize; ++offset) {
                    raw.push_back(source[offset]);
                    crc = crc16(crc, source[offset]);
                }
                raw.push_back(static_cast<std::uint8_t>(crc >> 8));
                raw.push_back(static_cast<std::uint8_t>(crc));
                put(84, 0x4E);
            }
        }
        raw.resize(BytesPerTrack, 0x4E);
        return raw;
    }

    void waitIndexRead() noexcept
    {
        if (!rotating()) {
            schedule(Phase::WaitIndexRead, Never);
            return;
        }
        buffer = rawTrack();
        position = 0;
        schedule(Phase::ReadBytes, nextIndex(now) + byteTicks);
    }

    void waitIndexWrite() noexcept
    {
        if (!rotating()) {
            schedule(Phase::WaitIndexWrite, Never);
            return;
        }
        if (angle(now) != 0) {
            schedule(Phase::WaitIndexWrite, nextIndex(now));
            return;
        }
        if (drq) {
            status |= LostData;
            finish();
            return;
        }
        buffer.clear();
        writeTrackByte();
    }

    void writeTrackByte() noexcept
    {
        std::uint8_t value = data;
        if (!buffer.empty() && drq) {
            status |= LostData;
            value = 0;
        }
        buffer.push_back(value);
        if (buffer.size() < BytesPerTrack) {
            drq = true;
            schedule(Phase::WriteTrackBytes, now + byteTicks);
            return;
        }
        formatTrack();
        finish();
    }

    // Stores the data fields of a written track: F5 writes A1 sync bytes, F7 the two CRC bytes.
    void formatTrack() noexcept
    {
        int pendingSector = -1;
        for (std::size_t index = 0; index + 3 < buffer.size(); ++index) {
            if (buffer[index] != 0xF5 || buffer[index + 1] != 0xF5 || buffer[index + 2] != 0xF5) continue;
            const std::uint8_t mark = buffer[index + 3];
            if (mark == 0xFE && index + 8 < buffer.size()) {
                const bool fullSector = buffer[index + 7] == 2;
                pendingSector = fullSector ? buffer[index + 6] : -1;
                index += 7;
            } else if ((mark == 0xFB || mark == 0xF8) && pendingSector >= 0 && index + 4 + SectorSize <= buffer.size()) {
                if (std::uint8_t* target = drive->writableSector(static_cast<unsigned>(pendingSector))) {
                    std::copy_n(buffer.begin() + static_cast<std::ptrdiff_t>(index + 4), SectorSize, target);
                }
                pendingSector = -1;
                index += 3 + SectorSize;
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Registers
    // ---------------------------------------------------------------------------------------------

    void writeCommand(std::uint8_t value) noexcept
    {
        if ((value & 0xF0) == 0xD0) {
            forceInterrupt(value);
            return;
        }
        if (commandBusy) return;
        command = value;
        if (!immediateInterrupt) intrq = false;
        if (value < 0x80) {
            startTypeI();
            return;
        }
        const std::uint8_t kind = value & 0xF0;
        const bool writing = kind == 0xA0 || kind == 0xB0 || kind == 0xF0;
        if (!startTransfer(writing)) return;
        switch (kind) {
        case 0xC0:
            schedule(Phase::SearchAddress, now + settleDelay());
            break;
        case 0xE0:
            schedule(Phase::WaitIndexRead, now + settleDelay());
            break;
        case 0xF0:
            drq = true;
            schedule(Phase::WaitIndexWrite, now + settleDelay());
            break;
        default:
            schedule(Phase::SearchSector, now + settleDelay());
            break;
        }
    }

    void forceInterrupt(std::uint8_t value) noexcept
    {
        // A running command stops with its status; otherwise the status shows Type I again.
        if (commandBusy) {
            commandBusy = false;
            phase = Phase::Idle;
            eventTime = Never;
        } else {
            status &= static_cast<std::uint8_t>(~(SeekError | CrcError));
            typeIStatus = true;
        }
        drq = false;
        command = value;
        immediateInterrupt = (value & 0x08) != 0;
        indexInterrupt = (value & 0x04) != 0;
        intrq = immediateInterrupt;
    }

    [[nodiscard]] std::uint8_t statusValue() const noexcept
    {
        std::uint8_t value = 0;
        if (!ready()) value |= NotReady;
        if (commandBusy) value |= Busy;
        if (typeIStatus) {
            if (drive != nullptr && drive->writeProtected()) value |= WriteProtect;
            if (headLoaded) value |= HeadLoaded;
            value |= status & (SeekError | CrcError);
            if (drive != nullptr && drive->track00()) value |= Track00Bit;
            if (indexPulse()) value |= IndexBit;
        } else {
            value |= status & (WriteProtect | RecordNotFound | CrcError | LostData);
            if (drq) value |= DataRequestBit;
        }
        return value;
    }

    void handleEvent() noexcept
    {
        switch (phase) {
        case Phase::Step:
            step();
            break;
        case Phase::Settle:
            endStepping();
            break;
        case Phase::Verify:
            awaitNextId(Phase::Verify, Phase::VerifyCheck, IdFieldBytes * byteTicks, SeekError);
            break;
        case Phase::VerifyCheck:
            verifyCheck();
            break;
        case Phase::SearchSector:
            awaitNextId(Phase::SearchSector, Phase::CheckId, IdFieldBytes * byteTicks, RecordNotFound);
            break;
        case Phase::CheckId:
            checkId();
            break;
        case Phase::ReadBytes:
            readByte();
            break;
        case Phase::ReadEnd:
            endRead();
            break;
        case Phase::WriteFirstCheck:
            writeFirstCheck();
            break;
        case Phase::WriteBytes:
            writeByte();
            break;
        case Phase::WriteEnd:
            endWrite();
            break;
        case Phase::SearchAddress:
            awaitNextId(Phase::SearchAddress, Phase::AddressFound, byteTicks, RecordNotFound);
            break;
        case Phase::AddressFound:
            addressFound();
            break;
        case Phase::WaitIndexRead:
            waitIndexRead();
            break;
        case Phase::WaitIndexWrite:
            waitIndexWrite();
            break;
        case Phase::WriteTrackBytes:
            writeTrackByte();
            break;
        default:
            phase = Phase::Idle;
            eventTime = Never;
            break;
        }
    }
};

WD2793::WD2793(std::uint32_t clockHz)
    : impl(std::make_unique<Impl>(clockHz))
{
    reset();
}

WD2793::~WD2793() = default;
WD2793::WD2793(WD2793&&) noexcept = default;
WD2793& WD2793::operator=(WD2793&&) noexcept = default;

void WD2793::reset()
{
    Impl& state = *impl;
    state.phase = Phase::Idle;
    state.eventTime = Never;
    state.status = 0;
    state.track = 0;
    state.data = 0;
    state.drq = false;
    state.intrq = false;
    state.immediateInterrupt = false;
    state.indexInterrupt = false;
    state.commandBusy = false;
    state.headLoaded = false;
    state.directionIn = true;
    state.sector = 0x01;
    state.writeCommand(0x03);
}

void WD2793::selectDrive(FloppyDrive* drive) noexcept
{
    impl->drive = drive;
}

std::uint8_t WD2793::readStatus() noexcept
{
    const std::uint8_t value = impl->statusValue();
    if (!impl->immediateInterrupt) impl->intrq = false;
    return value;
}

std::uint8_t WD2793::readTrack() const noexcept
{
    return impl->track;
}

std::uint8_t WD2793::readSector() const noexcept
{
    return impl->sector;
}

std::uint8_t WD2793::readData() noexcept
{
    impl->drq = false;
    return impl->data;
}

void WD2793::writeCommand(std::uint8_t value) noexcept
{
    impl->writeCommand(value);
}

void WD2793::writeTrack(std::uint8_t value) noexcept
{
    impl->track = value;
}

void WD2793::writeSector(std::uint8_t value) noexcept
{
    impl->sector = value;
}

void WD2793::writeData(std::uint8_t value) noexcept
{
    impl->data = value;
    impl->drq = false;
}

std::uint8_t WD2793::peekStatus() const noexcept
{
    return impl->statusValue();
}

std::uint8_t WD2793::peekData() const noexcept
{
    return impl->data;
}

bool WD2793::interruptRequest() const noexcept
{
    return impl->intrq;
}

bool WD2793::dataRequest() const noexcept
{
    return impl->drq;
}

bool WD2793::busy() const noexcept
{
    return impl->commandBusy;
}

void WD2793::advance(std::uint32_t elapsed) noexcept
{
    Impl& state = *impl;
    const std::uint64_t target = state.now + elapsed;
    if (state.indexInterrupt && state.rotating() && target / state.rotationTicks != state.now / state.rotationTicks) {
        state.intrq = true;
    }
    // A command waiting for a spinning disk resumes when the motor starts.
    if (state.eventTime == Never && state.phase != Phase::Idle && state.rotating()) state.eventTime = state.now;
    while (state.phase != Phase::Idle && state.eventTime <= target) {
        state.now = std::max(state.now, state.eventTime);
        state.handleEvent();
    }
    state.now = target;
}

namespace {

void put64(std::vector<std::uint8_t>& data, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8) data.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint64_t get64(const std::vector<std::uint8_t>& data, std::size_t& offset)
{
    std::uint64_t value = 0;
    for (unsigned index = 0; index < 8; ++index) value |= static_cast<std::uint64_t>(data[offset + index]) << (8 * index);
    offset += 8;
    return value;
}

} // namespace

std::vector<std::uint8_t> WD2793::saveState() const
{
    const auto& d = *impl;
    std::vector<std::uint8_t> state;
    for (const std::uint64_t value : {d.now, d.eventTime, d.deadline, d.byteTicks, d.rotationTicks, d.indexTicks,
                                      d.millisecondTicks, static_cast<std::uint64_t>(d.phase),
                                      static_cast<std::uint64_t>(d.stepPulses), static_cast<std::uint64_t>(d.slot),
                                      static_cast<std::uint64_t>(d.position), static_cast<std::uint64_t>(d.buffer.size())}) {
        put64(state, value);
    }
    for (const bool flag : {d.commandBusy, d.drq, d.intrq, d.immediateInterrupt, d.indexInterrupt, d.typeIStatus,
                            d.headLoaded, d.directionIn}) {
        state.push_back(flag ? 1 : 0);
    }
    state.insert(state.end(), {d.status, d.track, d.sector, d.data, d.command});
    state.insert(state.end(), d.buffer.begin(), d.buffer.end());
    return state;
}

bool WD2793::loadState(const std::vector<std::uint8_t>& state)
{
    constexpr std::size_t FixedSize = 12 * 8 + 8 + 5;
    if (state.size() < FixedSize) return false;
    std::size_t offset = 0;
    std::uint64_t values[12];
    for (auto& value : values) value = get64(state, offset);
    if (values[7] > static_cast<std::uint64_t>(Phase::WriteTrackBytes) || state.size() != FixedSize + values[11] ||
        values[10] > values[11]) {
        return false;
    }
    auto& d = *impl;
    d.now = values[0];
    d.eventTime = values[1];
    d.deadline = values[2];
    d.byteTicks = values[3];
    d.rotationTicks = values[4];
    d.indexTicks = values[5];
    d.millisecondTicks = values[6];
    d.phase = static_cast<Phase>(values[7]);
    d.stepPulses = static_cast<unsigned>(values[8]);
    d.slot = static_cast<unsigned>(values[9]);
    d.position = static_cast<std::size_t>(values[10]);
    bool* flags[] = {&d.commandBusy, &d.drq, &d.intrq, &d.immediateInterrupt, &d.indexInterrupt, &d.typeIStatus,
                     &d.headLoaded, &d.directionIn};
    for (bool* flag : flags) *flag = state[offset++] != 0;
    d.status = state[offset++];
    d.track = state[offset++];
    d.sector = state[offset++];
    d.data = state[offset++];
    d.command = state[offset++];
    d.buffer.assign(state.begin() + static_cast<std::ptrdiff_t>(offset), state.end());
    return true;
}

} // namespace wd2793
