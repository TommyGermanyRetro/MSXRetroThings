#include "SVI601/SVI601.hpp"
#include "SVI328/Keyboard.hpp"
#include "SVI328/SVI328.hpp"
#include "SVI801/SVI801.hpp"
#include "SVI802/SVI802.hpp"
#include "SVI801/FloppyDrive.hpp"
#include "SVI803/SVI803.hpp"
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

std::string screenText(svi328::SVI328& machine)
{
    const auto& vdp = machine.video();
    const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
    std::string screen;
    for (std::uint16_t i = 0; i < 40 * 24; ++i) screen += static_cast<char>(vdp.peekVram(base + i) + 0x20);
    return screen;
}

std::vector<std::uint8_t> readFile(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

bool slotsPass()
{
    svi601::SVI601 expander;
    if (expander.slotCount() != 7) return fail("The SVI-601 must have seven slots.");
    if (expander.insertCard(7, std::make_unique<svi803::SVI803>()) == nullptr) return fail("There is no eighth slot.");
    (void)expander.insertCard(svi601::DiskControllerSlot, std::make_unique<svi801::SVI801>());
    (void)expander.insertCard(0, std::make_unique<svi802::SVI802>());
    if (expander.readIo(0x12) != 0xFE) return fail("The SVI-802 must report a ready printer at 12h.");
    expander.writeIo(0x31, 0x5A);
    if (expander.readIo(0x31) != 0x5A) return fail("The SVI-801 must answer at 31h (FD1793 track register).");
    if (expander.readIo(0x20) != 0xFF) return fail("Unused ports must read FFh.");
    return true;
}

// The BASIC ROM prints through the SVI-802 in an SVI-601.
bool printerPass(const std::vector<std::uint8_t>& rom)
{
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi601::SVI601 expander;
    auto card = std::make_unique<svi802::SVI802>();
    auto* printerCard = card.get();
    (void)expander.insertCard(2, std::move(card));
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1000 && screen.find("Ok") == std::string::npos; ++field) {
        runFields(machine, 1);
        if (field % 50 == 49) screen = screenText(machine);
    }
    if (screen.find("Ok") == std::string::npos) return fail("BASIC must start with the SVI-601.");
    if (!typeLine(machine, u"lprint \"abc\"")) return fail("Test text must be mappable.");
    const std::vector<std::uint8_t> expected{'a', 'b', 'c', 0x0D, 0x0A};
    if (printerCard->port().printer().output() != expected) return fail("LPRINT must reach the SVI-802.");
    return true;
}

// Disk BASIC loads from drive A of the SVI-801 in SK6 and saves, loads and lists a program.
bool diskBasicPass(const std::vector<std::uint8_t>& rom, const char* diskPath)
{
    svi801::SviDisk disk;
    if (!disk.load(readFile(diskPath))) return fail("The Disk BASIC image must be an SVI disk image.");
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi601::SVI601 expander;
    auto card = std::make_unique<svi801::SVI801>();
    auto* controller = card.get();
    controller->disks().drive(0).insert(disk);
    (void)expander.insertCard(svi601::DiskControllerSlot, std::move(card));
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1500 && screen.find("Ok") == std::string::npos; ++field) {
        runFields(machine, 1);
        if (field % 25 == 24) screen = screenText(machine);
    }
    if (screen.find("Disk version 1.0 by Microsoft corp.") == std::string::npos) {
        return fail("Disk BASIC must load from drive A of the SVI-801.");
    }
    if (!typeLine(machine, u"10 print 6*7") || !typeLine(machine, u"save\"1:test\"")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    if (!typeLine(machine, u"new") || !typeLine(machine, u"load\"1:test\"")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    if (!typeLine(machine, u"run") || !typeLine(machine, u"files")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    screen = screenText(machine);
    if (screen.find(" 42 ") == std::string::npos || screen.find("test  .") == std::string::npos ||
        !controller->disks().drive(0).disk().modified()) {
        return fail("SAVE, LOAD, RUN and FILES must work through the SVI-801.");
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (!slotsPass()) return EXIT_FAILURE;
    if (argc < 2) {
        std::cerr << "Usage: svi601_tests <svi111.rom> [Disk BASIC .dsk|-]\n";
        return EXIT_FAILURE;
    }
    const auto rom = readFile(argv[1]);
    if (!printerPass(rom)) return EXIT_FAILURE;
    if (argc >= 3 && std::string(argv[2]) != "-" && !diskBasicPass(rom, argv[2])) return EXIT_FAILURE;
    std::cout << "SVI-601 passed.\n";
    return EXIT_SUCCESS;
}
