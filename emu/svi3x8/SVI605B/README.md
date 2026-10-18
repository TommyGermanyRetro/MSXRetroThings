# SVI-605B

SVI605B is a C++17 static library for the Spectravideo SVI-605B Super Expander and its main board (shared with the [SVI-608M](../SVI608M/README.md)). It connects to the SVI-318 and SVI-328 through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- Main board ([ExpanderBoard.hpp](include/SVI605B/ExpanderBoard.hpp)) with card slots on a shared bus: every card sees each memory and I/O cycle, CPUCLK and /RST. Data combines as on an open bus (AND of all drivers, `FFh` when none drives); /ROMDIS, /RAMDIS and /INT are wired-OR. The SVI-605B has four slots and drives A and B.
- Built-in Centronics printer port with the [PRINTER](../../chips/PRINTER/README.md) library: `10h` data (write), `11h` bit 0 /STROBE (write), `12h` bit 0 BUSY (read, `FEh` while the printer is ready). The printer latches the byte on the rising /STROBE edge; `LPRINT` and `LLIST` of the BASIC ROM print directly.
- Built-in disk unit: [FD1793](../../chips/FD1793/README.md) controller and double-sided 5.25" drives ([FloppyDrive.hpp](../SVI801/include/SVI801/FloppyDrive.hpp)). `30h`-`33h` controller registers; `34h` read: bit 7 INTRQ, bit 6 DRQ; `34h` write: bit 0 select drive A, bit 1 drive B (both, none or a drive the board does not have: no drive), bit 2 motor A, bit 3 motor B; `38h` write: bit 0 density (1 = FM), bit 1 side. A drive is ready with a disk and its motor on; the disk turns at 300 rpm with a 2 ms index pulse; the head moves over cylinders 0-41.
- SVI disk images (`.dsk`): 40 cylinders, one or two sides (172032 or 346112 bytes), stored cylinder by cylinder with side 0 before side 1. Track 0 on side 0 has 18 FM sectors of 128 bytes, all other tracks 17 MFM sectors of 256 bytes. A track can be read only in its own density. WRITE TRACK with this layout writes the sector data; other layouts are not stored. A blank disk is formatted with `E5h`.
- Formatting an empty drive: when software issues WRITE SECTOR or WRITE TRACK to a selected drive without a disk, the expander first inserts a blank disk (`E5h`, SVI format; single-sided on the SVI-605B, double-sided when the board is built for it), so that FORMAT of Disk BASIC or FORMAT.COM of CP/M works without a prepared image; `hasBlankDisk()` tells that the disk has no file yet. Reading an empty drive still fails. `setBlankDiskOnFormat(false)` switches this off.
- The BASIC ROM boots by itself from drive A: it restores both drives, waits for the index pulse (about 2 s without a disk), loads track 0, sector 1 to `C100h` and jumps there.
- The disk unit is `svi801::DiskUnit` from [SVI801](../SVI801/README.md), the printer port `svi802::CentronicsPort` from [SVI802](../SVI802/README.md), the slots `svi605b::CardCage`; `svi605b::ExpanderBoard` combines them. The cards are projects of their own: [SVI-801](../SVI801/README.md), [SVI-802](../SVI802/README.md), [SVI-803](../SVI803/README.md), [SVI-805](../SVI805/README.md), [SVI-806](../SVI806/README.md), [SVI-807](../SVI807/README.md).

## Interface and Integration

[ExpanderBoard.hpp](include/SVI605B/ExpanderBoard.hpp) exposes `svi605b::ExpanderBoard`, an `svi3x8::ExpansionPort` built with a slot count and a drive count; [SVI605B.hpp](include/SVI605B/SVI605B.hpp) derives `svi605b::SVI605B` with `SlotCount` 4 and `DriveCount` 2. `slotCount()` and `driveCount()` report the fit. `insertCard()` places a card in a slot below `slotCount()` and returns the card it replaces, `removeCard()` takes it out, `card()` reports it. `printer()` gives the printer on the Centronics port with its captured `output()`. `drive(0)`/`drive(1)` give drives A and B (`insert()` an `SviDisk`, `eject()`, `setWriteProtected()`, `disk()` with `image()` and `modified()`); `setBlankDiskOnFormat()` controls the blank disk for formatting; `diskController()` and `driveControl()` report the controller and port `34h` for status displays. Cards are `svi3x8::ExpansionPort` implementations; the cards themselves are projects of their own, for example `svi807::SVI807` in [SVI807](../SVI807/README.md).

```cpp
#include <SVI328/SVI328.hpp>
#include <SVI807/SVI807.hpp>
#include <SVI605B/SVI605B.hpp>

svi328::SVI328 machine;
svi605b::SVI605B expander;
expander.insertCard(0, std::make_unique<svi807::SVI807>());
machine.connectExpansion(&expander);
machine.reset();
```

The machine does not own the expander; it must stay alive while connected. Changes to the cards take effect at the next cycle; a host resets the machine afterwards as after switching on.

## Tests

svi605b_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI605B/tests/svi605b_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI605B/tests/svi605b_tests.cpp)) checks the slots (open-bus data, wired-OR lines, CPUCLK, /RST and /INT to every card, inserting and removing), the Centronics port (ready status, printing only on the rising /STROBE edge), and boots `svi111.rom` with the expander and an SVI-807 to the BASIC banner, then types `lprint "abc"` and expects `abc`, CR, LF on the printer. The disk test lets the BASIC ROM boot a self-made double-sided disk: its boot sector seeks to track 1 with verify, reads sector 1 in MFM and the test compares the 256 bytes; without a disk BASIC starts after the timeout. With a third argument, the Disk BASIC image `S328DBSS.DSK`, the test boots Disk BASIC 1.0 and runs `SAVE"1:test"`, `NEW`, `LOAD"1:test"`, `RUN` and `FILES`; [run_tests.ps1](../../svi328/Development/run_tests.ps1) passes it when it lies in `2100 - MSX - MEDIEN\SVIDSK\Svi328DiskBasicSS`. It also runs the Disk BASIC program FORMAT on the empty drive 2 and checks the blank disk, the directory and allocation table on track 20 and a program saved on it; with the option switched off an empty drive stays empty. Disk BASIC addresses the drives as `1:` and `2:` (`FILES 2` lists drive 2); without a drive the commands use the cassette. With a fourth argument, the CP/M 2.24 system disk (`SVIDSK\Cpm224SS\CPM224SS.DSK` in the media folder), the test boots it to `A>`, checks that `DIR` lists `STAT.COM` and formats the empty drive B with FORMAT.COM (blank disk, `E5h` throughout). Arguments: BASIC ROM, character ROM `svi806.rom`, the Disk BASIC image and the CP/M 2.24 image, each image or `-`.
