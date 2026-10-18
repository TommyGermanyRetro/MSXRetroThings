# SVI-328

SVI328 is a C++17 emulator for the Spectravideo SVI-328 MK I and MK II (PAL or NTSC, 64 KiB RAM, BASIC 1.11) with a static machine library, a Windows frontend and automated tests. It combines the component libraries Z80CPU, PPI8255, TMS99X8 with the chips TMS9929A and TMS9918A, and AY8910 of this solution. The [user manual](../../Distribution/MANUAL.md) describes how to use the emulator.

## Functionality

- 32 KiB BASIC ROM (bank 1) and RAM bank 21 in `0000h`-`7FFFh`, RAM bank 2 in `8000h`-`FFFFh`, selected by PSG port B (register 15). Cartridge banks 11 and 12 come from the inserted module (missing module bytes read `FFh`); RAM banks 22, 31 and 32 come from expansion devices.
- A T-state-stepped Z80 CPU without wait states, live raster output of the PAL TMS9929A (50 Hz, European machine) or the NTSC TMS9918A (60 Hz, US machine), AY-3-8910 sound and joystick input, 8255 keyboard and cassette control lines.
- SVI keyboard matrix with 11 rows (rows 9 and 10: numeric keypad), two joysticks and the keyboard click on PPI port C bit 7.
- Expansion connector ([ExpansionPort.hpp](../../../svi3x8/SVI3X8/include/SVI3X8/ExpansionPort.hpp)): memory cycles with /BK21-/BK32, /ROMDIS and /RAMDIS, I/O ports below `80h`, /INT, CPUCLK and /RST, for example for the [SVI-605B](../../../svi3x8/SVI605B/README.md).
- SVI cartridges (modules) as plain images up to 64 KiB, started by the BASIC ROM itself.
- SVI CAS playback and recording through the emulated motor, write and read lines, without BIOS hooks ([Cassette.hpp](include/SVI328/Cassette.hpp)).
- A Windows frontend (`svi328.exe`) with a 4x scaled picture and border, 44.1 kHz mono sound (PSG and keyboard click), US or German PC keyboard input (following the interface language), two keyboard joysticks and a Reset command; F12 plays a PSG test tone. The window title shows the effective CPU clock, the emulated T-states per second of wall time measured once per second (for example `SVI-328 | CPU@3,58MHz`); the status bar below the picture shows the joystick keys and `Audio: off` while no audio device is available.

## Interface and Integration

The public machine API is in [SVI328.hpp](include/SVI328/SVI328.hpp). The constructor takes the video standard (`VideoStandard::Pal`, default, or `VideoStandard::Ntsc`); `videoStandard()`, `cpuTicksPerField()` (71,364 or 59,736), `vdpMasterTicksPerField()` and `fieldRateHz()` report it. `loadRom()` supplies the BASIC ROM, `reset()` starts the machine, `tick()` advances one CPU T-state and the intervening VDP master clocks. `readMemory()`/`writeMemory()` and `readIo()`/`writeIo()` expose accesses as the CPU sees them with the current bank selection. `setKeyboardRow()`/`setKeyboardKey()` take the active-low SVI matrix, `setJoysticks()` takes both joysticks (bits 0-3 up, down, left, right, bit 4 trigger, active low). `connectExpansion()` attaches a device to the expansion connector (not owned, nullptr detaches). `insertCartridge()`, `ejectCartridge()` and `cartridgeInserted()` handle the module; a host calls `reset()` after a change. Cassette integration uses `loadCassette()`, `rewindCassette()`, `ejectCassette()`, `cassetteState()`, `cassetteImage()`, `startCassetteRecording()`, `stopCassetteRecording()` and `markCassetteSaved()`; a host calls the last one only after the image has been written. `bankRegister()`, `clickLevel()`, `video()` and `sound()` provide inspection. `saveState()` returns a snapshot (CPU, RAM banks 2 and 21, VDP with VRAM, PSG, PPI; not the ROM, module, cassette, expansion devices or host inputs), `loadState()` restores one of the same video standard and leaves the machine unchanged when the data does not fit, and `stateVideoStandard()` reads the standard from snapshot data. The components provide `saveState()`/`loadState()` of their own.

