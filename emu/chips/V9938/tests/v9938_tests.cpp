#include "V9938/V9938.hpp"

#include <cstdint>
#include <initializer_list>
#include <cstdlib>
#include <iostream>

namespace {

constexpr std::uint8_t VramPort = 0;
constexpr std::uint8_t ControlPort = 1;
constexpr std::uint8_t PalettePort = 2;
constexpr std::uint8_t IndirectPort = 3;
constexpr std::uint32_t Line = v9938::MasterTicksPerScanline;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void setRegister(v9938::V9938& vdp, std::uint8_t index, std::uint8_t value)
{
    vdp.write(ControlPort, value);
    vdp.write(ControlPort, static_cast<std::uint8_t>(0x80 | index));
}

void setAddress(v9938::V9938& vdp, std::uint32_t address, bool write)
{
    setRegister(vdp, 14, static_cast<std::uint8_t>(address >> 14));
    vdp.write(ControlPort, static_cast<std::uint8_t>(address));
    vdp.write(ControlPort, static_cast<std::uint8_t>(((address >> 8) & 0x3F) | (write ? 0x40 : 0x00)));
}

void run(v9938::V9938& vdp, std::uint32_t ticks)
{
    for (std::uint32_t tick = 0; tick < ticks; ++tick) vdp.advanceMasterTick();
}

void runToFrameStart(v9938::V9938& vdp)
{
    do {
        vdp.advanceMasterTick();
    } while (vdp.frameTick() != 0);
}

// Returns the frame tick at which the interrupt line rises, or 0xFFFFFFFF within one frame.
std::uint32_t ticksUntilInterrupt(v9938::V9938& vdp)
{
    for (std::uint32_t tick = 0; tick < v9938::PalScanlines * Line; ++tick) {
        vdp.advanceMasterTick();
        if (vdp.outputs().interruptRequest) return vdp.frameTick();
    }
    return 0xFFFFFFFF;
}

void testResetState()
{
    v9938::V9938 vdp;
    require(v9938::MasterTicksPerScanline == 1368, "a scanline lasts 1368 master clocks");
    require(!vdp.palTiming() && vdp.linesPerFrame() == 262, "the V9938 starts with NTSC timing");
    require(vdp.controlRegister(21) == 0x3F && vdp.controlRegister(22) == 0x05, "colour burst registers are preset");
    require(vdp.paletteEntry(0) == 0x000 && vdp.paletteEntry(2) == 0x611 && vdp.paletteEntry(13) == 0x265
                && vdp.paletteEntry(15) == 0x777,
            "reset palette follows appendix 8");
    require(vdp.peekStatus(1) == 0x00, "S#1 identifies a V9938");
    require(vdp.peekStatus(2) == 0x4C || vdp.peekStatus(2) == 0x6C, "S#2 has bits 3 and 2 set and VR at frame start");

    vdp.pokeVram(0x1FFFF, 0xA5);
    vdp.pokeVram(0x00000, 0x5A);
    require(vdp.peekVram(0x1FFFF) == 0xA5 && vdp.peekVram(0x20000) == 0x5A, "128 KiB VRAM wraps at 17 bits");
    run(vdp, 2 * v9938::PalScanlines * Line);
    require(!vdp.outputs().interruptRequest, "no interrupt is raised while interrupts are disabled");
    vdp.reset();
    require(vdp.peekVram(0x1FFFF) == 0x00, "reset clears VRAM");
}

void testRegisterWrites()
{
    v9938::V9938 vdp;
    setRegister(vdp, 7, 0x55);
    require(vdp.controlRegister(7) == 0x55, "port 1 data then 80h|reg writes a register");
    setRegister(vdp, 1, 0xFF);
    require(vdp.controlRegister(1) == 0x7B, "only existing bits of R#1 are stored");
    setRegister(vdp, 36, 0x12);
    setRegister(vdp, 37, 0xFF);
    require(vdp.controlRegister(36) == 0x12 && vdp.controlRegister(37) == 0x01, "command registers are written directly");
    setRegister(vdp, 24, 0xFF);
    require(vdp.controlRegister(24) == 0x00, "R#24-R#31 do not exist");

    vdp.write(ControlPort, 0x99);
    vdp.write(ControlPort, 0xC7);
    require(vdp.controlRegister(7) == 0x55, "a second byte with bits 7 and 6 set writes no register");

    vdp.write(ControlPort, 0x33);
    (void)vdp.read(ControlPort);
    setRegister(vdp, 7, 0x44);
    require(vdp.controlRegister(7) == 0x44, "a port read resets the half-written port 1 sequence");
}

void testVramAccess()
{
    v9938::V9938 vdp;
    setAddress(vdp, 0x04123, true);
    vdp.write(VramPort, 0xAA);
    vdp.write(VramPort, 0xBB);
    require(vdp.peekVram(0x04123) == 0xAA && vdp.peekVram(0x04124) == 0xBB, "R#14 supplies address bits 16-14");

    setAddress(vdp, 0x04123, false);
    require(vdp.read(VramPort) == 0xAA && vdp.read(VramPort) == 0xBB, "a read address set-up prefetches the first byte");

    vdp.write(VramPort, 0x77);
    require(vdp.read(VramPort) == 0x77, "the read-ahead latch is shared with VRAM writes");

    setAddress(vdp, 0x03FFF, true);
    vdp.write(VramPort, 0x11);
    vdp.write(VramPort, 0x22);
    require(vdp.peekVram(0x00000) == 0x22 && vdp.controlRegister(14) == 0,
            "GRAPHIC 1 wraps the address counter within 16 KiB");

    setRegister(vdp, 0, 0x06);
    setAddress(vdp, 0x1BFFF, true);
    vdp.write(VramPort, 0x33);
    vdp.write(VramPort, 0x44);
    require(vdp.peekVram(0x1BFFF) == 0x33 && vdp.peekVram(0x1C000) == 0x44 && vdp.controlRegister(14) == 7,
            "GRAPHIC 4 carries from A13 into R#14");

    setRegister(vdp, 45, 0x40);
    setAddress(vdp, 0x00100, true);
    vdp.write(VramPort, 0x99);
    require(vdp.peekVram(0x00100) == 0x00, "writes to the missing expansion RAM are lost");
    setAddress(vdp, 0x00100, false);
    require(vdp.read(VramPort) == 0xFF, "reads from the missing expansion RAM return FFh");
}

void testPalette()
{
    v9938::V9938 vdp;
    setRegister(vdp, 16, 3);
    vdp.write(PalettePort, 0x52);
    vdp.write(PalettePort, 0x06);
    vdp.write(PalettePort, 0xFF);
    vdp.write(PalettePort, 0xFF);
    require(vdp.paletteEntry(3) == 0x652 && vdp.paletteEntry(4) == 0x777, "two bytes per entry, 0RRR0BBB then 00000GGG");
    require(vdp.controlRegister(16) == 5, "R#16 increments after the second byte");

    vdp.write(PalettePort, 0x70);
    setRegister(vdp, 16, 15);
    vdp.write(PalettePort, 0x07);
    vdp.write(PalettePort, 0x00);
    require(vdp.paletteEntry(15) == 0x007 && vdp.controlRegister(16) == 0, "writing R#16 restarts the byte pair");
}

void testIndirectAccess()
{
    v9938::V9938 vdp;
    setRegister(vdp, 17, 32);
    vdp.write(IndirectPort, 0x10);
    vdp.write(IndirectPort, 0xFF);
    vdp.write(IndirectPort, 0x20);
    require(vdp.controlRegister(32) == 0x10 && vdp.controlRegister(33) == 0x01 && vdp.controlRegister(34) == 0x20,
            "port 3 writes consecutive registers");
    require(vdp.controlRegister(17) == 35, "R#17 auto-increments");

    setRegister(vdp, 17, 0x87);
    vdp.write(IndirectPort, 0x12);
    vdp.write(IndirectPort, 0x34);
    require(vdp.controlRegister(7) == 0x34 && vdp.controlRegister(17) == 0x87, "AII keeps the register pointer");

    setRegister(vdp, 17, 0x91);
    vdp.write(IndirectPort, 0x05);
    require(vdp.controlRegister(17) == 0x91, "R#17 cannot be written indirectly");
}

void testFrameTiming()
{
    v9938::V9938 vdp;
    runToFrameStart(vdp);
    std::uint32_t ticks = 0;
    do {
        vdp.advanceMasterTick();
        ++ticks;
    } while (vdp.frameTick() != 0);
    require(ticks == 262 * Line, "an NTSC field lasts 262 lines");

    setRegister(vdp, 9, 0x02);
    require(!vdp.palTiming(), "PAL takes effect at the next field");
    runToFrameStart(vdp);
    require(vdp.palTiming() && vdp.linesPerFrame() == 313, "R#9 NT selects 313 lines");
    ticks = 0;
    do {
        vdp.advanceMasterTick();
        ++ticks;
    } while (vdp.frameTick() != 0);
    require(ticks == 313 * Line, "a PAL field lasts 313 lines");

    const std::uint8_t field = vdp.peekStatus(2) & 0x02;
    runToFrameStart(vdp);
    require((vdp.peekStatus(2) & 0x02) != field, "S#2 EO toggles every field");
}

void testVerticalInterrupt()
{
    v9938::V9938 vdp;
    setRegister(vdp, 1, 0x60);
    runToFrameStart(vdp);
    (void)vdp.read(ControlPort);
    require(ticksUntilInterrupt(vdp) == (16 + 9 + 10 + 7) * Line + 202 + 192 * Line,
            "NTSC 192 lines: IRQ at the end of the display area");
    setRegister(vdp, 15, 0);
    require((vdp.read(ControlPort) & 0x80) != 0, "S#0 F is set at the end of the display area");
    require(!vdp.outputs().interruptRequest && (vdp.peekStatus(0) & 0x80) == 0, "reading S#0 clears F and the IRQ");

    setRegister(vdp, 9, 0x82);
    runToFrameStart(vdp);
    (void)vdp.read(ControlPort);
    require(ticksUntilInterrupt(vdp) == (16 + 36 + 0 + 7) * Line + 202 + 212 * Line,
            "PAL 212 lines: IRQ at the end of the display area");
    (void)vdp.read(ControlPort);

    setRegister(vdp, 1, 0x40);
    runToFrameStart(vdp);
    run(vdp, 300 * Line);
    require(!vdp.outputs().interruptRequest && (vdp.peekStatus(0) & 0x80) != 0, "F is set without IE0");
    setRegister(vdp, 1, 0x60);
    require(vdp.outputs().interruptRequest, "enabling IE0 with F set raises the IRQ");
}

void testLineInterrupt()
{
    v9938::V9938 vdp;
    const std::uint32_t displayStart = (16 + 9 + 10 + 7) * Line + 202;
    const std::uint32_t rightBorder = 202 + 56 + 1024;
    setRegister(vdp, 19, 100);
    setRegister(vdp, 0, 0x10);
    setRegister(vdp, 15, 1);
    runToFrameStart(vdp);
    (void)vdp.read(ControlPort);
    require(ticksUntilInterrupt(vdp) == displayStart - 202 + 100 * Line + rightBorder, "IRQ at the right border of line R#19");
    require((vdp.read(ControlPort) & 0x01) != 0, "S#1 FH is set");
    require(!vdp.outputs().interruptRequest && (vdp.peekStatus(1) & 0x01) == 0, "reading S#1 clears FH");

    setRegister(vdp, 23, 10);
    runToFrameStart(vdp);
    require(ticksUntilInterrupt(vdp) == displayStart - 202 + 90 * Line + rightBorder, "R#23 scroll shifts the match line");
    (void)vdp.read(ControlPort);

    setRegister(vdp, 0, 0x00);
    runToFrameStart(vdp);
    run(vdp, displayStart - 202 + 90 * Line + rightBorder);
    require((vdp.peekStatus(1) & 0x01) != 0 && !vdp.outputs().interruptRequest, "without IE1 FH only follows the match");
    run(vdp, Line);
    require((vdp.peekStatus(1) & 0x01) == 0, "without IE1 FH drops again");
}

void testRetraceFlags()
{
    v9938::V9938 vdp;
    runToFrameStart(vdp);
    require((vdp.peekStatus(2) & 0x40) != 0, "VR is set in the top border");
    const std::uint32_t displayStart = vdp.displayStartTick();
    run(vdp, displayStart + 100 * Line + 400);
    require((vdp.peekStatus(2) & 0x60) == 0, "inside the display area VR and HR are clear");
    run(vdp, 1282 - 400 - 202 + 10);
    require((vdp.peekStatus(2) & 0x20) != 0, "HR is set after the right border starts");
    run(vdp, 110 * Line);
    require((vdp.peekStatus(2) & 0x40) != 0, "VR is set below the display area");
}

// Renders one complete field with the current settings.
void renderField(v9938::V9938& vdp)
{
    runToFrameStart(vdp);
    runToFrameStart(vdp);
}

std::uint16_t pixel(const v9938::V9938& vdp, unsigned x, unsigned y)
{
    return vdp.frame()[y * v9938::ScreenWidth + x * 2];
}

std::uint16_t hiresPixel(const v9938::V9938& vdp, unsigned x, unsigned y)
{
    return vdp.frame()[y * v9938::ScreenWidth + x];
}

void testText1()
{
    v9938::V9938 vdp;
    setRegister(vdp, 1, 0x50);
    setRegister(vdp, 2, 0x00);
    setRegister(vdp, 4, 0x01);
    setRegister(vdp, 7, 0xF4);
    vdp.pokeVram(0x0000, 0x41);
    vdp.pokeVram(0x0800 + 0x41 * 8, 0x84);
    renderField(vdp);
    require(pixel(vdp, 8, 0) == 0x117, "TEXT 1 shows the backdrop left of the 40 columns");
    require(pixel(vdp, 9, 0) == 0x777 && pixel(vdp, 10, 0) == 0x117 && pixel(vdp, 14, 0) == 0x777,
            "TEXT 1 draws 6-dot characters in the R#7 colours from dot 9");
    require(pixel(vdp, 9, 1) == 0x117, "TEXT 1 uses the pattern line of the display line");
}

void testText2Blink()
{
    v9938::V9938 vdp;
    setRegister(vdp, 0, 0x04);
    setRegister(vdp, 1, 0x50);
    setRegister(vdp, 2, 0x03);
    setRegister(vdp, 3, 0x2F);
    setRegister(vdp, 4, 0x01);
    setRegister(vdp, 7, 0xF4);
    setRegister(vdp, 12, 0xA6);
    setRegister(vdp, 13, 0x11);
    vdp.pokeVram(0x0000, 0x41);
    vdp.pokeVram(0x0001, 0x41);
    vdp.pokeVram(0x0800 + 0x41 * 8, 0x80);
    vdp.pokeVram(0x0A00, 0x80);
    renderField(vdp);
    require(hiresPixel(vdp, 18, 0) == vdp.paletteEntry(10) && hiresPixel(vdp, 19, 0) == vdp.paletteEntry(6),
            "TEXT 2 blink attributes select the R#12 colours while blinking is on");
    require(hiresPixel(vdp, 24, 0) == 0x777, "TEXT 2 draws 80 columns of 6 dots on the 512-dot grid");
    for (int field = 0; field < 10; ++field) runToFrameStart(vdp);
    require(hiresPixel(vdp, 18, 0) == 0x777, "R#13 switches back to the R#7 colours after the ON period");
}

void testGraphic1AndScroll()
{
    v9938::V9938 vdp;
    setRegister(vdp, 1, 0x40);
    setRegister(vdp, 2, 0x06);
    setRegister(vdp, 3, 0x80);
    setRegister(vdp, 4, 0x00);
    setRegister(vdp, 7, 0x01);
    vdp.pokeVram(0x1800, 0x01);
    vdp.pokeVram(0x1801, 0x08);
    vdp.pokeVram(0x1800 + 32, 0x09);
    vdp.pokeVram(0x0008, 0xF0);
    vdp.pokeVram(0x0048, 0x0F);
    vdp.pokeVram(0x2000, 0x2A);
    vdp.pokeVram(0x2001, 0x30);
    renderField(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(2) && pixel(vdp, 4, 0) == vdp.paletteEntry(10),
            "GRAPHIC 1 takes the colours of a group of eight characters from the colour table");
    require(pixel(vdp, 8, 0) == vdp.paletteEntry(1), "colour 0 shows the backdrop");
    setRegister(vdp, 8, 0x28);
    renderField(vdp);
    require(pixel(vdp, 8, 0) == vdp.paletteEntry(0), "R#8 TP makes colour 0 a palette colour");
    setRegister(vdp, 23, 8);
    renderField(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(0) && pixel(vdp, 4, 0) == vdp.paletteEntry(3),
            "R#23 scrolls the character rows");
    setRegister(vdp, 1, 0x00);
    renderField(vdp);
    require(pixel(vdp, 4, 0) == vdp.paletteEntry(1), "a blanked screen shows only the backdrop");
}

void testGraphic2AndMulticolour()
{
    v9938::V9938 vdp;
    setRegister(vdp, 0, 0x02);
    setRegister(vdp, 1, 0x40);
    setRegister(vdp, 2, 0x06);
    setRegister(vdp, 3, 0xFF);
    setRegister(vdp, 4, 0x03);
    setRegister(vdp, 8, 0x08);
    vdp.pokeVram(0x1800 + 8 * 32, 0x05);
    vdp.pokeVram(0x0800 + 5 * 8, 0xFF);
    vdp.pokeVram(0x2800 + 5 * 8, 0x30);
    renderField(vdp);
    require(pixel(vdp, 0, 64) == vdp.paletteEntry(3), "GRAPHIC 2 uses the pattern and colour tables of the second third");

    setRegister(vdp, 0, 0x00);
    setRegister(vdp, 1, 0x48);
    setRegister(vdp, 4, 0x00);
    vdp.pokeVram(0x1800, 0x02);
    vdp.pokeVram(2 * 8 + 1, 0x9C);
    renderField(vdp);
    require(pixel(vdp, 0, 4) == vdp.paletteEntry(9) && pixel(vdp, 4, 4) == vdp.paletteEntry(12),
            "MULTICOLOR shows two 4 x 4 blocks per pattern byte");
}

void setupSpriteScreen(v9938::V9938& vdp, std::uint8_t r0, std::uint8_t r5)
{
    setRegister(vdp, 0, r0);
    setRegister(vdp, 1, 0x40);
    setRegister(vdp, 2, 0x06);
    setRegister(vdp, 5, r5);
    setRegister(vdp, 6, 0x07);
    setRegister(vdp, 7, 0x01);
    setRegister(vdp, 8, 0x08);
    setRegister(vdp, 15, 0);
}

// Clears F at the start of the field so that the fifth/ninth sprite number can latch.
void renderFieldAcknowledged(v9938::V9938& vdp)
{
    runToFrameStart(vdp);
    (void)vdp.read(ControlPort);
    runToFrameStart(vdp);
}

void testSpriteMode1()
{
    v9938::V9938 vdp;
    setupSpriteScreen(vdp, 0x00, 0x36);
    for (unsigned row = 0; row < 8; ++row) vdp.pokeVram(0x3800 + row, 0x80);
    const std::uint8_t sprites[][4] = {
        {9, 20, 0, 0x0F}, {9, 20, 0, 0x02}, {9, 40, 0, 0x8E}, {9, 60, 0, 0x03}, {9, 80, 0, 0x04}, {208, 0, 0, 0}};
    for (unsigned sprite = 0; sprite < 6; ++sprite) {
        for (unsigned byte = 0; byte < 4; ++byte) vdp.pokeVram(0x1B00 + sprite * 4 + byte, sprites[sprite][byte]);
    }
    renderFieldAcknowledged(vdp);
    require(pixel(vdp, 20, 10) == vdp.paletteEntry(15) && pixel(vdp, 20, 9) == vdp.paletteEntry(1),
            "sprites appear one line below their Y coordinate; lower numbers win");
    require(pixel(vdp, 8, 10) == vdp.paletteEntry(14), "EC shifts a sprite 32 dots to the left");
    require(pixel(vdp, 60, 10) == vdp.paletteEntry(3) && pixel(vdp, 80, 10) == vdp.paletteEntry(1),
            "sprite mode 1 shows four sprites per line");
    const std::uint8_t status = vdp.peekStatus(0);
    require((status & 0x40) != 0 && (status & 0x1F) == 4, "S#0 reports the fifth sprite");
    require((status & 0x20) != 0 && vdp.peekStatus(3) == 32 && vdp.peekStatus(5) == 17,
            "S#0 C and S#3/S#5 report the collision point");
    (void)vdp.read(ControlPort);
    require((vdp.peekStatus(0) & 0x60) == 0, "reading S#0 clears 5S and C");
}

void testSpriteMode2()
{
    v9938::V9938 vdp;
    setupSpriteScreen(vdp, 0x04, 0x3F);
    for (unsigned row = 0; row < 8; ++row) {
        vdp.pokeVram(0x3800 + row, 0xC0);
        vdp.pokeVram(0x3808 + row, 0x40);
    }
    const std::uint8_t sprites[][4] = {{9, 20, 0, 0}, {9, 20, 1, 0}, {216, 0, 0, 0}};
    for (unsigned sprite = 0; sprite < 3; ++sprite) {
        for (unsigned byte = 0; byte < 4; ++byte) vdp.pokeVram(0x1E00 + sprite * 4 + byte, sprites[sprite][byte]);
    }
    for (unsigned row = 0; row < 16; ++row) {
        vdp.pokeVram(0x1C00 + row, row == 0 ? 0x05 : 0x06);
        vdp.pokeVram(0x1C10 + row, 0x42);
    }
    renderFieldAcknowledged(vdp);
    require(pixel(vdp, 20, 10) == vdp.paletteEntry(5) && pixel(vdp, 20, 11) == vdp.paletteEntry(6),
            "sprite mode 2 takes a colour per sprite line");
    require(pixel(vdp, 21, 10) == vdp.paletteEntry(7), "CC ORs the colour into the preceding sprite");
    require((vdp.peekStatus(0) & 0x20) == 0, "sprites with CC do not collide");
    require((vdp.peekStatus(0) & 0x1F) == 2, "Y = 216 ends the attribute table in sprite mode 2");

    for (unsigned sprite = 0; sprite < 10; ++sprite) {
        vdp.pokeVram(0x1E00 + sprite * 4, 50);
        vdp.pokeVram(0x1E00 + sprite * 4 + 1, static_cast<std::uint8_t>(sprite * 16));
        vdp.pokeVram(0x1E00 + sprite * 4 + 2, 0);
        for (unsigned row = 0; row < 16; ++row) vdp.pokeVram(0x1C00 + sprite * 16 + row, 0x0F);
    }
    vdp.pokeVram(0x1E00 + 10 * 4, 216);
    renderFieldAcknowledged(vdp);
    require(pixel(vdp, 7 * 16, 51) == vdp.paletteEntry(15) && pixel(vdp, 8 * 16, 51) == vdp.paletteEntry(1),
            "sprite mode 2 shows eight sprites per line");
    require((vdp.peekStatus(0) & 0x5F) == 0x48, "S#0 reports the ninth sprite");
}

void writeVram(v9938::V9938& vdp, std::uint32_t address, std::initializer_list<std::uint8_t> bytes)
{
    setAddress(vdp, address, true);
    for (const std::uint8_t value : bytes) vdp.write(VramPort, value);
}

void setupBitmap(v9938::V9938& vdp, std::uint8_t r0, std::uint8_t r2)
{
    setRegister(vdp, 0, r0);
    setRegister(vdp, 1, 0x40);
    setRegister(vdp, 2, r2);
    setRegister(vdp, 7, 0x01);
    setRegister(vdp, 8, 0x0A);
}

void testGraphic4()
{
    v9938::V9938 vdp;
    setupBitmap(vdp, 0x06, 0x1F);
    vdp.pokeVram(0x0000, 0x2A);
    vdp.pokeVram(0x0001, 0x05);
    vdp.pokeVram(0x0080, 0x33);
    vdp.pokeVram(0x8000, 0x44);
    renderField(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(2) && pixel(vdp, 1, 0) == vdp.paletteEntry(10),
            "GRAPHIC 4 holds two dots per byte, high nibble first");
    require(pixel(vdp, 2, 0) == vdp.paletteEntry(1), "GRAPHIC 4 colour 0 shows the backdrop");
    require(pixel(vdp, 0, 1) == vdp.paletteEntry(3), "GRAPHIC 4 lines are 128 bytes apart");
    setRegister(vdp, 23, 1);
    renderField(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(3), "R#23 scrolls bitmap lines");
    setRegister(vdp, 23, 0);
    setRegister(vdp, 2, 0x3F);
    renderField(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(4), "R#2 A15 selects page 1");

    setRegister(vdp, 9, 0x8E);
    renderField(vdp);
    require(vdp.interlaced(), "R#9 IL is latched for the field");
    require(vdp.frameDisplayLines() == 212, "LN renders 212 lines");
    const bool renderedSecond = !vdp.secondField();
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(renderedSecond ? 4 : 2), "EO shows the odd page in the second field");
    runToFrameStart(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(renderedSecond ? 2 : 4), "EO shows the even page in the first field");

