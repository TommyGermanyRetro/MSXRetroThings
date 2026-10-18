#include "SVI728/SVI728.hpp"
#include "SVI728/Keyboard.hpp"
#include "SVI728/CassetteRecorder.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void testMainRomAndMirror()
{
    svi728::SVI728 machine;
    std::vector<std::uint8_t> rom(svi728::MainRomSize);
    for (std::size_t i = 0; i < rom.size(); ++i) {
        rom[i] = static_cast<std::uint8_t>((i / svi728::PageSize) + 0x10);
    }

    require(machine.loadMainRom(rom), "32-KB main ROM loads");
    require(machine.readMemory(0x0000) == 0x10, "slot 0 serves BIOS page 0");
    require(machine.readMemory(0x4000) == 0x11, "slot 0 serves BASIC page 1");
    require(machine.readMemory(0x8000) == 0x10, "SVI main ROM mirrors page 0 at 8000h");
    require(machine.readMemory(0xC000) == 0x11, "SVI main ROM mirrors page 1 at C000h");
    machine.writeMemory(0x0000, 0x55);
    require(machine.readMemory(0x0000) == 0x10, "main ROM ignores writes");
    require(!machine.loadMainRom(std::vector<std::uint8_t>(svi728::MainRomSize + 1)),
            "oversized main ROM is rejected");
}

void testPrimarySlotsAndRam()
{
    svi728::SVI728 machine;
    std::vector<std::uint8_t> bios(svi728::MainRomSize, 0x11);
    require(machine.loadMainRom(bios), "BIOS setup succeeds");

    machine.writeIo(0xA8, 0x55);
    require(machine.primarySlotRegister() == 0,
            "slot register does not change while PPI port A is an input");
    machine.writeIo(0xAB, 0x82);
    machine.writeIo(0xA8, 0x55);
    require(machine.primarySlotRegister() == 0x55, "PPI A writes the primary-slot register");
    require(machine.readIo(0xA8) == 0x55, "PPI A reads back the primary-slot register");
    machine.writeMemory(0x0000, 0xA5);
    machine.writeMemory(0x4000, 0x5A);
    machine.writeMemory(0xFFFF, 0xC3);
    require(machine.readMemory(0x0000) == 0xA5 && machine.readMemory(0x4000) == 0x5A &&
            machine.readMemory(0xFFFF) == 0xC3,
            "slot 1 exposes all 64 KB RAM pages");
}

void testCartridgeSlotAndIoDecode()
{
    svi728::SVI728 machine;
    const std::vector<std::uint8_t> cartridge = {0xC3, 0x00, 0x40};
    require(machine.loadSlotRom(2, 0x8000, cartridge), "cartridge image loads into slot 2");
    machine.writeIo(0xAB, 0x82);
    machine.writeIo(0xA8, 0x20);
    require(machine.readMemory(0x8000) == 0xC3 && machine.readMemory(0x8002) == 0x40,
            "primary page 2 selects cartridge slot 2");
    machine.writeMemory(0x8000, 0x00);
    require(machine.readMemory(0x8000) == 0xC3, "cartridge ROM ignores writes");
    require(!machine.loadSlotRom(1, 0, cartridge), "ROM cannot replace the RAM slot");
    require(machine.readIo(0xE0) == 0xFF, "unmapped I/O reads as open bus");
}

void testPrinterIo()
{
    svi728::SVI728 machine;
    require(machine.readIo(0x90) == 0xFD && machine.readIo(0x97) == 0xFD,
            "printer status reports ready across the mirrored ports");

    machine.writeIo(0x91, 0x41);
    machine.writeIo(0x90, 0x00);
    machine.writeIo(0x90, 0x01);
    machine.writeIo(0x95, 0x42);
    machine.writeIo(0x94, 0x00);
    machine.writeIo(0x96, 0x01);

    const std::vector<std::uint8_t> expected = {0x41, 0x42};
    require(machine.printer().output() == expected,
            "printer data and strobe port mirrors capture bytes on rising edges");
}

