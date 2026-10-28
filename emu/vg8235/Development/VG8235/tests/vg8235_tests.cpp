#include "USBSLOT/BoardSlot.hpp"
#include "VG8235/Keyboard.hpp"
#include "VG8235/VG8235.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::vector<std::uint8_t> filled(std::size_t size, std::uint8_t value)
{
    return std::vector<std::uint8_t>(size, value);
}

vg8235::VG8235 loadedMachine()
{
    vg8235::VG8235 machine;
    require(machine.loadMainRom(filled(vg8235::MainRomSize, 0xB0)), "BIOS image loads");
    require(machine.loadSubRom(filled(vg8235::SubRomSize, 0x5B)), "Sub-ROM image loads");
    require(machine.loadDiskRom(filled(vg8235::DiskRomSize, 0xD5)), "disk ROM image loads");
    machine.writeIo(0xAB, 0x82);
    return machine;
}

void selectAllPages(vg8235::VG8235& machine, std::uint8_t primary, std::uint8_t secondary)
{
    machine.writeIo(0xA8, static_cast<std::uint8_t>(primary * 0x55));
    machine.writeMemory(0xFFFF, static_cast<std::uint8_t>(secondary * 0x55));
}

void testRomSizes()
{
    vg8235::VG8235 machine;
    require(!machine.loadMainRom(filled(0x4000, 0)), "BIOS image must be 32 KiB");
    require(!machine.loadSubRom(filled(0x8000, 0)), "Sub-ROM image must be 16 KiB");
    require(!machine.loadDiskRom(filled(0x8000, 0)), "disk ROM image must be 16 KiB");
}

void testSlotZeroBios()
{
    auto machine = loadedMachine();
    require(machine.readMemory(0x0000) == 0xB0 && machine.readMemory(0x7FFF) == 0xB0,
            "slot 0 shows BIOS/BASIC in pages 0 and 1");
    require(machine.readMemory(0x8000) == 0xFF && machine.readMemory(0xFFFF) == 0xFF,
            "slot 0 is empty in pages 2 and 3");
}

void testSecondarySlotRegister()
{
    auto machine = loadedMachine();
    machine.writeIo(0xA8, 0x00);
    machine.writeMemory(0xFFFF, 0x12);
    require(machine.secondarySlotRegister() == 0x00,
            "#FFFF is only the secondary register while page 3 selects slot 3");
    machine.writeIo(0xA8, 0xC0);
    machine.writeMemory(0xFFFF, 0xA4);
    require(machine.secondarySlotRegister() == 0xA4, "slot 3 accepts the secondary register");
    require(machine.readMemory(0xFFFF) == 0x5B, "reading #FFFF returns the complement");
}

void testSubRomAndDiskRomVisibility()
{
    auto machine = loadedMachine();
    selectAllPages(machine, 3, 0);
    require(machine.readMemory(0x0000) == 0x5B && machine.readMemory(0x3FFF) == 0x5B,
            "slot 3-0 shows the Sub-ROM in page 0");
    require(machine.readMemory(0x4000) == 0xFF, "the Sub-ROM is not mirrored in page 1");
    selectAllPages(machine, 3, 1);
    require(machine.readMemory(0x0000) == 0xFF && machine.readMemory(0x8000) == 0xFF,
            "slot 3-1 is empty");
    selectAllPages(machine, 3, 3);
    require(machine.readMemory(0x4000) == 0xD5 && machine.readMemory(0x7FF7) == 0xD5,
            "slot 3-3 shows the disk ROM in page 1 below the FDC registers");
    require(machine.readMemory(0x0000) == 0xFF && machine.readMemory(0x8000) == 0xFF,
            "the disk ROM is only visible in page 1");
}