    setRegister(vdp, 9, 0x02);
    setRegister(vdp, 13, 0x11);
    renderField(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(2), "R#13 shows the even page during the ON period");
    for (int field = 0; field < 10; ++field) runToFrameStart(vdp);
    require(pixel(vdp, 0, 0) == vdp.paletteEntry(4), "R#13 shows the odd page during the OFF period");
}

void testGraphic5()
{
    v9938::V9938 vdp;
    setupBitmap(vdp, 0x08, 0x1F);
    setRegister(vdp, 7, 0x06);
    vdp.pokeVram(0x0000, 0xC0);
    renderField(vdp);
    require(hiresPixel(vdp, 0, 0) == vdp.paletteEntry(3), "GRAPHIC 5 holds four dots per byte");
    require(hiresPixel(vdp, 1, 0) == vdp.paletteEntry(2) && hiresPixel(vdp, 2, 0) == vdp.paletteEntry(1),
            "GRAPHIC 5 colour 0 shows the backdrop of even and odd dots");
    const auto border = vdp.borderRaster()[0];
    require(border.even == vdp.paletteEntry(1) && border.odd == vdp.paletteEntry(2),
            "the GRAPHIC 5 border uses R#7 bits 3-2 for even and 1-0 for odd dots");

    setRegister(vdp, 8, 0x08);
    setRegister(vdp, 5, 0xEF);
    setRegister(vdp, 6, 0x0F);
    vdp.pokeVram(0x7600, 9);
    vdp.pokeVram(0x7601, 10);
    vdp.pokeVram(0x7604, 216);
    vdp.pokeVram(0x7400, 0x0D);
    vdp.pokeVram(0x7800, 0x80);
    renderField(vdp);
    require(hiresPixel(vdp, 20, 10) == vdp.paletteEntry(3) && hiresPixel(vdp, 21, 10) == vdp.paletteEntry(1),
            "GRAPHIC 5 sprites colour even dots with bits 3-2 and odd dots with bits 1-0");
}