void testMegaRomCartridge()
{
    svi728::SVI728 machine;
    std::vector<std::uint8_t> megaRom(0x20000);
    for (std::size_t offset = 0; offset < megaRom.size(); ++offset) megaRom[offset] = static_cast<std::uint8_t>(offset / 0x4000);
    require(machine.insertCartridge(3, megaRom, cartridge::Mapper::Ascii16), "an ASCII16 MegaROM loads into slot 3");
    machine.writeIo(0xAB, 0x82);
    machine.writeIo(0xA8, 0x3C);
    machine.writeMemory(0x6000, 5);
    machine.writeMemory(0x7000, 2);
    require(machine.readMemory(0x4000) == 5 && machine.readMemory(0x8000) == 2, "CPU writes switch the slot 3 banks");
    machine.reset();
    machine.writeIo(0xAB, 0x82);
    machine.writeIo(0xA8, 0x3C);
    require(machine.readMemory(0x4000) == 0, "reset selects the power-on banks");
    machine.ejectCartridge(3);
    require(machine.readMemory(0x4000) == 0xFF && !machine.cartridgeSlot(3).inserted(), "an empty slot reads FFh");
}

void testPpiKeyboardAndSlotPorts()
{
    svi728::SVI728 machine;
    machine.setKeyboardRow(3, 0xFE);
    machine.writeIo(0xAB, 0x82);
    machine.writeIo(0xAA, 0x03);
    require(machine.readIo(0xA9) == 0xFE,
            "PPI port C binary row index selects keyboard row 3 on port B");

    machine.setKeyboardKey(4, 2, true);
    machine.writeIo(0xAA, 0x04);
    require((machine.readIo(0xA9) & 0x04) == 0,
            "individual keyboard key is active-low in the selected matrix row");
    machine.setKeyboardKey(4, 2, false);
    require((machine.readIo(0xA9) & 0x04) != 0,
            "releasing a key restores its matrix bit");

    machine.writeIo(0xAA, 0x00);
    require(!machine.oneBitSoundLevel(),
            "PPI port C bit 7 low produces a low one-bit audio level");
    machine.writeIo(0xAA, 0x80);
    require(machine.oneBitSoundLevel(),
            "PPI port C bit 7 drives the MSX one-bit sound output");

    machine.setKeyboardKey(2, 6, true);
    machine.writeIo(0xAA, 0x02);
    require((machine.readIo(0xA9) & 0x40) == 0,
            "MSX key A is read from matrix row 2 bit 6");

    machine.writeIo(0xA8, 0x12);
    require(machine.primarySlotRegister() == 0x12,
            "slot-select port remains available through PPI port A");
}

void testUsScanCodeToMsxMatrix()
{
    svi728::KeyboardPosition position{};
    bool msxShiftRequired = false;

    require(svi728::mapUsScanCodeToMsx(0x03, false, true, false, position, msxShiftRequired) &&
            position.row == 0 && position.bit == 2 && msxShiftRequired,
            "US Shift+2 maps to MSX Shift+2 (at sign)");
    require(svi728::mapUsScanCodeToMsx(0x07, false, true, false, position, msxShiftRequired) &&
            position.row == 0 && position.bit == 6 && msxShiftRequired,
            "US Shift+6 maps to MSX Shift+6 (caret)");
    require(svi728::mapUsScanCodeToMsx(0x27, false, true, false, position, msxShiftRequired) &&
            position.row == 1 && position.bit == 7 && msxShiftRequired,
            "US Shift+semicolon maps to MSX Shift+semicolon (colon)");
    require(svi728::mapUsScanCodeToMsx(0x2C, false, false, false, position, msxShiftRequired) &&
            position.row == 5 && position.bit == 7 && !msxShiftRequired,
            "US Z key scan code maps to the MSX Z contact");
    require(svi728::mapUsScanCodeToMsx(0x15, false, false, false, position, msxShiftRequired) &&
            position.row == 5 && position.bit == 6 && !msxShiftRequired,
            "US Y key scan code maps to the MSX Y contact");
    require(!svi728::mapUsScanCodeToMsx(0x10, false, false, true, position, msxShiftRequired),
            "the right Alt key adds no character on the US layout");
}

