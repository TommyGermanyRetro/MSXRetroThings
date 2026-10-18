#include "SVI609/SVI609.hpp"
#include "LANHD01/LANHD01.hpp"
#include "LANIF01/LanCard.hpp"
#include "SVI328/Keyboard.hpp"
#include "SVI328/SVI328.hpp"
#include "SVI801/FloppyDrive.hpp"
#include "SVI805/SVI805.hpp"
#include "SVI806/SVI806.hpp"
#include "SVI807/SVI807.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
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

constexpr svi328::KeyboardPosition SviCtrl{6, 1};

void pressKey(svi328::SVI328& machine, svi328::KeyboardPosition position, bool shift, bool ctrl = false)
{
    if (ctrl) machine.setKeyboardKey(SviCtrl.row, SviCtrl.bit, true);
    if (shift) machine.setKeyboardKey(svi328::SviShift.row, svi328::SviShift.bit, true);
    machine.setKeyboardKey(position.row, position.bit, true);
    runFields(machine, 3);
    machine.setKeyboardKey(position.row, position.bit, false);
    if (shift) machine.setKeyboardKey(svi328::SviShift.row, svi328::SviShift.bit, false);
    if (ctrl) machine.setKeyboardKey(SviCtrl.row, SviCtrl.bit, false);
    runFields(machine, 3);
}

bool typeLine(svi328::SVI328& machine, const std::u16string& text)
{
    for (const char16_t character : text) {
        svi328::KeyboardPosition position{};
        svi328::KeyModifier modifier{};
        if (!svi328::mapCharacterToSvi(character, position, modifier)) return false;
        pressKey(machine, position, modifier == svi328::KeyModifier::Shift);
    }
    pressKey(machine, {6, 6}, false);
    runFields(machine, 10);
    return true;
}

// CTRL-P switches the console echo to the list device (CP/M).
bool togglePrinterEcho(svi328::SVI328& machine)
{
    svi328::KeyboardPosition position{};
    svi328::KeyModifier modifier{};
    if (!svi328::mapCharacterToSvi(u'p', position, modifier)) return false;
    pressKey(machine, position, false, true);
    return true;
}

// Text of the TV screen followed by the 80-column screen of the SVI-806.
std::string bothScreens(svi328::SVI328& machine, const svi609::SVI609& expander)
{
    const auto& vdp = machine.video();
    const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
    std::string text;
    for (std::uint16_t i = 0; i < 40 * 24; ++i) text += static_cast<char>(vdp.peekVram(base + i) + 0x20);
    if (const auto* video = dynamic_cast<const svi806::SVI806*>(expander.card(svi609::VideoSlot))) {
        const auto& crtc = video->crtc();
        const auto cells = crtc.charactersPerRow() * crtc.displayedRows();
        for (unsigned i = 0; i < cells; ++i) {
            text += static_cast<char>(video->peekVideoRam(static_cast<std::uint16_t>(crtc.startAddress() + i)) + 0x20);
        }
    }
    return text;
}

// The Network Expander: five slots, one drive, all slots fitted at the factory.
bool factoryPass()
{
    svi609::SVI609 expander;
    if (expander.slotCount() != 5 || expander.driveCount() != 1) return fail("The SVI-609 expander has five slots and one drive.");
    const std::vector<std::uint8_t> charRom(svi806::SVI806CharacterRomSize, 0x00);
    const std::vector<std::uint8_t> bootRom(lanif01::BootRomSize, 0xF3);
    if (expander.fitFactoryCards(charRom, std::vector<std::uint8_t>(10))) return fail("A wrong boot ROM must be refused.");
    if (expander.card(0) != nullptr) return fail("A refused fit must leave the slots unchanged.");
    if (!expander.fitFactoryCards(charRom, bootRom)) return fail("The factory cards must fit.");
    if (dynamic_cast<svi806::SVI806*>(expander.card(svi609::VideoSlot)) == nullptr ||
        dynamic_cast<svi805::SVI805*>(expander.card(svi609::SerialSlot)) == nullptr ||
        dynamic_cast<svi807::SVI807*>(expander.card(svi609::RamSlot)) == nullptr ||
        dynamic_cast<lanhd01::LANHD01*>(expander.card(svi609::SasiSlot)) == nullptr ||
        dynamic_cast<lanif01::LanCard*>(expander.card(svi609::LanSlot)) == nullptr) {
        return fail("SVI-806, SVI-805, SVI-807, SASI adapter and LAN card must sit in slots 0-4.");
    }

    // The PIA of the LAN card answers at 60h: DDRA through CRA bit 2.
    expander.writeIo(0x61, 0x00);
    expander.writeIo(0x60, 0x5A);
    if (expander.readIo(0x60) != 0x5A) return fail("The LAN card must answer at 60h.");

    // On the machine the boot EPROM of the LAN card replaces the BASIC ROM after reset.
    svi328::SVI328 machine;
    if (!machine.loadRom(std::vector<std::uint8_t>(svi328::RomSize, 0xC9))) return fail("A 32 KiB ROM must load.");
    machine.connectExpansion(&expander);
    machine.reset();
    if (machine.readMemory(0x0000) != 0xF3) return fail("The SVI-609 must start from the EPROM of the LAN card.");
    return true;
}

// CP/M 2.27 on the factory-fitted SVI-609: the LAN card starts it from floppy, the BIOS finds the
// PIA of the LAN card and prints there (CTRL-P and DIR).
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
    svi609::SVI609 expander;
    if (!expander.fitFactoryCards(charRom, bootRom)) return fail("svi806.rom and svi609.rom must hold 4096 bytes each.");
    auto* sasi = dynamic_cast<lanhd01::LANHD01*>(expander.card(svi609::SasiSlot));
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
        return fail("The LAN card must start CP/M 2.27 from floppy to the A> prompt.");
    }
    if (expander.bootSource() != svi608m::BootSource::Floppy) return fail("The start must be recognised as from floppy.");
    if (sasi->controller().cylinders() != 306 || sasi->controller().heads() != 4) return fail("The BIOS must set 306 cylinders, 4 heads.");

    const auto& printer = dynamic_cast<lanif01::LanCard*>(expander.card(svi609::LanSlot))->printerPort().printer();
    if (!togglePrinterEcho(machine) || !typeLine(machine, u"dir")) return fail("Test text must be mappable.");
    runFields(machine, 600);
    if (!togglePrinterEcho(machine)) return fail("Test text must be mappable.");
    const std::string printed(printer.output().begin(), printer.output().end());
    if (printed.find("STAT") == std::string::npos) return fail("CP/M must print the directory on the PIA of the LAN card.");
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (!factoryPass()) return EXIT_FAILURE;
    if (argc < 4) {
        std::cerr << "Usage: svi609_tests <svi111.rom> <svi806.rom> <svi609.rom> [CP/M 2.27 .dsk|-]\n";
        return EXIT_FAILURE;
    }
    if (argc >= 5 && std::string(argv[4]) != "-" && !cpm227Pass(argv[1], argv[4], argv[2], argv[3])) return EXIT_FAILURE;
    std::cout << "SVI-609 passed.\n";
    return EXIT_SUCCESS;
}
