# Printer

The `printer` static library models a parallel (Centronics) printer as seen from its port: a data latch, a /STROBE input and a status byte.

## Functionality

- A rising edge on /STROBE appends the latched byte to the output buffer once.
- `status()` reads `0xFD`: bit 1 is the BUSY line and is clear to report that the emulated printer is ready.
- `reset()` clears the latch and the strobe state; bytes already captured stay in `output()` until `clearOutput()`.

## Interface and Integration

[Printer.hpp](include/PRINTER/Printer.hpp) exposes `printer::Printer` with `writeData()`, `writeStrobe()`, `status()`, `reset()`, `output()` and `clearOutput()`.

## In the SVI-318 and SVI-328

The printer hangs on the Centronics port built into the expanders [SVI-605B](../../svi3x8/SVI605B/README.md) and [SVI-608M](../../svi3x8/SVI608M/README.md): `10h` data, `11h` bit 0 /STROBE, `12h` bit 0 BUSY. The BASIC prints with `LPRINT` and `LLIST`; it waits while BUSY is set, writes the byte to `10h` and pulses /STROBE with `00h` and `FFh` on `11h`. The LAN card of the SVI-609 carries a second printer port (see [MC6821](../MC6821/README.md)). The frontend writes the output to text files; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln), [VG8235.sln](../../vg8235/Development/VG8235.sln), [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-728](../../svi728/Development/run_tests.ps1), [VG-8235](../../vg8235/Development/run_tests.ps1), [SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `printer` | Static library | `printer.lib` |
| `printer_tests` | Console application | `printer_tests.exe` |

## Tests

[printer_tests.cpp](tests/printer_tests.cpp) checks strobe capture, status, reset behavior and clearing the output.
