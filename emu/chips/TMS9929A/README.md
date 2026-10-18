# TMS9929A

TMS9929A is a C++17 static library for the Texas Instruments TMS9929A, the PAL video display processor of the European machines, with Y and R-Y/B-Y outputs. It builds on the family library [TMS99X8](../TMS99X8/README.md), which holds the common logic of the chips.

## Functionality

- 313 scanlines per field, active display from line 64, 50 fields per second (`tms99x8::Model::Pal`).
- Everything else, from VRAM access and the display modes to sprites and status, is the family logic of TMS99X8.

## Interface and Integration

[TMS9929A.hpp](include/TMS9929A/TMS9929A.hpp) exposes `tms9929a::TMS9929A`, a `tms99x8::TMS99X8` with its model fixed. A host adds the projects `tms9929a` and `tms99x8` and both `include` directories.

```cpp
#include <TMS9929A/TMS9929A.hpp>

tms9929a::TMS9929A vdp;
vdp.write(1, 0x40);
vdp.write(1, 0x81);
```

## In the SVI-318 and SVI-328

The PAL machine, the default (**System > PAL (TMS9929A, 50 Hz)** in the frontend), is fitted with this chip; ports and interrupt are those described in [TMS99X8](../TMS99X8/README.md). See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln), [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-728](../../svi728/Development/run_tests.ps1), [SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `tms9929a` | Static library | `tms9929a.lib` |
| `tms9929a_tests` | Console application | `tms9929a_tests.exe` |

## Tests

[tms9929a_tests.cpp](tests/tms9929a_tests.cpp) checks the model, the field rate, the 313 lines with the display from line 64, and that the frame interrupt comes after the last active line.
