#include "SVI806/SVI806.hpp"
#include "SVI605B/SVI605B.hpp"
#include "SVI318/SVI318.hpp"

#include <cstdint>
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

// The SVI-806 with the CRTC table of the BASIC ROM (3502h) and the original character ROM, where
// code 21h is "A" (the ROM starts with the space).
bool videoCardPass(const char* characterRomPath)
{
    std::ifstream file(characterRomPath, std::ios::binary);
    const std::vector<std::uint8_t> characters{std::istreambuf_iterator<char>(file), {}};
    svi318::SVI318 machine;
    svi605b::SVI605B expander;
    auto card = std::make_unique<svi806::SVI806>();
    if (!card->loadCharacterRom(characters)) return fail("svi806.rom must hold 4096 bytes.");
    auto* svi806 = card.get();
    expander.insertCard(3, std::move(card));
    machine.connectExpansion(&expander);
    machine.reset();
    const std::uint8_t table[16] = {0x6D, 0x50, 0x5C, 0x04, 0x1C, 0x04, 0x18, 0x1A, 0x00, 0x08, 0x60, 0x08, 0x00, 0x00, 0x00, 0x51};
    for (std::uint8_t r = 0; r < 16; ++r) {
        machine.writeIo(0x50, r);
        machine.writeIo(0x51, table[r]);
    }
    machine.writeIo(0x50, 15);
    if (machine.readIo(0x51) != 0x51) return fail("The cursor register must read back through 51h.");

    machine.writeMemory(0xF000, 0x77);
    machine.writeIo(0x58, 0x01);
    machine.writeMemory(0xF000, 0x21);
    machine.writeMemory(0xF851, 0x10);
    if (machine.readMemory(0xF000) != 0x21 || machine.readMemory(0xF800) != 0x21 || !svi806->videoRamEnabled()) {
        return fail("Port 58h must map the 2 KiB video RAM into F000h-FFFFh.");
    }
    machine.writeIo(0x58, 0x00);
    if (machine.readMemory(0xF000) != 0x77) return fail("Without 58h RAM bank 2 must return.");

    const auto frame = svi806->render();
    if (frame.width != 640 || frame.height != 216) return fail("The picture must be 640 x 216 pixels.");
    for (unsigned line = 0; line < 9; ++line) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            const auto expected = (characters[0x21 * 16 + line] >> (7 - bit)) & 1;
            if (frame.pixels[line * 640 + bit] != expected) return fail("Code 21h must show the ROM's A.");
            if (frame.pixels[(9 + line) * 640 + 8 + bit] != 1) return fail("The cursor at 51h must fill its cell.");
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: svi806_tests <svi806.rom>\n";
        return EXIT_FAILURE;
    }
    if (!videoCardPass(argv[1])) return EXIT_FAILURE;
    std::cout << "SVI-806 passed.\n";
    return EXIT_SUCCESS;
}
