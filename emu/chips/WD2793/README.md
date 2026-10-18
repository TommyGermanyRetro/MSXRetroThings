# WD2793

WD2793 is a standalone C++17 static library modeling the Western Digital WD2793 floppy disk controller together with a 3.5" double-density drive that holds a raw `.dsk` image. Source: WD279X-02 data sheet in the Western Digital Storage Management Products Handbook 1984 (excerpt in `9999 - MSX - Dokumentation`); drive signals follow the observable behaviour of openMSX.

## Functionality

- Registers: status, track, sector, data and command; INTRQ and DRQ outputs.
- Type I: Restore, Seek, Step, Step-in, Step-out with the step rates 6/12/20/30 ms (1 MHz clock), track register update (T), head load (h), verify (V) with 30 ms settling and a search for a matching ID field within five revolutions, seek error, Restore giving up after 255 steps without TRACK 00.
- Type II: Read Sector and Write Sector with side compare (C, S), 30 ms delay (E), multiple records (m), RECORD NOT FOUND after five revolutions, LOST DATA for bytes the host misses, write protect.
- Type III: Read Address (sector register receives the cylinder), Read Track (one revolution in IBM System 34 layout with CRCs), Write Track (formatting; F5 writes A1, F7 the CRC, data fields after ID fields with size code 2 are stored).
- Type IV: Force Interrupt with immediate (I3) and index-pulse (I2) interrupts; D0 stops a command without INTRQ.
- Status as in table 4 of the data sheet: Type I with NOT READY, WRITE PROTECT, HEAD LOADED, SEEK ERROR, TRACK 00, INDEX and BUSY; Type II/III with RECORD NOT FOUND, LOST DATA, DRQ and BUSY.
- Disk timing: 300 rpm, 32 us per byte (250 kbit/s MFM), 6250 bytes per track, sectors 1 to 9 (or 8) one after the other behind the index pulse.
- `FloppyDrive`: 320, 360, 640 and 720 KiB images, single- or double-sided drive, head position 0-83, motor, side select, READY, TRACK 00 and WRITE PROTECT (the latter two only with the motor on, like Philips drives), disk change flag, modified flag.
- `saveState()` and `loadState()` keep registers, command phase, timing and sector buffer of the controller, and motor, side and head cylinder of a `FloppyDrive`, for snapshots; the connected drive and the disk image are not part of the state.

## Interface and Integration

The public API is in [WD2793.hpp](include/WD2793/WD2793.hpp) and [FloppyDrive.hpp](include/WD2793/FloppyDrive.hpp). The host maps the registers into its address space, calls `selectDrive()` from its drive select latch and `advance(elapsed)` with clocks of the frequency given to the constructor. `readStatus()` clears INTRQ and `readData()`/`writeData()` clear DRQ; `peekStatus()` and `peekData()` have no side effects.

```cpp
#include <WD2793/WD2793.hpp>

wd2793::FloppyDrive drive;
wd2793::WD2793 fdc(3579545);
drive.insert(imageBytes, false);
drive.setMotor(true);
fdc.selectDrive(&drive);
fdc.writeSector(1);
fdc.writeCommand(0x80);              // Read Sector
while (fdc.busy()) {
    fdc.advance(1);
    if (fdc.dataRequest()) consume(fdc.readData());
}
```

For integration into another Visual Studio solution, add [wd2793.vcxproj](wd2793.vcxproj), reference it from the host project and add this project's `include` directory to the host's include directories. There are no external library dependencies.

## Limits

- Sector-level disk model: every track holds the sectors of the image in standard layout. Copy-protection tricks, deleted data marks, CRC errors and non-standard sector sizes cannot be represented; Write Track keeps only standard 512-byte data fields.
- Single density (FM) and the WD2795/2797 side output are not modeled.
- The Ready transition interrupts I0/I1 of Force Interrupt are not generated.
- Head load timing (HLT) is taken as always ready; head unloading after 15 idle index pulses is not modeled.

## Build and Test

The project is used by the solution [VG8235.sln](../../vg8235/Development/VG8235.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([VG-8235](../../vg8235/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `wd2793` | Static library | `wd2793.lib` |
| `wd2793_tests` | Console application | `wd2793_tests.exe` |

## Test Coverage

[wd2793_tests.cpp](tests/wd2793_tests.cpp) checks the drive (image sizes, single-sided drives, signals with motor off, head limits, disk change), all Type I commands with step rates, verify and the 255-step limit, Read Sector with LOST DATA, RECORD NOT FOUND, side compare, multiple records and track mismatch, Write Sector with write protect and NOT READY, Read Address, Read Track, Write Track formatting, and Force Interrupt with immediate and index interrupts.
