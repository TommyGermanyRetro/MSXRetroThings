# SVI-608M

SVI608M is a C++17 static library for the Spectravideo SVI-608M Super Expander with hard disk and its own cards. The Network Expander of the [SVI-609](../SVI609/README.md) builds on it. It builds on the expander main board of the [SVI605B](../SVI605B/README.md) library and connects to the SVI-318 and SVI-328 machine through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- The main board of the SVI-605B (`svi605b::ExpanderBoard`: shared bus, Centronics port at `10h`-`12h`, FD1793 at `30h`-`38h`) with five card slots and only drive A, double-sided; selecting drive B selects no drive, and no blank disk is inserted there. CP/M 2.27 uses 40 cylinders on both sides (326 KB): first side 0, cylinders 0-39, then side 1, again from cylinder 0. A blank disk for formatting is double-sided.
- No printer PIA: CP/M 2.27 finds none and prints through the Centronics port. The PIA sat only on the LAN card of the SVI-609 (see [LANIF01](../LANIF01/README.md)).
- **SASI host adapter** [LAN-HD-01](../LANHD01/README.md) with the WD1002-SHD and its hard disk, and **boot card** [LAN-IF-01](../LANIF01/README.md) with the EPROM `svi609.rom`; both are projects of their own.
- Factory cards: SVI-806 80-column card in slot 0, SASI host adapter in slot 1, boot card in slot 2; slots 3 and 4 are free. With the SVI-806 fitted, CP/M 2.27 writes to the 80-column screen only.
- The hard disk works with CP/M 2.27 for the SVI-608M; Disk BASIC and the BASIC ROM do not use the SASI ports. CP/M 2.27 divides it into four drives. Started from floppy, the floppy is A: and the hard disk B: to E:; started from the hard disk, the hard disk is A: to D: (the same four areas in the same order) and the floppy E:.

## Interface and Integration

[SVI608M.hpp](include/SVI608M/SVI608M.hpp) exposes `svi608m::SVI608M`, an `svi605b::ExpanderBoard` with `SlotCount` 5 and `DriveCount` 1; all slot, drive and printer functions of the board apply. `fitFactoryCards()` fits the factory cards from the character ROM `svi806.rom` and the boot EPROM `svi609.rom` into `VideoSlot`, `SasiSlot` and `BootSlot`; with a ROM of the wrong size it fails and leaves the slots unchanged. `bootSource()` tells where the first sector after the last reset came from: `Floppy` (FD1793 data with DRQ), `HardDisk` (Data In of a SASI Read command) or `Unknown`. `lanhd01::LANHD01::controller()` gives the WD1002-SHD for `attachDisk()`, `image()` and `modified()`; `lanif01::LANIF01::loadRom()` takes the 4 KiB EPROM.

```cpp
#include <SVI608M/SVI608M.hpp>
#include <LANHD01/LANHD01.hpp>
#include <SVI328/SVI328.hpp>

svi328::SVI328 machine;
svi608m::SVI608M expander;
expander.fitFactoryCards(characterRom, bootRom);
auto* sasi = dynamic_cast<lanhd01::LANHD01*>(expander.card(svi608m::SasiSlot));
sasi->controller().attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
machine.connectExpansion(&expander);
machine.reset();
```

## Tests

svi608m_tests.cpp ([SVI-318](../../svi318/Development/CardTests/SVI608M/tests/svi608m_tests.cpp), [SVI-328](../../svi328/Development/CardTests/SVI608M/tests/svi608m_tests.cpp)) checks the expander (five slots against four on the SVI-605B, one drive, nothing at `60h`, no blank disk in drive B, a double-sided blank disk in drive A, the factory cards, a refused ROM, the start from the boot EPROM). With a fourth argument, the CP/M 2.27 disk of the SVI-608M, the test boots it on the factory-fitted SVI-608M with a blank ST-212: the boot card starts the floppy (`bootSource()` reports `Floppy`), the BIOS sets 306 cylinders and 4 heads, `STAT E:` reports 3104k, PIP copies DUMP.COM to B: and it runs from there; MSYSGEN `H` writes the system to the hard disk, and a second SVI-608M with the hard disk and no floppy starts to `Booted From Hard Disk` and `A>` with `bootSource()` `HardDisk`. The screens are read from the TV and the SVI-806. [run_tests.ps1](../../svi328/Development/run_tests.ps1) passes the double-sided image `2100 - MSX - MEDIEN\SVIDSK\Cpm227Svi608mDS\CPM227DS.DSK` when it is there. Arguments: BASIC ROM, character ROM `svi806.rom`, boot EPROM `svi609.rom`, the CP/M 2.27 image or `-`.
