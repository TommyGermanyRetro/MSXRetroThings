#include "SVI803/SVI803.hpp"
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

bool svi803Pass()
{
    svi803::SVI803 card;
    (void)card.writeMemory(0x8123, 0x5A, 0xFF);
    const auto cycle = card.readMemory(0x8123, 0xFF);
    if (cycle.data != 0x5A || cycle.ramDisable || card.readMemory(0xC000, 0xFF).data != 0xFF) {
        return fail("SVI-803 must hold 16 KiB in 8000h-BFFFh without /RAMDIS.");
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