void testMemoryMapper()
{
    auto machine = loadedMachine();
    selectAllPages(machine, 3, 2);
    for (std::uint8_t page = 0; page < 4; ++page) {
        require(machine.mapperRegister(page) == 3 - page, "mapper starts with segments 3, 2, 1, 0");
    }
    machine.writeMemory(0x0000, 0x11);
    machine.writeMemory(0x8000, 0x22);
    require(machine.mapperRam()[3 * vg8235::PageSize] == 0x11 &&
            machine.mapperRam()[1 * vg8235::PageSize] == 0x22,
            "pages write into their mapped segments");

    machine.writeIo(0xFE, 0x05);
    machine.writeMemory(0x8123, 0x77);
    require(machine.mapperRam()[5 * vg8235::PageSize + 0x0123] == 0x77,
            "segment register #FE selects the segment for page 2");
    require(machine.readIo(0xFE) == 0xFD, "mapper read-back sets the unused upper bits");

    machine.writeIo(0xFF, 0x0D);
    machine.writeMemory(0xC000, 0x99);
    require(machine.mapperRam()[5 * vg8235::PageSize] == 0x99,
            "segment numbers wrap at the 8 segments of 128 KiB");
    require(machine.readMemory(0xFFFF) == static_cast<std::uint8_t>(~0xAA),
            "#FFFF stays the secondary register, not mapper RAM");
}

void testCartridgeSlots()
{
    auto machine = loadedMachine();
    require(machine.loadCartridge(1, 0x4000, filled(0x4000, 0xC1)), "slot 1 cartridge loads");
    require(machine.loadCartridge(2, 0x8000, filled(0x4000, 0xC2)), "slot 2 cartridge loads");
    require(!machine.loadCartridge(3, 0x4000, filled(0x10, 0)), "slot 3 takes no cartridge");
    machine.writeIo(0xA8, 0x24);
    require(machine.readMemory(0x4000) == 0xC1 && machine.readMemory(0x8000) == 0xC2,
            "pages 1 and 2 select slots 1 and 2");

    std::vector<std::uint8_t> megaRom(0x20000);
    for (std::size_t offset = 0; offset < megaRom.size(); ++offset) megaRom[offset] = static_cast<std::uint8_t>(offset / 0x4000);
    for (std::size_t offset = 0x100; offset < 0x110; offset += 3) {
        megaRom[offset] = 0x32;
        megaRom[offset + 1] = 0x00;
        megaRom[offset + 2] = 0x70;
    }
    require(machine.insertCartridge(1, megaRom) && machine.cartridgeSlot(1).mapper() == cartridge::Mapper::Ascii16,
            "an ASCII16 MegaROM is detected in slot 1");
    machine.writeIo(0xA8, 0x14);
    machine.writeMemory(0x6000, 3);
    machine.writeMemory(0x7000, 6);
    require(machine.readMemory(0x4000) == 3 && machine.readMemory(0x8000) == 6, "CPU writes switch the cartridge banks");
    machine.reset();
    machine.writeIo(0xAB, 0x82);
    machine.writeIo(0xA8, 0x14);
    require(machine.readMemory(0x4000) == 0, "reset selects the power-on banks");
    machine.ejectCartridge(1);
    require(machine.readMemory(0x4000) == 0xFF, "an empty slot reads FFh");
}

void testEngineMirrors()
{
    auto machine = loadedMachine();
    machine.writeIo(0xAC, 0xC3);
    require(machine.primarySlotRegister() == 0xC3, "PPI port A is mirrored at #AC");
    require(machine.readIo(0xAC) == 0xC3, "PPI port A reads back through the mirror");

    machine.writeIo(0xA4, 0x00);
    machine.writeIo(0xA5, 0x5A);
    machine.writeIo(0xA0, 0x00);
    require(machine.readIo(0xA6) == 0x5A, "PSG ports are mirrored at #A4-#A7");
}

void testVdpPorts()
{
    auto machine = loadedMachine();
    machine.writeIo(0x99, 0x60);
    machine.writeIo(0x99, 0x81);
    require(machine.video().controlRegister(1) == 0x60, "#99 writes VDP registers");

    machine.writeIo(0x99, 0x00);
    machine.writeIo(0x99, 0x40);
    machine.writeIo(0x98, 0xA5);
    require(machine.video().peekVram(0x0000) == 0xA5, "#98 writes VRAM");

    machine.writeIo(0x99, 0x02);
    machine.writeIo(0x99, 0x90);
    machine.writeIo(0x9A, 0x07);
    machine.writeIo(0x9A, 0x00);
    require(machine.video().paletteEntry(2) == 0x007, "#9A writes the palette");

    machine.writeIo(0x99, 0x07);
    machine.writeIo(0x99, 0x91);
    machine.writeIo(0x9B, 0x4E);
    require(machine.video().controlRegister(7) == 0x4E, "#9B writes registers indirectly");

    for (std::uint32_t tick = 0; tick < vg8235::CpuTicksPerPalField && !machine.video().outputs().interruptRequest; ++tick) {
        (void)machine.tick();
    }
    require(machine.video().outputs().interruptRequest, "IE0 raises the VDP interrupt once per field");
    require((machine.readIo(0x99) & 0x80) != 0 && !machine.video().outputs().interruptRequest,
            "reading S#0 through #99 acknowledges the interrupt");
}