void testGermanScanCodeToUsMsxMatrix()
{
    svi728::KeyboardPosition position{};
    bool msxShiftRequired = false;

    require(svi728::mapGermanScanCodeToMsx(0x0B, false, true, false,
                                           position, msxShiftRequired) &&
            position.row == 1 && position.bit == 3 && !msxShiftRequired,
            "German scan code Shift+0 maps to the unshifted MSX equals key");
    require(svi728::mapGermanScanCodeToMsx(0x2B, false, false, false,
                                           position, msxShiftRequired) &&
            position.row == 0 && position.bit == 3 && msxShiftRequired,
            "German hash-key scan code maps to US-MSX Shift+3");
    require(svi728::mapGermanScanCodeToMsx(0x1B, false, false, false,
                                           position, msxShiftRequired) &&
            position.row == 1 && position.bit == 3 && msxShiftRequired,
            "German plus-key scan code maps to US-MSX Shift+equals");
    require(svi728::mapGermanScanCodeToMsx(0x15, false, false, false,
                                           position, msxShiftRequired) &&
            position.row == 5 && position.bit == 7 && !msxShiftRequired,
            "German Z key scan code maps to the MSX Z contact");
    require(svi728::mapGermanScanCodeToMsx(0x2C, false, false, false,
                                           position, msxShiftRequired) &&
            position.row == 5 && position.bit == 6 && !msxShiftRequired,
            "German Y key scan code maps to the MSX Y contact");
    require(svi728::mapGermanScanCodeToMsx(0x0C, false, true, false,
                                           position, msxShiftRequired) &&
            position.row == 2 && position.bit == 4 && msxShiftRequired,
            "German shifted question-mark scan code maps to US-MSX Shift+slash");
    require(svi728::mapGermanScanCodeToMsx(0x08, false, true, false,
                                           position, msxShiftRequired) &&
            position.row == 2 && position.bit == 4 && !msxShiftRequired,
            "German Shift+7 slash maps to the US-MSX slash contact");
    require(svi728::mapGermanScanCodeToMsx(0x34, false, false, false,
                                           position, msxShiftRequired) &&
            position.row == 2 && position.bit == 3 && !msxShiftRequired,
            "German period scan code maps to the US-MSX period contact");
    require(svi728::mapGermanScanCodeToMsx(0x34, false, true, false,
                                           position, msxShiftRequired) &&
            position.row == 1 && position.bit == 7 && msxShiftRequired,
            "German Shift+period colon maps to US-MSX Shift+semicolon");
    require(svi728::mapGermanScanCodeToMsx(0x33, false, false, false,
                                           position, msxShiftRequired) &&
            position.row == 2 && position.bit == 2 && !msxShiftRequired,
            "German comma scan code maps to the US-MSX comma contact");
    require(svi728::mapGermanScanCodeToMsx(0x33, false, true, false,
                                           position, msxShiftRequired) &&
            position.row == 1 && position.bit == 7 && !msxShiftRequired,
            "German Shift+comma semicolon maps to the US-MSX semicolon contact");
    require(svi728::mapGermanScanCodeToMsx(0x08, false, false, true,
                                           position, msxShiftRequired) &&
            position.row == 1 && position.bit == 5 && msxShiftRequired,
            "German AltGr+7 scan code maps to US-MSX Shift+left bracket");
        require(svi728::mapGermanScanCodeToMsx(0x10, false, false, true,
                                                                                   position, msxShiftRequired) &&
                        position.row == 0 && position.bit == 2 && msxShiftRequired,
                        "German AltGr+Q scan code maps to US-MSX Shift+2");
    require(!svi728::mapGermanScanCodeToMsx(0x04, false, true, false,
                                            position, msxShiftRequired),
            "German section sign has no false ASCII matrix mapping");
}

