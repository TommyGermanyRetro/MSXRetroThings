#pragma once

#include "TMS99X8/TMS99X8.hpp"

namespace tms9918a {

// Texas Instruments TMS9918A, NTSC video display processor (composite output; the timing of the TMS9928A): 262 lines per field, active display from line 40, 60 fields per second.
class TMS9918A final : public tms99x8::TMS99X8 {
public:
    TMS9918A();
};

} // namespace tms9918a