void testRealTimeClockRegisters()
{
    auto machine = loadedMachine();
    machine.writeIo(0xB4, 0x0D);
    machine.writeIo(0xB5, 0x02);
    machine.writeIo(0xB4, 0x03);
    machine.writeIo(0xB5, 0x5A);
    require(machine.readIo(0xB5) == 0xFA, "RTC RAM block 2 stores 4-bit values, upper bits read 1");
    machine.writeIo(0xB4, 0x0D);
    machine.writeIo(0xB5, 0x01);
    machine.writeIo(0xB4, 0x00);
    machine.writeIo(0xB5, 0x0F);
    require(machine.readIo(0xB5) == 0xF0, "unimplemented bits of the alarm block read 0");
    machine.reset();
    machine.writeIo(0xB4, 0x0D);
    require(machine.readIo(0xB5) == 0xF8, "reset selects block 0 with the timer enabled");
    machine.writeIo(0xB5, 0x02);
    machine.writeIo(0xB4, 0x03);
    require(machine.readIo(0xB5) == 0xFA, "the RTC RAM survives a reset");
}

std::vector<std::uint8_t> readFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string screenRow(const vg8235::VG8235& machine, unsigned row)
{
    const auto& video = machine.video();
    const std::uint32_t nameTable = static_cast<std::uint32_t>(video.controlRegister(2) & 0x7F) << 10;
    std::string text;
    for (unsigned column = 0; column < 40; ++column) {
        text += static_cast<char>(video.peekVram(nameTable + row * 40 + column));
    }
    return text;
}

// Starts the real BIOS and Sub-ROM without disk ROM and waits for the BASIC prompt.
void testBootToBasic(const std::string& romDirectory)
{
    vg8235::VG8235 machine;
    require(machine.loadMainRom(readFile(romDirectory + "/VG8235-ROM1 - Basic2.0.bin")), "VG 8235 BIOS loads");
    require(machine.loadSubRom(readFile(romDirectory + "/VG8235-ROM2 - Basic2.0.bin")), "VG 8235 Sub-ROM loads");
    machine.reset();
    machine.setClock({26, 10, 10, 6, 19, 20, 0});
    for (std::uint32_t field = 0; field < 300; ++field) {
        for (std::uint32_t tick = 0; tick < vg8235::CpuTicksPerPalField; ++tick) (void)machine.tick();
    }
    const auto& video = machine.video();
    require(video.controlRegister(1) == 0x70 && video.controlRegister(0) == 0x00, "BASIC starts in TEXT 1 with IE0");
    require(screenRow(machine, 0).find("MSX BASIC version 2.0") != std::string::npos, "the BIOS prints the BASIC banner");
    require(screenRow(machine, 2).find("Ok") != std::string::npos, "BASIC reaches the Ok prompt");
    require(machine.printer().output().empty(), "BIOS startup must not produce printer output");
    const rp5c01::DateTime clock = machine.realTimeClock().dateTime();
    require(clock.hour == 19 && clock.minute == 20 && clock.second == 5 && clock.day == 10,
            "a cold start keeps the host time set in 24-hour mode");
    const auto& frame = video.frame();
    const std::uint16_t background = frame[0];
    require(std::any_of(frame.begin(), frame.begin() + 8 * v9938::ScreenWidth,
                        [&](std::uint16_t dot) { return dot != background; }),
            "the banner is rendered into the first character row");
}