void testGraphic6()
{
    v9938::V9938 vdp;
    setupBitmap(vdp, 0x0A, 0x1F);
    writeVram(vdp, 0x00000, {0x12, 0x34});
    require(vdp.peekVram(0x00000) == 0x12 && vdp.peekVram(0x10000) == 0x34,
            "GRAPHIC 6 interleaves CPU addresses over both 64 KiB banks");
    setAddress(vdp, 0x00000, false);
    require(vdp.read(VramPort) == 0x12 && vdp.read(VramPort) == 0x34, "GRAPHIC 6 reads interleaved addresses");
    writeVram(vdp, 0x10000, {0x56});
    require(vdp.peekVram(0x08000) == 0x56, "GRAPHIC 6 page 1 starts in the middle of both banks");
    renderField(vdp);
    require(hiresPixel(vdp, 0, 0) == vdp.paletteEntry(1) && hiresPixel(vdp, 1, 0) == vdp.paletteEntry(2)
                && hiresPixel(vdp, 2, 0) == vdp.paletteEntry(3) && hiresPixel(vdp, 3, 0) == vdp.paletteEntry(4),
            "GRAPHIC 6 holds two of 512 dots per byte");
    setRegister(vdp, 2, 0x3F);
    renderField(vdp);
    require(hiresPixel(vdp, 0, 0) == vdp.paletteEntry(5), "R#2 A16 selects GRAPHIC 6 page 1");
}

