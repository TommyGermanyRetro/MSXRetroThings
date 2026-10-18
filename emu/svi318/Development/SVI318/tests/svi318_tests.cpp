#include "SVI318/Keyboard.hpp"
#include "SVI318/SVI318.hpp"

#include <algorithm>
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

void writePsg(svi318::SVI318& machine, std::uint8_t reg, std::uint8_t value)
{
    machine.writeIo(0x88, reg);
    machine.writeIo(0x8C, value);
}

// The SVI-318 keyboard has rows 0-8 only: rows 9 and 10 (a numeric keypad) read FFh.
bool keyboardRowsPass()
{
    svi318::SVI318 machine;
    machine.reset();
    machine.writeIo(0x97, 0x92);
    for (std::uint8_t row = 0; row < 11; ++row) machine.setKeyboardRow(row, 0x00);
    machine.writeIo(0x96, 0x08);
    if (machine.readIo(0x99) != 0x00) return fail("Row 8 must be part of the keyboard.");
    for (std::uint8_t row = 9; row < 11; ++row) {
        machine.writeIo(0x96, row);
        if (machine.readIo(0x99) != 0xFF) return fail("Rows 9 and 10 must read FFh: the SVI-318 has no numeric keypad.");
    }
    return true;
}

// US PC layout: Shift+2 gives @, Shift+6 ^, Shift+; :, and the key right of the left Shift is Z.
bool usKeyboardPass()
{
    svi318::KeyboardPosition position{};
    svi318::KeyModifier modifier = svi318::KeyModifier::None;
    const auto expect = [&](std::uint8_t scanCode, bool shift, std::uint8_t row, std::uint8_t bit,
                            svi318::KeyModifier wanted, const char* what) {
        if (svi318::mapUsScanCodeToSvi(scanCode, false, shift, false, position, modifier) && position.row == row &&
            position.bit == bit && modifier == wanted) {
            return true;
        }
        std::cerr << "US keyboard: " << what << '\n';
        return false;
    };
    if (!expect(0x03, true, 0, 2, svi318::KeyModifier::Shift, "Shift+2 must give @")) return false;
    if (!expect(0x07, true, 0, 6, svi318::KeyModifier::Shift, "Shift+6 must give ^")) return false;
    if (!expect(0x27, true, 1, 2, svi318::KeyModifier::None, "Shift+; must give :")) return false;
    if (!expect(0x2C, false, 5, 2, svi318::KeyModifier::None, "the key right of the left Shift must give z")) return false;
    if (!expect(0x15, false, 5, 1, svi318::KeyModifier::None, "the key right of T must give y")) return false;
    if (svi318::mapUsScanCodeToSvi(0x10, false, false, true, position, modifier)) {
        std::cerr << "US keyboard: the right Alt key must add no character\n";
        return false;
    }
    return true;
}

