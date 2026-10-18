# SVI-801

SVI801 is a C++17 static library for the Spectravideo SVI-801 floppy disk controller cartridge with its two SVI-902 drives, and for the disk unit that the main board of the [SVI-605B](../SVI605B/README.md) and [SVI-608M](../SVI608M/README.md) has built in. It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- **SVI-801** disk controller cartridge ([SVI801.hpp](include/SVI801/SVI801.hpp)): a `DiskUnit` with two single-sided SVI-902 drives; a blank disk for formatting is single-sided.
- Disk unit ([DiskUnit.hpp](include/SVI801/DiskUnit.hpp)): [FD1793](../../chips/FD1793/README.md) controller and double-sided 5.25" drives ([FloppyDrive.hpp](include/SVI801/FloppyDrive.hpp)). `30h`-`33h` controller registers; `34h` read: bit 7 INTRQ, bit 6 DRQ; `34h` write: bit 0 select drive A, bit 1 drive B (both, none or a drive the unit does not have: no drive), bit 2 motor A, bit 3 motor B; `38h` write: bit 0 density (1 = FM), bit 1 side. A drive is ready with a disk and its motor on; the disk turns at 300 rpm with a 2 ms index pulse; the head moves over cylinders 0-41.
- SVI disk images (`.dsk`): 40 cylinders, one or two sides (172032 or 346112 bytes), stored cylinder by cylinder with side 0 before side 1. Track 0 on side 0 has 18 FM sectors of 128 bytes, all other tracks 17 MFM sectors of 256 bytes. A track can be read only in its own density. WRITE TRACK with this layout writes the sector data; other layouts are not stored. A blank disk is formatted with `E5h`.
- Formatting an empty drive: when software issues WRITE SECTOR or WRITE TRACK to a selected drive without a disk, the disk unit first inserts a blank disk (`E5h`, SVI format; single-sided on the SVI-605B and SVI-801, double-sided on the SVI-608M), so that FORMAT of Disk BASIC or FORMAT.COM of CP/M works without a prepared image; `hasBlankDisk()` tells that the disk has no file yet. Reading an empty drive still fails. `setBlankDiskOnFormat(false)` switches this off.

## Interface and Integration

[SVI801.hpp](include/SVI801/SVI801.hpp) exposes `svi801::SVI801`, an `svi3x8::ExpansionPort`; `disks()` returns its `svi801::DiskUnit`. [DiskUnit.hpp](include/SVI801/DiskUnit.hpp) gives `drive(0)` and `drive(1)` (`svi801::FloppyDrive`), `driveControl()` and `setBlankDiskOnFormat()`; [FloppyDrive.hpp](include/SVI801/FloppyDrive.hpp) holds the images (`svi801::SviDisk`). The main board `svi605b::ExpanderBoard` uses the same `DiskUnit` as its built-in disk unit.

```cpp
#include <SVI602/SVI602.hpp>
#include <SVI801/SVI801.hpp>
#include <SVI328/SVI328.hpp>

svi801::SviDisk disk;
disk.load(image);
auto card = std::make_unique<svi801::SVI801>();
card->disks().drive(0).insert(disk);
svi602::SVI602 expander;
expander.insertCard(std::move(card));
svi328::SVI328 machine;
machine.connectExpansion(&expander);
machine.reset();
```

## In the SVI-318 and SVI-328

The SVI-801 plugs into the SVI-601 (only into SK6) or the SVI-602. In the frontend it is **Expander > Slot > SVI-801 disk controller**; its drives appear as **Expander > Drive A / B**; see the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props) (on top of the chip settings in [chips](../../chips/Common.props)): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `svi801` | Static library | `svi801.lib` |
| `svi801_tests` | Console application | `svi801_tests.exe` |

## Tests

svi801_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI801/tests/svi801_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI801/tests/svi801_tests.cpp)) checks the disk images (only 172032 and 346112 bytes load, the layout of single- and double-sided images, sector writes with size check and modified flag) and the drive (not ready without the motor, the sectors offered on track 0 per side and density, the index hole while the motor runs). Disk BASIC through an SVI-801 runs in the tests of the [SVI-601](../SVI601/README.md), through the built-in disk unit in those of the [SVI-605B](../SVI605B/README.md).