void testGraphic7()
{
    v9938::V9938 vdp;
    setupBitmap(vdp, 0x0E, 0x1F);
    setRegister(vdp, 7, 0xE0);
    writeVram(vdp, 0x00000, {0xFF, 0x1C, 0x02, 0x00});
    setRegister(vdp, 8, 0x08);
    setRegister(vdp, 5, 0xF7);
    setRegister(vdp, 11, 0x01);
    setRegister(vdp, 6, 0x1E);
    writeVram(vdp, 0x0FA00, {9, 40, 0, 0, 216});
    writeVram(vdp, 0x0F800, {0x08});
    writeVram(vdp, 0x0F000, {0x80});
    renderField(vdp);
    require(pixel(vdp, 0, 0) == 0x777 && pixel(vdp, 1, 0) == 0x070 && pixel(vdp, 2, 0) == 0x005,
            "GRAPHIC 7 shows GGGRRRBB colours without the palette");
    require(pixel(vdp, 3, 0) == 0x000, "GRAPHIC 7 colour 0 is black, not the backdrop");
    require(vdp.borderRaster()[0].even == 0x700, "R#7 is a GGGRRRBB border colour in GRAPHIC 7");
    require(pixel(vdp, 40, 10) == 0x472, "GRAPHIC 7 sprites use the fixed sprite colours");
}