bool banksPass()
{
    svi318::SVI318 machine;
    std::vector<std::uint8_t> rom(svi318::RomSize, 0x00);
    rom[0x0000] = 0xC3;
    rom[0x7FFF] = 0x5A;
    if (!machine.loadRom(rom)) return fail("A 32 KiB ROM must load.");
    machine.reset();

    if (machine.bankRegister() != 0xFF) return fail("PSG port B must start as an input (FFh).");
    if (machine.readMemory(0x0000) != 0xC3 || machine.readMemory(0x7FFF) != 0x5A) {
        return fail("BASIC ROM must be visible in 0000h-7FFFh after reset.");
    }
    machine.writeMemory(0xC000, 0x11);
    machine.writeMemory(0xFFFF, 0x22);
    if (machine.readMemory(0xC000) != 0x11 || machine.readMemory(0xFFFF) != 0x22) {
        return fail("RAM bank 2 must be read/write in C000h-FFFFh.");
    }
    machine.writeMemory(0x8000, 0x66);
    machine.writeMemory(0xBFFF, 0x66);
    if (machine.readMemory(0x8000) != 0xFF || machine.readMemory(0xBFFF) != 0xFF) {
        return fail("8000h-BFFFh must have no RAM without an SVI-803.");
    }
    machine.writeMemory(0x1234, 0x33);
    if (machine.readMemory(0x1234) != rom[0x1234]) return fail("Writes must not change the ROM.");

    writePsg(machine, 7, 0x80);
    writePsg(machine, 15, 0xFD);
    if (machine.bankRegister() != 0xFD) return fail("Bank register must follow PSG register 15.");
    machine.writeMemory(0x0000, 0x44);
    if (machine.readMemory(0x0000) != 0xC3 || machine.readMemory(0x7FFF) != 0x5A) {
        return fail("/BK21 without an expansion device must leave the ROM: the SVI-318 has no bank 21.");
    }
    if (machine.readMemory(0xC000) != 0x11) return fail("/BK21 must leave RAM bank 2 in C000h-FFFFh.");

    writePsg(machine, 15, 0xFE);
    if (machine.readMemory(0x0000) != 0xFF) return fail("/CART without a cartridge must read FFh.");
    writePsg(machine, 15, 0xFB);
    if (machine.readMemory(0xC000) != 0x11) return fail("/BK22 without an expansion device must leave RAM bank 2.");
    writePsg(machine, 15, 0xF7);
    if (machine.readMemory(0x0000) != 0xC3) return fail("/BK31 without an expansion device must leave the ROM.");
    writePsg(machine, 15, 0xFC);
    if (machine.readMemory(0x0000) != 0xC3) {
        return fail("Two active low-bank signals must leave the ROM selected.");
    }
    return true;
}

// Records what the machine puts on the expansion connector and answers with fixed data.
class TestDevice final : public svi3x8::ExpansionPort {
public:
    std::uint8_t lastBankLines = 0;
    std::uint8_t lastIoPort = 0;
    std::uint8_t lastIoValue = 0;
    unsigned resets = 0;
    unsigned long ticks = 0;
    bool romDisable = false;
    bool ramDisable = false;
    bool interrupt = false;
    std::uint8_t lastWrite = 0;

    void reset() override { ++resets; }
    svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override
    {
        const_cast<TestDevice*>(this)->lastBankLines = bankLines;
        return {static_cast<std::uint8_t>(address >> 8), romDisable && address < 0x8000, ramDisable && address >= 0x8000};
    }
    svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value, std::uint8_t) override
    {
        lastWrite = value;
        return {0xFF, romDisable && address < 0x8000, ramDisable && address >= 0x8000};
    }
    std::uint8_t readIo(std::uint8_t port) override { return static_cast<std::uint8_t>(port ^ 0xFF); }
    void writeIo(std::uint8_t port, std::uint8_t value) override
    {
        lastIoPort = port;
        lastIoValue = value;
    }
    void tick() override { ++ticks; }
    bool interruptRequest() const override { return interrupt; }
};

