#include "PPI8255/PPI8255.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void testMode0AndBsr()
{
    ppi8255::PPI8255 ppi;
    ppi.write(3, 0x80);
    ppi.write(0, 0x5A);
    ppi.write(1, 0xC3);
    ppi.write(2, 0x00);

    auto outputs = ppi.outputs();
    require(outputs.portA == 0x5A && outputs.portADriveMask == 0xFF,
            "mode-0 output A is latched and driven");
    require(outputs.portB == 0xC3 && outputs.portBDriveMask == 0xFF,
            "mode-0 output B is latched and driven");

    ppi.write(3, 0x0B);
    require((ppi.read(2) & 0x20) != 0, "BSR sets PC5 without changing the I/O mode");
    ppi.write(3, 0x0A);
    require((ppi.read(2) & 0x20) == 0, "BSR resets PC5");
    require(ppi.controlWord() == 0x80, "BSR leaves the mode control word unchanged");
}

void testMode0Inputs()
{
    ppi8255::PPI8255 ppi;
    ppi.write(3, 0x9B);
    ppi8255::Inputs inputs;
    inputs.portA = 0x12;
    inputs.portB = 0x34;
    inputs.portC = 0xA5;
    ppi.setInputs(inputs);

    require(ppi.read(0) == 0x12, "mode-0 input A reads external pins");
    require(ppi.read(1) == 0x34, "mode-0 input B reads external pins");
    require(ppi.read(2) == 0xA5, "mode-0 input C reads external pins");
    require(ppi.outputs().portADriveMask == 0, "mode-0 input A is not driven");
}

void testMode1InputHandshake()
{
    ppi8255::PPI8255 ppi;
    ppi.write(3, 0xB0);
    ppi.write(3, 0x09);

    ppi8255::Inputs inputs;
    inputs.portA = 0xA5;
    inputs.strobeAActive = true;
    ppi.setInputs(inputs);
    require(ppi.outputs().inputBufferFullA, "STB A captures data and asserts IBF A");

    inputs.strobeAActive = false;
    ppi.setInputs(inputs);
    require(ppi.outputs().interruptA, "INTR A asserts after STB is released with INTE enabled");
    require((ppi.read(2) & 0x28) == 0x28, "Port C exposes IBF A and INTR A");

    require(ppi.read(0) == 0xA5, "mode-1 input returns the captured byte");
    require(!ppi.outputs().inputBufferFullA && !ppi.outputs().interruptA,
            "CPU read clears IBF A and INTR A");
}

void testMode1OutputHandshake()
{
    ppi8255::PPI8255 ppi;
    ppi.write(3, 0xA0);
    ppi.write(3, 0x0D);
    ppi.write(0, 0x5A);

    require(ppi.outputs().outputBufferFullA, "CPU write asserts OBF A");
    require((ppi.read(2) & 0x80) == 0, "active-low OBF A appears low on PC7");

    ppi8255::Inputs inputs;
    inputs.acknowledgeAActive = true;
    ppi.setInputs(inputs);
    require(!ppi.outputs().outputBufferFullA, "ACK A deasserts OBF A");

    inputs.acknowledgeAActive = false;
    ppi.setInputs(inputs);
    require(ppi.outputs().interruptA, "INTR A asserts after ACK is released");
}

void testMode2A()
{
    ppi8255::PPI8255 ppi;
    ppi.write(3, 0xC0);
    require(ppi.isMode2A(), "group A mode 2 is decoded");
    ppi.write(3, 0x09);
    ppi.write(3, 0x0D);

    ppi8255::Inputs inputs;
    inputs.portA = 0x69;
    inputs.strobeAActive = true;
    ppi.setInputs(inputs);
    require(ppi.outputs().inputBufferFullA, "mode 2 accepts strobed input");
    require(ppi.read(0) == 0x69, "mode 2 reads its input latch");

    ppi.write(0, 0x96);
    require(ppi.outputs().outputBufferFullA, "mode 2 accepts output data");
    require(ppi.outputs().portADriveMask == 0xFF, "mode-2 output data drives port A");

    inputs.acknowledgeAActive = true;
    ppi.setInputs(inputs);
    inputs.acknowledgeAActive = false;
    ppi.setInputs(inputs);
    require(ppi.outputs().interruptA, "mode 2 reports the output ACK interrupt");
}

void testMode1PortBInput()
{
    ppi8255::PPI8255 ppi;
    ppi.write(3, 0xB6);
    ppi.write(3, 0x05);

    ppi8255::Inputs inputs;
    inputs.portB = 0xD2;
    inputs.strobeBActive = true;
    ppi.setInputs(inputs);
    inputs.strobeBActive = false;
    ppi.setInputs(inputs);

    require(ppi.outputs().inputBufferFullB && ppi.outputs().interruptB,
            "mode-1 B input asserts IBF and INTR");
    require((ppi.read(2) & 0x03) == 0x03, "Port C exposes IBF B and INTR B");
    require(ppi.read(1) == 0xD2, "mode-1 B read returns the captured byte");
    require(!ppi.outputs().inputBufferFullB && !ppi.outputs().interruptB,
            "mode-1 B CPU read clears handshake state");
}

void testMode1PortBOutput()
{
    ppi8255::PPI8255 ppi;
    ppi.write(3, 0x84);
    ppi.write(3, 0x05);
    ppi.write(1, 0xC7);

    require(ppi.outputs().outputBufferFullB, "mode-1 B write asserts OBF B");
    require((ppi.read(2) & 0x02) == 0, "active-low OBF B appears low on PC1");

    ppi8255::Inputs inputs;
    inputs.acknowledgeBActive = true;
    ppi.setInputs(inputs);
    inputs.acknowledgeBActive = false;
    ppi.setInputs(inputs);
    require(ppi.outputs().interruptB, "mode-1 B asserts INTR after ACK release");
}

} // namespace

int main()
{
    testMode0AndBsr();
    testMode0Inputs();
    testMode1InputHandshake();
    testMode1OutputHandshake();
    testMode2A();
    testMode1PortBInput();
    testMode1PortBOutput();
    std::cout << "All PPI8255 tests passed.\n";
}