struct Command {
    unsigned sx = 0;
    unsigned sy = 0;
    unsigned dx = 0;
    unsigned dy = 0;
    unsigned nx = 0;
    unsigned ny = 0;
    std::uint8_t colour = 0;
    std::uint8_t argument = 0;
    std::uint8_t code = 0;
};

// Loads R#32-R#46 through the indirect port; the write to R#46 starts the command.
void issue(v9938::V9938& vdp, const Command& command)
{
    setRegister(vdp, 17, 32);
    const std::uint8_t bytes[] = {
        static_cast<std::uint8_t>(command.sx), static_cast<std::uint8_t>(command.sx >> 8),
        static_cast<std::uint8_t>(command.sy), static_cast<std::uint8_t>(command.sy >> 8),
        static_cast<std::uint8_t>(command.dx), static_cast<std::uint8_t>(command.dx >> 8),
        static_cast<std::uint8_t>(command.dy), static_cast<std::uint8_t>(command.dy >> 8),
        static_cast<std::uint8_t>(command.nx), static_cast<std::uint8_t>(command.nx >> 8),
        static_cast<std::uint8_t>(command.ny), static_cast<std::uint8_t>(command.ny >> 8),
        command.colour, command.argument, command.code};
    for (const std::uint8_t value : bytes) vdp.write(IndirectPort, value);
}

