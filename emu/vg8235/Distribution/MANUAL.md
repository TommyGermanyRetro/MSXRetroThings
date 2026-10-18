# VG-8235 Emulator – User Manual

This manual describes the Philips VG 8235/00 emulator `vg8235.exe`: what it emulates, how to start it, and how to use the keyboard, joysticks, disks, cassettes, modules, the clock and the printer. The VG 8235 is Philips' MSX2 computer with a built-in 3.5" disk drive. The program's menus and messages are in English; **System > Deutsch** switches them to German and **System > English** back, and the choice is kept in the settings. This manual names the English menu items.

## Contents

1. [What Is Emulated](#1-what-is-emulated)
2. [Starting the Emulator](#2-starting-the-emulator)
3. [The Window](#3-the-window)
4. [Keyboard](#4-keyboard)
5. [Joysticks](#5-joysticks)
6. [System Menu](#6-system-menu)
7. [Disk](#7-disk)
8. [Cassette](#8-cassette)
9. [Modules](#9-modules)
10. [Printer](#10-printer)
11. [Clock](#11-clock)
12. [Software Notes](#12-software-notes)
13. [Files and Settings](#13-files-and-settings)
14. [Accuracy](#14-accuracy)
15. [Troubleshooting](#15-troubleshooting)

Appendices: [A. Technical Reference](#appendix-a-technical-reference), [B. Sources](#appendix-b-sources)

## 1. What Is Emulated

| Part | Emulation |
| --- | --- |
| Machine | Philips VG 8235/00 (MSX2): BIOS with MSX BASIC 2.0 in slot 0, modules in slots 1 and 2, slot 3 with the Sub-ROM (3-0), 128 KiB memory mapper RAM (3-2) and the disk ROM (3-3) |
| CPU | Z80 at 3.58 MHz, stepped T-state by T-state, with the MSX wait state on every M1 cycle |
| Video | Yamaha V9938 with 128 KiB VRAM: TEXT 1/2, MULTICOLOR, GRAPHIC 1-7, sprite modes 1 and 2, palette, command engine, 192 or 212 lines, interlace; PAL as set by the European BIOS |
| Sound | YM2149 of the S-3527 MSX engine (three channels, 32-step envelope) and the one-bit keyboard click, 44.1 kHz mono |
| Keyboard | MSX keyboard matrix of the international VG 8235/00 including the numeric keypad, typed on a US PC keyboard (German PC keyboard with the German interface) |
| Joysticks | Two joystick ports with two triggers each, driven by the PC keyboard or by PC game controllers |
| Disk | WD2793 disk controller of the Philips disk interface with one 3.5" drive; disk images of 320, 360, 640 and 720 KB |
| Clock | Ricoh RP5C01 with calendar and battery-backed CMOS memory |
| Cassette | MSX CAS images, playback and recording through the real tape signals at 1200 baud |
| Modules | Slots 1 and 2: plain ROM modules and MegaROMs with the ASCII8, ASCII16, Konami and Konami SCC mappers (without SCC sound) |
| Printer | MSX printer port; output goes to text files |

The emulator models the hardware, not the software: BASIC, Disk BASIC, MSX-DOS and games run from their original ROM, disk, cassette and module images without patches.

## 2. Starting the Emulator

The emulator needs the three system ROMs below. It looks for each of them, by file name and size, in the subfolder `VG8235\rom` of the installation folder (where the setup with ROMs places them), then in the ROM folder `Documents\VG-8235\ROM`, then next to the program. The setup without ROMs (`VG8235-Setup-NoRoms.exe`) creates the empty ROM folder: place your own images there and start the emulator again. The emulator searches only at start. Without ROM1 or ROM2 it shows the ROM folder and quits; without the disk ROM the disk drive and the **Disk** menu are off, and a disk from the settings or the command line is left out with a message:

| File | Purpose |
| --- | --- |
| `VG8235-ROM1 - Basic2.0.bin` | BIOS with MSX BASIC 2.0 (32 KiB) |
| `VG8235-ROM2 - Basic2.0.bin` | Sub-ROM (16 KiB) |
| `VG8235-ROM3 - DiskDD.bin` | disk ROM (16 KiB) |

**Where to find the ROM images.** The emulator does not come with them in its public setup. Files with the same content (same SHA1) are known under these names; the addresses are given as text on purpose:

| File | SHA1 | Known as |
| --- | --- | --- |
| `VG8235-ROM1 - Basic2.0.bin` | `5e1a4bd6826b29302a1eb88c340477e7cbd0b50a` | openMSX machine `Philips_VG_8235-00`: `vg8235-00_basic-bios2.rom` |
| `VG8235-ROM2 - Basic2.0.bin` | `c289dad246364e2dd716c457ca5eecf98e76c9ab` | openMSX machine `Philips_VG_8235-00`: `vg8235-00_msx2sub.rom` |
| `VG8235-ROM3 - DiskDD.bin` | `c3efedda7ab947a06d9345f7b8261076fa7ceeef` | openMSX machines `Philips_NMS_8250-16` and `-19`: `nms8250_disk.rom`; blueMSX `Machines\Shared Roms`: `PHILIPSDISK.rom` (openMSX's `Philips_VG_8235-00` names a different disk ROM revision) |

The command line can name a disk image for drive A and, with `-state <file.sst>`, a snapshot to continue from:

```powershell
.\vg8235.exe
.\vg8235.exe "C:\path\disk.dsk"
.\vg8235.exe -state "%USERPROFILE%\Documents\VG-8235\STATE\Game.sst"
```

The machine starts like a switched-on VG 8235: with no bootable disk it shows the MSX2 logo and then Disk BASIC with `Ok`. The disk and the modules of the last session are restored from `vg8235.ini`; a disk on the command line takes precedence for drive A.

### System Requirements

| | Minimum | Recommended |
| --- | --- | --- |
| Windows | Windows 10 or Windows 11, 64-bit | Windows 11, 64-bit |
| Processor | 64-bit dual core with about half the single-core performance of an Intel Core i7-10700, for example an Intel Core i5/i7 of the 4th generation or an AMD Ryzen 1000 from about 3 GHz | four cores for video recording |
| Memory | 4 GB | |
| Disk space | 10 MB plus your media | |
| Screen | 1920 x 1080 at 100 % display scaling | 1920 x 1200 or more |
| Sound, game controllers | optional: without a sound device the emulator runs silently | |

The values come from measurements on an Intel Core i7-10700 under Windows 11: the VG-8235 uses about 56 % of one processor core, video recording adds about one more core spread over two threads, and the program needs 25 to 35 MB of memory, about 140 MB while recording. The minimum processor is derived from these figures; the emulator was not tried on such a PC.

The window has a fixed size in screen pixels: 1140 x 1029 pixels including frame, menu and status bar. A display scaling above 100 % makes frame, menu and status bar larger but not the picture, so the screen needs a few more lines. The setup needs the .NET Framework 4, which is part of Windows 10 and 11. The N editions of Windows lack Media Foundation; video recording needs the Media Feature Pack there.

## 3. The Window

- **Picture:** 512 dots per line (the 256-dot modes doubled) and up to 212 lines, enlarged, with the border colour around it. In interlace mode both fields are shown interleaved.
- **Title bar:** the effective CPU clock, measured once per second (for example `VG-8235 | CPU@3,58MHz`). Values around 3.58 MHz mean the emulation runs in real time.
- **Status bar** (below the picture), from left to right:
  - joystick assignment, for example `Joystick 1: arrows + C/V | Joystick 2: off (F11: on)`;
  - disk file (`DSK: ...`), `*` when it has unsaved changes, `(drive on)` while the drive motor runs;
  - inserted modules with file name and mapper;
  - cassette file, or `CAS: recording` while recording;
  - printer folder;
  - while the tape motor runs, the block, bytes, percentage and tape time (`Loading: ...`) or the bytes recorded (`Saving: ...`);
  - `Audio: off` when no audio device could be opened;
  - `*` when a cassette recording is not yet saved.

**F12** plays a short PSG test sequence.

**?** at the right end of the menu bar shows this manual as a web page in the PC's default browser. The emulator reads `MANUAL.md` next to the program and writes the page to the Windows temporary folder.

## 4. Keyboard

The PC keyboard layout follows the language (**System > English / Deutsch**): with English, the default, the keys are read as a US PC keyboard; with German as a German PC keyboard. Type as usual: each character is placed on the MSX key that produces it, together with MSX Shift where the MSX needs it. The PC Shift key acts as the MSX Shift key, also on its own: BASIC shows the F6-F10 labels while it is held. For characters the PC layout decides; while a character that the MSX types without Shift is held, Shift is taken back.

Keys without a character:

| PC key | MSX key |
| --- | --- |
| Cursor keys, Space, Tab, Backspace, Enter, Esc, Caps Lock | the same key |
| F1-F5 | F1-F5 |
| F6-F10 | Shift+F1 to Shift+F5 (F6-F10 of the MSX) |
| Home | HOME; with Shift, CLS |
| Insert / Delete | INS / DEL |
| End | STOP; with Ctrl, CTRL+STOP breaks a BASIC program |
| Page Up | SELECT |
| Left Ctrl | CTRL |
| Left Alt | GRAPH |
| Right Ctrl | CODE |
| Numeric keypad | MSX numeric keypad |

GRAPH and CODE lie left and right of the space bar, as on the MSX keyboard. With the German layout, AltGr gives `@ € { [ ] } \ ~ |`; the US layout has these characters on its own keys. F11 and F12 have their own functions in the emulator (keyboard joystick on/off, test tone).

## 5. Joysticks

The menu **Joystick > Port 1** and **Port 2** sets what drives each MSX joystick port:

| Item | Meaning |
| --- | --- |
| **off** | No joystick. All keys act on the MSX keyboard. This is the default. |
| **Keyboard** | Port 1: cursor keys, **C** as trigger A and **V** as trigger B. Port 2: **W A S D**, **Z** as trigger A and **X** as trigger B (with the German interface the same keys, labelled **Y** and **X** on a German keyboard). The keys count by their position on the keyboard. These keys then act on the joystick only, as on the real machine, where keyboard and joysticks are separate. |
| **PC game controller 1 / 2** | A joystick or gamepad connected to the PC. Stick or point-of-view hat give the directions, the second button is trigger B, any other button trigger A. Port 1 uses the first controller Windows reports, port 2 the second. A controller connected later is found within a second. |

**F11** (also **Joystick > Keyboard joystick active**) switches the keyboard joystick on and off. After the start it is off (`F11: off` in the status bar), so that loading commands such as `BLOAD"CAS:",R` can be typed; press F11 to play and again to type. Switching releases all held keys. The choice of each port is kept in the ini file.

## 6. System Menu

| Item | Meaning |
| --- | --- |
| **Reset** | Works like the reset button: the machine restarts, while disk, cassette, RAM contents and the CMOS settings stay. A bootable MSX-DOS disk in the drive starts MSX-DOS. |
| **New disk...** | Makes a writable copy of a disk image: the first dialog chooses the template (it opens in the `Original` folder of the disk media folder), the second asks for the name of the copy. |
| **Load state...** | Loads a snapshot (`*.sst`): the machine continues exactly where the snapshot was taken. |
| **Save state...** | Saves the running machine as a snapshot: CPU, PPI, video chip with its video RAM, sound chip, clock, disk controller and drive mechanics, memory mapper RAM and registers, slot selection and the bank registers of the modules. |
| **Record video...** | Records the picture and the sound into an MP4 file (H.264 video, AAC sound) until **Stop video**; see [Video Recording](#video-recording) below. |
| **Stop video** | Ends the recording and finishes the file. |
| **English** / **Deutsch** | Language of menus, status bar and messages, and the PC keyboard layout: English with a US keyboard (the default) or German with a German keyboard. The switch takes effect at once and is kept in the settings file. |

The snapshot dialogs start in the `STATE` media folder (see [Files and Settings](#13-files-and-settings)). A snapshot does not contain the ROMs, the module images, the disk, the cassette or the printer; it loads only with the same modules inserted, otherwise the emulator reports that it does not fit and the running machine stays as it was. After loading, the clock shows the time of the snapshot and runs on from there.

### Video Recording

**Record video...** suggests a file name with date and time in the `VIDEO` media folder (see [Files and Settings](#13-files-and-settings)). The video shows the window picture as it is on screen, 1124 x 948 pixels, one picture per emulated field (50.16 per second), with the sound of the machine, also when the PC has no sound device. Picture and sound follow the emulated time: the video plays at the speed of the real machine even when the emulation stuttered while recording, and when the PC cannot encode fast enough, the emulation slows down instead of losing pictures. The status bar shows `REC` with the recorded time. Quitting the emulator finishes the file. The data rate follows the picture: a still screen takes a few kilobytes per second, a moving one more.

## 7. Disk

The **Disk** menu inserts a disk image into drive A (**Insert image...**, `*.dsk` of 320, 360, 640 or 720 KB) or ejects it (**Eject**). A read-only image file is a write-protected disk.

Changes are written back to the image file when the drive motor stops, on eject and when the window closes; until then the status bar shows `*` after the file name. Disk BASIC addresses the drive as `A:`, for example `FILES`, `SAVE"A:NAME"` and `LOAD"A:NAME"`; `CALL FORMAT` formats a disk.

## 8. Cassette

The **Cassette** menu:

| Item | Meaning |
| --- | --- |
| **Open image...** | Inserts an MSX CAS image (`*.cas`). |
| **Rewind** | Rewinds the tape. |
| **Eject** | Ejects the tape. |
| **New recording** | Inserts an empty tape for recording. |
| **Stop recording and save...** | Saves the recording as a CAS file; the name is suggested from the recorded file header. |

The tape runs only while the program switches the cassette motor on, exactly as on the real machine. Loading takes the original time: at 1200 baud about 110 data bytes per second plus the pause and leader before each block.

| File type | Load with |
| --- | --- |
| BASIC program (saved with `CSAVE`) | `CLOAD` then `RUN`, or `RUN"CAS:"` |
| BASIC program as ASCII text (saved with `SAVE"CAS:name"`) | `LOAD"CAS:name"` then `RUN` |
| Machine code (saved with `BSAVE`) | `BLOAD"CAS:",R` |

To record, choose **New recording**, save in BASIC (for example `CSAVE"NAME"`), then choose **Stop recording and save...**. An unsaved recording is confirmed before it is discarded or the window closes.

## 9. Modules

The **Modules** menu has the same items for slot 1 and slot 2:

| Item | Meaning |
| --- | --- |
| **Slot n: insert (detect mapper)...** | Inserts a module image (`*.rom`); the emulator detects the mapper. |
| **Slot n: insert as** | Inserts a module image with a fixed mapper: no mapper, ASCII8, ASCII16, Konami or Konami SCC. Use it when the automatic detection guesses wrong. |
| **Slot n: remove** | Removes the module. |

Inserting and removing restart the machine as after switching it on. Images up to 64 KiB count as plain ROMs; larger images are MegaROMs, whose mapper the emulator guesses from the bank switching writes in the image. The SCC sound chip of Konami SCC modules is not emulated, so their music is missing.

## 10. Printer

`LPRINT`, `LLIST`, the BIOS printer calls and direct output to the printer ports write to text files. By default they go to `Documents\VG-8235\PRINT`; **Printer > Output folder...** chooses another folder, which is kept in `vg8235.ini`.

Each `LLIST` and each `LPRINT` in direct mode becomes its own file; a running BASIC program writes all its output into one file, which closes when BASIC returns to direct mode. File names carry the local time.

## 11. Clock

The RP5C01 clock starts with the PC's time and runs on the emulated CPU clock. Its CMOS memory, which holds the screen and language settings of the MSX2 setup (`SET SCREEN`, `SET BEEP` and others), is kept in `vg8235.cmos` next to the program and restored at the next start.

## 12. Software Notes

**Disk BASIC.** Starts by itself when no bootable disk is in the drive.

**MSX-DOS.** A disk with MSX-DOS boots at switch-on or reset. `CALL SYSTEM` returns only to an MSX-DOS that has already been booted.

**Nextor.** The Nextor ROM in the `CARD` media folder runs as a module in slot 1 or 2 inserted as ASCII16; with the disk `NEXTOR.DSK` from the `DISK` media folder it boots Nextor with MSX-DOS 2.

## 13. Files and Settings

| File or folder | Contents |
| --- | --- |
| `vg8235.ini` | settings next to the program: `language` (`en` or `de`), `joystick1`/`joystick2` (`off`, `keyboard` or `controller`), `disk` (path of the disk in drive A), `slot1`/`slot2` (mapper number and path of the module), `printerDirectory` |
| `vg8235.cmos` | CMOS memory of the clock |
| `Documents\VG-8235\DISK` | disk images; templates for new disks in `DISK\Original`, where the setup puts the blank disks `LEER_1DD_360K.DSK` (360 KB, single-sided) and `LEER_2DD_720K.DSK` (720 KB, double-sided) |
| `Documents\VG-8235\CAS` | cassette images |
| `Documents\VG-8235\CARD` | module images |
| `Documents\VG-8235\ROM` | ROM images (see [Starting the Emulator](#2-starting-the-emulator)) |
| `Documents\VG-8235\STATE` | snapshots |
| `Documents\VG-8235\VIDEO` | video recordings (`*.mp4`) |
| `Documents\VG-8235\PRINT` | printer output |

The emulator creates the folders in `Documents\VG-8235` at its first start (the setup creates them as well); the disk, cassette, module, snapshot and video dialogs start there. Uninstalling leaves them and their contents in place. Deleting `vg8235.ini` starts the next session with the joysticks off, without disk and modules and with printer output in `Documents\VG-8235\PRINT`; deleting `vg8235.cmos` returns the MSX2 setup to its defaults.

## 14. Accuracy

The emulator reproduces the timing of the real machine rather than just its results:

- The CPU is clocked T-state by T-state, with the MSX wait state on every opcode fetch; the V9938 runs on the same 21.477 MHz master clock, six master clocks per CPU T-state, and builds the picture line by line.
- The V9938 command engine works through its commands in steps of fixed duration while the CPU runs on.
- The disk controller follows the WD2793 command set with the timing of an MFM drive.
- As on every MSX, PSG port A is an input (joysticks, cassette) and port B an output, whatever the program writes to the direction bits.
- The cassette runs at its original speed through the original signals.

Not modelled: analog picture and sound circuits, electrical timing of the chips, the SCC sound chip and the mechanics of the cassette recorder and the disk drive.

## 15. Troubleshooting

| Symptom | Cause and remedy |
| --- | --- |
| The first start of a new `vg8235.exe` takes 15 seconds or more | The virus scanner checks the new program; later starts are fast. |
| `The VG-8235 ROMs ... are missing.` | ROM1 or ROM2 is in none of the places named in [Starting the Emulator](#2-starting-the-emulator), or has the wrong size. |
| `The disk ROM ... is missing, so the disk drive is off ...` | ROM3 is missing; the settings or the command line asked for a disk. |
| `The DSK image on the command line could not be loaded.` | The file on the command line does not exist or is not a 320, 360, 640 or 720 KB disk image. |
| Typing in a game moves the joystick | The keyboard joystick is on; press F11 or set the port to **off**. |
| `Game controller ... (not found)` | No game controller at that position; connect one, it is found within a second. |
| `Audio: off` in the status bar | No audio device could be opened, for example because another program holds it exclusively. The emulator retries every second. |
| A disk cannot be written | The image file is read-only, which makes it a write-protected disk; copy it and remove the read-only attribute. |
| A MegaROM crashes or shows garbage | The automatic mapper detection guessed wrong; insert it again with **insert as** and the right mapper. |
| A snapshot does not load | It was saved with other modules inserted, or by another version of the emulator. |

## Appendix A: Technical Reference

This appendix describes the emulated hardware at register level: clocks, slots, I/O ports, every chip, the disk interface and the file formats. It is what the emulator implements; the developer guide in the source code describes the programming interfaces.

### A.1 Clocks and Interrupts

| Signal | Frequency | Derivation |
| --- | --- | --- |
| Master clock | 21,477,270 Hz | crystal |
| Z80 CPU | 3,579,545 Hz | master / 6; one wait state in every M1 cycle |
| V9938 | 1,368 master clocks per line | 342 dots of 4 master clocks |
| YM2149 (in the S-3527) | 1,789,773 Hz | CPU clock / 2 |
| RP5C01 | one second per 3,579,545 CPU T-states | time base from the CPU clock |
| WD2793 | timed as a 1 MHz MFM controller | clocked with the CPU clock |

The European BIOS sets the V9938 to PAL: 313 lines per field, 428,184 master clocks or 71,364 CPU T-states, about 50.16 fields per second. The Z80 runs in interrupt mode 1; INT is the V9938 interrupt output (frame and line interrupt).

**M1 wait state:** every opcode fetch and every prefix byte (`CBh`, `DDh`, `EDh`, `FDh`) takes one T-state longer; interrupt acknowledge cycles are not affected. `OUT (n),A` takes 12 T-states, `OUTI` 18 and `OTIR` 23 per byte.

### A.2 Slots and Memory

The address space has four 16 KiB pages. PPI port A selects the primary slot of each page (bits 0-1 page 0 at `0000h`, bits 2-3 page 1, bits 4-5 page 2, bits 6-7 page 3). Slot 3 is expanded: `FFFFh` in slot 3 is the secondary slot register with the same layout, and it reads back as its complement.

| Slot | Content | Pages |
| --- | --- | --- |
| 0 | BIOS with MSX BASIC 2.0 (32 KiB) | `0000h`-`7FFFh`; `8000h`-`FFFFh` read `FFh` |
| 1 | module slot 1 | depending on the module |
| 2 | module slot 2 | depending on the module |
| 3-0 | Sub-ROM (16 KiB) | `0000h`-`3FFFh` only, not mirrored |
| 3-1 | empty | – |
| 3-2 | 128 KiB memory mapper | all four pages |
| 3-3 | disk ROM (16 KiB) with the WD2793 registers | `4000h`-`7FFFh`; the registers are mirrored at `BFF8h`-`BFFFh` |

**Memory mapper:** eight 16 KiB segments. The registers at ports `FCh`-`FFh` hold the segment of pages 0-3; the segment number is taken modulo 8, and reading returns the register with the upper bits set (`value OR F8h`, "largest" read-back). After reset pages 0-3 hold segments 3, 2, 1, 0.

### A.3 I/O Map

The S-3527 MSX engine decodes its devices with mirrors.

| Port | R/W | Device | Function |
| --- | --- | --- | --- |
| `90h`, `92h`, `94h`, `96h` | R | printer | status: bit 1 BUSY (`FDh`, always ready) |
| `90h`, `92h`, `94h`, `96h` | W | printer | bit 0 /STROBE; a rising edge prints the latched byte |
| `91h`, `93h`, `95h`, `97h` | W | printer | data |
| `98h`-`9Fh` | R/W | V9938 | port & 3: 0 VRAM data, 1 control/status, 2 palette, 3 indirect register |
| `A0h`-`A7h` | W | YM2149 | port & 3: 0 register select, 1 data |
| `A0h`-`A7h` | R | YM2149 | port & 3 = 2: data; other ports read `FFh` |
| `A8h`-`AFh` | R/W | 8255 PPI | port & 3: port A, B, C, control |
| `B4h` | W | RP5C01 | register number (4 bits) |
| `B5h` | R/W | RP5C01 | 4-bit data; reads with bits 4-7 set |
| `FCh`-`FFh` | R/W | memory mapper | segment of pages 0-3 |

### A.4 PPI, PSG Ports and Keyboard

| Unit | Bits |
| --- | --- |
| PPI port A (output) | primary slot select (A.2) |
| PPI port B (input) | keyboard row selected by port C bits 0-3 (active low) |
| PPI port C (output) | bits 0-3 keyboard row, bit 4 cassette motor (0 = on), bit 5 cassette write data, bit 6 Caps LED, bit 7 keyboard click |
| PSG port A (input) | bits 0-3 up, down, left, right, bit 4 trigger A, bit 5 trigger B of the selected joystick (active low), bit 6 reads 1, bit 7 cassette read data |
| PSG port B (output) | bit 6 selects joystick port 1 (0) or 2 (1); the other bits (joystick output pins, Kana LED) have no effect |

As in every MSX engine, port A is always an input and port B always an output, whatever register 7 bits 6-7 say.

Keyboard matrix of the international VG 8235/00 (row = PPI port C bits 0-3, bit = PPI port B):

| Row | Bit 0 | Bit 1 | Bit 2 | Bit 3 | Bit 4 | Bit 5 | Bit 6 | Bit 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| 1 | 8 | 9 | - | = | \ | [ | ] | ; |
| 2 | ' | ` | , | . | / | dead key | A | B |
| 3 | C | D | E | F | G | H | I | J |
| 4 | K | L | M | N | O | P | Q | R |
| 5 | S | T | U | V | W | X | Y | Z |
| 6 | Shift | Ctrl | Graph | Caps | Code | F1 | F2 | F3 |
| 7 | F4 | F5 | Esc | Tab | Stop | BS | Select | Return |
| 8 | Space | Home | Ins | Del | Left | Up | Down | Right |
| 9 | keypad * | keypad + | keypad / | keypad 0 | keypad 1 | keypad 2 | keypad 3 | keypad 4 |
| 10 | keypad 5 | keypad 6 | keypad 7 | keypad 8 | keypad 9 | keypad - | keypad , | keypad . |

### A.5 Chips

#### Z80 CPU

- T-state-stepped core (floooh/chips) with memory, I/O, refresh, HALT, WAIT, BUSREQ and the interrupt modes IM0, IM1 and IM2.
- The machine adds the MSX wait state to every M1 cycle (A.1).

#### Yamaha V9938 Video Display Processor

- **Ports:** VRAM data with read-ahead; control (two-byte register or address sequence; status register selected by R#15); palette (two bytes per entry, R#16 auto-increment); indirect register access through R#17.
- **Registers:** control registers R#0-R#23 and R#32-R#46 with the bit masks of the data book; status registers S#0-S#9. 128 KiB VRAM with a 17-bit address (R#14 holds the upper bits).
- **Time base:** 1,368 master clocks per line, 262 (NTSC) or 313 (PAL) lines latched per field (R#9 bit 1), 192 or 212 display lines (R#9 LN), interlace with even/odd fields (R#9 IL, EO), display position R#18, vertical scroll R#23.
- **Interrupts:** vertical interrupt (IE0, S#0 bit 7 F) and line interrupt (IE1, line in R#19, S#1 FH); HR, VR and EO flags in S#2.
- **Screen modes:** TEXT 1, TEXT 2 (80 columns, blinking through R#12 and R#13), MULTICOLOR, GRAPHIC 1-3; bitmap modes GRAPHIC 4 (256 x 212, 16 colours), GRAPHIC 5 (512 x 212, 4 colours, even and odd dots coloured separately), GRAPHIC 6 (512 x 212, 16 colours), GRAPHIC 7 (256 x 212, 256 colours GGGRRRBB). In GRAPHIC 6 and 7 CPU, table and command accesses interleave the two 64 KiB banks. Page selection through R#2, alternating pages through EO and R#13.
- **Palette:** 16 entries of 3 bits red, green and blue (512 colours); GRAPHIC 7 uses fixed colours; colour 0 shows the backdrop unless R#8 TP is set.
- **Sprites:** mode 1 as on the TMS9918A (4 per line) in GRAPHIC 1, 2 and MULTICOLOR; mode 2 from GRAPHIC 3 on with 8 sprites per line, a colour per line from the colour table before the attribute table, bits EC, CC and IC; fifth or ninth sprite number, collision flag and coordinates in S#3-S#6. GRAPHIC 7 sprites have fixed colours.
- **Command engine:** HMMC, YMMM, HMMM, HMMV, LMMC, LMCM, LMMM, LMMV, LINE, SRCH, PSET, POINT and STOP with the logical operations IMP, AND, OR, EOR, NOT and their T variants; flags CE, TR and BD in S#2, colour register S#7, border X in S#8/S#9; registers hold their end values after a command. Commands work in GRAPHIC 4-7 only and take a fixed time per step.
- **Output:** each raster line holds up to 512 dots of 0GRB colour plus a border colour; 256-dot modes are doubled horizontally, interlace shows both fields interleaved.
- **Not emulated:** VRAM access slots (CPU accesses take effect at once), expansion RAM (MXC, MXS and MXD read `FFh`, as on the VG 8235), light pen, mouse, superimpose, digitize, colour bus, horizontal adjust within the picture (R#18 shifts only the timing), V9958 features.

#### YM2149 Programmable Sound Generator

- AY-3-8910 register set; three tone channels, noise generator (17-bit LFSR), mixer, fixed or envelope volume.
- YM2149 envelope: 32 steps per cycle at twice the AY step rate; output through a 32-level logarithmic table, a fixed level L selects step 2L + 1.
- Fixed port directions (A.4). The three channels are mixed with the keyboard click into 44.1 kHz mono sound.

#### 8255 PPI

- Mode 0, Mode 1 and Mode 2 for port A, Mode 0 and Mode 1 for port B, bit set/reset for port C, handshakes and interrupt enables. The machine uses Mode 0 (A.4); bit set/reset writes reach the cassette pins.

#### Ricoh RP5C01 Real-Time Clock

- Sixteen 4-bit registers; the mode register (D) selects block 0 (time), block 1 (alarm, 12/24-hour selector, leap-year counter) or the RAM blocks 2 and 3 with 13 nibbles each. Unimplemented bits read 0; the test (E) and reset (F) registers are write-only.
- Time and calendar in BCD: seconds, minutes, hours in 12-hour (PM in bit 1 of the 10-hour register) or 24-hour mode, day of week, day, month, year. February has 29 days when the leap-year counter is 0.
- Mode register bit 3 (timer enable) stops the count at the next second; reset register bit 0 clears the alarm, bit 1 the divider below one second.
- The 52 nibbles of all four blocks are the CMOS memory of the MSX2 setup (screen, beep and language settings).

#### WD2793 Floppy Disk Controller

- Registers: status/command, track, sector, data; INTRQ and DRQ.
- Type I: Restore, Seek, Step, Step-in, Step-out; step rates 6, 12, 20 and 30 ms; track update (T), head load (h), verify (V) with 30 ms settling and an ID search within five revolutions; seek error; Restore gives up after 255 steps without TRACK 00.
- Type II: Read Sector and Write Sector with side compare (C, S), 30 ms delay (E), multiple records (m); RECORD NOT FOUND after five revolutions, LOST DATA, write protect.
- Type III: Read Address (the sector register receives the cylinder), Read Track (one revolution in IBM System 34 layout with CRCs), Write Track (`F5h` writes `A1h`, `F7h` the CRC; data fields after ID fields with size code 2 are stored).
- Type IV: Force Interrupt immediately (I3) or on the index pulse (I2); `D0h` stops a command without INTRQ.
- Status as in table 4 of the data sheet. Disk timing: 300 rpm, 32 µs per byte (250 kbit/s MFM), 6,250 bytes per track, sectors 1 to 9 (or 8) one after the other behind the index pulse.
- Drive: 3.5", 80 cylinders, head position 0-83; READY, TRACK 00 and WRITE PROTECT only with the motor on, as on Philips drives; disk change flag.

#### Printer

- A rising /STROBE edge appends the data latch to the output once; status bit 1 BUSY is clear, the printer is always ready.
- The frontend writes each `LLIST`, each direct-mode `LPRINT` and each BASIC program run to its own text file.

### A.6 Philips Disk Interface

The WD2793 sits in slot 3-3 at `7FF8h`-`7FFFh` of the disk ROM page, mirrored at `BFF8h`-`BFFFh`:

| Address | Read | Write |
| --- | --- | --- |
| `7FF8h` | status (clears INTRQ) | command |
| `7FF9h` | track | track |
| `7FFAh` | sector | sector |
| `7FFBh` | data (clears DRQ) | data |
| `7FFCh` | side latch | bit 0 side |
| `7FFDh` | drive latch; bit 2 = 1 while the disk has not been changed (reading clears the change flag) | bits 1-0 drive: 0 or 2 = A, 1 = B, 3 = none; bit 7 motor |
| `7FFFh` | bit 6 /INTRQ, bit 7 /DRQ (active low) | – |

The VG 8235 has only drive A; selecting drive B selects no drive.

### A.7 Module Mappers

| Mapper | Banks | Bank registers |
| --- | --- | --- |
| none | plain ROM up to 64 KiB, placed by its `AB` header (page 2 for BASIC ROMs with a page 2 entry, otherwise page 1; `0000h` above 48 KiB) | – |
| ASCII8 | 4 x 8 KiB at `4000h`/`6000h`/`8000h`/`A000h` | `6000h`, `6800h`, `7000h`, `7800h` |
| ASCII16 | 2 x 16 KiB at `4000h`/`8000h` | `6000h`-`67FFh`, `7000h`-`77FFh` |
| Konami | 8 KiB at `6000h`/`8000h`/`A000h`, `4000h` fixed to bank 0 | `6000h`, `8000h`, `A000h` |
| Konami SCC | 4 x 8 KiB at `4000h`-`BFFFh` | `5000h`, `7000h`, `9000h`, `B000h` (each `x000h`-`x7FFh`) |

Bank numbers wrap at the ROM size; reset selects bank 0 (banks 0-3 for the Konami mappers); unmapped regions read `FFh`. Images up to 64 KiB count as plain ROMs; for larger ones the emulator counts the `LD (nnnn),A` instructions that write to bank registers (ties go to ASCII16, then ASCII8, Konami, Konami SCC).

### A.8 File Formats

**Disk images** (`*.dsk`): raw sectors of 512 bytes, 80 cylinders, stored cylinder by cylinder with side 0 before side 1:

| Size | Sides | Sectors per track |
| --- | --- | --- |
| 327,680 bytes (320 KB) | 1 | 8 |
| 368,640 bytes (360 KB) | 1 | 9 |
| 655,360 bytes (640 KB) | 2 | 8 |
| 737,280 bytes (720 KB) | 2 | 9 |

**Cassette images** (`*.cas`, MSX format): decoded tape bytes. A block starts with the marker `1F A6 DE BA CC 13 7D 74` on an offset divisible by eight. A file header is ten equal type bytes (`D3h` tokenized BASIC, `EAh` ASCII, `D0h` binary) and six name characters; a binary data block starts with start, end and execution address. Playback generates the MSX BIOS signal at 1200 baud: a start bit 0, eight data bits least significant first, two stop bits 1; a 0 is one 1200 Hz period, a 1 two 2400 Hz periods. Before a file header come 2 s of silence and 8,000 one bits, before a data block 1 s and 2,000 one bits. PPI port C bit 4 runs the motor, bit 5 is the write signal, PSG port A bit 7 the read signal. Recording decodes the written edges at 1200 or 2400 baud; each new leader starts a block.

**CMOS file** (`vg8235.cmos`): the 52 nibbles of the RP5C01, one nibble per byte.

**Snapshots** (`*.sst`): CPU, PPI, V9938 with VRAM, palette and command engine, PSG, RTC, disk controller and drive mechanics, mapper RAM and registers, slot selection and the bank registers of the modules; not the ROMs, module images, disk, cassette or printer.

**ROM images** (`VG8235\rom`):

| File | Size | SHA1 | Content |
| --- | --- | --- | --- |
| `VG8235-ROM1 - Basic2.0.bin` | 32 KiB | `5e1a4bd6826b29302a1eb88c340477e7cbd0b50a` | BIOS and MSX BASIC 2.0 of the VG 8235/00 |
| `VG8235-ROM2 - Basic2.0.bin` | 16 KiB | `c289dad246364e2dd716c457ca5eecf98e76c9ab` | Sub-ROM of the VG 8235/00 |
| `VG8235-ROM3 - DiskDD.bin` | 16 KiB | `c3efedda7ab947a06d9345f7b8261076fa7ceeef` | disk ROM of the Philips NMS 8250, which uses the same disk interface |

## Appendix B: Sources

| Source | Used for |
| --- | --- |
| Yamaha, *V9938 MSX-Video Technical Data Book* (`9999 - MSX - Dokumentation`), with the transcription on map.grauw.nl | V9938 ports, registers, modes, sprites, command engine |
| Western Digital, *WD279X-02 Data Sheet*, 1984 (WD Storage Products Handbook vol. 1, bitsavers.org; extract in `9999 - MSX - Dokumentation`) | WD2793 commands, status table, timing |
| Ricoh, *Electronic Devices Data Book*, 1988, RP5C01 (bitsavers.org; extract in `9999 - MSX - Dokumentation`) | RTC registers and modes |
| Yamaha, *YM2149 SSG data sheet*, 1992 (bitsavers.org; in `9999 - MSX - Dokumentation`) | 32-step envelope and volume table |
| openMSX machine definition `Philips_VG_8235-00.xml` and the openMSX sources (built in `D:\openMSX_build`) | slot layout, ROM identification (SHA1), behaviour comparison; no code taken |
| MSX Assembly Page (map.grauw.nl): *I/O ports*, *BIOS calls*, *System variables* | port assignment, cassette pins, BIOS tape and printer calls |
| MSX Wiki (msx.org/wiki): *Emulation related file formats*, *CSAVE*, *BSAVE* | CAS format and file headers |
| openMSX `CasImage.cc` | comparison of the cassette encoding |
| Nextor 2.1.3 (GitHub) | Nextor module and MSX-DOS 2 disk |
| floooh/chips (github.com/floooh/chips) | Z80 core |
