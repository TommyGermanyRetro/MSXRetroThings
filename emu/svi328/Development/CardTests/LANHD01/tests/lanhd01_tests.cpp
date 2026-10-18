#include "LANHD01/LANHD01.hpp"

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

// LAN-IF-01's partner LAN-HD-01: bus free, selection, the six command bytes of TEST UNIT READY, the
// status byte and the message byte, then bus free again (46h phases with bit 5 for the target).
bool sasiPass()
{
    lanhd01::LANHD01 card;
    card.reset();
    if (card.readIo(0x46) != 0x20) return fail("After reset the bus must be free (46h = 20h).");
    card.writeIo(0x41, 0x01);
    card.writeIo(0x43, 0x01);
    if (card.readIo(0x46) != 0x2B) return fail("Selecting ID 0 must lead to the Command phase (46h = 2Bh).");
    for (int index = 0; index < 6; ++index) card.writeIo(0x41, 0x00);
    if (card.readIo(0x46) != 0x3B) return fail("After six command bytes the Status phase must follow (46h = 3Bh).");
    (void)card.readIo(0x42);
    if (card.readIo(0x46) != 0x3F) return fail("After the status byte the Message In phase must follow (46h = 3Fh).");
    if (card.readIo(0x42) != 0x00) return fail("The message byte must be Command Complete (00h).");
    if (card.readIo(0x46) != 0x20) return fail("After the message the bus must be free again.");
    if (card.readIo(0x40) != 0xFF || card.interruptRequest()) return fail("Other ports must read FFh, without /INT.");
    return true;
}

} // namespace

int main()
{
    if (!sasiPass()) return EXIT_FAILURE;
    std::cout << "LAN-HD-01 passed.\n";
    return EXIT_SUCCESS;
}
