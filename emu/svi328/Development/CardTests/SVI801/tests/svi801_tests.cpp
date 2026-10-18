#include "SVI801/FloppyDrive.hpp"
#include "SVI328/SVI328.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

bool diskImagePass()
{
    svi801::SviDisk disk;
    if (disk.load(std::vector<std::uint8_t>(100000)) || !disk.load(std::vector<std::uint8_t>(svi801::SviDiskSingleSidedSize))) {
        return fail("Only 172032 and 346112 byte images must load.");
    }
    if (disk.doubleSided() || disk.sectorOffset(0, 0, 18) != 17 * 128 || disk.sectorOffset(1, 0, 1) != 2304 ||
        disk.sectorOffset(0, 1, 1) != disk.image().size() || disk.sectorOffset(0, 0, 19) != disk.image().size()) {
        return fail("A single-sided image must hold 18 x 128 bytes on track 0, then 17 x 256 per track.");
    }
    auto twoSides = svi801::SviDisk::blank(true);
    if (!twoSides.doubleSided() || twoSides.image().size() != svi801::SviDiskDoubleSidedSize ||
        twoSides.image()[1000] != 0xE5 || twoSides.sectorOffset(0, 1, 1) != 2304 || twoSides.sectorOffset(1, 0, 1) != 2304 + 17 * 256 ||
        twoSides.sectorOffset(39, 1, 17) != svi801::SviDiskDoubleSidedSize - 256) {
        return fail("A double-sided image must store side 0 before side 1 for each cylinder.");
    }
    twoSides.markSaved();
    if (!twoSides.writeSector(2, 1, 5, std::vector<std::uint8_t>(256, 0x11)) || !twoSides.modified() ||
        twoSides.writeSector(2, 1, 5, std::vector<std::uint8_t>(128, 0x11))) {
        return fail("Sector writes must check the size and mark the image modified.");
    }

    svi801::FloppyDrive drive(svi328::CpuClockHz);
    drive.insert(twoSides);
    if (drive.ready()) return fail("A drive must not be ready without the motor.");
    drive.setMotor(true);
    if (!drive.ready() || drive.sectors(0, false).size() != 18 || !drive.sectors(0, true).empty() ||
        drive.sectors(1, true).size() != 17) {
        return fail("Track 0 must offer 18 FM sectors on side 0 and 17 MFM sectors on side 1.");
    }
    bool high = false;
    bool low = false;
    for (std::uint32_t t = 0; t < svi328::CpuClockHz / 4; ++t) {
        drive.tick();
        if (drive.index()) high = true;
        else low = true;
    }
    if (!high || !low) return fail("The index hole must pass while the motor runs.");
    return true;
}

} // namespace

int main()
{
    if (!diskImagePass()) return EXIT_FAILURE;
    std::cout << "SVI-801 passed.\n";
    return EXIT_SUCCESS;
}