bool commandBusy(const v9938::V9938& vdp)
{
    return (vdp.peekStatus(2) & 0x01) != 0;
}

void finish(v9938::V9938& vdp)
{
    for (std::uint32_t tick = 0; tick < 4 * v9938::PalScanlines * Line && commandBusy(vdp); ++tick) vdp.advanceMasterTick();
    require(!commandBusy(vdp), "the command finishes");
}

void execute(v9938::V9938& vdp, const Command& command)
{
    issue(vdp, command);
    finish(vdp);
}

std::uint8_t pointAt(v9938::V9938& vdp, unsigned x, unsigned y)
{
    Command point;
    point.sx = x;
    point.sy = y;
    point.code = 0x40;
    execute(vdp, point);
    return vdp.peekStatus(7);
}

void setupCommandScreen(v9938::V9938& vdp, std::uint8_t r0)
{
    setRegister(vdp, 0, r0);
    setRegister(vdp, 1, 0x40);
}

void testHighSpeedFill()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x06);
    Command fill;
    fill.dx = 10;
    fill.dy = 5;
    fill.nx = 20;
    fill.ny = 3;
    fill.colour = 0x4A;
    fill.code = 0xC0;
    issue(vdp, fill);
    require(commandBusy(vdp), "S#2 CE is set while a command runs");
    run(vdp, 100);
    require(commandBusy(vdp), "HMMV takes time per byte");
    finish(vdp);
    require(vdp.peekVram(5 * 128 + 5) == 0x4A && vdp.peekVram(7 * 128 + 14) == 0x4A, "HMMV fills whole bytes");
    require(vdp.peekVram(5 * 128 + 4) == 0x00 && vdp.peekVram(5 * 128 + 15) == 0x00 && vdp.peekVram(8 * 128 + 5) == 0x00,
            "HMMV stays inside the rectangle");
    require(vdp.controlRegister(38) == 8 && vdp.controlRegister(42) == 0, "HMMV leaves DY after the last line and NY = 0");

    fill.dy = 100;
    fill.ny = 100;
    issue(vdp, fill);
    run(vdp, 200);
    vdp.write(ControlPort, 0x00);
    vdp.write(ControlPort, 0x80 | 46);
    require(!commandBusy(vdp), "STOP aborts a running command");
}