void testVdpAndPsgPorts()
{
    svi728::SVI728 machine;
    machine.writeIo(0x99, 0x40);
    machine.writeIo(0x99, 0x81);
    require(machine.video().registerValue(1) == 0x40,
            "VDP control port writes register 1");

    machine.setJoystickPorts(0xA5, 0x5A);
    machine.writeIo(0xA0, 7);
        machine.writeIo(0xA1, 0x80);
    machine.writeIo(0xA0, 14);
    require(machine.readIo(0xA2) == 0xA5,
            "PSG register 14 reads joystick 1 selected by register 15 bit 6");

    machine.writeIo(0xA0, 15);
    machine.writeIo(0xA1, 0x40);
    machine.writeIo(0xA0, 14);
        require(machine.readIo(0xA2) == 0xDA,
            "PSG register 15 bit 6 selects joystick 2 on register 14");
}

// A snapshot continues exactly like the original: CPU, RAM, VRAM and the module bank registers;
// it is refused for another video standard, without the module it was taken with, or truncated.
void testSnapshots()
{
    std::vector<std::uint8_t> bios(svi728::MainRomSize, 0x00);
    // DI; PPI port A output; page 3 = slot 1 (RAM); HL = C000h; loop: INC (HL), OUT (98h),(HL), next byte.
    const std::uint8_t program[] = {0xF3, 0x3E, 0x82, 0xD3, 0xAB, 0x3E, 0x40, 0xD3, 0xA8, 0x21, 0x00, 0xC0,
                                    0x34, 0x7E, 0xD3, 0x98, 0x23, 0x7C, 0xF6, 0xC0, 0x67, 0x18, 0xF5};
    std::copy(std::begin(program), std::end(program), bios.begin());
    std::vector<std::uint8_t> megaRom(0x20000);
    for (std::size_t offset = 0; offset < megaRom.size(); ++offset) megaRom[offset] = static_cast<std::uint8_t>(offset / 0x4000);
    const auto runFields = [](svi728::SVI728& machine, unsigned fields) {
        for (unsigned field = 0; field < fields; ++field) {
            for (std::uint32_t tick = 0; tick < machine.cpuTicksPerField(); ++tick) (void)machine.tick();
        }
    };
    const auto fit = [&](svi728::SVI728& machine, bool module) {
        require(machine.loadMainRom(bios), "the test ROM loads");
        if (module) require(machine.insertCartridge(3, megaRom, cartridge::Mapper::Ascii16), "the MegaROM loads");
        machine.reset();
    };

    svi728::SVI728 original;
    fit(original, true);
    runFields(original, 5);
    original.writeIo(0xA8, 0x40 | 0x0C);
    original.writeMemory(0x6000, 5);
    original.writeIo(0xA8, 0x40);
    require(original.cartridgeSlot(3).bankAt(0x4000) == 10, "the MegaROM switches to bank 5");
    const auto state = original.saveState();
    require(svi728::SVI728::stateVideoStandard(state) == svi728::VideoStandard::Pal, "a snapshot records its video standard");

    svi728::SVI728 copy;
    fit(copy, true);
    require(copy.loadState(state), "a snapshot loads into a machine with the same module");
    require(copy.cartridgeSlot(3).bankAt(0x4000) == 10, "the module bank registers are restored");
    runFields(original, 3);
    runFields(copy, 3);
    require(original.ram() == copy.ram(), "a restored machine continues exactly like the original (RAM)");
    for (std::uint32_t address = 0; address < tms99x8::VramSize; ++address) {
        require(original.video().peekVram(static_cast<std::uint16_t>(address)) ==
                    copy.video().peekVram(static_cast<std::uint16_t>(address)),
                "a restored machine continues exactly like the original (VRAM)");
    }
    require(original.ram()[0xC000] != 0, "the test program runs");

    svi728::SVI728 bare;
    fit(bare, false);
    require(!bare.loadState(state), "a snapshot is refused without the module it was taken with");
    svi728::SVI728 ntsc(svi728::VideoStandard::Ntsc);
    fit(ntsc, true);
    require(!ntsc.loadState(state), "a PAL snapshot is refused by the NTSC machine");
    const auto before = copy.saveState();
    auto shortened = state;
    shortened.resize(shortened.size() - 1);
    require(!copy.loadState(shortened) && copy.saveState() == before, "a truncated snapshot leaves the machine unchanged");
    require(!svi728::SVI728::stateVideoStandard({0x00, 0x01}) && !copy.loadState({}), "data without the header is refused");
}

