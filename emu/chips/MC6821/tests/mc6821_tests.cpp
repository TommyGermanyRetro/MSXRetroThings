#include "MC6821/MC6821.hpp"

#include <cstdlib>
#include <iostream>

namespace {

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

bool registersPass()
{
    mc6821::MC6821 pia;
    if (pia.read(1) != 0 || pia.read(3) != 0 || pia.read(0) != 0) return fail("After reset all registers must read 00h.");
    pia.write(0, 0xF0);
    if (pia.read(0) != 0xF0) return fail("With CRA bit 2 = 0, register 0 must be DDRA.");
    pia.write(1, 0x04);
    pia.write(0, 0xA5);
    pia.setPortAInputs(0x0F);
    if (pia.read(0) != 0xAF || pia.portA() != 0xAF) return fail("PRA must read outputs for DDR bits 1 and inputs for the rest.");
    pia.write(1, 0x00);
    if (pia.read(0) != 0xF0) return fail("Clearing CRA bit 2 must select DDRA again.");
    pia.write(3, 0xFF);
    if (pia.read(3) != 0x3F) return fail("CRB bits 6 and 7 must be read-only.");
    return true;
}

bool control2Pass()
{
    mc6821::MC6821 pia;
    pia.write(1, 0x34);
    if (pia.ca2()) return fail("CRA 34h must drive CA2 low (manual output).");
    pia.write(1, 0x3C);
    if (!pia.ca2()) return fail("CRA 3Ch must drive CA2 high.");
    pia.write(3, 0x24);
    pia.write(2, 0x55);
    if (pia.cb2()) return fail("In write-strobe handshake CB2 must go low on a PRB write.");
    pia.setCB1(false);
    if (!pia.cb2()) return fail("The active CB1 edge must end the CB2 low phase.");
    pia.write(1, 0x2C);
    (void)pia.read(0);
    if (!pia.ca2()) return fail("In pulse mode CA2 must return high after the read.");
    return true;
}

bool interruptsPass()
{
    mc6821::MC6821 pia;
    pia.write(1, 0x05);
    pia.setCA1(false);
    if (!(pia.read(1) & 0x80) || !pia.irqA()) return fail("A falling CA1 edge must set IRQA1 and request an interrupt.");
    (void)pia.read(0);
    if ((pia.read(1) & 0x80) || pia.irqA()) return fail("Reading PRA must clear IRQA1.");
    pia.setCA1(true);
    if (pia.read(1) & 0x80) return fail("A rising CA1 edge must not count with bit 1 = 0.");
    pia.write(1, 0x04);
    pia.setCA1(false);
    if (!(pia.read(1) & 0x80) || pia.irqA()) return fail("With bit 0 = 0 the flag sets but no interrupt is requested.");
    pia.write(3, 0x1C);
    pia.setCB2(false);
    pia.setCB2(true);
    if (!(pia.read(3) & 0x40) || !pia.irqB()) return fail("A rising CB2 edge must set IRQB2 with bit 4 = 1.");
    return true;
}

} // namespace

int main()
{
    if (!registersPass()) return EXIT_FAILURE;
    if (!control2Pass()) return EXIT_FAILURE;
    if (!interruptsPass()) return EXIT_FAILURE;
    std::cout << "MC6821 passed.\n";
    return EXIT_SUCCESS;
}
