#include "LANIF01/LANIF01.hpp"
#include "SVI608M/SVI608M.hpp"
#include "LANHD01/LANHD01.hpp"
#include "SVI328/Keyboard.hpp"
#include "SVI328/SVI328.hpp"
#include "SVI801/FloppyDrive.hpp"
#include "SVI803/SVI803.hpp"
#include "SVI807/SVI807.hpp"
#include "SVI605B/SVI605B.hpp"
#include "SVI806/SVI806.hpp"

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

std::string bootScreen(svi328::SVI328& machine)
{
    const auto& vdp = machine.video();
    const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
    std::string screen;
    for (std::uint16_t i = 0; i < 40 * 24; ++i) screen += static_cast<char>(vdp.peekVram(base + i) + 0x20);
    return screen;
}

// SVI-608M: five slots, one drive, no printer PIA, the factory cards in slots 0-2.
bool svi608mPass()
{
    svi605b::SVI605B classic;
    svi608m::SVI608M expander;
    if (classic.slotCount() != 4 || classic.driveCount() != 2) return fail("The SVI-605B must have four slots and two drives.");
    if (expander.slotCount() != 5 || expander.driveCount() != 1) return fail("The SVI-608M must have five slots and one drive.");
    if (classic.insertCard(4, std::make_unique<svi807::SVI807>()) == nullptr) return fail("The SVI-605B has no fifth slot.");
    if (expander.insertCard(4, std::make_unique<svi807::SVI807>()) != nullptr || expander.card(4) == nullptr) {
        return fail("The SVI-608M must take a card in its fifth slot.");
    }

    // No PIA at 60h-63h: the printer PIA sits on the LAN card of the SVI-609 only.
    expander.writeIo(0x61, 0x00);
    expander.writeIo(0x60, 0x5A);
    if (expander.readIo(0x60) != 0xFF) return fail("The SVI-608M has no printer PIA at 60h.");

    // Selecting drive B selects no drive, and formatting there inserts no blank disk.
    expander.writeIo(0x34, 0x0A);
    expander.writeIo(0x30, 0xF4);
    if (expander.drive(1).hasDisk()) return fail("The SVI-608M has no drive B.");

    // Formatting the empty drive A inserts a double-sided blank disk, as CP/M 2.27 expects.
    expander.writeIo(0x34, 0x05);
    expander.writeIo(0x30, 0xF4);
    if (!expander.drive(0).hasBlankDisk() || !expander.drive(0).disk().doubleSided()) {
        return fail("The blank disk of the SVI-608M must be double-sided.");
    }
    expander.drive(0).eject();

    const std::vector<std::uint8_t> charRom(svi806::SVI806CharacterRomSize, 0x00);
    const std::vector<std::uint8_t> bootRom(lanif01::BootRomSize, 0xF3);
    if (expander.fitFactoryCards(charRom, std::vector<std::uint8_t>(10))) return fail("A wrong boot ROM must be refused.");
    if (expander.card(0) != nullptr) return fail("A refused fit must leave the slots unchanged.");
    if (!expander.fitFactoryCards(charRom, bootRom)) return fail("The factory cards must fit.");
    if (dynamic_cast<svi806::SVI806*>(expander.card(svi608m::VideoSlot)) == nullptr ||
        dynamic_cast<lanhd01::LANHD01*>(expander.card(svi608m::SasiSlot)) == nullptr ||
        dynamic_cast<lanif01::LANIF01*>(expander.card(svi608m::BootSlot)) == nullptr) {
        return fail("SVI-806, SASI adapter and boot card must sit in slots 0-2.");
    }

    // On the machine the boot EPROM replaces the BASIC ROM after reset.
    svi328::SVI328 machine;
    if (!machine.loadRom(std::vector<std::uint8_t>(svi328::RomSize, 0xC9))) return fail("A 32 KiB ROM must load.");
    machine.connectExpansion(&expander);
    machine.reset();
    if (machine.readMemory(0x0000) != 0xF3) return fail("The SVI-608M must start from its boot EPROM.");
    if (expander.bootSource() != svi608m::BootSource::Unknown) return fail("Nothing is read yet after reset.");
    return true;
}

// Text of the TV screen followed by the 80-column screen of an SVI-806 (if any).
std::string bothScreens(svi328::SVI328& machine, const svi608m::SVI608M& expander)
{
    auto text = bootScreen(machine);
    if (const auto* video = dynamic_cast<const svi806::SVI806*>(expander.card(svi608m::VideoSlot))) {
        const auto& crtc = video->crtc();
        const auto cells = crtc.charactersPerRow() * crtc.displayedRows();
        for (unsigned i = 0; i < cells; ++i) {
            text += static_cast<char>(video->peekVideoRam(static_cast<std::uint16_t>(crtc.startAddress() + i)) + 0x20);
        }
    }
    return text;
}

