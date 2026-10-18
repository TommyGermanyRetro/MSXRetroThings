# LAN-HD-01

LANHD01 is a C++17 static library for the LAN-HD-01 SASI host adapter of the [SVI-608M](../SVI608M/README.md) and the [SVI-605B with hard disk](../SVI605BHD/README.md) with the [WD1002-SHD](../../chips/WD1002SHD/README.md) and its hard disk. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **SASI host adapter** LAN-HD-01 ([LANHD01.hpp](include/LANHD01/LANHD01.hpp)) with the [WD1002-SHD](../../chips/WD1002SHD/README.md) and its hard disk: `40h` bus reset, `41h` data out with automatic ACK (selection ID in bus free), `42h` data in with automatic ACK, `43h` SEL, `44h` without effect, `46h` bus signals (bits 0-4) and bit 5 = target connected.

## Interface and Integration

[LANHD01.hpp](include/LANHD01/LANHD01.hpp) exposes `lanhd01::LANHD01`, an `svi3x8::ExpansionPort`; `controller()` returns the WD1002-SHD with `attachDisk()` for the image.

```cpp
#include <LANHD01/LANHD01.hpp>

auto card = std::make_unique<lanhd01::LANHD01>();
card->controller().attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
```

## In the SVI-318 and SVI-328

The SVI-608M has the adapter fitted from the factory; the SVI-605B with hard disk has it built in. In the frontend it is **Expander > Slot > LAN-HD-01 hard disk adapter** (SVI-608M only); **Expander > Hard disk** chooses the image; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `lanhd01` | Static library | `lanhd01.lib` |
| `lanhd01_tests` | Console application | `lanhd01_tests.exe` |

## Tests

lanhd01_tests.cpp ([SVI-318](../../svi318/Development/CardTests/LANHD01/tests/lanhd01_tests.cpp), [SVI-328](../../svi328/Development/CardTests/LANHD01/tests/lanhd01_tests.cpp)) runs TEST UNIT READY through the ports: bus free after reset (`46h` = `20h`), selection of ID 0 to the Command phase (`2Bh`), the six command bytes to the Status phase (`3Bh`), the status byte to Message In (`3Fh`), Command Complete (`00h`) and bus free again; other ports read `FFh` without /INT. CP/M 2.27 on the hard disk runs in the tests of the [SVI-608M](../SVI608M/README.md).
