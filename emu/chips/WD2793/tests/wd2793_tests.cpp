#include "WD2793/WD2793.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr std::uint32_t ClockHz = 1000000;
constexpr std::uint32_t Millisecond = ClockHz / 1000;
constexpr std::size_t DoubleSidedSize = 80 * 2 * 9 * wd2793::SectorSize;
constexpr std::size_t SingleSidedSize = 80 * 1 * 9 * wd2793::SectorSize;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// Every byte holds its sector number, cylinder and side so that misplaced reads show up.
std::vector<std::uint8_t> patternImage()
{
    std::vector<std::uint8_t> image(DoubleSidedSize);
    for (std::size_t offset = 0; offset < image.size(); ++offset) {
        const std::size_t sector = offset / wd2793::SectorSize;
        image[offset] = static_cast<std::uint8_t>(sector * 7 + offset % wd2793::SectorSize);
    }
    return image;
}

std::size_t sectorOffset(unsigned cylinder, unsigned side, unsigned sector)
{
    return ((cylinder * 2 + side) * 9 + (sector - 1)) * wd2793::SectorSize;
}

struct Bench {
    wd2793::FloppyDrive drive;
    wd2793::WD2793 fdc{ClockHz};

    Bench()
    {
        require(drive.insert(patternImage(), false), "a 720 KiB image is accepted");
        drive.setMotor(true);
        fdc.selectDrive(&drive);
        fdc.writeCommand(0xD0);
        fdc.writeCommand(0x00);
        waitIdle();
    }

    void run(std::uint32_t clocks)
    {
        for (std::uint32_t clock = 0; clock < clocks; ++clock) fdc.advance(1);
    }

    void waitIdle(std::uint32_t limit = 3000 * Millisecond)
    {
        for (std::uint32_t clock = 0; clock < limit && fdc.busy(); clock += 10) fdc.advance(10);
        require(!fdc.busy(), "the command finishes");
    }

    // Reads data bytes as the CPU would, polling DRQ every 10 us.
    std::vector<std::uint8_t> readAll(std::uint32_t limit = 3000 * Millisecond)
    {
        std::vector<std::uint8_t> bytes;
        for (std::uint32_t clock = 0; clock < limit && fdc.busy(); clock += 10) {
            fdc.advance(10);
            if (fdc.dataRequest()) bytes.push_back(fdc.readData());
        }
        return bytes;
    }

    void seekTo(unsigned cylinder)
    {
        fdc.writeData(static_cast<std::uint8_t>(cylinder));
        fdc.writeCommand(0x10);
        waitIdle();
    }
};

void testFloppyDrive()
{
    wd2793::FloppyDrive single(false);
    require(!single.insert(std::vector<std::uint8_t>(DoubleSidedSize), false), "a single-sided drive rejects 720 KiB");
    require(single.insert(std::vector<std::uint8_t>(SingleSidedSize), false) && single.sectorsPerTrack() == 9,
            "a single-sided drive takes 360 KiB");
    single.setSide(1);
    require(!single.ready(), "side 1 of a single-sided drive is not ready");
    require(single.takeDiskChanged() && !single.takeDiskChanged(), "the disk change flag is cleared when read");

    wd2793::FloppyDrive drive;
    require(!drive.insert(std::vector<std::uint8_t>(1000), false), "odd image sizes are rejected");
    require(drive.insert(std::vector<std::uint8_t>(80 * 2 * 8 * 512), true) && drive.sectorsPerTrack() == 8,
            "640 KiB images have 8 sectors per track");
    require(!drive.track00() && !drive.writeProtected(), "TRACK 00 and WRITE PROTECT need the motor");
    drive.setMotor(true);
    require(drive.track00() && drive.writeProtected(), "TRACK 00 and WRITE PROTECT with the motor on");
    for (int step = 0; step < 90; ++step) drive.step(true);
    require(drive.headCylinder() == wd2793::MaximumHeadCylinder && !drive.trackFormatted(), "the head stops past the last cylinder");
    drive.eject();
    require(!drive.ready() && drive.diskChanged(), "ejecting clears READY and flags a disk change");
}