// The NTSC machine carries the TMS9918A: 262 scanlines, 60 fields per second.
void testNtscCpuVdpFieldTiming()
{
    svi728::SVI728 machine(svi728::VideoStandard::Ntsc);
    require(machine.videoStandard() == svi728::VideoStandard::Ntsc && machine.fieldRateHz() == 60 &&
            svi728::CpuTicksPerNtscField == 59736 && machine.cpuTicksPerField() == svi728::CpuTicksPerNtscField &&
            machine.vdpMasterTicksPerField() == svi728::VdpMasterTicksPerNtscField,
            "NTSC field timing follows the TMS9918A");
    machine.reset();
    for (std::uint32_t tick = 0; tick < machine.cpuTicksPerField(); ++tick) (void)machine.tick();
    require(machine.video().outputs().scanline == 0 && machine.video().outputs().dot == 0,
            "one NTSC field ends after exactly 262 scanlines");
    require(svi728::SVI728().fieldRateHz() == 50, "the default machine is PAL");
}

void testPalCpuVdpFieldTiming()
{
    svi728::SVI728 machine;
        require(machine.sound().masterClockHz() == svi728::AyMasterClockHz,
                        "AY clock is half the MSX master clock");
        require(svi728::VdpMasterClockHz == svi728::CpuClockHz * 6 &&
                svi728::VdpMasterTicksPerScanline == 1368 &&
                svi728::VdpMasterTicksPerPalField == 428184 &&
                svi728::VdpDotsPerPalField == 107046 &&
                svi728::CpuTicksPerPalField == 71364,
                        "PAL field timing derives from the shared VDP master clock");
    for (unsigned tick = 0; tick < 2; ++tick) (void)machine.tick();
    require(machine.video().outputs().scanline == 0 &&
            machine.video().outputs().dot == 3,
            "two Z80 T-states advance three VDP dots on the shared master clock");
    (void)machine.tick();
    require(machine.video().outputs().scanline == 0 &&
            machine.video().outputs().dot == 4,
            "VDP dot divider retains its phase across CPU T-states");
    machine.reset();
        for (std::uint32_t tick = 0; tick < svi728::CpuTicksPerPalField; ++tick) {
        (void)machine.tick();
    }
    require(machine.video().outputs().scanline == 0 && machine.video().outputs().dot == 0,
            "one PAL field ends after exactly 313 scanlines and 107046 VDP dots");
}

