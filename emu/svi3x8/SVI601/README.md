# SVI-601

SVI601 is a C++17 static library for the Spectravideo SVI-601 Super Expander: seven cartridge slots on the expansion connector of the SVI-318 and SVI-328.

## Functionality

- Seven slots SK1-SK7 on a shared bus (`svi605b::CardCage`): every cartridge sees each memory and I/O cycle, CPUCLK and /RST; data lines combine as on an open bus, /ROMDIS, /RAMDIS and /INT are wired-OR.
- No built-in drives and no printer port. Disks come with the SVI-801 disk controller cartridge in SK6 (`DiskControllerSlot`) and its two external SVI-902 drives; printing comes with the SVI-802 Centronics cartridge. All other cartridges go into the slots other than SK6.

## Interface and Integration

[SVI601.hpp](include/SVI601/SVI601.hpp) exposes `svi601::SVI601` with `slotCount()`, `insertCard()`, `removeCard()` and `card()`. The cartridges are projects of their own, for example [SVI801](../SVI801/README.md) (`svi801::SVI801`), [SVI802](../SVI802/README.md) (`svi802::SVI802`) and [SVI807](../SVI807/README.md) (`svi807::SVI807`).

```cpp
#include <SVI601/SVI601.hpp>
#include <SVI328/SVI328.hpp>
#include <SVI801/SVI801.hpp>
#include <SVI802/SVI802.hpp>

svi328::SVI328 machine;
svi601::SVI601 expander;
expander.insertCard(svi601::DiskControllerSlot, std::make_unique<svi801::SVI801>());
expander.insertCard(0, std::make_unique<svi802::SVI802>());
machine.connectExpansion(&expander);
machine.reset();
```

## In the SVI-318 and SVI-328

The SVI-601 is the first-generation expander of the SVI-328 (1983). In the frontend it is **Expander > SVI-601 Super Expander (7 slots)**; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi601` | Static library | `svi601.lib` |
| `svi601_tests` | Console application | `svi601_tests.exe` |

## Tests

svi601_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI601/tests/svi601_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI601/tests/svi601_tests.cpp)) checks the seven slots, the SVI-802 at `12h` and the SVI-801 at `31h`, boots the BASIC ROM with an SVI-802 and expects `LPRINT "abc"` on its printer, and with the Disk BASIC image as second argument boots Disk BASIC from drive A of the SVI-801 in SK6 and runs `SAVE"1:test"`, `NEW`, `LOAD"1:test"`, `RUN` and `FILES`. Arguments: BASIC ROM, Disk BASIC image or `-`.