void testTypeICommands()
{
    Bench bench;
    for (int step = 0; step < 3; ++step) bench.drive.step(true);
    bench.fdc.writeCommand(0x08);
    require(bench.fdc.busy(), "Restore sets BUSY");
    bench.run(3 * 6 * Millisecond + 10);
    require(!bench.fdc.busy() && bench.fdc.readTrack() == 0 && bench.drive.headCylinder() == 0,
            "Restore steps out until TRACK 00 at 6 ms per step");
    require(bench.fdc.interruptRequest(), "Restore ends with INTRQ");
    const std::uint8_t status = bench.fdc.readStatus();
    require((status & 0x24) == 0x24 && !bench.fdc.interruptRequest(), "Type I status shows HEAD LOADED and TRACK 00; reading it clears INTRQ");

    bench.fdc.writeData(10);
    bench.fdc.writeCommand(0x11);
    bench.run(10 * 12 * Millisecond - 20);
    require(bench.fdc.busy(), "Seek uses the 12 ms step rate");
    bench.waitIdle();
    require(bench.fdc.readTrack() == 10 && bench.drive.headCylinder() == 10, "Seek moves to the data register track");

    bench.fdc.writeCommand(0x50);
    bench.waitIdle();
    require(bench.fdc.readTrack() == 11 && bench.drive.headCylinder() == 11, "Step-in with T updates the track register");
    bench.fdc.writeCommand(0x20);
    bench.waitIdle();
    require(bench.fdc.readTrack() == 11 && bench.drive.headCylinder() == 12, "Step without T keeps the track register");

    bench.fdc.writeCommand(0x00);
    bench.waitIdle();
    bench.fdc.writeData(14);
    bench.fdc.writeCommand(0x14);
    bench.waitIdle();
    require((bench.fdc.readStatus() & 0x10) == 0, "Seek with verify finds the destination track");
    bench.fdc.writeTrack(30);
    bench.fdc.writeData(32);
    bench.fdc.writeCommand(0x14);
    bench.waitIdle();
    require((bench.fdc.readStatus() & 0x10) != 0, "verify reports a seek error on a track mismatch");

    bench.drive.setMotor(false);
    bench.fdc.writeCommand(0x00);
    bench.run(255 * 6 * Millisecond + 10);
    require(!bench.fdc.busy() && (bench.fdc.readStatus() & 0x10) != 0, "Restore without TRACK 00 gives up after 255 steps");
}

void testReadSector()
{
    Bench bench;
    bench.seekTo(2);
    bench.drive.setSide(1);
    bench.fdc.writeSector(5);
    bench.fdc.writeCommand(0x80);
    const std::vector<std::uint8_t> bytes = bench.readAll();
    require(bytes.size() == wd2793::SectorSize && bench.fdc.readStatus() == 0x00, "Read Sector delivers 512 bytes without error");
    const std::vector<std::uint8_t> image = patternImage();
    require(std::equal(bytes.begin(), bytes.end(), image.begin() + static_cast<std::ptrdiff_t>(sectorOffset(2, 1, 5))),
            "Read Sector returns the sector of cylinder, side and number");

    bench.fdc.writeCommand(0x80);
    bench.waitIdle();
    require((bench.fdc.readStatus() & 0x04) != 0, "unread bytes set LOST DATA");

    bench.fdc.writeSector(10);
    bench.fdc.writeCommand(0x80);
    bench.run(900 * Millisecond);
    require(bench.fdc.busy(), "a missing sector is searched for five revolutions");
    bench.waitIdle();
    require((bench.fdc.readStatus() & 0x10) != 0, "a missing sector sets RECORD NOT FOUND");

    bench.fdc.writeSector(5);
    bench.fdc.writeCommand(0x82);
    bench.waitIdle();
    require((bench.fdc.readStatus() & 0x10) != 0, "the side compare flag rejects the other side");

    bench.fdc.writeSector(8);
    bench.fdc.writeCommand(0x90);
    const std::vector<std::uint8_t> multiple = bench.readAll();
    require(multiple.size() == 2 * wd2793::SectorSize && (bench.fdc.readStatus() & 0x10) != 0 && bench.fdc.readSector() == 10,
            "multiple-record reads continue until the sector number exceeds the track");

    bench.fdc.writeTrack(3);
    bench.fdc.writeSector(1);
    bench.fdc.writeCommand(0x80);
    bench.waitIdle();
    require((bench.fdc.readStatus() & 0x10) != 0, "a track register that differs from the head position finds no sector");
}

void testWriteSector()
{
    Bench bench;
    bench.seekTo(1);
    bench.fdc.writeSector(3);
    bench.fdc.writeCommand(0xA0);
    std::size_t written = 0;
    for (std::uint32_t clock = 0; clock < 1000 * Millisecond && bench.fdc.busy(); clock += 10) {
        bench.fdc.advance(10);
        if (bench.fdc.dataRequest()) bench.fdc.writeData(static_cast<std::uint8_t>(0xC0 + written++));
    }
    require(written == wd2793::SectorSize && bench.fdc.readStatus() == 0x00, "Write Sector takes 512 bytes");
    const std::size_t offset = sectorOffset(1, 0, 3);
    require(bench.drive.image()[offset] == 0xC0 && bench.drive.image()[offset + 511] == static_cast<std::uint8_t>(0xC0 + 511)
                && bench.drive.modified(),
            "the written sector lands in the image");

    bench.fdc.writeCommand(0xA0);
    bench.waitIdle();
    require((bench.fdc.readStatus() & 0x04) != 0, "a missing first byte ends Write Sector with LOST DATA");

    wd2793::FloppyDrive locked;
    require(locked.insert(patternImage(), true), "a write-protected image is accepted");
    locked.setMotor(true);
    bench.fdc.selectDrive(&locked);
    bench.fdc.writeCommand(0xA0);
    require(!bench.fdc.busy() && (bench.fdc.readStatus() & 0x40) != 0, "a protected disk ends Write Sector at once");

    bench.fdc.selectDrive(nullptr);
    bench.fdc.writeCommand(0x80);
    require(!bench.fdc.busy() && bench.fdc.interruptRequest() && (bench.fdc.readStatus() & 0x80) != 0,
            "without a ready drive Type II commands end with NOT READY");
}