bool expansionPass()
{
    svi318::SVI318 machine;
    std::vector<std::uint8_t> rom(svi318::RomSize, 0x00);
    rom[0x1000] = 0xC3;
    if (!machine.loadRom(rom)) return fail("A 32 KiB ROM must load.");
    TestDevice device;
    machine.connectExpansion(&device);
    machine.reset();
    if (device.resets != 1) return fail("reset() must reach the expansion device (/RST).");

    if (machine.readIo(0x30) != 0xCF || machine.readIo(0x7F) != 0x80) return fail("Ports below 80h must read from the bus.");
    machine.writeIo(0x10, 0x5A);
    if (device.lastIoPort != 0x10 || device.lastIoValue != 0x5A) return fail("Ports below 80h must write to the bus.");
    if (machine.readIo(0xA0) != 0xFF) return fail("Unused ports from 80h up must not reach the bus.");
    device.lastIoPort = 0;
    machine.writeIo(0x81, 0x00);
    if (device.lastIoPort != 0) return fail("Internal ports must not reach the bus.");

    machine.writeMemory(0xD000, 0x44);
    if (device.lastWrite != 0x44 || machine.readMemory(0xD000) != 0x44) {
        return fail("Writes must reach the bus and RAM bank 2 without /RAMDIS.");
    }
    if (machine.readMemory(0x1000) != 0xC3) return fail("The ROM must answer without /ROMDIS.");
    device.romDisable = true;
    if (machine.readMemory(0x1000) != 0x10) return fail("/ROMDIS must replace the ROM by bus data.");
    device.ramDisable = true;
    machine.writeMemory(0xD000, 0x55);
    if (machine.readMemory(0xD000) != 0xD0) return fail("/RAMDIS must replace RAM bank 2 by bus data.");
    device.ramDisable = false;
    if (machine.readMemory(0xD000) != 0x44) return fail("/RAMDIS must keep RAM bank 2 from being written.");
    if (machine.readMemory(0x9000) != 0x90) return fail("8000h-BFFFh must read the bus: there is no internal RAM.");

    writePsg(machine, 7, 0x80);
    writePsg(machine, 15, 0xE5);
    (void)machine.readMemory(0x8000);
    if ((device.lastBankLines & 0x1E) != 0x04) return fail("The bus must see /BK21, /BK22, /BK31 and /BK32.");
    writePsg(machine, 15, 0xFD);
    device.romDisable = true;
    if (machine.readMemory(0x2000) != 0x20) return fail("/BK21 with /ROMDIS must read the bus: bank 21 is external.");

    for (int i = 0; i < 100; ++i) (void)machine.tick();
    if (device.ticks != 100) return fail("Every T-state must clock the expansion device.");
    machine.connectExpansion(nullptr);
    if (machine.readIo(0x30) != 0xFF) return fail("Without a device the bus must read FFh.");
    return true;
}

// The expansion /INT shares the CPU interrupt with the VDP: IM 1 code counts interrupts in RAM.
bool expansionInterruptPass()
{
    svi318::SVI318 machine;
    std::vector<std::uint8_t> rom(svi318::RomSize, 0x00);
    const std::vector<std::uint8_t> boot{0xF3, 0x31, 0x00, 0xF0, 0xED, 0x56, 0xFB, 0x18, 0xFE};
    std::copy(boot.begin(), boot.end(), rom.begin());
    const std::vector<std::uint8_t> handler{0x21, 0x00, 0xD0, 0x34, 0xFB, 0xC9};
    std::copy(handler.begin(), handler.end(), rom.begin() + 0x38);
    if (!machine.loadRom(rom)) return fail("A 32 KiB ROM must load.");
    TestDevice device;
    machine.connectExpansion(&device);
    machine.reset();
    for (int i = 0; i < 2000; ++i) (void)machine.tick();
    if (machine.readMemory(0xD000) != 0) return fail("Without /INT no interrupt may occur.");
    device.interrupt = true;
    for (int i = 0; i < 2000; ++i) (void)machine.tick();
    if (machine.readMemory(0xD000) == 0) return fail("Expansion /INT must interrupt the CPU.");
    return true;
}