// CP/M 2.27 on the factory-fitted SVI-608M: the boot card starts it from floppy, the BIOS sets
// the parameters of the blank ST-212, the partitions B: to E: are empty, a file copied to B: runs
// from there; after MSYSGEN H the SVI-608M starts without a floppy from the hard disk.
bool cpm227Pass(const char* romPath, const char* diskPath, const char* charRomPath, const char* bootRomPath)
{
    std::ifstream romFile(romPath, std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(romFile), {}};
    std::ifstream diskFile(diskPath, std::ios::binary);
    svi801::SviDisk disk;
    if (!disk.load({std::istreambuf_iterator<char>(diskFile), {}})) return fail("The CP/M 2.27 image must be an SVI disk image.");
    std::ifstream charRomFile(charRomPath, std::ios::binary);
    const std::vector<std::uint8_t> charRom{std::istreambuf_iterator<char>(charRomFile), {}};
    std::ifstream bootRomFile(bootRomPath, std::ios::binary);
    const std::vector<std::uint8_t> bootRom{std::istreambuf_iterator<char>(bootRomFile), {}};
    svi328::SVI328 machine;
    if (!machine.loadRom(rom)) return fail("BASIC ROM not found or not 32 KiB.");
    svi608m::SVI608M expander;
    if (!expander.fitFactoryCards(charRom, bootRom)) return fail("svi806.rom and svi609.rom must hold 4096 bytes each.");
    auto* sasi = dynamic_cast<lanhd01::LANHD01*>(expander.card(svi608m::SasiSlot));
    sasi->controller().attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
    expander.drive(0).insert(disk);
    machine.connectExpansion(&expander);
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 3000 && screen.find("A>") == std::string::npos; ++field) {
        runFields(machine, 1);
        if (field % 25 == 24) screen = bothScreens(machine, expander);
    }
    if (screen.find("For SV-608M") == std::string::npos || screen.find("A>") == std::string::npos) {
        return fail("The boot card must start CP/M 2.27 from floppy to the A> prompt.");
    }
    if (expander.bootSource() != svi608m::BootSource::Floppy) return fail("The start must be recognised as from floppy.");
    if (sasi->controller().cylinders() != 306 || sasi->controller().heads() != 4) return fail("The BIOS must set 306 cylinders, 4 heads.");
    if (!typeLine(machine, u"stat e:")) return fail("Test text must be mappable.");
    runFields(machine, 400);
    if (bothScreens(machine, expander).find("Bytes Remaining On E: 3104k") == std::string::npos) return fail("Partition E: must be empty.");
    if (!typeLine(machine, u"pip b:=dump.com")) return fail("Test text must be mappable.");
    runFields(machine, 1500);
    if (!typeLine(machine, u"b:") || !typeLine(machine, u"dir")) return fail("Test text must be mappable.");
    runFields(machine, 300);
    if (bothScreens(machine, expander).find("B: DUMP     COM") == std::string::npos || !sasi->controller().modified()) {
        return fail("PIP must copy DUMP.COM to the hard disk.");
    }
    if (!typeLine(machine, u"dump dump.com")) return fail("Test text must be mappable.");
    runFields(machine, 400);
    if (bothScreens(machine, expander).find("01F0 ") == std::string::npos) return fail("DUMP.COM must run from the hard disk.");

    // MSYSGEN H writes T0T1.SYS to the reserved tracks of the hard disk.
    if (!typeLine(machine, u"a:") || !typeLine(machine, u"msysgen")) return fail("Test text must be mappable.");
    runFields(machine, 300);
    if (!typeLine(machine, u"H")) return fail("Test text must be mappable.");
    runFields(machine, 1000);
    if (bothScreens(machine, expander).find("Function complete.") == std::string::npos) return fail("MSYSGEN must write the system to the hard disk.");

    // Cold start without a floppy: the boot card loads the system from the hard disk.
    svi328::SVI328 second;
    (void)second.loadRom(rom);
    svi608m::SVI608M secondExpander;
    (void)secondExpander.fitFactoryCards(charRom, bootRom);
    dynamic_cast<lanhd01::LANHD01*>(secondExpander.card(svi608m::SasiSlot))->controller().attachDisk(sasi->controller().image());
    second.connectExpansion(&secondExpander);
    second.reset();
    screen.clear();
    for (unsigned field = 0; field < 3000 && screen.find("A>") == std::string::npos; ++field) {
        runFields(second, 1);
        if (field % 25 == 24) screen = bothScreens(second, secondExpander);
    }
    if (screen.find("Booted From Hard Disk") == std::string::npos || screen.find("A>") == std::string::npos) {
        return fail("The boot card must start CP/M 2.27 from the hard disk.");
    }
    if (secondExpander.bootSource() != svi608m::BootSource::HardDisk) return fail("The start must be recognised as from the hard disk.");
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (!svi608mPass()) return EXIT_FAILURE;
    if (argc < 4) {
        std::cerr << "Usage: svi608m_tests <svi111.rom> <svi806.rom> <svi609.rom> [CP/M 2.27 .dsk|-]\n";
        return EXIT_FAILURE;
    }
    if (argc >= 5 && std::string(argv[4]) != "-" && !cpm227Pass(argv[1], argv[4], argv[2], argv[3])) return EXIT_FAILURE;
    std::cout << "SVI-608M passed.\n";
    return EXIT_SUCCESS;
}
