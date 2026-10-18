#include "FD1793/FD1793.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

namespace {

constexpr std::uint32_t ClockHz = 3579545;

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// Drive with 40 cylinders; every track holds 9 MFM sectors of 512 bytes or 10 FM sectors of
// 256 bytes (sector numbers from 1), spread evenly over the revolution.
class TestDrive final : public fd1793::Drive {
public:
    unsigned cylinder = 5;
    std::uint64_t ticks = 0;
    bool inserted = true;
    bool protectedDisk = false;
    unsigned steps = 0;
    std::map<std::uint32_t, std::vector<std::uint8_t>> written;
    std::vector<fd1793::FormattedSector> formatted;

    void advance(std::uint64_t count) { ticks += count; }

    bool ready() const override { return inserted; }
    bool writeProtected() const override { return protectedDisk; }
    bool track0() const override { return cylinder == 0; }
    bool index() const override { return inserted && rotationUs() < 2000; }
    std::uint32_t rotationUs() const override
    {
        return static_cast<std::uint32_t>((ticks * 1000000 / ClockHz) % fd1793::RevolutionUs);
    }
    void step(bool towardsCenter) override
    {
        ++steps;
        if (towardsCenter && cylinder < 41) ++cylinder;
        if (!towardsCenter && cylinder > 0) --cylinder;
    }
    std::vector<fd1793::TrackSector> sectors(std::uint8_t side, bool doubleDensity) const override
    {
        std::vector<fd1793::TrackSector> list;
        if (!inserted) return list;
        const unsigned count = doubleDensity ? 9 : 10;
        for (unsigned i = 0; i < count; ++i) {
            fd1793::TrackSector entry;
            entry.id = {static_cast<std::uint8_t>(cylinder), side, static_cast<std::uint8_t>(i + 1),
                        static_cast<std::uint8_t>(doubleDensity ? 2 : 1)};
            entry.positionUs = 10000 + i * (fd1793::RevolutionUs - 20000) / count;
            list.push_back(entry);
        }
        return list;
    }
    bool readSector(std::uint8_t side, bool doubleDensity, std::size_t index, std::vector<std::uint8_t>& data) const override
    {
        const auto id = sectors(side, doubleDensity)[index].id;
        data.assign(static_cast<std::size_t>(128) << id.sizeCode, 0);
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<std::uint8_t>(id.track * 7 + id.side * 3 + id.sector + i);
        }
        return true;
    }
    bool writeSector(std::uint8_t side, bool doubleDensity, std::size_t index, const std::vector<std::uint8_t>& data) override
    {
        const auto id = sectors(side, doubleDensity)[index].id;
        written[id.track * 256u + id.sector] = data;
        return true;
    }
    bool formatTrack(std::uint8_t, bool, const std::vector<fd1793::FormattedSector>& laidDown) override
    {
        formatted = laidDown;
        return true;
    }
};

struct Bench {
    fd1793::FD1793 fdc{ClockHz};
    TestDrive drive;

    Bench()
    {
        fdc.setDrive(&drive);
        fdc.write(0, 0xD0);
    }
    void run(std::uint64_t count)
    {
        for (std::uint64_t i = 0; i < count; ++i) {
            fdc.tick();
            drive.advance(1);
        }
    }
    bool waitIdle(std::uint64_t limit)
    {
        for (std::uint64_t i = 0; i < limit; ++i) {
            if (!(fdc.peekStatus() & fd1793::StatusBusy)) return true;
            run(1);
        }
        return false;
    }
    // Polls like the SVI BASIC boot loop: data on DRQ until INTRQ.
    std::vector<std::uint8_t> readTransfer(std::uint64_t limit, unsigned skipEvery = 0)
    {
        std::vector<std::uint8_t> bytes;
        unsigned seen = 0;
        for (std::uint64_t i = 0; i < limit && !fdc.interruptRequest(); ++i) {
            if (fdc.dataRequest()) {
                ++seen;
                if (skipEvery && seen % skipEvery == 0) {
                    run(400);
                    continue;
                }
                bytes.push_back(fdc.read(3));
            }
            run(20);
        }
        return bytes;
    }
};