bool cartridgeBanksPass()
{
    svi318::SVI318 machine;
    std::vector<std::uint8_t> cartridge(svi318::CartridgeMaxSize, 0x00);
    cartridge[0x0000] = 0xF3;
    cartridge[0x7FFF] = 0x11;
    cartridge[0x8000] = 0x22;
    cartridge[0xBFFF] = 0x33;
    cartridge[0xC000] = 0x44;
    cartridge[0xFFFF] = 0x55;
    if (machine.insertCartridge(std::vector<std::uint8_t>(svi318::CartridgeMaxSize + 1))) {
        return fail("Cartridge images above 64 KiB must be rejected.");
    }
    if (!machine.insertCartridge(cartridge) || !machine.cartridgeInserted()) return fail("A 64 KiB cartridge must load.");
    machine.reset();
    machine.writeMemory(0x8000, 0x66);
    machine.writeMemory(0xC000, 0x77);

    writePsg(machine, 7, 0x80);
    writePsg(machine, 15, 0xFE);
    if (machine.readMemory(0x0000) != 0xF3 || machine.readMemory(0x7FFF) != 0x11) {
        return fail("/CART must place cartridge bank 11 in 0000h-7FFFh.");
    }
    if (machine.readMemory(0x8000) != 0xFF || machine.readMemory(0xC000) != 0x77) {
        return fail("/CART alone must leave bank 2 in 8000h-FFFFh.");
    }
    writePsg(machine, 15, 0xBE);
    if (machine.readMemory(0x8000) != 0x22 || machine.readMemory(0xBFFF) != 0x33 || machine.readMemory(0xC000) != 0x77) {
        return fail("/ROMEN0 must place bank 12 only in 8000h-BFFFh.");
    }
    machine.writeMemory(0x8000, 0x99);
    if (machine.readMemory(0x8000) != 0x22) return fail("Writes must not change the cartridge.");
    writePsg(machine, 15, 0x7E);
    if (machine.readMemory(0xC000) != 0x44 || machine.readMemory(0xFFFF) != 0x55 || machine.readMemory(0x8000) != 0xFF) {
        return fail("/ROMEN1 must place bank 12 only in C000h-FFFFh.");
    }
    writePsg(machine, 15, 0x3F);
    if (machine.readMemory(0x0000) == 0xF3 || machine.readMemory(0xC000) != 0x77) {
        return fail("/ROMEN0 and /ROMEN1 must have no effect without /CART.");
    }

    machine.ejectCartridge();
    writePsg(machine, 15, 0xFE);
    if (machine.cartridgeInserted() || machine.readMemory(0x0000) != 0xFF) return fail("Ejecting must empty bank 11.");
    std::vector<std::uint8_t> small(0x4000, 0x5A);
    if (!machine.insertCartridge(small) || machine.readMemory(0x3FFF) != 0x5A || machine.readMemory(0x4000) != 0xFF) {
        return fail("Bytes beyond a smaller image must read FFh.");
    }
    return true;
}

bool portsPass()
{
    svi318::SVI318 machine;
    machine.reset();

    machine.writeIo(0x81, 0x00);
    machine.writeIo(0x81, 0x40);
    machine.writeIo(0x80, 0xA5);
    for (int i = 0; i < 64; ++i) (void)machine.tick();
    if (machine.video().peekVram(0x0000) != 0xA5) return fail("VDP data must be written through 80h.");
    machine.writeIo(0x81, 0x00);
    machine.writeIo(0x81, 0x00);
    for (int i = 0; i < 64; ++i) (void)machine.tick();
    if (machine.readIo(0x84) != 0xA5) return fail("VDP data must be read through 84h.");

    writePsg(machine, 0, 0x5A);
    machine.writeIo(0x88, 0);
    if (machine.readIo(0x90) != 0x5A) return fail("PSG registers must read back through 90h.");

    machine.setJoysticks(static_cast<std::uint8_t>(0xFF & ~svi318::JoystickLeft),
                         static_cast<std::uint8_t>(0xFF & ~(svi318::JoystickUp | svi318::JoystickTrigger)));
    machine.writeIo(0x88, 14);
    if (machine.readIo(0x90) != 0xEB) return fail("PSG port A must hold joystick 1 in bits 0-3, 2 in bits 4-7.");
    machine.writeIo(0x88, 7);
    machine.writeIo(0x8C, 0xFF);
    machine.writeIo(0x88, 14);
    machine.writeIo(0x8C, 0x21);
    if (machine.readIo(0x90) != 0xEB) return fail("Joystick lines must read through register 14 with port A as output.");

    machine.writeIo(0x97, 0x92);
    machine.setKeyboardKey(8, 0, true);
    machine.writeIo(0x96, 0x08);
    if (machine.readIo(0x99) != 0xFE) return fail("PPI port B must return the row selected by port C.");
    machine.writeIo(0x96, 0x07);
    if (machine.readIo(0x99) != 0xFF) return fail("Other keyboard rows must be released.");
    if ((machine.readIo(0x98) & 0x30) != 0x10) return fail("PPI port A bit 5 must be the joystick 2 trigger.");

    machine.writeIo(0x96, 0x80);
    if (!machine.clickLevel()) return fail("PPI port C bit 7 must drive the keyboard click.");
    return true;
}

