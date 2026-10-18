# SVI-807

SVI807 is a C++17 static library for the Spectravideo SVI-807 64 KiB RAM cartridge. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **SVI-807** 64 KiB RAM card ([SVI807.hpp](include/SVI807/SVI807.hpp)) with DIP switches S1-S5 for banks 21, 22, 31, 32 and 02 (factory setting S2 and S3). One 32 KiB half serves `0000h`-`7FFFh` (banks 21/31) and asserts /ROMDIS, the other `8000h`-`FFFFh` (banks 22/32/02) and asserts /RAMDIS. In bank 02 it stands in for RAM bank 2 and, like the internal RAM, gives way while another card pulls /RAMDIS (the [SVI-806](../SVI806/README.md) video RAM at `F000h`-`FFFFh`), so CP/M runs on the 80-column card. On an SVI-328, which has bank 21 itself, S1 stays off. One card with S2 and S3 gives 128 KiB in all; a second card with S3 and S4 next to one with S2 gives 144 KiB.

## Interface and Integration

[SVI807.hpp](include/SVI807/SVI807.hpp) exposes `svi807::SVI807`, an `svi3x8::ExpansionPort` built with its DIP switches `svi807::SVI807Switches` (banks 21, 22, 31, 32, 02); `switches()` reports them.

```cpp
#include <SVI807/SVI807.hpp>

auto card = std::make_unique<svi807::SVI807>(svi807::SVI807Switches{false, true, false, false, false});
```

## In the SVI-318 and SVI-328

In the frontend the settings that do not overlap are the choices **SVI-807 64 KB RAM** under **Expander > Slot**; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi807` | Static library | `svi807.lib` |
| `svi807_tests` | Console application | `svi807_tests.exe` |

## Tests

svi807_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI807/tests/svi807_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI807/tests/svi807_tests.cpp)) checks the SVI-807 with factory switches on the machine (banks 22 and 31 separate from banks 2 and 21, bank 32 left to RAM bank 2, ROM and RAM bank 2 back without bank signals), two SVI-807 cards for banks 22, 31 and 32.
