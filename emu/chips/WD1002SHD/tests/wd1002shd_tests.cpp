#include "WD1002SHD/WD1002SHD.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// One SASI transaction as the SVI-608M BIOS runs it: selection, command block, data, status and
// completion byte. Returns false if a phase does not come as expected.
struct Result {
    bool ok = false;
    std::uint8_t status = 0xFF;
    std::vector<std::uint8_t> dataIn;
};

Result transaction(wd1002shd::WD1002SHD& controller, std::vector<std::uint8_t> command,
                   const std::vector<std::uint8_t>& dataOut = {})
{
    Result result;
    if (controller.signals() != wd1002shd::PhaseBusFree || !controller.select(0x01)) return result;
    for (const auto byte : command) {
        if (controller.signals() != wd1002shd::PhaseCommand) return result;
        controller.writeData(byte);
    }
    std::size_t sent = 0;
    while (controller.signals() == wd1002shd::PhaseDataOut && sent < dataOut.size()) controller.writeData(dataOut[sent++]);
    while (controller.signals() == wd1002shd::PhaseDataIn) result.dataIn.push_back(controller.readData());
    if (controller.signals() != wd1002shd::PhaseStatus) return result;
    result.status = controller.readData();
    if (controller.signals() != wd1002shd::PhaseMessage || controller.readData() != 0x00) return result;
    result.ok = controller.signals() == wd1002shd::PhaseBusFree;
    return result;
}

std::vector<std::uint8_t> sector(std::uint8_t seed)
{
    std::vector<std::uint8_t> data(wd1002shd::SectorSize);
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(seed + i);
    return data;
}

bool phasesPass()
{
    wd1002shd::WD1002SHD controller;
    if (controller.select(0x02)) return fail("The controller must answer only to ID bit 0.");
    if (!controller.select(0x01) || controller.signals() != 0x0B) return fail("Selection must lead to the Command phase (0Bh).");
    controller.reset();
    if (controller.signals() != 0x00) return fail("A bus reset must free the bus.");
    const auto noDisk = transaction(controller, {0x00, 0, 0, 0, 0, 0});
    if (!noDisk.ok || noDisk.status != 0x02) return fail("Test Drive Ready without a disk must end with the error flag.");
    const auto sense = transaction(controller, {0x03, 0, 0, 0, 0, 0});
    if (!sense.ok || sense.dataIn.size() != 4 || sense.dataIn[0] != wd1002shd::ErrorNotReady || sense.status != 0x00) {
        return fail("Request Status must report error 04h in four bytes.");
    }
    controller.attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
    if (transaction(controller, {0x00, 0, 0, 0, 0, 0}).status != 0x00) return fail("Test Drive Ready with a disk must succeed.");
    const auto invalid = transaction(controller, {0x09, 0, 0, 0, 0, 0});
    if (invalid.status != 0x02 || transaction(controller, {0x03, 0, 0, 0, 0, 0}).dataIn[0] != wd1002shd::ErrorInvalidCommand) {
        return fail("An unknown opcode must give error 20h.");
    }
    if (transaction(controller, {0x00, 0x20, 0, 0, 0, 0}).status != 0x22) return fail("LUN 1 without a drive must report not ready.");
    return true;
}

bool readWritePass()
{
    wd1002shd::WD1002SHD controller;
    controller.attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
    // Set Parameters as the CP/M 2.27 BIOS sends it (LUN field 7): 306 cylinders, 4 heads.
    const auto parameters = transaction(controller, {0x0C, 0xFF, 0x00, 0x00, 0xFF, 0xFF}, {0x01, 0x32, 0x04, 0x00, 0x80, 0x00, 0x40, 0x0B});
    if (!parameters.ok || parameters.status & 0x02 || controller.cylinders() != 306 || controller.heads() != 4) {
        return fail("Set Parameters must take 306 cylinders and 4 heads, whatever the LUN field.");
    }
    std::vector<std::uint8_t> two = sector(1);
    const auto second = sector(77);
    two.insert(two.end(), second.begin(), second.end());
    const std::uint32_t lba = 37119;
    const auto write = transaction(controller, {0x0A, static_cast<std::uint8_t>(lba >> 16), static_cast<std::uint8_t>(lba >> 8),
                                                static_cast<std::uint8_t>(lba), 0x01, 0x07}, sector(9));
    if (!write.ok || write.status != 0x00 || !controller.modified()) return fail("Write must store a sector.");
    (void)transaction(controller, {0x0A, 0x00, 0x00, 0x10, 0x02, 0x07}, two);
    const auto read = transaction(controller, {0x08, 0x00, 0x00, 0x10, 0x02, 0x47});
    if (!read.ok || read.dataIn != two) return fail("Read must return two written sectors.");
    const auto last = transaction(controller, {0x08, static_cast<std::uint8_t>(lba >> 16), static_cast<std::uint8_t>(lba >> 8),
                                               static_cast<std::uint8_t>(lba), 0x01, 0x47});
    if (last.dataIn != sector(9)) return fail("The last partition track must be readable.");
    const std::uint32_t beyond = 306 * 4 * 32;
    const auto outside = transaction(controller, {0x08, static_cast<std::uint8_t>(beyond >> 16), static_cast<std::uint8_t>(beyond >> 8),
                                                  static_cast<std::uint8_t>(beyond), 0x01, 0x47});
    // Byte 0 of Request Status: bit 7 = logical address valid, bits 0-5 = error code.
    if (outside.status != 0x02 || transaction(controller, {0x03, 0, 0, 0, 0, 0}).dataIn[0] != (0x80 | wd1002shd::ErrorInvalidAddress)) {
        return fail("A sector beyond the capacity must give error 21h.");
    }
    (void)transaction(controller, {0x0F, 0, 0, 0, 0, 0}, sector(5));
    if (transaction(controller, {0x10, 0, 0, 0, 0, 0}).dataIn != sector(5)) return fail("The sector buffer must read back.");
    (void)transaction(controller, {0x06, 0x00, 0x01, 0x00, 0x00, 0x00});
    const auto formatted = transaction(controller, {0x08, 0x00, 0x01, 0x1F, 0x01, 0x47});
    if (formatted.dataIn != std::vector<std::uint8_t>(wd1002shd::SectorSize, 0x6C)) return fail("Format Track must fill with 6Ch.");
    return true;
}

} // namespace

int main()
{
    if (!phasesPass()) return EXIT_FAILURE;
    if (!readWritePass()) return EXIT_FAILURE;
    std::cout << "WD1002-SHD passed.\n";
    return EXIT_SUCCESS;
}
