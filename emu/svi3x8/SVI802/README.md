# SVI-802

SVI802 is a C++17 static library for the Spectravideo SVI-802 Centronics interface cartridge, and for the Centronics port that the main board of the [SVI-605B](../SVI605B/README.md) and [SVI-608M](../SVI608M/README.md) has built in. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **SVI-802** Centronics interface cartridge ([SVI802.hpp](include/SVI802/SVI802.hpp)): a `CentronicsPort`.
- Centronics port ([CentronicsPort.hpp](include/SVI802/CentronicsPort.hpp)) with the [PRINTER](../../chips/PRINTER/README.md) library: `10h` data (write), `11h` bit 0 /STROBE (write), `12h` bit 0 BUSY (read, `FEh` while the printer is ready). The printer latches the byte on the rising /STROBE edge; `LPRINT` and `LLIST` of the BASIC ROM print directly.

## Interface and Integration

[SVI802.hpp](include/SVI802/SVI802.hpp) exposes `svi802::SVI802`, an `svi3x8::ExpansionPort`; `port()` returns its `svi802::CentronicsPort`, and `port().printer()` the [PRINTER](../../chips/PRINTER/README.md) behind it with the printed bytes. The main board `svi605b::ExpanderBoard` uses the same `CentronicsPort` as its built-in printer port.

```cpp
#include <SVI602/SVI602.hpp>
#include <SVI802/SVI802.hpp>

svi602::SVI602 expander;
expander.insertCard(std::make_unique<svi802::SVI802>());
// after LPRINT:
auto* card = dynamic_cast<svi802::SVI802*>(expander.card());
const auto& bytes = card->port().printer().output();
```

## In the SVI-318 and SVI-328

The SVI-802 plugs into the SVI-601 or the SVI-602. In the frontend it is **Expander > Slot > SVI-802 Centronics printer**; its output goes to the printer folder (**Expander > Printer folder...**); see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi802` | Static library | `svi802.lib` |
| `svi802_tests` | Console application | `svi802_tests.exe` |

## Tests

svi802_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI802/tests/svi802_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI802/tests/svi802_tests.cpp)) checks the ready status on `12h` and that only the rising /STROBE edge prints the data byte, once. `LPRINT` from BASIC through an SVI-802 runs in the tests of the [SVI-601](../SVI601/README.md).
