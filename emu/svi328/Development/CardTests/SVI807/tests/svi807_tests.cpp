#include "SVI807/SVI807.hpp"
#include "SVI605B/SVI605B.hpp"
#include "SVI328/SVI328.hpp"

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

void selectBanks(svi328::SVI328& machine, std::uint8_t portB)
{
    machine.writeIo(0x88, 7);
    machine.writeIo(0x8C, 0x80);
    machine.writeIo(0x88, 15);
    machine.writeIo(0x8C, portB);
}

bool svi807Pass()
{
    svi328::SVI328 machine;
    std::vector<std::uint8_t> rom(svi328::RomSize, 0xC9);
    if (!machine.loadRom(rom)) return fail("A 32 KiB ROM must load.");
    svi605b::SVI605B expander;
    expander.insertCard(1, std::make_unique<svi807::SVI807>());
    machine.connectExpansion(&expander);
    machine.reset();

    machine.writeMemory(0x8000, 0x02);
    selectBanks(machine, 0xFB);
    if (machine.readMemory(0x8000) != 0x00) return fail("/BK22 must select the SVI-807 high half (S2).");
    machine.writeMemory(0x8000, 0x22);
    selectBanks(machine, 0xF7);
    if (machine.readMemory(0x0000) != 0x00) return fail("/BK31 must select the SVI-807 low half (S3).");
    machine.writeMemory(0x0000, 0x31);
    selectBanks(machine, 0xEF);
    if (machine.readMemory(0x8000) != 0x02) return fail("/BK32 with S4 off must leave RAM bank 2.");
    selectBanks(machine, 0xFD);
    machine.writeMemory(0x0000, 0x21);
    selectBanks(machine, 0xFF);
    if (machine.readMemory(0x0000) != 0xC9 || machine.readMemory(0x8000) != 0x02) {
        return fail("Without bank signals the ROM and RAM bank 2 must return.");
    }
    selectBanks(machine, 0xF9);
    if (machine.readMemory(0x0000) != 0x21 || machine.readMemory(0x8000) != 0x22) {
        return fail("Banks 21 and 22 must keep their own contents.");
    }
    selectBanks(machine, 0xF7);
    if (machine.readMemory(0x0000) != 0x31) return fail("Bank 31 must keep its contents.");
    return true;
}

// Two SVI-807 cards, one with S2 and one with S3 and S4, give banks 22, 31 and 32: 144 KiB in all.
bool twoCardsPass()
{
    svi328::SVI328 machine;
    svi605b::SVI605B expander;
    expander.insertCard(0, std::make_unique<svi807::SVI807>(svi807::SVI807Switches{false, true, false, false, false}));
    expander.insertCard(2, std::make_unique<svi807::SVI807>(svi807::SVI807Switches{false, false, true, true, false}));
    machine.connectExpansion(&expander);
    machine.reset();
    const std::uint8_t banks[] = {0xFF, 0xFB, 0xEF};
    for (std::uint8_t i = 0; i < 3; ++i) {
        selectBanks(machine, banks[i]);
        machine.writeMemory(0xC000, static_cast<std::uint8_t>(0xA0 + i));
    }
    selectBanks(machine, 0xF7);
    machine.writeMemory(0x4000, 0xB1);
    for (std::uint8_t i = 0; i < 3; ++i) {
        selectBanks(machine, banks[i]);
        if (machine.readMemory(0xC000) != 0xA0 + i) return fail("Banks 2, 22 and 32 must be separate memory.");
    }
    selectBanks(machine, 0xF7);
    if (machine.readMemory(0x4000) != 0xB1) return fail("Bank 31 must come from the second card.");
    return true;
}

} // namespace

int main()
{
    if (!svi807Pass()) return EXIT_FAILURE;
    if (!twoCardsPass()) return EXIT_FAILURE;
    std::cout << "SVI-807 passed.\n";
    return EXIT_SUCCESS;
}