void testLogicalOperations()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x0E);
    const struct {
        std::uint8_t start;
        std::uint8_t colour;
        std::uint8_t operation;
        std::uint8_t expected;
        const char* message;
    } cases[] = {
        {0x0F, 0xF0, 0x0, 0xF0, "IMP replaces the dot"},
        {0x0F, 0x3C, 0x1, 0x0C, "AND"},
        {0x0F, 0xF0, 0x2, 0xFF, "OR"},
        {0x0F, 0xFF, 0x3, 0xF0, "EOR"},
        {0x0F, 0x0F, 0x4, 0xF0, "NOT"},
        {0x0F, 0x00, 0x8, 0x0F, "TIMP keeps the dot for colour 0"},
        {0x0F, 0xF0, 0xA, 0xFF, "TOR"},
        {0x0F, 0xF0, 0x5, 0x0F, "undefined operations leave the dot"},
    };
    for (const auto& check : cases) {
        Command pset;
        pset.dx = 7;
        pset.dy = 3;
        pset.colour = check.start;
        pset.code = 0x50;
        execute(vdp, pset);
        pset.colour = check.colour;
        pset.code = static_cast<std::uint8_t>(0x50 | check.operation);
        execute(vdp, pset);
        require(pointAt(vdp, 7, 3) == check.expected, check.message);
    }
    require(vdp.peekVram(0x10000 + 3 * 128 + 3) == 0x0F, "GRAPHIC 7 dots are interleaved over both banks");

    setupCommandScreen(vdp, 0x06);
    vdp.pokeVram(0x0000, 0xA5);
    Command invert;
    invert.dx = 1;
    invert.colour = 0x03;
    invert.code = 0x54;
    execute(vdp, invert);
    require(vdp.peekVram(0x0000) == 0xAC, "NOT in GRAPHIC 4 changes only the addressed dot");
}

void testLogicalMoves()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x08);
    Command fill;
    fill.dx = 1;
    fill.dy = 0;
    fill.nx = 3;
    fill.ny = 1;
    fill.colour = 2;
    fill.code = 0x80;
    execute(vdp, fill);
    require(vdp.peekVram(0) == 0x2A, "LMMV sets single 2-bit dots in GRAPHIC 5");

    Command copy;
    copy.sx = 0;
    copy.sy = 0;
    copy.dx = 5;
    copy.dy = 10;
    copy.nx = 4;
    copy.ny = 1;
    copy.code = 0x90;
    execute(vdp, copy);
    require(vdp.controlRegister(34) == 1 && vdp.controlRegister(38) == 11, "LMMM leaves SY and DY after the last line");
    require(pointAt(vdp, 5, 10) == 0 && pointAt(vdp, 6, 10) == 2 && pointAt(vdp, 8, 10) == 2 && pointAt(vdp, 9, 10) == 0,
            "LMMM copies dots to a different position within the byte");

    copy.sx = 8;
    copy.sy = 10;
    copy.dx = 20;
    copy.dy = 11;
    copy.nx = 4;
    copy.argument = 0x04;
    copy.code = 0x98;
    vdp.pokeVram(11 * 128 + 4, 0x30);
    execute(vdp, copy);
    require(pointAt(vdp, 20, 11) == 2 && pointAt(vdp, 18, 11) == 2 && pointAt(vdp, 17, 11) == 3 && pointAt(vdp, 16, 11) == 0,
            "LMMM with DIX copies to the left; TIMP skips colour 0");
}

void testHighSpeedMoves()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x0A);
    Command fill;
    fill.dx = 0;
    fill.dy = 0;
    fill.nx = 8;
    fill.ny = 1;
    fill.colour = 0x12;
    fill.code = 0xC0;
    execute(vdp, fill);
    require(vdp.peekVram(0x00000) == 0x12 && vdp.peekVram(0x10000) == 0x12 && vdp.peekVram(0x00001) == 0x12,
            "HMMV in GRAPHIC 6 writes interleaved bytes");

    Command copy;
    copy.sx = 0;
    copy.sy = 0;
    copy.dx = 100;
    copy.dy = 300;
    copy.nx = 8;
    copy.ny = 1;
    copy.code = 0xD0;
    execute(vdp, copy);
    require(pointAt(vdp, 100, 300) == 1 && pointAt(vdp, 107, 300) == 2 && pointAt(vdp, 108, 300) == 0,
            "HMMM copies bytes into page 1");

    Command lines;
    lines.sy = 300;
    lines.dx = 100;
    lines.dy = 310;
    lines.ny = 1;
    lines.code = 0xE0;
    execute(vdp, lines);
    require(pointAt(vdp, 100, 310) == 1 && pointAt(vdp, 98, 310) == 0, "YMMM copies from DX to the end of the line");
}

void testLine()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x06);
    Command line;
    line.dx = 0;
    line.dy = 0;
    line.nx = 4;
    line.ny = 2;
    line.colour = 15;
    line.code = 0x70;
    execute(vdp, line);
    require(vdp.controlRegister(38) == 2, "LINE leaves DY at the end point");
    require(pointAt(vdp, 0, 0) == 15 && pointAt(vdp, 1, 1) == 15 && pointAt(vdp, 2, 1) == 15 && pointAt(vdp, 3, 2) == 15
                && pointAt(vdp, 4, 2) == 15,
            "LINE draws NX + 1 dots along the major axis");
    require(pointAt(vdp, 1, 0) == 0 && pointAt(vdp, 5, 2) == 0, "LINE steps the minor axis by NY over NX");


    line.dx = 50;
    line.dy = 50;
    line.nx = 3;
    line.ny = 0;
    line.argument = 0x0D;
    execute(vdp, line);
    require(pointAt(vdp, 50, 47) == 15 && pointAt(vdp, 49, 47) == 0, "MAJ draws along Y, DIY draws upwards");
}

