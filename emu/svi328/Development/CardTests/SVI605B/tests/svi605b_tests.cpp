#include "SVI328/Keyboard.hpp"
#include "SVI328/SVI328.hpp"
#include "SVI801/FloppyDrive.hpp"
#include "SVI803/SVI803.hpp"
#include "SVI807/SVI807.hpp"
#include "SVI805/SVI805.hpp"
#include "SVI806/SVI806.hpp"
#include "SVI605B/SVI605B.hpp"

#include <algorithm>
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

void selectBanks(svi328::SVI328& machine, std::uint8_t portB)
{
    machine.writeIo(0x88, 7);
    machine.writeIo(0x8C, 0x80);
    machine.writeIo(0x88, 15);
    machine.writeIo(0x8C, portB);
}

// Test card on a fixed I/O port with a fixed memory byte, to check how the expander combines slots.
class ProbeCard final : public svi3x8::ExpansionPort {
public:
    explicit ProbeCard(std::uint8_t pattern) : pattern(pattern) {}
    std::uint8_t pattern;
    std::uint8_t written = 0;
    unsigned resets = 0;
    unsigned long ticks = 0;
    bool interrupt = false;

    void reset() override { ++resets; }
    svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t) const override
    {
        return {pattern, address < 0x8000 && pattern == 0x0F, false};
    }
    svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t, std::uint8_t, std::uint8_t) override { return {}; }
    std::uint8_t readIo(std::uint8_t) override { return pattern; }
    void writeIo(std::uint8_t, std::uint8_t value) override { written = value; }
    void tick() override { ++ticks; }
    bool interruptRequest() const override { return interrupt; }
};

bool slotsPass()
{
    svi605b::SVI605B expander;
    if (expander.readIo(0x50) != 0xFF || expander.readMemory(0x0000, 0xFF).data != 0xFF) {
        return fail("An empty expander must read FFh.");
    }
    auto first = std::make_unique<ProbeCard>(0x0F);
    auto second = std::make_unique<ProbeCard>(0x3C);
    auto* firstCard = first.get();
    auto* secondCard = second.get();
    if (expander.insertCard(0, std::move(first)) || expander.insertCard(3, std::move(second))) {
        return fail("Inserting into an empty slot must return nothing.");
    }
    if (expander.insertCard(4, std::make_unique<ProbeCard>(0)) == nullptr) return fail("Slot 4 must not exist.");
    if (expander.card(0) != firstCard || expander.card(1) != nullptr) return fail("card() must report the slots.");
    if (expander.readIo(0x50) != 0x0C) return fail("I/O data of all cards must combine with AND.");
    const auto cycle = expander.readMemory(0x0000, 0xFF);
    if (cycle.data != 0x0C || !cycle.romDisable || cycle.ramDisable) return fail("Memory data must AND, /ROMDIS must OR.");
    expander.writeIo(0x10, 0x77);
    expander.tick();
    expander.reset();
    if (firstCard->written != 0x77 || secondCard->written != 0x77 || firstCard->ticks != 1 || secondCard->resets != 1) {
        return fail("Writes, CPUCLK and /RST must reach every card.");
    }
    secondCard->interrupt = true;
    if (!expander.interruptRequest()) return fail("/INT of any card must reach the bus.");
    if (expander.removeCard(3).get() != secondCard || expander.interruptRequest() || expander.readIo(0x50) != 0x0F) {
        return fail("A removed card must leave the bus.");
    }
    return true;
}




