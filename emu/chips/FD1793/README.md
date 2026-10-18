# FD1793

FD1793 is a C++17 static library modeling the Western Digital / Fujitsu FD1793 floppy disk controller (true data bus) as used with 5.25" drives at 1 MHz. A host clocks it in its own clock (for the SVI-318 and SVI-328 the CPU clock) and connects one drive at a time through the `fd1793::Drive` interface.

## Functionality

- Registers: command/status (0), track (1), sector (2), data (3); INTRQ and DRQ outputs; side select and DDEN as inputs from the board.
- Type I: RESTORE, SEEK, STEP, STEP IN, STEP OUT with update flag, head load flag, verify (30 ms settling, ID field with the track register value within five revolutions, otherwise seek error) and step rates 6, 12, 20 and 30 ms. RESTORE steps out until TR00, at most 255 steps.
- Type II: READ SECTOR and WRITE SECTOR, single and multiple, 30 ms delay flag, side compare. Sector search within five revolutions (record not found), write protect, lost data on unserved DRQ. Data runs at 32 us per byte in MFM and 64 us in FM; after the last byte the two CRC bytes pass before INTRQ.
- Type III: READ ADDRESS (six ID bytes with CRC, track into the sector register), READ TRACK (one revolution: 6250 MFM or 3125 FM bytes with gaps, marks and CRCs), WRITE TRACK (F5h/F6h/F7h conventions; the ID and data fields found are handed to the drive as formatted sectors).
- Type IV: FORCE INTERRUPT with immediate interrupt, index pulse, ready and not-ready conditions.
- Status bits per command type; NOT READY, WRITE PROTECT, TR00 and INDEX follow the drive. Reading status clears INTRQ (not after an immediate interrupt); reading or writing the data register clears DRQ.
- The head unloads after 15 idle revolutions. Master reset sets the sector register to 1 and runs RESTORE (03h).
- `crc16()` computes the CRC-16/CCITT used in ID and data fields.

## Interface and Integration

[FD1793.hpp](include/FD1793/FD1793.hpp) exposes `fd1793::FD1793` and the `fd1793::Drive` interface. The drive reports ready, write protect, TR00, the index hole and its rotation position, steps the head, and serves the sectors on its current cylinder for a side and density (`sectors()`, `readSector()`, `writeSector()`, `formatTrack()`). `setDrive()` selects the drive (nullptr: none), `setSide()` and `setDoubleDensity()` set the board outputs, `tick()` advances one host clock.

## In the SVI-318 and SVI-328

The disk unit of the expanders [SVI-605B](../../svi3x8/SVI605B/README.md) and [SVI-608M](../../svi3x8/SVI608M/README.md) clocks the controller with the CPU clock: `30h`-`33h` are its registers, `34h` reads INTRQ (bit 7) and DRQ (bit 6) and selects drive and motors when written, `38h` sets density and side. SVI disks have 40 cylinders; track 0 of side 0 is FM with 18 sectors of 128 bytes, all other tracks MFM with 17 sectors of 256 bytes. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `fd1793` | Static library | `fd1793.lib` |
| `fd1793_tests` | Console application | `fd1793_tests.exe` |

## Tests

[fd1793_tests.cpp](tests/fd1793_tests.cpp) checks register read-back, RESTORE, SEEK with verify, STEP with and without update, a failing verify, the 30 ms step rate, the index bit, READ SECTOR (single, multiple up to a missing sector, lost data, FM, wrong track after five revolutions, not ready), WRITE SECTOR (stored data, lost data, write protect), READ ADDRESS with CRC, READ TRACK length, WRITE TRACK formatting, FORCE INTERRUPT conditions and the master reset.