bool registersPass()
{
    Bench bench;
    for (unsigned value = 0; value < 256; ++value) {
        bench.fdc.write(2, static_cast<std::uint8_t>(value));
        if (bench.fdc.read(2) != value) return fail("The sector register must read back.");
    }
    bench.fdc.write(1, 0x27);
    bench.fdc.write(3, 0x5A);
    if (bench.fdc.read(1) != 0x27 || bench.fdc.read(3) != 0x5A) return fail("Track and data registers must read back.");
    return true;
}

bool typeOnePass()
{
    Bench bench;
    bench.fdc.write(0, 0x03);
    if (!(bench.fdc.peekStatus() & fd1793::StatusBusy)) return fail("RESTORE must set BUSY.");
    if (!bench.waitIdle(ClockHz) || bench.drive.cylinder != 0 || bench.fdc.trackRegister() != 0) {
        return fail("RESTORE must step out to track 0.");
    }
    if (bench.drive.steps != 5 || !bench.fdc.interruptRequest()) return fail("RESTORE must take 5 steps and interrupt.");
    auto status = bench.fdc.read(0);
    if (!(status & fd1793::StatusTrack0) || (status & fd1793::StatusSeekError) || bench.fdc.interruptRequest()) {
        return fail("Type I status must show TR00; reading it must clear INTRQ.");
    }

    bench.fdc.write(3, 12);
    bench.fdc.write(0, 0x1C);
    if (!bench.waitIdle(ClockHz) || bench.drive.cylinder != 12 || bench.fdc.trackRegister() != 12) {
        return fail("SEEK with verify must reach track 12.");
    }
    status = bench.fdc.read(0);
    if ((status & fd1793::StatusSeekError) || !(status & fd1793::StatusHeadLoaded)) {
        return fail("A verified SEEK must load the head without seek error.");
    }

    bench.fdc.write(0, 0x50);
    (void)bench.waitIdle(ClockHz);
    bench.fdc.write(0, 0x70);
    (void)bench.waitIdle(ClockHz);
    if (bench.drive.cylinder != 12 || bench.fdc.trackRegister() != 12) return fail("STEP IN/OUT with update must track.");
    bench.fdc.write(0, 0x40);
    (void)bench.waitIdle(ClockHz);
    if (bench.drive.cylinder != 13 || bench.fdc.trackRegister() != 12) return fail("STEP IN without update must keep TR.");
    bench.fdc.write(0, 0x24);
    (void)bench.waitIdle(2 * ClockHz);
    if (!(bench.fdc.read(0) & fd1793::StatusSeekError)) return fail("Verify against a wrong track must set seek error.");

    // Step rate 30 ms: ten steps take about 300 ms.
    bench.fdc.write(3, 22);
    bench.fdc.write(0, 0x13);
    const auto before = bench.drive.ticks;
    (void)bench.waitIdle(ClockHz);
    const auto ms = (bench.drive.ticks - before) * 1000 / ClockHz;
    if (ms < 295 || ms > 310) return fail("Step rate 3 must take 30 ms per step.");
    return true;
}

bool indexPass()
{
    Bench bench;
    bool seenHigh = false;
    bool seenLow = false;
    for (unsigned i = 0; i < 1000; ++i) {
        const auto status = bench.fdc.read(0);
        if (status & fd1793::StatusIndex) seenHigh = true;
        else seenLow = true;
        bench.run(ClockHz / 4000);
    }
    if (!seenHigh || !seenLow) return fail("The type I status must show the index pulse.");
    bench.drive.inserted = false;
    if (!(bench.fdc.read(0) & fd1793::StatusNotReady)) return fail("A drive without disk must be not ready.");
    return true;
}

