#include "TMS9918A/TMS9918A.hpp"

#include <cstdlib>
#include <iostream>

// The TMS9918A fixes the family model: 262 lines per field, active display from line 40,
// 60 fields per second, and the frame interrupt after the last active line.
int main()
{
    tms9918a::TMS9918A vdp;
    const auto outputs = vdp.outputs();
    if (vdp.model() != tms99x8::Model::Ntsc || vdp.fieldRateHz() != 60 || outputs.scanlinesPerFrame != 262 ||
        outputs.displayStartScanline != 40) {
        std::cerr << "The TMS9918A must run 262 lines from line 40 at 60 Hz.\n";
        return EXIT_FAILURE;
    }
    vdp.write(1, 0x20);
    vdp.write(1, 0x81);
    unsigned lines = 0;
    while (!vdp.outputs().interruptRequest && lines < 400) {
        vdp.advanceScanline();
        ++lines;
    }
    if (lines != 40 + tms99x8::ScreenHeight + 1) {
        std::cerr << "The TMS9918A must interrupt after its last active line.\n";
        return EXIT_FAILURE;
    }
    std::cout << "TMS9918A passed.\n";
    return EXIT_SUCCESS;
}