// A snapshot of BASIC continues exactly like the original: CPU, mapper RAM, VRAM, clock and the
// module bank registers; it is refused without the module it was taken with, or truncated.
void testSnapshots(const std::string& romDirectory)
{
    std::vector<std::uint8_t> megaRom(0x20000);
    for (std::size_t offset = 0; offset < megaRom.size(); ++offset) megaRom[offset] = static_cast<std::uint8_t>(offset / 0x4000);
    const auto fit = [&](vg8235::VG8235& machine, bool module) {
        require(machine.loadMainRom(readFile(romDirectory + "/VG8235-ROM1 - Basic2.0.bin")), "VG 8235 BIOS loads");
        require(machine.loadSubRom(readFile(romDirectory + "/VG8235-ROM2 - Basic2.0.bin")), "VG 8235 Sub-ROM loads");
        if (module) require(machine.insertCartridge(2, megaRom, cartridge::Mapper::Ascii16), "the MegaROM loads");
        machine.reset();
        machine.setClock({26, 10, 16, 5, 12, 0, 0});
    };
    const auto runFields = [](vg8235::VG8235& machine, unsigned fields) {
        for (unsigned field = 0; field < fields; ++field) {
            for (std::uint32_t tick = 0; tick < vg8235::CpuTicksPerPalField; ++tick) (void)machine.tick();
        }
    };

    vg8235::VG8235 original;
    fit(original, true);
    runFields(original, 300);
    const auto slots = original.primarySlotRegister();
    original.writeIo(0xA8, static_cast<std::uint8_t>((slots & ~0x0C) | 0x08));
    original.writeMemory(0x6000, 5);
    original.writeIo(0xA8, slots);
    require(original.cartridgeSlot(2).bankAt(0x4000) == 10, "the MegaROM switches to bank 5");
    const auto state = original.saveState();
    require(vg8235::VG8235::isState(state), "a snapshot carries its header");

    vg8235::VG8235 copy;
    fit(copy, true);
    require(copy.loadState(state), "a snapshot loads into a machine with the same module");
    require(copy.cartridgeSlot(2).bankAt(0x4000) == 10, "the module bank registers are restored");
    runFields(original, 60);
    runFields(copy, 60);
    require(original.mapperRam() == copy.mapperRam(), "a restored machine continues exactly like the original (RAM)");
    for (std::uint32_t address = 0; address < v9938::VramSize; ++address) {
        require(original.video().peekVram(address) == copy.video().peekVram(address),
                "a restored machine continues exactly like the original (VRAM)");
    }
    const auto clockA = original.realTimeClock().dateTime();
    const auto clockB = copy.realTimeClock().dateTime();
    require(clockA.hour == clockB.hour && clockA.minute == clockB.minute && clockA.second == clockB.second,
            "the clock continues from the snapshot");
    require(screenRow(copy, 2).find("Ok") != std::string::npos, "the restored machine shows BASIC");

    vg8235::VG8235 bare;
    fit(bare, false);
    require(!bare.loadState(state), "a snapshot is refused without the module it was taken with");
    const auto before = copy.saveState();
    auto shortened = state;
    shortened.resize(shortened.size() - 1);
    require(!copy.loadState(shortened) && copy.saveState() == before, "a truncated snapshot leaves the machine unchanged");
    require(!vg8235::VG8235::isState({0x00, 0x01}) && !copy.loadState({}), "data without the header is refused");
}

// With the disk ROM and no disk inserted the machine starts Disk BASIC.
void testBootToDiskBasic(const std::string& romDirectory)
{
    vg8235::VG8235 machine;
    require(machine.loadMainRom(readFile(romDirectory + "/VG8235-ROM1 - Basic2.0.bin")), "VG 8235 BIOS loads");
    require(machine.loadSubRom(readFile(romDirectory + "/VG8235-ROM2 - Basic2.0.bin")), "VG 8235 Sub-ROM loads");
    require(machine.loadDiskRom(readFile(romDirectory + "/VG8235-ROM3 - Disc.rom")), "disk ROM loads");
    machine.reset();
    for (std::uint32_t field = 0; field < 1500; ++field) {
        for (std::uint32_t tick = 0; tick < vg8235::CpuTicksPerPalField; ++tick) (void)machine.tick();
        if (screenRow(machine, 4).find("Ok") != std::string::npos) break;
    }
    require(screenRow(machine, 3).find("Disk BASIC version 1.0") != std::string::npos, "the disk ROM starts Disk BASIC");
    require(screenRow(machine, 4).find("Ok") != std::string::npos, "Disk BASIC reaches the Ok prompt");
    require(machine.printer().output().empty(), "Disk BASIC startup must not produce printer output");
}

