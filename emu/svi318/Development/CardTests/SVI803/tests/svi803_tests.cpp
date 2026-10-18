#include "SVI803/SVI803.hpp"
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

// The SVI-803 fills the 16 KiB of RAM bank 2 that the SVI-318 lacks: with it bank 2 is 32 KiB.
bool svi803Pass()
{
    svi803::SVI803 card;
    (void)card.writeMemory(0x8123, 0x5A, 0xFF);
    const auto cycle = card.readMemory(0x8123, 0xFF);
    if (cycle.data != 0x5A || cycle.ramDisable || card.readMemory(0xC000, 0xFF).data != 0xFF) {
        return fail("SVI-803 must hold 16 KiB in 8000h-BFFFh without /RAMDIS.");
    }
    svi318::SVI318 machine;
    if (!machine.loadRom(std::vector<std::uint8_t>(svi318::RomSize, 0xC9))) return fail("A 32 KiB ROM must load.");
    svi605b::SVI605B expander;
    expander.insertCard(0, std::make_unique<svi803::SVI803>());
    machine.connectExpansion(&expander);
    machine.reset();
    machine.writeMemory(0x8000, 0x11);
    machine.writeMemory(0xBFFF, 0x22);
    machine.writeMemory(0xC000, 0x33);
    if (machine.readMemory(0x8000) != 0x11 || machine.readMemory(0xBFFF) != 0x22 || machine.readMemory(0xC000) != 0x33) {
        return fail("With an SVI-803 the SVI-318 must have 32 KiB of RAM in 8000h-FFFFh.");
    }
    return true;
}

} // namespace

int main()
{
    if (!svi803Pass()) return EXIT_FAILURE;
    std::cout << "SVI-803 passed.\n";
    return EXIT_SUCCESS;
}