// The SVI BASIC stores characters as ASCII - 20h (its font starts with the space).
std::string screenText(const svi318::SVI318& machine)
{
    const auto& vdp = machine.video();
    const bool text = (vdp.registerValue(1) & 0x10) != 0;
    const unsigned columns = text ? 40 : 32;
    const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
    std::string screen;
    for (unsigned row = 0; row < 24; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            const auto c = vdp.peekVram(static_cast<std::uint16_t>(base + row * columns + column)) + 0x20;
            screen += (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
        }
        screen += '\n';
    }
    return screen;
}

bool bootPass(const char* romPath)
{
    std::ifstream file(romPath, std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file), {}};
    if (image.size() != svi318::RomSize) return fail("BASIC ROM not found or not 32 KiB.");
    svi318::SVI318 machine;
    if (!machine.loadRom(image)) return fail("BASIC ROM must load.");
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1000; ++field) {
        for (std::uint32_t t = 0; t < svi318::CpuTicksPerPalField; ++t) (void)machine.tick();
        if (field % 50 == 49) {
            screen = screenText(machine);
            if (screen.find("Ok") != std::string::npos) break;
        }
    }
    if (screen.find("SV extended BASIC version 1.1") == std::string::npos ||
        screen.find("Copyright 1983 (C) by Microsoft corp.") == std::string::npos ||
        screen.find("Ok") == std::string::npos) {
        std::cerr << screen << "R0-7:";
        for (std::uint8_t r = 0; r < 8; ++r) std::cerr << ' ' << std::hex << int(machine.video().registerValue(r));
        std::cerr << '\n';
        return fail("BASIC 1.11 must start with its banner, the copyright line and Ok.");
    }

    const auto runFields = [&](unsigned fields) {
        for (unsigned field = 0; field < fields; ++field) {
            for (std::uint32_t t = 0; t < svi318::CpuTicksPerPalField; ++t) (void)machine.tick();
        }
    };
    const auto pressContact = [&](svi318::KeyboardPosition position, bool shift) {
        if (shift) machine.setKeyboardKey(svi318::SviShift.row, svi318::SviShift.bit, true);
        machine.setKeyboardKey(position.row, position.bit, true);
        runFields(3);
        machine.setKeyboardKey(position.row, position.bit, false);
        if (shift) machine.setKeyboardKey(svi318::SviShift.row, svi318::SviShift.bit, false);
        runFields(3);
    };
    const auto typeLine = [&](const std::u16string& text) {
        for (const char16_t character : text) {
            svi318::KeyboardPosition position{};
            svi318::KeyModifier modifier{};
            if (!svi318::mapCharacterToSvi(character, position, modifier)) return false;
            pressContact(position, modifier == svi318::KeyModifier::Shift);
        }
        pressContact({6, 6}, false);
        runFields(10);
        return true;
    };
    if (!typeLine(u"print 1+2")) return fail("Test text must be mappable.");
    screen = screenText(machine);
    if (screen.find("print 1+2") == std::string::npos || screen.find(" 3 ") == std::string::npos) {
        std::cerr << screen;
        return fail("Typing print 1+2 and Enter through the SVI matrix must print 3.");
    }

    // Cassette round trip: CSAVE into a recording, then NEW, CLOAD from it and LIST.
    if (!typeLine(u"10 print 123")) return fail("Test text must be mappable.");
    machine.startCassetteRecording();
    if (!typeLine(u"csave\"t\"")) return fail("Test text must be mappable.");
    runFields(1500);
    if (!machine.stopCassetteRecording()) return fail("CSAVE must produce a decodable recording.");
    const auto& recording = machine.cassetteImage();
    const std::vector<std::uint8_t> header(svi318::Cassette::BlockHeader.begin(), svi318::Cassette::BlockHeader.end());
    if (recording.size() < 34 || !std::equal(header.begin(), header.end(), recording.begin()) ||
        recording[17] != 0xD3 || recording[27] != 't') {
        return fail("The recording must start with a BASIC file header block named t.");
    }
    if (svi318::suggestedCassetteFileName(recording) != L"t.cas") return fail("The recording must suggest t.cas.");
    if (!typeLine(u"new") || !typeLine(u"cload")) return fail("Test text must be mappable.");
    runFields(1500);
    if (!typeLine(u"list")) return fail("Test text must be mappable.");
    screen = screenText(machine);
    if (screen.find("Found:t") == std::string::npos || screen.find("10 PRINT 123") == std::string::npos) {
        std::cerr << screen;
        return fail("CLOAD must read the CSAVE recording back.");
    }
    return true;
}