bool readSectorPass()
{
    Bench bench;
    bench.fdc.setDoubleDensity(true);
    bench.fdc.write(1, 5);
    bench.fdc.write(2, 3);
    bench.fdc.write(0, 0x80);
    const auto bytes = bench.readTransfer(ClockHz);
    std::vector<std::uint8_t> expected;
    if (!bench.drive.readSector(0, true, 2, expected) || bytes != expected) return fail("READ SECTOR must deliver the sector.");
    if (bench.fdc.read(0) & (fd1793::StatusLostData | fd1793::StatusNotFound | fd1793::StatusBusy)) {
        return fail("A served READ SECTOR must end without errors.");
    }

    bench.fdc.write(2, 8);
    bench.fdc.write(0, 0x90);
    const auto multi = bench.readTransfer(2 * ClockHz);
    if (multi.size() != 2 * 512 || bench.fdc.sectorRegister() != 10) {
        return fail("Multiple READ SECTOR must read 8 and 9 and stop at the missing sector 10.");
    }
    if (!(bench.fdc.read(0) & fd1793::StatusNotFound)) return fail("The missing sector must set RNF.");

    bench.fdc.write(2, 2);
    bench.fdc.write(0, 0x80);
    (void)bench.readTransfer(ClockHz, 100);
    if (!(bench.fdc.read(0) & fd1793::StatusLostData)) return fail("Unserved bytes must set LOST DATA.");

    bench.fdc.setDoubleDensity(false);
    bench.fdc.write(2, 10);
    bench.fdc.write(0, 0x80);
    if (bench.readTransfer(ClockHz).size() != 256) return fail("FM sectors must read in single density.");

    bench.fdc.write(1, 6);
    bench.fdc.write(0, 0x80);
    const auto start = bench.drive.ticks;
    (void)bench.waitIdle(2 * ClockHz);
    const auto ms = (bench.drive.ticks - start) * 1000 / ClockHz;
    if (!(bench.fdc.read(0) & fd1793::StatusNotFound) || ms < 990 || ms > 1010) {
        return fail("A wrong track must set RNF after five revolutions.");
    }
    bench.drive.inserted = false;
    bench.fdc.write(0, 0x80);
    if (!bench.fdc.interruptRequest() || !(bench.fdc.read(0) & fd1793::StatusNotReady)) {
        return fail("Type II on a not ready drive must end at once.");
    }
    return true;
}

bool writeSectorPass()
{
    Bench bench;
    bench.fdc.setDoubleDensity(true);
    bench.fdc.write(1, 5);
    bench.fdc.write(2, 4);
    bench.fdc.write(0, 0xA0);
    std::uint8_t value = 0;
    for (std::uint64_t i = 0; i < ClockHz && !bench.fdc.interruptRequest(); ++i) {
        if (bench.fdc.dataRequest()) bench.fdc.write(3, value++);
        bench.run(20);
    }
    const auto& sector = bench.drive.written[5 * 256 + 4];
    if (sector.size() != 512 || sector[0] != 0 || sector[511] != 0xFF) return fail("WRITE SECTOR must store 512 bytes.");
    if (bench.fdc.read(0) & (fd1793::StatusLostData | fd1793::StatusWriteProtect)) return fail("WRITE SECTOR must succeed.");

    bench.fdc.write(0, 0xA0);
    (void)bench.waitIdle(ClockHz);
    if (!(bench.fdc.read(0) & fd1793::StatusLostData)) return fail("An unserved write must end with LOST DATA.");

    bench.drive.protectedDisk = true;
    bench.fdc.write(0, 0xA0);
    (void)bench.waitIdle(ClockHz);
    if (!(bench.fdc.read(0) & fd1793::StatusWriteProtect)) return fail("A protected disk must refuse WRITE SECTOR.");
    return true;
}

bool readAddressPass()
{
    Bench bench;
    bench.fdc.setDoubleDensity(true);
    bench.fdc.write(0, 0xC0);
    const auto id = bench.readTransfer(ClockHz);
    if (id.size() != 6 || id[0] != 5 || id[3] != 2) return fail("READ ADDRESS must deliver the six ID bytes.");
    const std::uint8_t field[] = {0xA1, 0xA1, 0xA1, 0xFE, id[0], id[1], id[2], id[3]};
    const auto crc = fd1793::crc16(field, sizeof(field));
    if (id[4] != (crc >> 8) || id[5] != (crc & 0xFF) || bench.fdc.sectorRegister() != 5) {
        return fail("READ ADDRESS must send the CRC and copy the track into the sector register.");
    }
    return true;
}