void testCassetteSignal()
{
    const std::vector<std::uint8_t> image =
        {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74, 0xA5};
    svi728::Cassette tape;
    require(!tape.insert({0x55, 0x7F}), "non-MSX CAS format is rejected");
    require(tape.insert(image), "MSX CAS marker and data are accepted");
    tape.tick(4800);
    require(tape.state().positionTicks == 0, "stopped motor holds the tape position");
    tape.setPins(true, false);
    for (unsigned sample = 0; sample < 9600; ++sample) tape.tick(4800);
    require(tape.inputHigh(), "leader starts high after the initial silence");
    require(tape.state().bytePosition == 0 && tape.state().byteCount == 1 &&
            tape.state().blockIndex == 0 && tape.state().blockCount == 1,
            "progress counts CAS data bytes without markers");
    tape.tick(4800);
    require(!tape.inputHigh(), "leader alternates at 2400 Hz for 1200 baud");
    tape.setPins(false, true);
    const auto stoppedPosition = tape.state().positionTicks;
    tape.tick(4800);
    require(tape.inputHigh() && tape.state().positionTicks == stoppedPosition &&
            tape.state().outputHigh, "motor pause idles CASRD and preserves the write pin");
    tape.setPins(true, false);
    require(!tape.inputHigh(), "motor restart resumes the same signal phase");
    for (unsigned sample = 1; sample < 32000; ++sample) tape.tick(4800);
    for (unsigned bitIndex = 0; bitIndex < 11; ++bitIndex) {
        const bool bit = bitIndex >= 9 ||
            (bitIndex > 0 && (0xA5 & (1u << (bitIndex - 1))) != 0);
        for (unsigned quarter = 0; quarter < 4; ++quarter) {
            require(tape.inputHigh() == (bit ? (quarter % 2 == 0) : quarter < 2),
                    "CAS byte has a start bit, LSB-first data and two stop bits");
            tape.tick(4800);
        }
    }
    require(tape.state().atEnd && tape.inputHigh(), "end of tape has an idle input");
    require(tape.state().bytePosition == 1 && tape.state().blockIndex == 1,
            "progress reaches the byte count at the end of the tape");
    require(!tape.insert({}), "empty CAS image is rejected");
    require(tape.state().atEnd, "failed insertion leaves the mounted tape intact");
    tape.rewind();
    require(tape.state().positionTicks == 0 && !tape.state().atEnd,
            "rewind returns to the beginning");
    tape.eject();
    require(!tape.state().inserted && tape.inputHigh(), "eject removes the signal source");

        const std::vector<std::uint8_t> marker(image.begin(), image.begin() + 8);
        std::vector<std::uint8_t> multiBlock = marker;
        multiBlock.insert(multiBlock.end(), 8, 0x00);
        multiBlock.insert(multiBlock.end(), marker.begin(), marker.end());
        multiBlock.push_back(0xA5);
        require(tape.insert(multiBlock), "aligned markers split CAS data into blocks");
        tape.setPins(true, false);
        for (unsigned sample = 0; sample < 9600 + 32000 + 8 * 44 + 4800; ++sample) {
                tape.tick(4800);
        }
        tape.tick(4800);
        require(!tape.inputHigh(), "second block starts its own leader after the short gap");
        for (unsigned sample = 1; sample < 8000; ++sample) tape.tick(4800);
        tape.tick(4800);
        require(tape.inputHigh(), "data block uses a short leader followed by a zero start bit");

        std::vector<std::uint8_t> unaligned = marker;
        unaligned.push_back(0);
        unaligned.insert(unaligned.end(), marker.begin(), marker.end());
        require(tape.insert(unaligned), "unaligned marker bytes remain payload");
        for (unsigned sample = 0; sample < 9600 + 32000 + 9 * 44; ++sample) tape.tick(4800);
        require(tape.state().atEnd, "unaligned marker does not generate another leader");
}