void testTypeIIICommands()
{
    Bench bench;
    bench.seekTo(7);
    bench.drive.setSide(1);
    bench.fdc.writeCommand(0xC0);
    const std::vector<std::uint8_t> id = bench.readAll();
    require(id.size() == 6 && id[0] == 7 && id[1] == 1 && id[3] == 2 && id[2] >= 1 && id[2] <= 9,
            "Read Address returns cylinder, side, sector and size code");
    require(bench.fdc.readSector() == 7, "Read Address copies the cylinder into the sector register");

    bench.fdc.writeCommand(0xE0);
    const std::vector<std::uint8_t> track = bench.readAll();
    require(track.size() == 6250, "Read Track delivers one revolution");
    const std::vector<std::uint8_t> image = patternImage();
    bool foundData = false;
    for (std::size_t index = 0; index + 4 + 512 <= track.size(); ++index) {
        if (track[index] == 0xA1 && track[index + 1] == 0xA1 && track[index + 2] == 0xA1 && track[index + 3] == 0xFB) {
            foundData = std::equal(track.begin() + static_cast<std::ptrdiff_t>(index + 4),
                                   track.begin() + static_cast<std::ptrdiff_t>(index + 4 + 512),
                                   image.begin() + static_cast<std::ptrdiff_t>(sectorOffset(7, 1, 1)));
            break;
        }
    }
    require(foundData, "Read Track contains the data fields");

    // Format cylinder 7 side 1: nine sectors filled with E5.
    std::vector<std::uint8_t> format;
    const auto put = [&](std::size_t count, std::uint8_t value) { format.insert(format.end(), count, value); };
    put(80, 0x4E);
    put(12, 0x00);
    put(3, 0xF6);
    put(1, 0xFC);
    put(50, 0x4E);
    for (std::uint8_t sector = 1; sector <= 9; ++sector) {
        put(12, 0x00);
        put(3, 0xF5);
        format.insert(format.end(), {0xFE, 7, 1, sector, 2, 0xF7});
        put(22, 0x4E);
        put(12, 0x00);
        put(3, 0xF5);
        put(1, 0xFB);
        put(512, 0xE5);
        put(1, 0xF7);
        put(84, 0x4E);
    }
    std::size_t next = 0;
    bench.fdc.writeCommand(0xF0);
    for (std::uint32_t clock = 0; clock < 1000 * Millisecond && bench.fdc.busy(); clock += 10) {
        bench.fdc.advance(10);
        if (bench.fdc.dataRequest()) bench.fdc.writeData(next < format.size() ? format[next++] : 0x4E);
    }
    require(!bench.fdc.busy() && (bench.fdc.readStatus() & 0x04) == 0, "Write Track writes one revolution");
    const std::size_t first = sectorOffset(7, 1, 1);
    const std::size_t last = sectorOffset(7, 1, 9) + 511;
    require(bench.drive.image()[first] == 0xE5 && bench.drive.image()[last] == 0xE5
                && bench.drive.image()[sectorOffset(7, 0, 1)] == image[sectorOffset(7, 0, 1)],
            "Write Track stores the data fields of the formatted track only");
}

void testForceInterrupt()
{
    Bench bench;
    bench.fdc.writeSector(1);
    bench.fdc.writeCommand(0x80);
    bench.run(5 * Millisecond);
    bench.fdc.writeCommand(0xD0);
    require(!bench.fdc.busy() && !bench.fdc.interruptRequest(), "D0 stops a command without INTRQ");

    bench.fdc.writeCommand(0xD8);
    require(bench.fdc.interruptRequest(), "D8 raises INTRQ at once");
    (void)bench.fdc.readStatus();
    require(bench.fdc.interruptRequest(), "reading the status keeps the immediate interrupt");
    bench.fdc.writeCommand(0xD0);
    require(!bench.fdc.interruptRequest(), "only D0 clears the immediate interrupt");

    bench.fdc.writeCommand(0xD4);
    bench.run(201 * Millisecond);
    require(bench.fdc.interruptRequest(), "D4 raises INTRQ at the index pulse");
    bench.fdc.writeCommand(0xD0);

    bool sawIndex = false;
    for (std::uint32_t clock = 0; clock < 200 * Millisecond; clock += 100) {
        bench.fdc.advance(100);
        sawIndex = sawIndex || (bench.fdc.readStatus() & 0x02) != 0;
    }
    require(sawIndex, "the Type I status shows index pulses of the spinning disk");
}

} // namespace

int main()
{
    testFloppyDrive();
    testTypeICommands();
    testReadSector();
    testWriteSector();
    testTypeIIICommands();
    testForceInterrupt();
    std::cout << "WD2793 library passed.\n";
}
