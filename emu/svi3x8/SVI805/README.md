# SVI-805

SVI805 is a C++17 static library for the Spectravideo SVI-805 RS-232 interface cartridge. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **SVI-805** RS-232 card ([SVI805.hpp](include/SVI805/SVI805.hpp)): [INS8250](../../chips/INS8250/README.md) at `28h`-`2Fh` with a 3.072 MHz crystal (divisor 20 = 9600 baud), its interrupt on /INT. TXD, DTR and RTS go to a `SerialLink`, RXD, CTS, DSR and DCD come from it (RI is not connected). Every millisecond the card passes sent characters on, takes received ones and follows line settings and modem lines; received characters enter the UART at line speed. `ComPortLink` opens a Windows COM port as the link, for example a serial port connected through a null-modem cable to a terminal; `availablePorts()` lists the system's COM ports.

## Interface and Integration

[SVI805.hpp](include/SVI805/SVI805.hpp) exposes `svi805::SVI805`, an `svi3x8::ExpansionPort` built with a `svi805::SerialLink` (or none), `setLink()` to change it and `uart()` for the [INS8250](../../chips/INS8250/README.md); `svi805::ComPortLink` is the link to a Windows COM port.

```cpp
#include <SVI805/SVI805.hpp>

svi805::ComPortLink link;
link.open(L"COM3");
auto card = std::make_unique<svi805::SVI805>(&link);
```

## In the SVI-318 and SVI-328

In the frontend the card is **Expander > Slot > SVI-805 RS-232**; **Expander > RS-232 (SVI-805)** chooses the COM port and is available only while an SVI-805 is fitted; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi805` | Static library | `svi805.lib` |
| `svi805_tests` | Console application | `svi805_tests.exe` |

## Tests

svi805_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI805/tests/svi805_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI805/tests/svi805_tests.cpp)): the RS-232 test runs an SVI-805 against a test link: 9600 8N1 and DTR/RTS reach the link, two sent characters arrive, two received characters reach the receiver buffer with the receive interrupt, CTS, DSR and DCD appear in MSR without RI, and opening a missing COM port fails.
