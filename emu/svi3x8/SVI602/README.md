# SVI-602

SVI602 is a C++17 static library for the Spectravideo SVI-602 Mini Expander: one slot for an interface cartridge on the expansion connector of the SVI-318 and SVI-328.

## Functionality

- One cartridge slot. Every memory and I/O cycle, CPUCLK (`tick()`), /RST (`reset()`) and /INT pass straight between the computer and the cartridge.
- With the slot empty the bus stays open: data `FFh`, no /ROMDIS, /RAMDIS or /INT.
- No disk drives and no printer port; a device of its own is always a cartridge in the slot.

## Interface and Integration

[SVI602.hpp](include/SVI602/SVI602.hpp) exposes `svi602::SVI602`, an `svi3x8::ExpansionPort`. `insertCard()` places a cartridge and returns the one it replaces, `removeCard()` takes it out, `card()` reports it. Cartridges are `svi3x8::ExpansionPort` implementations, for example `svi801::SVI801` from [SVI801](../SVI801/README.md) or `svi807::SVI807` from [SVI807](../SVI807/README.md).

```cpp
#include <SVI602/SVI602.hpp>
#include <SVI328/SVI328.hpp>
#include <SVI801/SVI801.hpp>

svi328::SVI328 machine;
svi602::SVI602 expander;
expander.insertCard(std::make_unique<svi801::SVI801>());
machine.connectExpansion(&expander);
machine.reset();
```

## In the SVI-318 and SVI-328

The SVI-602 is the economical way to fit one interface cartridge, for example the SVI-801 disk controller with two SVI-902 drives, the SVI-802 Centronics card or an SVI-807 for further RAM banks. In the frontend it is **Expander > SVI-602 Mini Expander (1 slot)**; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi602` | Static library | `svi602.lib` |
| `svi602_tests` | Console application | `svi602_tests.exe` |

## Tests

svi602_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI602/tests/svi602_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI602/tests/svi602_tests.cpp)) checks the open bus with an empty slot, the pass-through of memory and I/O cycles, CPUCLK, /RST and /INT to a test cartridge, removing it, and on the machine an SVI-807 set to S2 in the SVI-602 that adds RAM bank 22.