// The BASIC ROM checks bank 11 for F3h 31h and jumps to 0000h; the test module marks RAM and halts.
bool cartridgeBootPass(const std::vector<std::uint8_t>& rom)
{
    svi318::SVI318 machine;
    const std::vector<std::uint8_t> module{0xF3, 0x31, 0x00, 0x00, 0x3E, 0xA5, 0x32, 0x00, 0xD0, 0x18, 0xFE};
    if (!machine.loadRom(rom) || !machine.insertCartridge(module)) return fail("ROM and module must load.");
    machine.reset();
    for (unsigned field = 0; field < 500 && machine.readMemory(0xD000) != 0xA5; ++field) {
        for (std::uint32_t t = 0; t < svi318::CpuTicksPerPalField; ++t) (void)machine.tick();
    }
    if (machine.readMemory(0xD000) != 0xA5 || (machine.bankRegister() & 0x01) != 0) {
        return fail("The BASIC ROM must start a module beginning with F3h 31h.");
    }
    if (machine.readMemory(0x0000) != 0xF3) return fail("The module must run with bank 11 selected.");
    return true;
}

} // namespace

// NTSC machine with the TMS9918A: 262 lines per field, 60 fields per second; BASIC starts and its
// interrupt runs 60 times per second.
bool ntscPass(const char* romPath)
{
    std::ifstream file(romPath, std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file), {}};
    svi318::SVI318 machine(svi318::VideoStandard::Ntsc);
    if (machine.videoStandard() != svi318::VideoStandard::Ntsc || machine.cpuTicksPerField() != 59736 ||
        machine.fieldRateHz() != 60 || machine.video().outputs().scanlinesPerFrame != 262) {
        return fail("The NTSC machine must run 262 lines and 59736 T-states per field at 60 Hz.");
    }
    if (!machine.loadRom(image)) return fail("BASIC ROM must load.");
    machine.reset();
    std::string screen;
    for (unsigned field = 0; field < 1200 && screen.find("Ok") == std::string::npos; ++field) {
        for (std::uint32_t t = 0; t < machine.cpuTicksPerField(); ++t) (void)machine.tick();
        if (field % 60 == 59) screen = screenText(machine);
    }
    if (screen.find("SV extended BASIC version 1.1") == std::string::npos || screen.find("Ok") == std::string::npos) {
        return fail("BASIC must start on the NTSC machine.");
    }
    unsigned interrupts = 0;
    bool previous = machine.video().outputs().interruptRequest;
    for (std::uint32_t t = 0; t < svi318::CpuClockHz; ++t) {
        (void)machine.tick();
        const bool request = machine.video().outputs().interruptRequest;
        if (request && !previous) ++interrupts;
        previous = request;
    }
    if (interrupts < 59 || interrupts > 61) return fail("The NTSC VDP must interrupt 60 times per second.");
    return true;
}

