# LAN-IF-01

LANIF01 is a C++17 static library for the LAN-IF-01 board in its two versions: the boot card of the [SVI-608M](../SVI608M/README.md) and the fully fitted LAN card of the SVI-609 Master Station. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **Boot card** LAN-IF-01 ([LANIF01.hpp](include/LANIF01/LANIF01.hpp)) with the 4 KiB EPROM "NETROM 1.0" (`rom/svi609.rom`, SHA1 `73f9717e...`): after /RST it asserts /ROMDIS and supplies the EPROM in place of the BASIC ROM, mirrored every 4 KiB through `0000h`-`7FFFh`, for reads only; a write to `6Fh` switches it off until the next reset. The EPROM starts from floppy drive A when a disk answers there, otherwise from the SASI hard disk.
- **LAN card** of the SVI-609 ([LanCard.hpp](include/LANIF01/LanCard.hpp)): the same boot logic and EPROM (the original EPROM of the LAN card is not available), plus the printer port ([PiaPrinterPort.hpp](include/LANIF01/PiaPrinterPort.hpp)): [MC6821](../../chips/MC6821/README.md) at `60h`-`63h`; port A carries the data inverted, CA2 drives /STROBE through an inverter, /ACK goes to CA1, BUSY to PB0 (other port B lines high), the PIA interrupt to /INT. The CP/M 2.27 BIOS detects it by switching CRB bit 2 between DDRB and PRB and prints on it; without it CP/M 2.27 prints through the Centronics port. The [Z8530](../../chips/Z8530/README.md) SCC, the RS-422 drivers and the station DIP switches (1-7 station number, 8 loopback) are not connected: their ports are not documented.

## Interface and Integration

[LANIF01.hpp](include/LANIF01/LANIF01.hpp) exposes `lanif01::LANIF01`, [LanCard.hpp](include/LANIF01/LanCard.hpp) `lanif01::LanCard`, both an `svi3x8::ExpansionPort`; `loadRom()` takes the EPROM of `lanif01::BootRomSize` bytes. `LanCard::printerPort()` gives the `lanif01::PiaPrinterPort` with its `printer()`.

```cpp
#include <LANIF01/LanCard.hpp>

auto card = std::make_unique<lanif01::LanCard>();
card->loadRom(eprom);   // rom/svi609.rom
const auto& output = card->printerPort().printer().output();
```

## In the SVI-318 and SVI-328

The SVI-608M has the boot card fitted from the factory, the SVI-609 the LAN card. In the frontend they are **Expander > Slot > LAN-IF-01 boot card** (SVI-608M only) and **LAN-IF-01 LAN card** (SVI-609 only); the SVI-318 frontend offers only the boot card. The EPROM is `rom\svi609.rom` in this folder or `svi609.rom` next to the program; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `lanif01` | Static library | `lanif01.lib` |
| `lanif01_tests` | Console application | `lanif01_tests.exe` |

## Tests

lanif01_tests.cpp ([SVI-318](../../svi318/Development/CardTests/LANIF01/tests/lanif01_tests.cpp), [SVI-328](../../svi328/Development/CardTests/LANIF01/tests/lanif01_tests.cpp)) checks the boot card on the machine: after reset the EPROM replaces the BASIC ROM, mirrored every 4 KiB, with RAM bank 2 above; a write to `6Fh` brings the BASIC ROM back; reset switches the EPROM on again; an EPROM of the wrong size is refused. For the LAN card it checks the same boot logic and runs the printer routines of the CP/M 2.27 BIOS (initialisation, detection found on the LAN card and not found on an SVI-608M, printing `HII` with the inverted data and the CA2 strobe, the /ACK interrupt and its clearing).
