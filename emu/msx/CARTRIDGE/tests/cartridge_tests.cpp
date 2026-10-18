#include "CARTRIDGE/Cartridge.hpp"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
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

// Every byte holds the number of its bank so that reads show the selected bank.
std::vector<std::uint8_t> bankedImage(std::size_t size, std::size_t bankSize)
{
    std::vector<std::uint8_t> image(size);
    for (std::size_t offset = 0; offset < size; ++offset) image[offset] = static_cast<std::uint8_t>(offset / bankSize);
    return image;
}

// A 128 KiB image whose code writes to the given bank registers with LD (nnnn),A.
std::vector<std::uint8_t> imageWritingTo(std::initializer_list<std::uint16_t> addresses)
{
    std::vector<std::uint8_t> image(0x20000, 0x00);
    std::size_t offset = 0x100;
    for (const std::uint16_t address : addresses) {
        for (int copy = 0; copy < 3; ++copy) {
            image[offset++] = 0x32;
            image[offset++] = static_cast<std::uint8_t>(address);
            image[offset++] = static_cast<std::uint8_t>(address >> 8);
        }
    }
    return image;
}

void testDetection()
{
    require(cartridge::detectMapper(std::vector<std::uint8_t>(0x8000)) == cartridge::Mapper::None, "32 KiB ROMs have no mapper");
    require(cartridge::detectMapper(imageWritingTo({0x6000, 0x7000})) == cartridge::Mapper::Ascii16, "#6000/#7000 writes mean ASCII16");
    require(cartridge::detectMapper(imageWritingTo({0x6000})) == cartridge::Mapper::Ascii16, "a single #6000 register means ASCII16");
    require(cartridge::detectMapper(imageWritingTo({0x6000, 0x6800, 0x7000, 0x7800})) == cartridge::Mapper::Ascii8,
            "#6800/#7800 writes mean ASCII8");
    require(cartridge::detectMapper(imageWritingTo({0x6000, 0x8000, 0xA000})) == cartridge::Mapper::Konami,
            "#6000/#8000/#A000 writes mean Konami");
    require(cartridge::detectMapper(imageWritingTo({0x5000, 0x7000, 0x9000, 0xB000})) == cartridge::Mapper::KonamiScc,
            "#5000/#7000/#9000/#B000 writes mean Konami SCC");
}

void testPlain()
{
    cartridge::Cartridge cart;
    std::vector<std::uint8_t> basic(0x4000, 0x11);
    basic[0] = 'A';
    basic[1] = 'B';
    basic[2] = 0x00;
    basic[3] = 0x00;
    basic[8] = 0x10;
    basic[9] = 0x80;
    basic[4] = 0x10;
    basic[5] = 0x80;
    require(cart.insert(basic, cartridge::Mapper::None) && cart.read(0x8000) == 'A' && cart.read(0x4000) == 0xFF,
            "a 16 KiB ROM with a page 2 entry starts at #8000");
    require(cart.insert(std::vector<std::uint8_t>(0x8000, 0x22), cartridge::Mapper::None) && cart.read(0x4000) == 0x22
                && cart.read(0xBFFF) == 0x22 && cart.read(0xC000) == 0xFF,
            "a 32 KiB ROM fills pages 1 and 2");
    require(cart.insert(std::vector<std::uint8_t>(0x10000, 0x33), cartridge::Mapper::None) && cart.read(0x0000) == 0x33,
            "a 64 KiB ROM starts at #0000");
    require(cart.insertAt(std::vector<std::uint8_t>(0x2000, 0x44), 0x6000) && cart.read(0x6000) == 0x44 && cart.read(0x4000) == 0xFF,
            "insertAt places a plain ROM at a fixed address");
    require(!cart.insertAt(std::vector<std::uint8_t>(0x4000), 0xE000), "a plain ROM must fit into 64 KiB");
    cart.eject();
    require(!cart.inserted() && cart.read(0x4000) == 0xFF, "an empty slot reads FFh");
}

void testAscii16()
{
    cartridge::Cartridge cart;
    require(cart.insert(bankedImage(0x20000, 0x4000), cartridge::Mapper::Ascii16), "ASCII16 takes 128 KiB");
    require(cart.read(0x4000) == 0 && cart.read(0x8000) == 0 && cart.read(0x0000) == 0xFF && cart.read(0xC000) == 0xFF,
            "ASCII16 starts with bank 0 in pages 1 and 2");
    cart.write(0x6000, 3);
    cart.write(0x77FF, 5);
    require(cart.read(0x4000) == 3 && cart.read(0x7FFF) == 3 && cart.read(0x8000) == 5 && cart.read(0xBFFF) == 5,
            "#6000-#67FF and #7000-#77FF select 16 KiB banks");
    cart.write(0x6800, 1);
    require(cart.read(0x4000) == 3, "#6800 is no ASCII16 register");
    cart.write(0x6000, 9);
    require(cart.read(0x4000) == 1, "bank numbers wrap at the ROM size");
    cart.reset();
    require(cart.read(0x4000) == 0 && cart.read(0x8000) == 0, "reset selects bank 0 again");
    require(!cart.insert(std::vector<std::uint8_t>(0x6000), cartridge::Mapper::Ascii16), "ASCII16 needs whole 16 KiB banks");
}

void testAscii8()
{
    cartridge::Cartridge cart;
    require(cart.insert(bankedImage(0x20000, 0x2000), cartridge::Mapper::Ascii8), "ASCII8 takes 128 KiB");
    cart.write(0x6000, 1);
    cart.write(0x6800, 2);
    cart.write(0x7000, 3);
    cart.write(0x7FFF, 4);
    require(cart.read(0x4000) == 1 && cart.read(0x6000) == 2 && cart.read(0x8000) == 3 && cart.read(0xA000) == 4,
            "#6000/#6800/#7000/#7800 select the four 8 KiB banks");
}

void testKonami()
{
    cartridge::Cartridge cart;
    require(cart.insert(bankedImage(0x20000, 0x2000), cartridge::Mapper::Konami), "Konami takes 128 KiB");
    require(cart.read(0x4000) == 0 && cart.read(0x6000) == 1 && cart.read(0x8000) == 2 && cart.read(0xA000) == 3,
            "Konami starts with banks 0-3");
    cart.write(0x4000, 9);
    cart.write(0x6000, 7);
    cart.write(0x9FFF, 8);
    cart.write(0xA000, 9);
    require(cart.read(0x4000) == 0 && cart.read(0x6000) == 7 && cart.read(0x8000) == 8 && cart.read(0xA000) == 9,
            "Konami keeps bank 0 at #4000 and switches the other regions");

    require(cart.insert(bankedImage(0x20000, 0x2000), cartridge::Mapper::KonamiScc), "Konami SCC takes 128 KiB");
    cart.write(0x5000, 4);
    cart.write(0x7000, 5);
    cart.write(0x9000, 6);
    cart.write(0xB7FF, 7);
    cart.write(0x5800, 1);
    require(cart.read(0x4000) == 4 && cart.read(0x6000) == 5 && cart.read(0x8000) == 6 && cart.read(0xA000) == 7,
            "Konami SCC switches at #5000/#7000/#9000/#B000 only");
    require(cart.bankAt(0x4000) == 4 && cart.bankAt(0x0000) == -1, "bankAt reports the mapped banks");
}

} // namespace

int main()
{
    testDetection();
    testPlain();
    testAscii16();
    testAscii8();
    testKonami();
    std::cout << "Cartridge library passed.\n";
}