void testPrinterPort()
{
    auto machine = loadedMachine();
    require(machine.readIo(0x90) == 0xFD, "printer status reports ready");
    machine.writeIo(0x90, 0x01);
    require(machine.printer().output().empty(), "a startup strobe without data is not printer output");

    machine.writeIo(0x91, 0x41);
    machine.writeIo(0x90, 0x00);
    machine.writeIo(0x90, 0x01);
    machine.writeIo(0x93, 0x42);
    machine.writeIo(0x92, 0x00);
    machine.writeIo(0x96, 0x01);
    const std::vector<std::uint8_t> expected = {0x41, 0x42};
    require(machine.printer().output() == expected,
            "printer port mirrors capture bytes on data writes and rising strobe edges");
    for (unsigned port = 0x90; port <= 0x97; ++port) {
        require(machine.readIo(static_cast<std::uint8_t>(port)) == 0xFD,
                "printer status reports ready on every mirror");
    }
}

void testRealTimeClockRuns()
{
    auto machine = loadedMachine();
    machine.realTimeClock().setDateTime({26, 10, 10, 6, 12, 0, 59});
    for (std::uint32_t tick = 0; tick < vg8235::CpuClockHz; ++tick) (void)machine.tick();
    machine.writeIo(0xB4, 0x0D);
    machine.writeIo(0xB5, 0x08);
    machine.writeIo(0xB4, 0x00);
    const std::uint8_t second = machine.readIo(0xB5);
    machine.writeIo(0xB4, 0x02);
    const std::uint8_t minute = machine.readIo(0xB5);
    require(second == 0xF0 && minute == 0xF1, "the RTC counts seconds from the CPU clock");
}

void testFixedPsgPortDirections()
{
    auto machine = loadedMachine();
    machine.setJoystickPorts(0x3F, 0x2A);
    machine.writeIo(0xA0, 7);
    machine.writeIo(0xA1, 0x00);
    machine.writeIo(0xA0, 15);
    machine.writeIo(0xA1, 0x40);
    machine.writeIo(0xA0, 14);
    require((machine.readIo(0xA2) & 0x3F) == 0x2A, "PSG port B selects joystick 2 even with R7 bit 7 clear");
    machine.writeIo(0xA0, 7);
    machine.writeIo(0xA1, 0xC0);
    machine.writeIo(0xA0, 15);
    machine.writeIo(0xA1, 0x00);
    machine.writeIo(0xA0, 14);
    require((machine.readIo(0xA2) & 0x3F) == 0x3F, "PSG port A reads the joystick even with R7 bit 6 set");
}

void testPhilipsDiskInterface()
{
    auto machine = loadedMachine();
    selectAllPages(machine, 3, 3);
    machine.writeMemory(0x7FF9, 0x27);
    machine.writeMemory(0x7FFA, 0x05);
    require(machine.readMemory(0x7FF9) == 0x27 && machine.readMemory(0x7FFA) == 0x05,
            "#7FF9 and #7FFA are the WD2793 track and sector registers");
    require(machine.readMemory(0xBFF9) == 0x27, "the FDC registers are mirrored at #BFF8-#BFFF");
    machine.writeMemory(0x7FFC, 0x01);
    machine.writeMemory(0x7FFD, 0x80);
    require(machine.diskDrive().side() == 1 && machine.diskDrive().motor(), "#7FFC selects the side, #7FFD bit 7 the motor");
    require(machine.readMemory(0x7FFC) == 0x01 && (machine.readMemory(0x7FFD) & 0x84) == 0x84,
            "the latches read back; bit 2 of #7FFD is set while the disk is unchanged");
    require(machine.diskDrive().insert(std::vector<std::uint8_t>(737280, 0xE5), false), "a 720 KiB image is accepted");
    require((machine.readMemory(0x7FFD) & 0x04) == 0, "bit 2 of #7FFD reports a disk change");
    machine.writeMemory(0x7FF8, 0xD8);
    require((machine.readMemory(0x7FFF) & 0xC0) == 0x80, "#7FFF bit 6 is the inverted INTRQ");
    machine.writeMemory(0x7FF8, 0xD0);
    require(machine.readMemory(0x7FFF) == 0xFF, "#7FFF reads FFh without INTRQ and DRQ");
    selectAllPages(machine, 3, 2);
    machine.writeMemory(0x7FF9, 0x11);
    selectAllPages(machine, 3, 3);
    require(machine.readMemory(0x7FF9) == 0x27, "the FDC answers only in slot 3-3");
}

