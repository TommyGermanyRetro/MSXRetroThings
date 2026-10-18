# SVI-605B with hard disk

SVI605BHD is a C++17 static library for the Spectravideo SVI-605B Super Expander in its version with hard disk. It builds on the expander main board of the [SVI605B](../SVI605B/README.md) library and connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- The main board of the SVI-605B (`svi605b::ExpanderBoard`: shared bus, Centronics port at `10h`-`12h`, FD1793 at `30h`-`38h`) with four card slots and only drive A, double-sided; selecting drive B selects no drive. A blank disk for formatting is double-sided.
- In place of drive B a Seagate ST-212 (10 MB) on a [WD1002-SHD](../../chips/WD1002SHD/README.md), behind the built-in SASI host adapter [LAN-HD-01](../LANHD01/README.md) at `40h`-`46h`. The adapter is part of the expander and takes no slot.
- The adapter has no ROM, so there is no start from the hard disk: the BASIC ROM starts from the floppy, and CP/M 2.27 (the version for the SVI-608M) sets the drive to 306 cylinders and 4 heads and uses the hard disk as drives B: to E:. Disk BASIC and the BASIC ROM do not use the SASI ports.

## Interface and Integration

[SVI605BHD.hpp](include/SVI605BHD/SVI605BHD.hpp) exposes `svi605bhd::SVI605BHD`, an `svi605b::ExpanderBoard` with `SlotCount` 4 and `DriveCount` 1; all slot, drive and printer functions of the board apply. `sasi()` gives the built-in `lanhd01::LANHD01`; its `controller()` is the WD1002-SHD with `attachDisk()`, `image()` and `modified()`.

```cpp
#include <SVI605BHD/SVI605BHD.hpp>
#include <SVI328/SVI328.hpp>

svi328::SVI328 machine;
svi605bhd::SVI605BHD expander;
expander.sasi().controller().attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
machine.connectExpansion(&expander);
machine.reset();
```

## In the SVI-318 and SVI-328

In the frontend the expander is **Expander > SVI-605B with hard disk**; **Expander > Hard disk** chooses the image. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi605bhd` | Static library | `svi605bhd.lib` |
| `svi605bhd_tests` | Console application | `svi605bhd_tests.exe` |

## Tests

svi605bhd_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI605BHD/tests/svi605bhd_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI605BHD/tests/svi605bhd_tests.cpp)) checks the board (four free slots, one drive, the adapter at `46h` with bus free, selection and bus reset). With the CP/M 2.27 disk as second argument it starts CP/M from the floppy through the BASIC ROM with a blank ST-212, checks 306 cylinders and 4 heads, `STAT E:` with 3104k free and a PIP copy to drive B: on the hard disk. Arguments: BASIC ROM, the CP/M 2.27 image or `-`.