// A snapshot taken in BASIC continues exactly like the original machine in a second machine; data
// of the other video standard or cut short is refused and leaves the machine untouched.
bool statePass(const std::vector<std::uint8_t>& rom)
{
    const auto runFields = [](svi318::SVI318& machine, unsigned fields) {
        for (unsigned field = 0; field < fields; ++field) {
            for (std::uint32_t t = 0; t < machine.cpuTicksPerField(); ++t) (void)machine.tick();
        }
    };
    svi318::SVI318 original;
    if (!original.loadRom(rom)) return fail("BASIC ROM must load.");
    original.reset();
    runFields(original, 300);
    const auto state = original.saveState();
    if (svi318::SVI318::stateVideoStandard(state) != svi318::VideoStandard::Pal) {
        return fail("A snapshot must record its video standard.");
    }

    svi318::SVI318 copy;
    if (!copy.loadRom(rom)) return fail("BASIC ROM must load.");
    copy.reset();
    if (!copy.loadState(state)) return fail("A snapshot must load into a machine of the same standard.");
    for (const auto machine : {&original, &copy}) {
        machine->setKeyboardKey(4, 0, true);
        runFields(*machine, 3);
        machine->setKeyboardKey(4, 0, false);
        runFields(*machine, 20);
    }
    for (std::uint32_t address = 0; address < tms99x8::VramSize; ++address) {
        if (original.video().peekVram(static_cast<std::uint16_t>(address)) != copy.video().peekVram(static_cast<std::uint16_t>(address))) {
            return fail("A restored machine must continue exactly like the original (VRAM).");
        }
    }
    for (std::uint32_t address = 0x8000; address < 0x10000; ++address) {
        if (original.readMemory(static_cast<std::uint16_t>(address)) != copy.readMemory(static_cast<std::uint16_t>(address))) {
            return fail("A restored machine must continue exactly like the original (RAM).");
        }
    }
    // Row 4 bit 0 is the P key.
    if (screenText(copy).find("p") == std::string::npos) return fail("The restored machine must take keyboard input.");

    svi318::SVI318 ntsc(svi318::VideoStandard::Ntsc);
    if (ntsc.loadState(state)) return fail("A PAL snapshot must not load into the NTSC machine.");
    const auto before = copy.saveState();
    auto shortened = state;
    shortened.resize(shortened.size() - 1);
    if (copy.loadState(shortened) || copy.saveState() != before) {
        return fail("A shortened snapshot must be refused without changing the machine.");
    }
    if (svi318::SVI318::stateVideoStandard({0x00, 0x01}) || copy.loadState({})) {
        return fail("Data without the snapshot header must be refused.");
    }
    return true;
}

int main(int argc, char** argv)
{
    if (!banksPass()) return EXIT_FAILURE;
    if (!usKeyboardPass()) return EXIT_FAILURE;
    if (!keyboardRowsPass()) return EXIT_FAILURE;
    if (!portsPass()) return EXIT_FAILURE;
    if (!cartridgeBanksPass()) return EXIT_FAILURE;
    if (!expansionPass()) return EXIT_FAILURE;
    if (!expansionInterruptPass()) return EXIT_FAILURE;
    if (argc < 2) {
        std::cerr << "Usage: svi318_tests <svi111.rom>\n";
        return EXIT_FAILURE;
    }
    if (!bootPass(argv[1])) return EXIT_FAILURE;
    if (!ntscPass(argv[1])) return EXIT_FAILURE;
    std::ifstream romFile(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>(romFile), {}};
    if (!cartridgeBootPass(rom)) return EXIT_FAILURE;
    if (!statePass(rom)) return EXIT_FAILURE;
    std::cout << "SVI-318 machine passed.\n";
    return EXIT_SUCCESS;
}