void testM1WaitStates()
{
    // NOP, NOP, OUT (#A0),A, RLC B, LD IX,#1234, LD B,3, LD C,0, OTIR (3 bytes), HALT
    const std::vector<std::uint8_t> program = {
        0x00, 0x00, 0xD3, 0xA0, 0xCB, 0x00, 0xDD, 0x21, 0x34, 0x12,
        0x06, 0x03, 0x0E, 0x00, 0xED, 0xB3, 0x76};
    auto rom = filled(vg8235::MainRomSize, 0x76);
    std::copy(program.begin(), program.end(), rom.begin());
    vg8235::VG8235 machine;
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

// Board that answers every memory read with NOP and records the cycles it sees; port 40h reads 3Fh, port FEh
// (a Memory Mapper on the board) E5h.
class RecordingBoard final : public usbslot::Transport {
public:
    struct Cycle {
        std::uint8_t slot;
        std::uint16_t address;
        bool m1;
        std::uint8_t value;
    };
    std::vector<Cycle> reads;
    std::vector<Cycle> blocks;
    std::vector<Cycle> writes;
    std::vector<Cycle> ioWrites;
    int ioReads = 0;
    int resets = 0;
    std::uint32_t ioWaits = 0;
    std::deque<std::uint8_t> interrupts;

    bool memRead(std::uint8_t slot, std::uint16_t address, bool m1, std::uint8_t& data, std::uint32_t& waits) override
    {
        reads.push_back({slot, address, m1, 0});
        data = 0x00;
        waits = 0;
        return true;
    }
    bool memReadBlock(std::uint8_t slot, std::uint16_t address, std::uint16_t count, std::vector<std::uint8_t>& data) override
    {
        blocks.push_back({slot, address, false, 0});
        data.assign(count, 0x00);
        return true;
    }
    bool memWrite(std::uint8_t slot, std::uint16_t address, std::uint8_t value) override
    {
        writes.push_back({slot, address, false, value});
        return true;
    }
    bool ioRead(std::uint8_t port, std::uint8_t& data, std::uint32_t& waits) override
    {
        ++ioReads;
        data = port == 0x40 ? 0x3F : (port == 0xFE ? 0xE5 : 0xFF);
        waits = ioWaits;
        return true;
    }
    bool ioWrite(std::uint8_t port, std::uint8_t value, std::uint32_t& waits) override
    {
        ioWrites.push_back({0, port, false, value});
        waits = 0;
        return true;
    }
    bool reset() override
    {
        ++resets;
        return true;
    }
    bool run() override { return true; }
    bool sync(bool& running, std::uint8_t& intLevel) override
    {
        running = true;
        intLevel = 1;
        return true;
    }
    bool poll() override { return true; }
    bool popInterrupt(std::uint8_t& level) override
    {
        if (interrupts.empty()) return false;
        level = interrupts.front();
        interrupts.pop_front();
        return true;
    }
    std::string error() const override { return {}; }
};

// LD A,#82; OUT (#AB),A; LD A,#80; OUT (#A8),A (page 3 = slot 2); LD (#C100),A; OUT (#40),A;
// IN A,(#40); JP #C000
std::vector<std::uint8_t> boardTestRom()
{
    const std::vector<std::uint8_t> program = {
        0x3E, 0x82, 0xD3, 0xAB, 0x3E, 0x80, 0xD3, 0xA8, 0x32, 0x00, 0xC1, 0xD3, 0x40, 0xDB, 0x40,
        0xC3, 0x00, 0xC0};
    auto rom = filled(vg8235::MainRomSize, 0x76);
    std::copy(program.begin(), program.end(), rom.begin());
    return rom;
}

void testUsbSlotBridge()
{
    vg8235::VG8235 machine;
    require(machine.loadMainRom(boardTestRom()), "board test ROM loads");
    auto transport = std::make_unique<RecordingBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    usbslot::SlotConfig config;
    config.rules[0].noCache(0xC000, 0x4000);
    config.ioPorts = {0x40};
    usbslot::BoardSlot boardSlot(bridge, 2, config);
    require(machine.connectBridgeSlot(2, &boardSlot), "slot 2 of the board goes into slot 2");
    require(!machine.connectBridgeSlot(3, &boardSlot), "only slots 1 and 2 take a board slot");
    require(machine.bridgeSlot(2) == 2 && machine.bridgeSlot(1) == 0, "the machine reports the board slot");
    machine.reset();
    require(board->resets == 1, "a machine reset pulses /RESET on the board");

    for (int tick = 0; tick < 2000 && board->reads.size() < 20; ++tick) (void)machine.tick();
    require(board->writes.size() == 1 && board->writes[0].slot == 2 && board->writes[0].address == 0xC100 &&
            board->writes[0].value == 0x80,
            "the CPU write to the board slot reaches the board once");
    require(board->ioWrites.size() == 1 && board->ioWrites[0].address == 0x40 && board->ioReads == 1,
            "a claimed I/O port reaches the board once per I/O cycle");
    require(board->reads.size() == 20 && board->blocks.empty(), "the NOPs run from the uncached board slot");
    for (std::size_t index = 0; index < board->reads.size(); ++index) {
        const auto& read = board->reads[index];
        require(read.slot == 2 && read.address == 0xC000 + index && read.m1,
                "every opcode fetch reaches the board once, with /M1, despite the MSX wait state");
    }
    require(machine.connectBridgeSlot(2, nullptr) && machine.bridgeSlot(2) == 0, "the board slot can be freed");
}

void testUsbSlotCache()
{
    vg8235::VG8235 machine;
    require(machine.loadMainRom(boardTestRom()), "board test ROM loads");
    auto transport = std::make_unique<RecordingBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    usbslot::BoardSlot boardSlot(bridge, 1, usbslot::SlotConfig{});
    require(machine.connectBridgeSlot(2, &boardSlot), "slot 1 of the board goes into slot 2");
    machine.reset();
    for (int tick = 0; tick < 1000; ++tick) (void)machine.tick();
    require(board->blocks.size() == 1 && board->blocks[0].address == 0xC000 && board->reads.empty(),
            "with the cache the NOPs cost one line fetch");
    require(board->ioWrites.empty(), "ports the slot does not claim stay in the machine");
    require(machine.readMemory(0xC010) == 0x00 && machine.readMemory(0xC200) == 0xFF,
            "reads without a bus cycle show the cache");
}

void testUsbSlotSharedMapperPorts()
{
    vg8235::VG8235 machine;
    auto transport = std::make_unique<RecordingBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    usbslot::SlotConfig config;
    config.ioPorts = {0xFC, 0xFD, 0xFE, 0xFF};
    usbslot::BoardSlot boardSlot(bridge, 2, config);
    require(machine.connectBridgeSlot(1, &boardSlot), "board slot in slot 1");
    machine.writeIo(0xFE, 0x05);
    require(machine.mapperRegister(2) == 0x05 && board->ioWrites.size() == 1 && board->ioWrites[0].address == 0xFE &&
            board->ioWrites[0].value == 0x05,
            "a mapper port write reaches the internal mapper and the board");
    require(machine.readIo(0xFE) == ((0x05 | 0xF8) & 0xE5), "both mappers drive the bus: the CPU reads the AND");
}

// T-states from the opcode fetch at one address to the opcode fetch at another.
std::uint64_t ticksBetweenFetches(vg8235::VG8235& machine, std::uint16_t from, std::uint16_t to)
{
    std::uint64_t start = 0;
    bool started = false;
    bool previousFetch = false;
    for (std::uint64_t tick = 0; tick < 1000; ++tick) {
        const auto pins = machine.tick();
        const bool fetch = pins.m1 && pins.memoryRequest && pins.read;
        if (fetch && !previousFetch) {
            if (!started && pins.address == from) {
                start = tick;
                started = true;
            } else if (started && pins.address == to) {
                return tick - start;
            }
        }
        previousFetch = fetch;
    }
    return 0;
}

void testUsbSlotWaitAndInterrupt()
{
    // IN A,(#40); NOP; IM 1; EI; HALT; ... #38: HALT
    auto rom = filled(vg8235::MainRomSize, 0x00);
    const std::vector<std::uint8_t> program = {0xDB, 0x40, 0x00, 0xED, 0x56, 0xFB, 0x76};
    std::copy(program.begin(), program.end(), rom.begin());
    rom[0x38] = 0x76;
    vg8235::VG8235 machine;
    require(machine.loadMainRom(rom), "wait test ROM loads");
    auto transport = std::make_unique<RecordingBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    usbslot::SlotConfig config;
    config.ioPorts = {0x40};
    usbslot::BoardSlot boardSlot(bridge, 1, config);
    require(machine.connectBridgeSlot(1, &boardSlot), "board slot connected");
    board->ioWaits = 10;
    machine.reset();
    // IN A,(n): 11 T-states plus the M1 wait state of the machine, plus the 10 wait clocks of the cartridge
    require(ticksBetweenFetches(machine, 0x0000, 0x0002) == 22, "/WAIT of the cartridge lengthens the I/O cycle");

    bool interrupted = false;
    for (int tick = 0; tick < 200; ++tick) {
        const auto pins = machine.tick();
        if (pins.m1 && pins.memoryRequest && pins.read && pins.address == 0x0038) interrupted = true;
    }
    require(!interrupted, "no interrupt while /INT of the board is inactive");
    board->interrupts = {0};
    bridge.heartbeat();
    for (int tick = 0; tick < 200 && !interrupted; ++tick) {
        const auto pins = machine.tick();
        if (pins.m1 && pins.memoryRequest && pins.read && pins.address == 0x0038) interrupted = true;
    }
    require(interrupted, "/INT of the board interrupts the CPU");
}

} // namespace