bool trackPass()
{
    Bench bench;
    bench.fdc.setDoubleDensity(true);
    bench.fdc.write(0, 0xE0);
    const auto raw = bench.readTransfer(2 * ClockHz);
    if (raw.size() != 6250) return fail("READ TRACK must deliver one revolution (6250 MFM bytes).");

    // Format two 256-byte sectors on the test track with WRITE TRACK.
    std::vector<std::uint8_t> stream(80, 0x4E);
    for (std::uint8_t s = 1; s <= 2; ++s) {
        stream.insert(stream.end(), 12, 0x00);
        stream.insert(stream.end(), {0xF5, 0xF5, 0xF5, 0xFE, 5, 0, s, 1, 0xF7});
        stream.insert(stream.end(), 22, 0x4E);
        stream.insert(stream.end(), 12, 0x00);
        stream.insert(stream.end(), {0xF5, 0xF5, 0xF5, 0xFB});
        stream.insert(stream.end(), 256, static_cast<std::uint8_t>(0xE0 + s));
        stream.insert(stream.end(), {0xF7});
        stream.insert(stream.end(), 54, 0x4E);
    }
    bench.fdc.write(0, 0xF0);
    std::size_t next = 0;
    for (std::uint64_t i = 0; i < 2 * ClockHz && !bench.fdc.interruptRequest(); ++i) {
        if (bench.fdc.dataRequest()) bench.fdc.write(3, next < stream.size() ? stream[next++] : 0x4E);
        bench.run(20);
    }
    const auto& laid = bench.drive.formatted;
    if (laid.size() != 2 || laid[1].id.sector != 2 || laid[1].data.size() != 256 || laid[1].data[0] != 0xE2) {
        return fail("WRITE TRACK must lay down both sectors.");
    }
    if (bench.fdc.read(0) & fd1793::StatusLostData) return fail("A served WRITE TRACK must end without LOST DATA.");
    return true;
}

bool forceInterruptPass()
{
    Bench bench;
    bench.fdc.write(3, 30);
    bench.fdc.write(0, 0x13);
    bench.run(1000);
    bench.fdc.write(0, 0xD0);
    if ((bench.fdc.peekStatus() & fd1793::StatusBusy) || bench.fdc.interruptRequest()) {
        return fail("D0h must stop the command without interrupt.");
    }
    bench.fdc.write(0, 0xD8);
    (void)bench.fdc.read(0);
    if (!bench.fdc.interruptRequest()) return fail("D8h must interrupt at once and keep INTRQ on status reads.");
    bench.fdc.write(0, 0xD0);
    if (bench.fdc.interruptRequest()) return fail("D0h must clear the immediate interrupt.");
    bench.fdc.write(0, 0xD4);
    bench.run(ClockHz / 4);
    if (!bench.fdc.interruptRequest()) return fail("D4h must interrupt on the index pulse.");
    bench.fdc.write(0, 0xD0);
    bench.drive.inserted = false;
    bench.fdc.write(0, 0xD1);
    bench.drive.inserted = true;
    bench.run(10);
    if (!bench.fdc.interruptRequest()) return fail("D1h must interrupt when the drive becomes ready.");
    return true;
}

bool resetPass()
{
    fd1793::FD1793 fdc{ClockHz};
    TestDrive drive;
    fdc.setDrive(&drive);
    fdc.reset();
    for (std::uint64_t i = 0; i < ClockHz && (fdc.peekStatus() & fd1793::StatusBusy); ++i) {
        fdc.tick();
        drive.advance(1);
    }
    if (drive.cylinder != 0 || fdc.sectorRegister() != 1 || !fdc.interruptRequest()) {
        return fail("Master reset must run RESTORE and set the sector register to 1.");
    }
    return true;
}

} // namespace

int main()
{
    if (!registersPass()) return EXIT_FAILURE;
    if (!typeOnePass()) return EXIT_FAILURE;
    if (!indexPass()) return EXIT_FAILURE;
    if (!readSectorPass()) return EXIT_FAILURE;
    if (!writeSectorPass()) return EXIT_FAILURE;
    if (!readAddressPass()) return EXIT_FAILURE;
    if (!trackPass()) return EXIT_FAILURE;
    if (!forceInterruptPass()) return EXIT_FAILURE;
    if (!resetPass()) return EXIT_FAILURE;
    std::cout << "FD1793 passed.\n";
    return EXIT_SUCCESS;
}
