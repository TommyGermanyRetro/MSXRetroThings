# SVI-609

SVI609 is a C++17 static library for the Network Expander of the Spectravideo SVI-609 Master Station, the server of the SVI local area network. The SVI-609 is an SVI-328 with this expander. It builds on the [SVI608M](../SVI608M/README.md) library (main board of the [SVI-605B](../SVI605B/README.md) with five slots, drive A only, boot source detection) and connects to the SVI-328 machine through the expansion connector interface [ExpansionPort.hpp](../SVI3X8/include/SVI3X8/ExpansionPort.hpp).

## Functionality

- The expander of the SVI-608M: five card slots, Centronics port at `10h`-`12h`, FD1793 at `30h`-`38h` with only drive A (double-sided), and the boot source detection (`bootSource()`).
- All five slots fitted at the factory: SVI-806 80 columns (slot 0), SVI-805 RS-232 (slot 1), SVI-807 64 KB RAM with S2+S3 (slot 2), SASI host adapter [LAN-HD-01](../LANHD01/README.md) with the WD1002-SHD and the Seagate ST-212 (slot 3) and the LAN card ([LAN-IF-01](../LANIF01/README.md) fully fitted, slot 4).
- The LAN card starts the machine like the boot card of the SVI-608M (EPROM `svi609.rom`; the original EPROM of the LAN card is not available) and carries the printer PIA at `60h`-`63h`, which CP/M 2.27 finds and prints on.
- Not emulated: the network. The Z8530 SCC of the LAN card ([Z8530](../../chips/Z8530/README.md)) with its RS-422 drivers and the station DIP switches is not connected, because its ports and the BOND-NET firmware and protocol are not documented.

## Interface and Integration

[SVI609.hpp](include/SVI609/SVI609.hpp) exposes `svi609::SVI609`, an `svi608m::SVI608M` with the slot constants `VideoSlot`, `SerialSlot`, `RamSlot`, `SasiSlot` and `LanSlot`. `fitFactoryCards()` fits the five cards from the character ROM `svi806.rom` and the EPROM `svi609.rom`; with a ROM of the wrong size it fails and leaves the slots unchanged.

```cpp
#include <SVI609/SVI609.hpp>
#include <LANHD01/LANHD01.hpp>
#include <SVI328/SVI328.hpp>

svi328::SVI328 machine;
svi609::SVI609 expander;
expander.fitFactoryCards(characterRom, bootRom);
auto* sasi = dynamic_cast<lanhd01::LANHD01*>(expander.card(svi609::SasiSlot));
sasi->controller().attachDisk(std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5));
machine.connectExpansion(&expander);
machine.reset();
```

## In the SVI-328

In the frontend the expander is **Expander > SVI-609 Master Station**; choosing it fits the factory cards. See the [user manual](../../svi328/Distribution/MANUAL.md).

## Build and Test

The SVI-609 is an SVI-328, so only the solution [SVI328.sln](../../svi328/Development/SVI328.sln) uses the project; shared compiler settings are in [Common.props](../Common.props), and [run_tests.ps1](../../svi328/Development/run_tests.ps1) runs the tests. The test program boots the SVI-328 and lives in its `Development\CardTests\SVI609`.

| Project | Type | Output |
| --- | --- | --- |
| `svi609` | Static library | `svi609.lib` |
| `svi609_tests` | Console application | `svi609_tests.exe` |

## Tests

[svi609_tests.cpp](../../svi328/Development/CardTests/SVI609/tests/svi609_tests.cpp) checks the expander (five slots, one drive, the five factory cards in their slots, a refused ROM leaving the slots empty, the PIA of the LAN card at `60h`, the start from the EPROM of the LAN card). With a fourth argument, the CP/M 2.27 disk, it boots CP/M on the factory-fitted SVI-609 with a blank ST-212 to `A>` (`bootSource()` reports `Floppy`, the BIOS sets 306 cylinders and 4 heads), then prints the directory with CTRL-P and `DIR`: the output must arrive on the printer of the LAN card. Arguments: BASIC ROM, character ROM `svi806.rom`, EPROM `svi609.rom`, the CP/M 2.27 image or `-`.