[tools/svi328_state.cpp](tools/svi328_state.cpp) (`svi328_state.exe`) runs the machine without a window as fast as possible and works through its arguments as a script: load a snapshot, insert a tape, type, wait for the tape, hold keys or joysticks, write pictures, PSG registers, memory, a code and I/O profile, and snapshots. The [user manual](../../Distribution/MANUAL.md#2-starting-the-emulator) shows how it made the CHESS snapshot.

```cpp
#include <SVI328/SVI328.hpp>

bool runOneField(svi328::SVI328& machine, const std::vector<std::uint8_t>& rom)
{
	if (!machine.loadRom(rom)) {
		return false;
	}
	machine.reset();
	for (std::uint32_t tick = 0; tick < svi328::CpuTicksPerPalField; ++tick) {
		static_cast<void>(machine.tick());
	}
	return true;
}
```

For integration into another Visual Studio solution, keep the directory layout of this solution, add [svi328.vcxproj](svi328.vcxproj) and the component projects `z80bus`, `ppi8255`, `tms99x8`, `tms9918a`, `tms9929a` and `ay8910`, and reference them from the host project. Add the `include` directories of SVI328 and the components to the host's include directories.

### I/O Mapping

| Port | Direction | Device |
| --- | --- | --- |
| `80h` / `81h` | write | VDP data / control |
| `84h` / `85h` | read | VDP data / status |
| `88h` | write | PSG register select |
| `8Ch` / `90h` | write / read | PSG data |
| `94h`-`97h` | write | PPI ports A, B, C, control |
| `98h`-`9Bh` | read | PPI ports A, B, C, control |
| `00h`-`7Fh` | read / write | expansion bus |

### Bank Selection

PSG port B, active low: bit 0 /CART (cartridge bank 11 in `0000h`-`7FFFh`; together with bit 6 /ROMEN0 bank 12 in `8000h`-`BFFFh`, with bit 7 /ROMEN1 in `C000h`-`FFFFh`), bit 1 /BK21, bit 2 /BK22, bit 3 /BK31, bit 4 /BK32, bit 5 Caps LED. Internally, /CART or /BK21 alone selects its bank, none or both leave the BASIC ROM. /BK22, /BK31 and /BK32 only go to the expansion bus: the BASIC ROM and RAM bank 2 answer every cycle outside the cartridge banks and bank 21 unless an expansion device asserts /ROMDIS or /RAMDIS, in which case its data counts. While PSG register 7 has port B as an input, the register reads `FFh` and the default banks apply.

### Inputs

- PSG port A: joystick 1 in bits 0-3, joystick 2 in bits 4-7.
- PPI port A: bit 4 trigger joystick 1, bit 5 trigger joystick 2.
- PPI port B: keyboard row selected by PPI port C bits 0-3.
- PPI port C outputs: bits 0-3 keyboard row, bit 4 cassette motor (0 = on), bit 5 cassette write data, bit 6 cassette audio, bit 7 keyboard click.

The BASIC writes text as ASCII minus `20h` into the VDP name table; its font starts with the space character.

## Cartridges

SVI modules differ from MSX cartridges: there is no slot and no `AB` header. A module is a plain ROM image that replaces the BASIC ROM in `0000h`-`7FFFh` (bank 11) while /CART is active; bytes `8000h`-`FFFFh` of a larger image form bank 12. At power-on the BASIC ROM selects only /CART from RAM, reads `0000h`/`0001h` and jumps to `0000h` when it finds `F3h 31h` (`DI`, `LD SP,nn`); otherwise it restores the banks and starts BASIC. The module then runs with RAM bank 2 in `8000h`-`FFFFh`. Images up to 64 KiB are accepted; bytes beyond a shorter image read `FFh`.

## Cassette

A CAS image is a sequence of blocks, each starting with 16 x `55h` and `7Fh`, as in the TOSEC SVI-318/328 collection. The first block of a file holds 10 type bytes (`D3h` BASIC, `D0h` binary, `EAh` ASCII) and 6 name characters.

- Playback runs while PPI port C bit 4 holds the motor on and drives PPI port A bit 7. The signal uses 4800 samples per second: a 1 bit is one 2400 Hz period, a 0 bit one 1200 Hz period, most significant bit first. Each block is 0.25 s of silence, one 1 bit, 199 x `55h`, `7Fh`, then every data byte preceded by a 0 bit.
- Recording takes PPI port C bit 5 while the motor runs. The BASIC writes half periods of about 650 T-states for a 1 bit and 1300 or more for a 0 bit; two equal half periods form a bit, a block starts after `55h 7Fh`, data bytes follow with their 0 bit, and each motor-on section becomes one block with the 16 x `55h` + `7Fh` header.
- `CLOAD`, `BLOAD"CAS:",R` and `RUN"CAS:"` load from the tape; at roughly 180 data bytes per second a 50 KiB game takes about 4.5 minutes.

## Frontend

`svi328.exe` loads `svi111.rom` from its own directory (installed) or from `Development\SVI328\rom` (started as `Build\svi328.exe`), or the ROM file given on the command line. Characters typed on the PC keyboard (US layout with the English interface, German layout with the German one) are placed on the SVI key that produces them, together with SVI Shift or a Graph key where the SVI needs it ([Keyboard.hpp](include/SVI328/Keyboard.hpp), character table as in openMSX `unicodemap.svi`). The PC Shift key acts as the SVI Shift key, also on its own: BASIC shows the F6-F10 labels while it is held. For characters the PC layout decides; while a character that the SVI types without Shift is held, Shift is taken back.

| PC key | SVI key |
| --- | --- |
| Cursor keys, Space, Tab, Backspace, Enter, Esc, Caps Lock | same key |
| F1-F5 / F6-F10 | F1-F5 / Shift+F1-F5 |
| Home / Insert / Delete | CLS / INS / DEL |
| End | STOP |
| Page Up / Page Down | SELECT / PRINT |
| Left Ctrl / Right Ctrl / Left Alt | CTRL / right GRAPH / left GRAPH |
| Numeric keypad | SVI keypad (rows 9 and 10) |

**Joystick > Port 1 / 2** chooses what drives each SVI joystick port: **off** (default; all keys act on the SVI keyboard only), **Keyboard** (port 1: cursor keys with C as trigger, port 2: W, A, S, D with Z as trigger, the key left of X, labelled Y on a German keyboard; the keys count by their scan codes; these keys then act on the joystick only and no longer on the SVI keyboard; **F11** switches the keyboard joystick on and off; after the start it is off, so that commands can be typed, and the status bar shows `F11: on` or `F11: off`) or **PC game controller 1 / 2** (a joystick or gamepad of the Windows joystick interface: axes beyond a quarter of their range and the point-of-view hat give the directions, any button the trigger; read once per field, a missing controller is looked for again every second). The choice is kept in `svi328.ini`; the status bar shows it. **System > Reset** restarts the machine. **System > PAL (TMS9929A, 50 Hz)** and **NTSC (TMS9918A, 60 Hz)** switch the video standard like switching on the other machine; BASIC ROM, module, cassette and expander stay in place, and the choice is kept in `svi328.ini`. The status bar shows the standard. **System > New disk...** copies a disk image: the first dialog picks the template (it opens in `2100 - MSX - MEDIEN\SVIDSK\Original`, where the write-protected originals lie), the second asks for the name of the copy (in `SVIDSK`); the copy is writable.

The **Cassette** menu opens a CAS image, rewinds or ejects it, starts a new recording and saves it (the file name is suggested from the recorded header). While the motor runs, the status bar shows the block, the data bytes, the percentage and the tape time, during recording the bytes captured. Unsaved recordings are confirmed before they are discarded or the window closes.

The **Module** menu inserts a module image (`*.bin`, `*.rom`) or removes it; both restart the machine as after switching it on, and the status bar shows the module file name.

The file dialogs start in the SVI media folders of `2100 - MSX - MEDIEN` next to the emulator folder: `SVICAS` (cassettes), `SVIMOD` (modules), `SVIDSK` (disks), `SVIHDD` (hard disks), `SVIPRINT` (printer output).

### Expander menu

**Expander > No expander / SVI-601 Super Expander / SVI-602 Mini Expander / SVI-605B / SVI-605B with hard disk / SVI-608M (hard disk) / SVI-609 Master Station** chooses what sits on the expansion connector ([SVI601](../../../svi3x8/SVI601/README.md), [SVI602](../../../svi3x8/SVI602/README.md), [SVI605B](../../../svi3x8/SVI605B/README.md), [SVI605BHD](../../../svi3x8/SVI605BHD/README.md), [SVI608M](../../../svi3x8/SVI608M/README.md), [SVI609](../../../svi3x8/SVI609/README.md); SVI-601 and SVI-602 have seven and one slots and take drives and printer port as SVI-801 and SVI-802 cards) and restarts the machine; so do changes of the slots. Choosing another model fits a new expander: the SVI-608M comes with its factory cards SVI-806 (slot 1), SASI host adapter (slot 2) and boot card (slot 3), the SVI-609 with SVI-806 (slot 1), SVI-805 (slot 2), SVI-807 S2+S3 (slot 3), SASI host adapter (slot 4) and LAN card (slot 5), both with only drive A; the disks stay inserted where the model has the drive. Without an expander the items below are grayed; otherwise the slots, drives, hard disk and printer folder follow the model and its cards (the drives on the SVI-601 and SVI-602 without an SVI-801, slot 5 and the hard disk on the SVI-605B, drive B on the SVI-608M and SVI-609).

- **Drive A / B:** insert a disk image (dialog in `2100 - MSX - MEDIEN\SVIDSK`), eject it, switch write protection. A read-only file is inserted write-protected (the originals in `SVIDSK\Original`). Changes are written back to the file once a second and on eject or exit. A blank disk the expander inserted for formatting has no file: on eject, when disconnecting or changing the expander and on exit the frontend asks whether to save it.
- **Hard disk** (SVI-605B with hard disk, SVI-608M and SVI-609 with SASI host adapter): **Choose image...** attaches a hard disk image (dialog in `2100 - MSX - MEDIEN\SVIHDD`, `*.hdd` or `*.img`, sectors of 256 bytes, at most 10027008 bytes); **New hard disk...** saves a blank ST-212 (`E5h` throughout) under a new name and attaches it; **Remove** detaches it. Changes are written back once a second, when the card or the expander changes and on exit; a read-only image keeps its changes in memory (`[read-only]`). CP/M 2.27 uses a blank image as is; after `MSYSGEN` with `H` the SVI-608M and SVI-609 start from the hard disk without a floppy (boot EPROM `svi3x8\LANIF01\rom\svi609.rom` or `svi609.rom` next to the program).
- **Slot 1-7** (SVI-601: **1-7**, SVI-602: **1**, SVI-605B and SVI-605B with hard disk: **1-4**, SVI-608M and SVI-609: **1-5**): empty, on the SVI-601 and SVI-602 the SVI-801 disk controller (two single-sided SVI-902 drives A and B; on the SVI-601 only in SK6, which takes no other card) and the SVI-802 Centronics card, SVI-807 64 KB RAM with S2+S3 (banks 22 and 31), only S2 (bank 22) or S3+S4 (banks 31 and 32), SVI-805 RS-232, SVI-806 80 columns, on the SVI-608M and SVI-609 the SASI host adapter LAN-HD-01, on the SVI-608M the boot card LAN-IF-01 and on the SVI-609 the LAN card (LAN-IF-01 with printer PIA). A choice is grayed when it would clash with another slot: an SVI-805, SVI-806, SASI host adapter, boot card or LAN card only once (fixed ports), SVI-807 cards only without a common bank, so one card with S2+S3 (128 KB) or two cards with S2 and S3+S4 (144 KB). The SVI-803 is not offered: in an SVI-328 RAM bank 2 answers its addresses. With an SVI-806 the window grows by a panel on the right that shows its picture in green, 1:1 horizontally with doubled lines (`svi3x8\SVI806\rom\svi806.rom` or `svi806.rom` next to the program).
- **RS-232 (SVI-805)**, available while an SVI-805 is fitted: the COM ports the system lists (refreshed when the menu opens) or none; the SVI-805 cards use the chosen port.
- **Printer folder...:** folder for printer output of the Centronics port, the printer port of the SVI-609 LAN card and the SVI-802; default `2100 - MSX - MEDIEN\SVIPRINT`. Each print job becomes a file `SVI328_YYYYMMDD_HHMMSS.txt`; a pause of three seconds starts a new file.

After the SVI-608M or SVI-609 has started from the hard disk, the floppy appears as drive E, as CP/M 2.27 names it then, in the status bar, the drive menu and the dialogs. The status bar shows the expander model, its drives (file name, `new (no file)`, `[WP]`, `(motor)`), the hard disk and the COM port. Expander and model, slots, disks, hard disk, COM port and printer folder are kept in `svi328.ini` next to the program.

## Tests

[svi328_tests.cpp](tests/svi328_tests.cpp) checks the bank selection (ROM and RAM bank 2 after reset, /BK21 with retained contents, missing banks, several active signals), the port mapping of VDP, PSG and PPI, both joysticks, keyboard rows and the click bit, boots `rom/svi111.rom` until the screen shows `SV extended BASIC version 1.1`, the copyright line and `Ok`, then types `print 1+2` and Enter through the SVI matrix and expects ` 3`. The expansion test uses a test device on the connector: ports below `80h` reach it, internal ports do not, /ROMDIS and /RAMDIS replace ROM and RAM bank 2 by its data and keep RAM bank 2 from being written, the bank lines arrive, bank 21 stays in front of the bus, every T-state clocks it, and its /INT interrupts the CPU. The cartridge test maps a 64 KiB image through /CART, /ROMEN0 and /ROMEN1, checks write protection, ejecting and short images, and lets the BASIC ROM start a small module with `F3h 31h` that marks RAM. A cassette round trip saves `10 print 123` with `csave"t"` into a recording, checks the header block and the suggested file name `t.cas`, and reads it back with `new`, `cload` and `list`. The ROM path is the first argument.
