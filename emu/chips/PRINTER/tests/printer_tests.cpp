#include "PRINTER/Printer.hpp"

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

void testStrobeCapturesBytes()
{
    printer::Printer device;
    device.writeStrobe(true);
    require(device.output().empty(), "a strobe without a data write is not printer output");
    device.writeStrobe(false);

    device.writeData('A');
    device.writeStrobe(false);
    require(device.output().empty(), "the low strobe phase does not print a byte");
    device.writeStrobe(true);
    require(device.output().size() == 1 && device.output()[0] == 'A',
            "the rising strobe edge captures the data latch");

    device.writeData('B');
    device.writeStrobe(true);
    require(device.output().size() == 1, "a held strobe does not repeat the byte");
    device.writeStrobe(false);
    device.writeStrobe(true);
    require(device.output().size() == 2 && device.output()[1] == 'B',
            "a second strobe pulse captures the next byte");
    device.writeStrobe(false);
    device.writeStrobe(true);
    require(device.output().size() == 2,
            "a strobe pulse without a new data write does not repeat the previous byte");
}

void testStatusAndReset()
{
    printer::Printer device;
    device.writeData('X');
    device.writeStrobe(true);
    device.reset();
    require((device.status() & 0x02) == 0,
            "printer status reports ready with the active-low busy bit clear");
    require(device.output().size() == 1 && device.output()[0] == 'X',
            "machine reset preserves already printed output");
    device.writeData('Y');
    device.writeStrobe(true);
    require(device.output().size() == 2 && device.output()[1] == 'Y',
            "reset returns the strobe to its inactive state");
    device.clearOutput();
    require(device.output().empty(), "saved or discarded output can be cleared");
}

} // namespace

int main()
{
    testStrobeCapturesBytes();
    testStatusAndReset();
    std::cout << "Printer tests passed.\n";
}