// The same key position gives the character of the PC layout: Shift+2 is " on a German keyboard and @ on a
// US keyboard; the key left of X is Y on a German keyboard and Z on a US keyboard.
void testKeyboardLayouts()
{
    vg8235::KeyboardPosition position{};
    bool shift = false;
    require(vg8235::mapGermanScanCodeToMsx(0x03, false, true, false, position, shift) &&
            position.row == 2 && position.bit == 0 && shift,
            "German Shift+2 gives the double quote (MSX Shift+quote)");
    require(vg8235::mapUsScanCodeToMsx(0x03, false, true, false, position, shift) &&
            position.row == 0 && position.bit == 2 && shift,
            "US Shift+2 gives the at sign (MSX Shift+2)");
    require(vg8235::mapGermanScanCodeToMsx(0x2C, false, false, false, position, shift) &&
            position.row == 5 && position.bit == 6 && !shift,
            "German key left of X gives Y");
    require(vg8235::mapUsScanCodeToMsx(0x2C, false, false, false, position, shift) &&
            position.row == 5 && position.bit == 7 && !shift,
            "US key left of X gives Z");
    require(vg8235::mapUsScanCodeToMsx(0x27, false, true, false, position, shift) &&
            position.row == 1 && position.bit == 7 && shift,
            "US Shift+semicolon gives the colon");
    require(!vg8235::mapUsScanCodeToMsx(0x10, false, false, true, position, shift),
            "the right Alt key adds no character on the US layout");
}

int main(int argc, char** argv)
{
    testRomSizes();
    testSlotZeroBios();
    testSecondarySlotRegister();
    testSubRomAndDiskRomVisibility();
    testMemoryMapper();
    testCartridgeSlots();
    testEngineMirrors();
    testVdpPorts();
    testRealTimeClockRegisters();
    testPrinterPort();
    testRealTimeClockRuns();
    testFixedPsgPortDirections();
    testPhilipsDiskInterface();
    testM1WaitStates();
    testUsbSlotBridge();
    testUsbSlotCache();
    testUsbSlotSharedMapperPorts();
    testUsbSlotWaitAndInterrupt();
    testKeyboardLayouts();
    if (argc > 1) {
        testBootToBasic(argv[1]);
        testBootToDiskBasic(argv[1]);
        testSnapshots(argv[1]);
    } else {
        std::cout << "Boot tests skipped: no ROM folder given.\n";
    }
    std::cout << "All VG8235 tests passed.\n";
}
