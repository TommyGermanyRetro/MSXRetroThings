#include "SVI802/SVI802.hpp"

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

// SVI-802: ready status on 12h, printing only on the rising /STROBE edge.
bool centronicsPass()
{
    svi802::SVI802 expander;
    if (expander.readIo(0x12) != 0xFE) return fail("Port 12h must report a ready printer (BUSY = 0).");
    expander.writeIo(0x10, 'A');
    expander.writeIo(0x11, 0x00);
    if (!expander.port().printer().output().empty()) return fail("A low /STROBE must not print yet.");
    expander.writeIo(0x11, 0xFF);
    expander.writeIo(0x11, 0xFF);
    if (expander.port().printer().output() != std::vector<std::uint8_t>{'A'}) {
        return fail("The rising /STROBE edge must print the data byte once.");
    }
    return true;
}

} // namespace

int main()
{
    if (!centronicsPass()) return EXIT_FAILURE;
    std::cout << "SVI-802 passed.\n";
    return EXIT_SUCCESS;
}
