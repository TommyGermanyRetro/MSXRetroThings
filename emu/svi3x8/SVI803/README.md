# SVI-803

SVI803 is a C++17 static library for the Spectravideo SVI-803 16 KiB RAM cartridge. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **SVI-803** 16 KiB RAM card ([SVI803.hpp](include/SVI803/SVI803.hpp)) for the SVI-318: fixed in `8000h`-`BFFFh`, where the SVI-318 has no internal RAM, so it completes RAM bank 2 to 32 KiB. It has no /RAMDIS, so in an SVI-328 the internal RAM bank 2 answers reads at the same addresses.

## Interface and Integration

[SVI803.hpp](include/SVI803/SVI803.hpp) exposes `svi803::SVI803`, an `svi3x8::ExpansionPort` without settings.

```cpp
#include <SVI602/SVI602.hpp>
#include <SVI803/SVI803.hpp>

svi602::SVI602 expander;
expander.insertCard(std::make_unique<svi803::SVI803>());
```

## In the SVI-318 and SVI-328

In the frontend the card is **Expander > Slot > SVI-803 16 KB RAM**; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi803` | Static library | `svi803.lib` |
| `svi803_tests` | Console application | `svi803_tests.exe` |

## Tests

svi803_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI803/tests/svi803_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI803/tests/svi803_tests.cpp)) checks the 16 KiB at `8000h`-`BFFFh` on the machine.
