#include "MC6845/MC6845.hpp"

#include <cstdlib>
#include <iostream>

namespace {

constexpr std::uint32_t HostHz = 3579545;
constexpr std::uint32_t CharacterHz = 2000000;

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// The CRTC table of the SVI-328 BASIC ROM (3502h): 80 x 24 characters with 9 raster lines.
void programSvi80(mc6845::MC6845& crtc)
{
    const std::uint8_t table[16] = {0x6D, 0x50, 0x5C, 0x04, 0x1C, 0x04, 0x18, 0x1A, 0x00, 0x08, 0x60, 0x08, 0x00, 0x00, 0x00, 0x00};
    for (std::uint8_t r = 0; r < 16; ++r) {
        crtc.selectRegister(r);
        crtc.writeRegister(table[r]);
    }
}

bool registersPass()
{
    mc6845::MC6845 crtc(HostHz, CharacterHz);
    programSvi80(crtc);
    if (crtc.charactersPerRow() != 80 || crtc.displayedRows() != 24 || crtc.rasterLinesPerRow() != 9) {
        return fail("The SVI table must give 80 x 24 characters with 9 raster lines.");
    }
    crtc.selectRegister(9);
    if (crtc.readRegister() != 0) return fail("R9 must read 00h.");
    crtc.selectRegister(14);
    crtc.writeRegister(0xFF);
    crtc.selectRegister(15);
    crtc.writeRegister(0x34);
    crtc.selectRegister(14);
    if (crtc.readRegister() != 0x3F || crtc.cursorAddress() != 0x3F34) return fail("R14 must hold six bits and read back.");
    crtc.selectRegister(16);
    crtc.writeRegister(0x55);
    if (crtc.readRegister() != 0) return fail("The light pen registers must be read-only.");
    crtc.selectRegister(12);
    crtc.writeRegister(0x12);
    crtc.selectRegister(13);
    crtc.writeRegister(0x80);
    if (crtc.startAddress() != 0x1280) return fail("R12/R13 must form the start address.");
    return true;
}

bool cursorPass()
{
    mc6845::MC6845 crtc(HostHz, CharacterHz);
    programSvi80(crtc);
    // 110 characters x (29 rows x 9 lines + 4) = 29150 character clocks per field.
    if (crtc.characterClocksPerField() != 29150) return fail("The field must last 29150 character clocks.");
    if (!crtc.cursorVisible(0) || !crtc.cursorVisible(8) || crtc.cursorVisible(9)) {
        return fail("The cursor must cover raster lines 0-8.");
    }
    const auto ticksPerField = static_cast<std::uint64_t>(29150) * HostHz / CharacterHz + 1;
    for (std::uint64_t i = 0; i < 16 * ticksPerField; ++i) crtc.tick();
    if (crtc.fieldCount() != 16 || crtc.cursorVisible(0)) return fail("Blink mode 3 must hide the cursor after 16 fields.");
    for (std::uint64_t i = 0; i < 16 * ticksPerField; ++i) crtc.tick();
    if (!crtc.cursorVisible(0)) return fail("Blink mode 3 must show the cursor again after 32 fields.");
    crtc.selectRegister(10);
    crtc.writeRegister(0x20);
    if (crtc.cursorVisible(0)) return fail("Blink mode 1 must hide the cursor.");
    crtc.writeRegister(0x02);
    if (crtc.cursorVisible(1) || !crtc.cursorVisible(2)) return fail("The cursor must start at R10.");
    return true;
}

} // namespace

int main()
{
    if (!registersPass()) return EXIT_FAILURE;
    if (!cursorPass()) return EXIT_FAILURE;
    std::cout << "MC6845 passed.\n";
    return EXIT_SUCCESS;
}
