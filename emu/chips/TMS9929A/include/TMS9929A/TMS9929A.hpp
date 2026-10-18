#pragma once

#include "TMS99X8/TMS99X8.hpp"

namespace tms9929a {

// Texas Instruments TMS9929A, PAL video display processor: 313 lines per field, active display from line 64, 50 fields per second.
class TMS9929A final : public tms99x8::TMS99X8 {
public:
    TMS9929A();
};

} // namespace tms9929a
