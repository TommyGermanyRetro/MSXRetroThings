#include "SVI605BHD/SVI605BHD.hpp"
#include "SVI328/Keyboard.hpp"
#include "SVI328/SVI328.hpp"
#include "SVI605B/SVI605B.hpp"
#include "SVI801/FloppyDrive.hpp"
#include "SVI807/SVI807.hpp"

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

void runFields(svi328::SVI328& machine, unsigned fields)
{
    for (unsigned field = 0; field < fields; ++field) {
        for (std::uint32_t t = 0; t < svi328::CpuTicksPerPalField; ++t) (void)machine.tick();
    }
}

std::string bootScreen(svi328::SVI328& machine)
{
    const auto& vdp = machine.video();
    const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
    std::string screen;
    for (std::uint16_t i = 0; i < 40 * 24; ++i) screen += static_cast<char>(vdp.peekVram(base + i) + 0x20);
    return screen;
}

bool typeLine(svi328::SVI328& machine, const std::u16string& text)
{
    const auto press = [&](svi328::KeyboardPosition position, bool shift) {
        if (shift) machine.setKeyboardKey(svi328::SviShift.row, svi328::SviShift.bit, true);
        machine.setKeyboardKey(position.row, position.bit, true);
        runFields(machine, 3);
        machine.setKeyboardKey(position.row, position.bit, false);
        if (shift) machine.setKeyboardKey(svi328::SviShift.row, svi328::SviShift.bit, false);
        runFields(machine, 3);
    };
    for (const char16_t character : text) {
        svi328::KeyboardPosition position{};
        svi328::KeyModifier modifier{};
        if (!svi328::mapCharacterToSvi(character, position, modifier)) return false;
        press(position, modifier == svi328::KeyModifier::Shift);
    }
    press({6, 6}, false);
    runFields(machine, 10);
    return true;
}

// The board: four free slots, drive A only, the SASI host adapter built in at 40h-46h.
bool boardPass()
{
    svi605bhd::SVI605BHD expander;
    if (expander.slotCount() != 4 || expander.driveCount() != 1) return fail("The SVI-605B with hard disk has four slots and one drive.");
    for (std::size_t slot = 0; slot < expander.slotCount(); ++slot) {
        if (expander.card(slot) != nullptr) return fail("The SASI host adapter must not take a slot.");
    }
    if ((expander.readIo(0x46) & 0x1F) != 0) return fail("Port 46h must report bus free.");
    expander.sasi().controller().attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
    expander.writeIo(0x41, 0x01);
    expander.writeIo(0x43, 0x00);
    if ((expander.readIo(0x46) & 0x1F) != wd1002shd::PhaseCommand) return fail("Selecting ID 0 must start the Command phase.");
    expander.reset();
    if ((expander.readIo(0x46) & 0x1F) != 0) return fail("/RST must return the SASI bus to bus free.");
    return true;
}

// CP/M 2.27 starts from the floppy through the BASIC ROM and uses the hard disk as B: to E:.
bool cpm227Pass(const char* romPath, const char* diskPath)
{
    std::ifstream romFile(romPath, std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(romFile), {}};
    std::ifstream diskFile(diskPath, std::ios::binary);
    svi801::SviDisk disk;
    if (!disk.load({std::istreambuf_iterator<char>(diskFile), {}})) return fail("The CP/M 2.27 image must be an SVI disk image.");
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi605bhd::SVI605BHD expander;
    expander.sasi().controller().attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
    expander.drive(0).insert(disk);
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 3000 && screen.find("A>") == std::string::npos; ++field) {
        runFields(machine, 1);
        if (field % 25 == 24) screen = bootScreen(machine);
    }
    if (screen.find("Booted From Floppy Disk") == std::string::npos || screen.find("A>") == std::string::npos) {
        return fail("The BASIC ROM must start CP/M 2.27 from the floppy to the A> prompt.");
    }
    const auto& controller = expander.sasi().controller();
    if (controller.cylinders() != 306 || controller.heads() != 4) return fail("The BIOS must set 306 cylinders, 4 heads.");
    if (!typeLine(machine, u"stat e:")) return fail("Test text must be mappable.");
    runFields(machine, 400);
    if (bootScreen(machine).find("Bytes Remaining On E: 3104k") == std::string::npos) return fail("Partition E: must be empty.");
    if (!typeLine(machine, u"pip b:=stat.com")) return fail("Test text must be mappable.");
    runFields(machine, 1500);
    if (!typeLine(machine, u"dir b:")) return fail("Test text must be mappable.");
    runFields(machine, 300);
    if (bootScreen(machine).find("B: STAT     COM") == std::string::npos || !controller.modified()) {
        return fail("PIP must copy STAT.COM to the hard disk.");
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (!boardPass()) return EXIT_FAILURE;
    if (argc < 2) {
        std::cerr << "Usage: svi605bhd_tests <svi111.rom> [CP/M 2.27 .dsk|-]\n";
        return EXIT_FAILURE;
    }
    if (argc >= 3 && std::string(argv[2]) != "-" && !cpm227Pass(argv[1], argv[2])) return EXIT_FAILURE;
    std::cout << "SVI-605B with hard disk passed.\n";
    return EXIT_SUCCESS;
}
