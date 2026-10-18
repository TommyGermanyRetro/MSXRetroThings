#include "TMS9929A/TMS9929A.hpp"

#include <cstdlib>
#include <iostream>

// The TMS9929A fixes the family model: 313 lines per field, active display from line 64,
// 50 fields per second, and the frame interrupt after the last active line.
int main()
{
    tms9929a::TMS9929A vdp;
    const auto outputs = vdp.outputs();
    if (vdp.model() != tms99x8::Model::Pal || vdp.fieldRateHz() != 50 || outputs.scanlinesPerFrame != 313 ||
        outputs.displayStartScanline != 64) {
        std::cerr << "The TMS9929A must run 313 lines from line 64 at 50 Hz.\n";
        return EXIT_FAILURE;
    }
    vdp.write(1, 0x20);
    vdp.write(1, 0x81);
    unsigned lines = 0;
    while (!vdp.outputs().interruptRequest && lines < 400) {
        vdp.advanceScanline();
        ++lines;
    }
    if (lines != 64 + tms99x8::ScreenHeight + 1) {
        std::cerr << "The TMS9929A must interrupt after its last active line.\n";
        return EXIT_FAILURE;
    }
    std::cout << "TMS9929A passed.\n";
    return EXIT_SUCCESS;
}
