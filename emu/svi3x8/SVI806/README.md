# SVI-806

SVI806 is a C++17 static library for the Spectravideo SVI-806 80-column cartridge. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **SVI-806** 80-column card ([SVI806.hpp](include/SVI806/SVI806.hpp)): [MC6845](../../chips/MC6845/README.md) at `50h` (register select) and `51h` (data) with a 2 MHz character clock, 2 KiB video RAM in `F000h`-`FFFFh` (mirrored every 2 KiB, /RAMDIS) while port `58h` holds a value other than 0, and a 4 KiB character ROM with 16 lines per character (`rom/svi806.rom`, English, SHA1 `ed45cb0e...`). The ROM starts with the space: code `21h` is `A`. `render()` returns a monochrome picture of R1 x 8 by R6 x (R9 + 1) pixels; the cursor fills its raster lines; `peekVideoRam()` reads the character codes. Disk BASIC uses the card after `WIDTH 80`.

## Interface and Integration

[SVI806.hpp](include/SVI806/SVI806.hpp) exposes `svi806::SVI806`, an `svi3x8::ExpansionPort`; `loadCharacterRom()` takes the 4 KiB character ROM, `render()` returns the picture as `svi806::MonochromeFrame`.

```cpp
#include <SVI806/SVI806.hpp>

auto card = std::make_unique<svi806::SVI806>();
card->loadCharacterRom(characterRom);   // rom/svi806.rom
const auto frame = card->render();
```

## In the SVI-318 and SVI-328

In the frontend the card is **Expander > Slot > SVI-806 80 columns**; its picture appears in a green panel next to the TV picture. The SVI-608M has it fitted from the factory; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi806` | Static library | `svi806.lib` |
| `svi806_tests` | Console application | `svi806_tests.exe` |

## Tests

svi806_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI806/tests/svi806_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI806/tests/svi806_tests.cpp)): the SVI-806 test programs the CRTC table of the BASIC ROM through the machine, maps the video RAM with `58h` and checks that RAM bank 2 returns without it, then renders 640 x 216 pixels and compares the ROM's `A` and the cursor cell. Argument: character ROM `svi806.rom`.
