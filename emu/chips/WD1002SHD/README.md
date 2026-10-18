# WD1002-SHD

WD1002SHD is a C++17 static library modeling the Western Digital WD1002-SHD Winchester disk controller as a SASI target with one hard disk (Seagate ST-212: 306 cylinders, 4 heads, 32 sectors of 256 bytes). The disk content is a flat image of sectors in logical order.

## Functionality

- SASI target ID 0: selection with ID bit 0, then Command phase; six-byte command blocks; Data In or Data Out as the command needs; Status byte (bit 1 error flag, bits 5-7 LUN) and completion byte `00h`; then bus free. A bus reset returns to bus free.
- Bus phases in the bit order of the SVI-608M host adapter: BSY bit 0, REQ bit 1, MSG bit 2, C/D bit 3, I/O bit 4 (Command `0Bh`, Data Out `03h`, Data In `13h`, Status `1Bh`, Message In `1Fh`).
- Commands: Test Drive Ready, Restore, Request Status (four bytes: bit 7 address valid plus error code, LUN and 21-bit address), Format Drive / Check Track Format / Format Track / Format Bad Track (data fields `6Ch` or the sector buffer), Read and Write (block count 0 = 256), Seek, Set Parameters (8 bytes; for both drives, bytes 1-5 of the command block are not used), Return Burst Error Length, Format Alternate Track, Write/Read Sector Buffer, RAM/Drive/Controller Diagnostic, Read/Write Long (sector plus four ECC bytes).
- Error codes: `04h` drive not ready (no image, or LUN other than 0), `20h` invalid command, `21h` sector address beyond the capacity set by the parameters. Power-up parameters are those of the ST-506 (153 cylinders, 4 heads).
- A shorter image reads `E5h` beyond its end and grows when written there.

## Interface and Integration

[WD1002SHD.hpp](include/WD1002SHD/WD1002SHD.hpp) exposes `wd1002shd::WD1002SHD` with the bus side `reset()`, `select()`, `signals()`, `writeData()`, `readData()` and the disk side `attachDisk()`, `detachDisk()`, `hasDisk()`, `image()`, `modified()`, `markSaved()`, `cylinders()`, `heads()`. `St212Size` is the size of a full ST-212 image (10027008 bytes).

## In the SVI-318 and SVI-328

The SASI host adapter of the [SVI-608M](../../svi3x8/SVI608M/README.md) reaches the controller through `40h`-`46h`; CP/M 2.27 for the SVI-608M uses the ST-212 as four drives. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `wd1002shd` | Static library | `wd1002shd.lib` |
| `wd1002shd_tests` | Console application | `wd1002shd_tests.exe` |

## Tests

[wd1002shd_tests.cpp](tests/wd1002shd_tests.cpp) runs complete SASI transactions: selection by ID bit 0, bus reset, Test Drive Ready without and with a disk, Request Status, invalid opcode, LUN 1, Set Parameters with the command block of the CP/M 2.27 BIOS, write and read of single and multiple sectors up to the last partition sector, an address beyond the capacity, the sector buffer and Format Track.