void testCassettePins()
{
    svi728::SVI728 machine;
    require(machine.loadMainRom(std::vector<std::uint8_t>(svi728::MainRomSize, 0x76)),
            "cassette pin test uses a halted CPU");
    require(machine.loadCassette({0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74, 0xA5}),
            "cassette mounts in the machine");
    require(!machine.cassetteState().motorOn, "undriven PPI motor pin keeps the tape stopped");
    machine.writeIo(0xAB, 0x82);
    require(machine.cassetteState().motorOn, "driven low PC4 starts the tape motor");
    machine.writeIo(0xAB, 0x09);
    require(!machine.cassetteState().motorOn, "8255 BSR set PC4 stops the motor");
    machine.writeIo(0xAB, 0x0B);
    require(machine.cassetteState().outputHigh, "8255 BSR set PC5 drives CASWR high");
    machine.writeIo(0xAB, 0x0A);
    require(!machine.cassetteState().outputHigh, "8255 BSR reset PC5 drives CASWR low");
    machine.writeIo(0xAB, 0x08);
    (void)machine.tick();
    require(machine.cassetteState().positionTicks == 1,
            "band position advances by one CPU T-state when motor is on");
    machine.writeIo(0xAB, 0x09);
    (void)machine.tick();
    require(machine.cassetteState().positionTicks == 1, "CPU ticking cannot move a stopped tape");
    machine.setJoystickPorts(0x25, 0x5A);
    machine.writeIo(0xA0, 7);
    machine.writeIo(0xA1, 0x80);
    machine.writeIo(0xA0, 15);
    machine.writeIo(0xA1, 0);
    machine.writeIo(0xA0, 14);
    require(machine.readIo(0xA2) == 0xA5,
            "PSG input combines joystick lower bits with the independent CASRD pin");
    machine.writeIo(0xA0, 7);
    machine.writeIo(0xA1, 0xC0);
    machine.writeIo(0xA0, 14);
    machine.writeIo(0xA1, 0x12);
    require(machine.readIo(0xA2) == 0xA5, "PSG port A stays an input: R14 reads the joystick and CASRD whatever R7 says");
    machine.reset();
    require(machine.cassetteState().inserted && !machine.cassetteState().motorOn,
            "machine reset leaves the cassette inserted and stops the motor");
}

void testCassetteRecording()
{
        for (const unsigned baud : {1200u, 2400u}) {
                svi728::CassetteRecorder recorder;
                recorder.start();
                recorder.setPins(true, false);
                bool high = false;
                const auto halfPulse = 48000u / (baud * 4);
                const auto writeBit = [&](bool bit) {
                        for (unsigned pulse = 0; pulse < (bit ? 4u : 2u); ++pulse) {
                                high = !high;
                                recorder.setPins(true, high);
                                for (unsigned tick = 0; tick < halfPulse * (bit ? 1u : 2u); ++tick) {
                                        recorder.tick(48000);
                                }
                        }
                };
                for (unsigned bit = 0; bit < 100; ++bit) writeBit(true);
                for (const unsigned value : {0xA5u, 0x00u, 0xFFu, 0x81u}) {
                        writeBit(false);
                        for (unsigned bit = 0; bit < 8; ++bit) writeBit((value & (1u << bit)) != 0);
                        writeBit(true);
                        writeBit(true);
                }
                recorder.setPins(false, high);
                recorder.stop();
                const std::vector<std::uint8_t> expected =
                        {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74, 0xA5, 0x00, 0xFF, 0x81};
                require(!recorder.error() && recorder.image() == expected,
                                "CASWR decoder records 1200/2400 baud with correct bytes and CAS marker");

                recorder.start();
                recorder.setPins(true, high);
                for (unsigned bit = 0; bit < 100; ++bit) writeBit(true);
                writeBit(false);
                writeBit(true);
                recorder.setPins(false, high);
                recorder.stop();
                require(recorder.error(), "an incomplete CASWR byte cannot be exported as success");

                recorder.start();
                recorder.setPins(true, high);
                for (unsigned bit = 0; bit < 100; ++bit) writeBit(true);
                for (const auto value : std::vector<std::uint8_t>
                         {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74}) {
                        writeBit(false);
                        for (unsigned bit = 0; bit < 8; ++bit) writeBit((value & (1u << bit)) != 0);
                        writeBit(true);
                        writeBit(true);
                }
                recorder.setPins(false, high);
                recorder.stop();
                require(recorder.error(), "reserved aligned CAS marker in payload cannot be exported");
        }
}

void testCpuBusCycle()
{
    svi728::SVI728 machine;
    std::vector<std::uint8_t> rom(svi728::MainRomSize, 0x00);
    require(machine.loadMainRom(rom), "CPU test ROM loads");
    const auto pins = machine.tick();
    require(pins.memoryRequest && pins.read && pins.m1,
            "Z80 starts by requesting its reset-vector opcode from slot 0");
}

