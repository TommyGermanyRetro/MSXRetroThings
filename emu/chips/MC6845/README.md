# MC6845

MC6845 is a C++17 static library modeling the Motorola MC6845 CRT controller as far as a host needs it to draw a character display: registers, display geometry, start and cursor address, and the cursor blink over counted fields.

## Functionality

- Address register selecting R0-R17 and a data register. Registers keep only their defined bits (R3 four bits, R4/R6/R7 seven, R5/R9/R11 five, R8 two, R10 seven, R12/R14 six); R14-R17 read back, the other registers read `00h`; R16/R17 (light pen) are read-only.
- Geometry: R1 characters per row, R6 displayed rows, R9 + 1 raster lines per row, start address R12/R13, cursor address R14/R15.
- Field length in character clocks: `(R0 + 1) x ((R4 + 1) x (R9 + 1) + R5)`. `tick()` counts character clocks at `characterClockHz` against the host clock and counts fields.
- Cursor between raster lines R10 bits 0-4 and R11; blink mode R10 bits 5-6: steady, off, every 16 or every 32 fields.

## Interface and Integration

[MC6845.hpp](include/MC6845/MC6845.hpp) exposes `mc6845::MC6845(hostClockHz, characterClockHz)` with `selectRegister()`, `writeRegister()`, `readRegister()`, `tick()`, `registerValue()`, the geometry accessors, `cursorVisible(rasterLine)`, `characterClocksPerField()` and `fieldCount()`. The host keeps video RAM and character generator and draws the picture from these values.

## In the SVI-318 and SVI-328

The 80-column card SVI-806 of the [SVI-605B](../../svi3x8/SVI605B/README.md) places the CRTC at `50h` (register select) and `51h` (data) with a 2 MHz character clock; the frontend draws its picture in a green panel beside the TV picture. Disk BASIC uses it after `WIDTH 80`, CP/M sends its console there. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `mc6845` | Static library | `mc6845.lib` |
| `mc6845_tests` | Console application | `mc6845_tests.exe` |

## Tests

[mc6845_tests.cpp](tests/mc6845_tests.cpp) programs the CRTC table of the SVI-318/328 BASIC ROM (80 x 24, 9 raster lines), checks register masks and read-back, the start and cursor address, the field length of 29150 character clocks, the cursor raster range and the blink modes over 32 fields.