void testSearchAndPoint()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x06);
    vdp.pokeVram(5, 0x07);
    vdp.pokeVram(6, 0x77);
    Command search;
    search.sx = 0;
    search.sy = 0;
    search.colour = 7;
    search.code = 0x60;
    execute(vdp, search);
    require((vdp.peekStatus(2) & 0x10) != 0 && vdp.peekStatus(8) == 11 && vdp.peekStatus(9) == 0xFE,
            "SRCH finds the border colour and reports its X in S#8/S#9");
    setRegister(vdp, 15, 9);
    (void)vdp.read(ControlPort);
    require((vdp.peekStatus(2) & 0x10) == 0, "reading S#9 clears BD");

    search.sx = 11;
    search.argument = 0x02;
    execute(vdp, search);
    require((vdp.peekStatus(2) & 0x10) != 0 && vdp.peekStatus(8) == 14, "SRCH with EQ stops at another colour");

    (void)vdp.read(ControlPort);
    search.sx = 20;
    search.argument = 0x00;
    execute(vdp, search);
    require((vdp.peekStatus(2) & 0x10) == 0, "SRCH ends at the screen edge without BD");
    require(pointAt(vdp, 12, 0) == 7, "POINT returns the dot colour in S#7");
}

void testCpuTransfers()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x06);
    Command transfer;
    transfer.dx = 0;
    transfer.dy = 20;
    transfer.nx = 4;
    transfer.ny = 2;
    transfer.colour = 0x11;
    transfer.code = 0xF0;
    issue(vdp, transfer);
    require((vdp.peekStatus(2) & 0x81) == 0x81, "HMMC sets TR and CE");
    setRegister(vdp, 44, 0x22);
    setRegister(vdp, 44, 0x33);
    require(commandBusy(vdp), "HMMC waits for the remaining byte");
    setRegister(vdp, 44, 0x44);
    require(!commandBusy(vdp), "HMMC ends with the last byte");
    require(vdp.peekVram(20 * 128) == 0x11 && vdp.peekVram(20 * 128 + 1) == 0x22 && vdp.peekVram(21 * 128) == 0x33
                && vdp.peekVram(21 * 128 + 1) == 0x44,
            "HMMC writes the bytes from R#44, the first one before the command");

    transfer.dx = 1;
    transfer.dy = 30;
    transfer.nx = 2;
    transfer.ny = 1;
    transfer.colour = 0x05;
    transfer.code = 0xB0;
    issue(vdp, transfer);
    setRegister(vdp, 44, 0x0C);
    require(!commandBusy(vdp) && vdp.peekVram(30 * 128) == 0x05 && vdp.peekVram(30 * 128 + 1) == 0xC0,
            "LMMC writes single dots from R#44");

    transfer.sx = 0;
    transfer.sy = 20;
    transfer.nx = 3;
    transfer.ny = 1;
    transfer.code = 0xA0;
    issue(vdp, transfer);
    setRegister(vdp, 15, 7);
    require(commandBusy(vdp) && vdp.peekStatus(7) == 1, "LMCM fetches the first dot at the start");
    const std::uint8_t first = vdp.read(ControlPort);
    require(commandBusy(vdp), "LMCM fetches the next dot after each S#7 read");
    const std::uint8_t second = vdp.read(ControlPort);
    require(!commandBusy(vdp), "LMCM ends when the last dot has been fetched");
    const std::uint8_t third = vdp.read(ControlPort);
    require(first == 1 && second == 1 && third == 2, "LMCM hands the dots to the CPU via S#7");
    require(vdp.controlRegister(34) == 21, "LMCM leaves SY after the last line");
}

void testCommandLimits()
{
    v9938::V9938 vdp;
    setupCommandScreen(vdp, 0x00);
    Command fill;
    fill.nx = 8;
    fill.ny = 1;
    fill.colour = 0xFF;
    fill.code = 0xC0;
    issue(vdp, fill);
    require(!commandBusy(vdp) && vdp.peekVram(0) == 0x00, "the V9938 ignores commands outside GRAPHIC 4 to 7");

    setupCommandScreen(vdp, 0x06);
    fill.argument = 0x20;
    execute(vdp, fill);
    require(vdp.peekVram(0) == 0x00, "writes to the missing expansion RAM are lost");

    fill.argument = 0x00;
    fill.dx = 250;
    fill.nx = 0;
    execute(vdp, fill);
    require(vdp.peekVram(125) == 0xFF && vdp.peekVram(127) == 0xFF && vdp.peekVram(128) == 0x00,
            "NX = 0 fills to the right edge of the line");
}

} // namespace

int main()
{
    testResetState();
    testRegisterWrites();
    testVramAccess();
    testPalette();
    testIndirectAccess();
    testFrameTiming();
    testVerticalInterrupt();
    testLineInterrupt();
    testRetraceFlags();
    testText1();
    testText2Blink();
    testGraphic1AndScroll();
    testGraphic2AndMulticolour();
    testSpriteMode1();
    testSpriteMode2();
    testGraphic4();
    testGraphic5();
    testGraphic6();
    testGraphic7();
    testHighSpeedFill();
    testLogicalOperations();
    testLogicalMoves();
    testHighSpeedMoves();
    testLine();
    testSearchAndPoint();
    testCpuTransfers();
    testCommandLimits();
    std::cout << "V9938 library passed.\n";
}