void testM1WaitStates()
{
    // NOP, NOP, OUT (#A0),A, RLC B, LD IX,#1234, LD B,3, LD C,0, OTIR (3 bytes), HALT
    const std::vector<std::uint8_t> program = {
        0x00, 0x00, 0xD3, 0xA0, 0xCB, 0x00, 0xDD, 0x21, 0x34, 0x12,
        0x06, 0x03, 0x0E, 0x00, 0xED, 0xB3, 0x76};
    std::vector<std::uint8_t> rom(svi728::MainRomSize, 0x76);
    std::copy(program.begin(), program.end(), rom.begin());
    svi728::SVI728 machine;
    require(machine.loadMainRom(rom), "M1 wait test ROM loads");

    const std::vector<std::uint16_t> instructionStarts = {
        0x00, 0x01, 0x02, 0x04, 0x06, 0x0A, 0x0C, 0x0E, 0x0E, 0x0E, 0x10};
    std::vector<std::uint64_t> startTicks;
    bool previousFetch = false;
    for (std::uint64_t tick = 0; tick < 400 && startTicks.size() < instructionStarts.size(); ++tick) {
        const auto pins = machine.tick();
        const bool fetch = pins.m1 && pins.memoryRequest && pins.read;
        if (fetch && !previousFetch &&
            pins.address == instructionStarts[startTicks.size()]) {
            startTicks.push_back(tick);
        }
        previousFetch = fetch;
    }
    require(startTicks.size() == instructionStarts.size(), "M1 wait test reaches HALT");

    // Z80 T-states plus one MSX wait state per M1 cycle (opcode and each prefix byte)
    const std::vector<std::uint64_t> expected = {5, 5, 12, 10, 16, 8, 8, 23, 23, 18};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(startTicks[index + 1] - startTicks[index] == expected[index],
                "every M1 cycle carries exactly one MSX wait state");
    }
}

void testCassetteFileNameSuggestion()
{
    std::vector<std::uint8_t> image =
        {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74};
    image.insert(image.end(), 10, 0xD3);
    image.insert(image.end(), {'T', 'E', 'S', 'T', ' ', ' '});
    require(svi728::suggestedCassetteFileName(image) == L"TEST.cas",
            "CSAVE filename prefills the Windows save dialog without padding");
    for (const auto type : {0xD0, 0xEA}) {
        std::fill(image.begin() + 8, image.begin() + 18, type);
        require(svi728::suggestedCassetteFileName(image) == L"TEST.cas",
                "binary and ASCII tape headers also supply the save filename");
    }
    const std::vector<std::uint8_t> invalidName = {'A', '/', 'B', ':', '?', ' '};
    std::copy(invalidName.begin(), invalidName.end(), image.begin() + 18);
    require(svi728::suggestedCassetteFileName(image) == L"A_B__.cas",
            "Windows filename separators and reserved characters are sanitized");
    std::fill(image.begin() + 18, image.end(), ' ');
    require(svi728::suggestedCassetteFileName(image) == L"Aufnahme.cas",
            "empty tape filenames have a usable fallback");
    std::copy_n("CON   ", 6, image.begin() + 18);
    require(svi728::suggestedCassetteFileName(image) == L"_CON.cas",
            "Windows device names cannot become save targets");
    image.resize(23);
    require(svi728::suggestedCassetteFileName(image) == L"Aufnahme.cas",
            "truncated tape headers cannot supply a filename");
}

} // namespace

int main()
{
    testMainRomAndMirror();
    testPrimarySlotsAndRam();
    testCartridgeSlotAndIoDecode();
    testPrinterIo();
    testMegaRomCartridge();
    testPpiKeyboardAndSlotPorts();
        testGermanScanCodeToUsMsxMatrix();
        testUsScanCodeToMsxMatrix();
    testVdpAndPsgPorts();
        testCassetteSignal();
        testCassettePins();
        testCassetteRecording();
        testPalCpuVdpFieldTiming();
    testNtscCpuVdpFieldTiming();
    testSnapshots();
    testCpuBusCycle();
    testM1WaitStates();
        testCassetteFileNameSuggestion();
    std::cout << "All SVI728 tests passed.\n";
}