bool centronicsPass()
{
    svi605b::SVI605B expander;
    if (expander.readIo(0x12) != 0xFE) return fail("Port 12h must report a ready printer (BUSY = 0).");
    expander.writeIo(0x10, 'A');
    expander.writeIo(0x11, 0x00);
    if (!expander.printer().output().empty()) return fail("A low /STROBE must not print yet.");
    expander.writeIo(0x11, 0xFF);
    expander.writeIo(0x11, 0xFF);
    if (expander.printer().output() != std::vector<std::uint8_t>{'A'}) {
        return fail("The rising /STROBE edge must print the data byte once.");
    }
    return true;
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





std::string bootScreen(svi328::SVI328& machine)
{
    const auto& vdp = machine.video();
    const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
    std::string screen;
    for (std::uint16_t i = 0; i < 40 * 24; ++i) screen += static_cast<char>(vdp.peekVram(base + i) + 0x20);
    return screen;
}

// The BASIC ROM boots from drive A: it loads track 0, sector 1 (FM) to C100h and jumps there.
// The test boot sector seeks to track 1, reads sector 1 (MFM) to C200h and marks C1F1h.
bool diskBootPass(const char* romPath)
{
    static const std::vector<std::uint8_t> bootCode{
        0xAF, 0xD3, 0x38, 0x3E, 0x01, 0xD3, 0x33, 0x3E, 0x14, 0xD3, 0x30, 0xDB, 0x34, 0x07, 0x30, 0xFB,
        0xDB, 0x30, 0x32, 0xF2, 0xC1, 0x3E, 0x01, 0xD3, 0x32, 0x21, 0x00, 0xC2, 0x0E, 0x33, 0x3E, 0x80,
        0xD3, 0x30, 0xDB, 0x34, 0x87, 0x38, 0x07, 0xF2, 0x22, 0xC1, 0xED, 0xA2, 0x18, 0xF4, 0xDB, 0x30,
        0x32, 0xF0, 0xC1, 0x3E, 0x5A, 0x32, 0xF1, 0xC1, 0x18, 0xFE};
    std::ifstream file(romPath, std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(file), {}};
    auto disk = svi801::SviDisk::blank(true);
    std::vector<std::uint8_t> sector(128, 0);
    std::copy(bootCode.begin(), bootCode.end(), sector.begin());
    std::vector<std::uint8_t> payload(256);
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::uint8_t>(i ^ 0x3C);
    if (!disk.writeSector(0, 0, 1, sector) || !disk.writeSector(1, 0, 1, payload)) return fail("Test disk must be writable.");

    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi605b::SVI605B expander;
    expander.drive(0).insert(disk);
    machine.connectExpansion(&expander);
    machine.reset();
    for (unsigned field = 0; field < 600 && machine.readMemory(0xC1F1) != 0x5A; ++field) {
        for (std::uint32_t t = 0; t < svi328::CpuTicksPerPalField; ++t) (void)machine.tick();
    }
    if (machine.readMemory(0xC1F1) != 0x5A) return fail("The BASIC ROM must boot the disk in drive A.");
    if ((machine.readMemory(0xC1F2) & 0x18) != 0 || (machine.readMemory(0xC1F0) & 0x9C) != 0) {
        return fail("The boot code must seek and read without errors.");
    }
    for (std::uint16_t i = 0; i < 256; ++i) {
        if (machine.readMemory(static_cast<std::uint16_t>(0xC200 + i)) != payload[i]) return fail("The MFM sector must arrive intact.");
    }
    if (expander.drive(0).cylinder() != 1 || !expander.drive(0).motorOn()) return fail("Drive A must sit on cylinder 1.");

    // Without a disk the ROM gives up after the index timeout and starts BASIC.
    svi328::SVI328 plain;
    (void)plain.loadRom(rom);
    svi605b::SVI605B empty;
    plain.connectExpansion(&empty);
    plain.reset();
    std::string screen;
    for (unsigned field = 0; field < 1000 && screen.find("Ok") == std::string::npos; ++field) {
        for (std::uint32_t t = 0; t < svi328::CpuTicksPerPalField; ++t) (void)plain.tick();
        if (field % 25 == 24) screen = bootScreen(plain);
    }
    if (screen.find("Ok") == std::string::npos) return fail("Without a disk BASIC must start.");
    return true;
}

// Disk BASIC from the original disk: boot, SAVE"1:test", NEW, LOAD"1:test", RUN and FILES.
bool diskBasicPass(const char* romPath, const char* diskPath)
{
    std::ifstream romFile(romPath, std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(romFile), {}};
    std::ifstream diskFile(diskPath, std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(diskFile), {}};
    svi801::SviDisk disk;
    if (!disk.load(image)) return fail("The Disk BASIC image must be an SVI disk image.");
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi605b::SVI605B expander;
    expander.drive(0).insert(disk);
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1500 && screen.find("Ok") == std::string::npos; ++field) {
        runFields(machine, 1);
        if (field % 25 == 24) screen = bootScreen(machine);
    }
    if (screen.find("Disk version 1.0 by Microsoft corp.") == std::string::npos) {
        return fail("Disk BASIC must load from drive A.");
    }
    if (!typeLine(machine, u"10 print 6*7") || !typeLine(machine, u"save\"1:test\"")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    if (!typeLine(machine, u"new") || !typeLine(machine, u"load\"1:test\"")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    if (!typeLine(machine, u"run") || !typeLine(machine, u"files")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    screen = bootScreen(machine);
    if (screen.find(" 42 ") == std::string::npos || screen.find("test  .") == std::string::npos ||
        !expander.drive(0).disk().modified()) {
        return fail("SAVE, LOAD, RUN and FILES must work on the disk.");
    }
    return true;
}

// CP/M 2.24 system disk (SVIDSK/Cpm224SS/CPM224SS.DSK): the BIOS banner, the A> prompt, and DIR listing STAT.COM.
bool cpmPass(const char* romPath, const char* diskPath)
{
    std::ifstream romFile(romPath, std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(romFile), {}};
    std::ifstream diskFile(diskPath, std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(diskFile), {}};
    svi801::SviDisk disk;
    if (!disk.load(image)) return fail("The CP/M image must be an SVI disk image.");
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi605b::SVI605B expander;
    expander.drive(0).insert(disk);
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1500 && screen.find("A>") == std::string::npos; ++field) {
        runFields(machine, 1);
        if (field % 25 == 24) screen = bootScreen(machine);
    }
    if (screen.find("Spectravideo CP/M-80 Revision 2.24") == std::string::npos || screen.find("A>") == std::string::npos) {
        return fail("CP/M 2.24 must boot to the A> prompt.");
    }
    if (!typeLine(machine, u"dir")) return fail("Test text must be mappable.");
    runFields(machine, 150);
    screen = bootScreen(machine);
    if (screen.find("STAT     COM") == std::string::npos) return fail("DIR must list STAT.COM.");

    // FORMAT.COM on the empty drive B: the expander inserts a blank disk at WRITE TRACK.
    if (!typeLine(machine, u"format")) return fail("Test text must be mappable.");
    runFields(machine, 150);
    if (!typeLine(machine, u"b")) return fail("Test text must be mappable.");
    runFields(machine, 2500);
    screen = bootScreen(machine);
    if (screen.find("Disk formatted.") == std::string::npos || !expander.drive(1).hasBlankDisk()) {
        return fail("FORMAT must format the empty drive B.");
    }
    const auto& formatted = expander.drive(1).disk().image();
    if (std::count(formatted.begin(), formatted.end(), static_cast<std::uint8_t>(0xE5)) != static_cast<long long>(formatted.size())) {
        return fail("A formatted CP/M disk must hold E5h only.");
    }
    return true;
}

// FORMAT of Disk BASIC on the empty drive 2: the expander inserts a blank disk at the first write,
// FORMAT writes directory, information sector and allocation table on track 20, and the new disk
// takes a program.
bool formatEmptyDrivePass(const char* romPath, const char* diskPath)
{
    std::ifstream romFile(romPath, std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(romFile), {}};
    std::ifstream diskFile(diskPath, std::ios::binary);
    svi801::SviDisk disk;
    if (!disk.load({std::istreambuf_iterator<char>(diskFile), {}})) return fail("Disk BASIC image missing.");
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi605b::SVI605B expander;
    expander.drive(0).insert(disk);
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1500 && screen.find("Ok") == std::string::npos; ++field) {
        runFields(machine, 1);
        if (field % 25 == 24) screen = bootScreen(machine);
    }
    if (!typeLine(machine, u"run\"1:format\"")) return fail("Test text must be mappable.");
    runFields(machine, 300);
    if (!typeLine(machine, u"2")) return fail("Test text must be mappable.");
    runFields(machine, 100);
    if (expander.drive(1).hasDisk()) return fail("Drive 2 must stay empty until FORMAT writes.");
    if (!typeLine(machine, u"")) return fail("Test text must be mappable.");
    runFields(machine, 1000);
    const auto& formatted = expander.drive(1);
    std::vector<std::uint8_t> directory;
    std::vector<std::uint8_t> allocation;
    if (!formatted.hasBlankDisk() || !formatted.disk().readSector(20, 0, 1, directory) ||
        !formatted.disk().readSector(20, 0, 15, allocation)) {
        return fail("FORMAT on the empty drive 2 must get a blank disk.");
    }
    const std::vector<std::uint8_t> fatStart{0xFE, 0xFE, 0xFE, 0xFF};
    if (directory != std::vector<std::uint8_t>(256, 0xFF) || !std::equal(fatStart.begin(), fatStart.end(), allocation.begin())) {
        return fail("FORMAT must write the directory and the allocation table on track 20.");
    }
    if (!typeLine(machine, u"10 print 7*6") || !typeLine(machine, u"save\"2:probe\"")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    if (!typeLine(machine, u"files 2")) return fail("Test text must be mappable.");
    runFields(machine, 250);
    if (bootScreen(machine).find("probe .") == std::string::npos) return fail("The new disk must take a program.");

    svi605b::SVI605B strict;
    strict.setBlankDiskOnFormat(false);
    strict.writeIo(0x34, 0x0E);
    strict.writeIo(0x30, 0xA0);
    if (strict.drive(1).hasDisk()) return fail("Without the option an empty drive must stay empty.");
    return true;
}

// The BASIC must still start with the expander and an SVI-807 on the bus, and LPRINT must reach
// the Centronics port.
bool basicPass(const char* romPath)
{
    std::ifstream file(romPath, std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(file), {}};
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi605b::SVI605B expander;
    expander.insertCard(0, std::make_unique<svi807::SVI807>());
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1000; ++field) {
        for (std::uint32_t t = 0; t < svi328::CpuTicksPerPalField; ++t) (void)machine.tick();
        if (field % 50 != 49) continue;
        const auto& vdp = machine.video();
        const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
        screen.clear();
        for (std::uint16_t i = 0; i < 40 * 24; ++i) screen += static_cast<char>(vdp.peekVram(base + i) + 0x20);
        if (screen.find("Ok") != std::string::npos) break;
    }
    if (screen.find("SV extended BASIC version 1.1") == std::string::npos || screen.find("Ok") == std::string::npos) {
        return fail("BASIC must start with the SVI-605B and an SVI-807.");
    }
    if (!typeLine(machine, u"lprint \"abc\"")) return fail("Test text must be mappable.");
    const std::vector<std::uint8_t> expected{'a', 'b', 'c', 0x0D, 0x0A};
    if (expander.printer().output() != expected) return fail("LPRINT \"abc\" must print abc, CR, LF.");
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (!slotsPass()) return EXIT_FAILURE;
    if (!centronicsPass()) return EXIT_FAILURE;
    if (argc < 2) {
        std::cerr << "Usage: svi605b_tests <svi111.rom>\n";
        return EXIT_FAILURE;
    }
    if (!basicPass(argv[1])) return EXIT_FAILURE;
    if (argc < 3) {
        std::cerr << "Usage: svi605b_tests <svi111.rom> <svi806.rom> [Disk BASIC .dsk|-] [CP/M 2.24 .dsk|-]\n";
        return EXIT_FAILURE;
    }
    if (!diskBootPass(argv[1])) return EXIT_FAILURE;
    if (argc >= 4 && std::string(argv[3]) != "-" && !diskBasicPass(argv[1], argv[3])) return EXIT_FAILURE;
    if (argc >= 4 && std::string(argv[3]) != "-" && !formatEmptyDrivePass(argv[1], argv[3])) return EXIT_FAILURE;
    if (argc >= 5 && std::string(argv[4]) != "-" && !cpmPass(argv[1], argv[4])) return EXIT_FAILURE;
    std::cout << "SVI-605B passed.\n";
    return EXIT_SUCCESS;
}
