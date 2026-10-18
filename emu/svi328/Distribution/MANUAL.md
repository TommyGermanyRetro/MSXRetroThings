# SVI-328 Emulator – User Manual

This manual describes the Spectravideo SVI-328 emulator `svi328.exe`: what it emulates, how to start it, and how to use the keyboard, joysticks, cassettes, modules, the disk expanders, the hard disk, the printer and the RS-232 port. The program's menus and messages are in English; **System > Deutsch** switches them to German and **System > English** back, and the choice is kept in the settings. This manual names the English menu items.

## Contents

1. [What Is Emulated](#1-what-is-emulated)
2. [Starting the Emulator](#2-starting-the-emulator)
3. [The Window](#3-the-window)
4. [Keyboard](#4-keyboard)
5. [Joysticks](#5-joysticks)
6. [System Menu](#6-system-menu)
7. [Cassette](#7-cassette)
8. [Modules](#8-modules)
9. [Expanders and Cards](#9-expanders-and-cards)
10. [Disks](#10-disks)
11. [Hard Disk](#11-hard-disk)
12. [Printer](#12-printer)
13. [RS-232](#13-rs-232)
14. [Software Notes](#14-software-notes)
15. [Media Folders and Settings](#15-media-folders-and-settings)
16. [Accuracy](#16-accuracy)
17. [Troubleshooting](#17-troubleshooting)

Appendices: [A. Technical Reference](#appendix-a-technical-reference), [B. Sources](#appendix-b-sources)

## 1. What Is Emulated

| Part | Emulation |
| --- | --- |
| Machine | Spectravideo SVI-328 MK I and MK II (same memory and ports; the MK II has a redesigned main board) with 64 KiB RAM and BASIC 1.11 (`svi111.rom`) |
| CPU | Z80 at 3.58 MHz, stepped T-state by T-state, without wait states |
| Video | TMS9929A (PAL, 50 Hz, European machine) or TMS9918A (NTSC, 60 Hz, US machine), with raster-exact output and the VDP's VRAM access timing |
| Sound | AY-3-8910 (three channels) and the keyboard click, 44.1 kHz mono |
| Keyboard | SVI keyboard matrix including the numeric keypad, typed on a US PC keyboard (German PC keyboard with the German interface) |
| Joysticks | Two joystick ports, driven by the PC keyboard or by PC game controllers |
| Cassette | SVI CAS images, playback and recording through the real tape signals |
| Modules | SVI cartridges (plain ROM images up to 64 KiB) |
| SVI-601 | Super Expander with seven slots for interface cartridges |
| SVI-602 | Mini Expander with one slot |
| SVI-605B | Super Expander: four card slots, two disk drives, Centronics printer port |
| SVI-605B with hard disk | Super Expander: four card slots, one disk drive, 10 MB hard disk on a built-in SASI adapter, Centronics printer port |
| SVI-608M | Super Expander with hard disk: five card slots, one disk drive, 10 MB hard disk, boot card, Centronics printer port |
| SVI-609 | Master Station: the Network Expander with all five slots fitted at the factory (80 columns, RS-232, 64 KiB RAM, SASI host adapter, LAN card with printer port), one disk drive, 10 MB hard disk; the network itself is not emulated |
| Cards | SVI-801 disk controller with two SVI-902 drives, SVI-802 Centronics, SVI-805 RS-232, SVI-806 80 columns, SVI-807 64 KiB RAM, SASI host adapter, boot card, LAN card |

The emulator models the hardware, not the software: BASIC, Disk BASIC, CP/M and games run from their original ROM, cassette and disk images without patches.

## 2. Starting the Emulator

The emulator needs the original ROM images below. It looks for each of them by file name and size, in this order: the ROM folder `Documents\SVI-328\ROM`, then the program folder, where the setup with ROMs places them. The setup without ROMs (`SVI328-Setup-NoRoms.exe`) creates the empty ROM folder: place your own images there and start the emulator again. The emulator searches only at start:

- Without `svi111.rom` it shows the ROM folder and quits.
- Without `svi806.rom` the SVI-806 card is not offered, without `svi609.rom` the boot card and the LAN card; the SVI-608M and SVI-609, which come with these cards, need both. The menu shows them grayed.
- When the saved settings use a part whose ROM is missing, the part is left out and a message names the missing files.

| File | Purpose |
| --- | --- |
| `svi111.rom` | BASIC 1.11 (32 KiB), required |
| `svi806.rom` | Character ROM of the SVI-806 80-column card |
| `svi609.rom` | EPROM of the SVI-608M boot card and of the SVI-609 LAN card |

**Where to find the ROM images.** The emulator does not come with them in its public setup. Files with the same content (same SHA1) are known under these names; the addresses are given as text on purpose:

| File | SHA1 | Known as |
| --- | --- | --- |
| `svi111.rom` | `10349ce675f6d6d47f0976e39cb7188eba858d89` | openMSX machine `Spectravideo_SVI-328`: `svi-3x8_v111.rom`; blueMSX machines `SVI - Spectravideo SVI-328 80 Column` and `SVI-328 MK2`: `svi328a.rom` |
| `svi806.rom` | `ed45cb0e9bd18a9d7bd74f87e620f016a7ae840f` | blueMSX machines `SVI - Spectravideo SVI-328 80 Column` and `SVI-328 MK2`: `svi806.rom` (same SHA1 as in MAME) |
| `svi609.rom` | `73f9717e0439d03976a23c1765b6ad114076e029` | `svi-328.dev`, SVI-608M pages: bootstrap EPROM `svi-609.bin` |

A different BASIC ROM can be given on the command line: `svi328.exe D:\path\to\basic.rom`. `-state <file.sst>` starts with a snapshot, for example a game that would otherwise load from tape for minutes:

```powershell
.\svi328.exe -state "%USERPROFILE%\Documents\SVI-328\STATE\Chess.sst"
```

The machine starts like a switched-on SVI-328: the BASIC banner `SV extended BASIC version 1.1` appears, followed by `Ok`. The last configuration (video standard, joysticks, expander, cards, disks, hard disk, COM port, printer folder) is restored from `svi328.ini`.

### System Requirements

| | Minimum | Recommended |
| --- | --- | --- |
| Windows | Windows 10 or Windows 11, 64-bit | Windows 11, 64-bit |
| Processor | 64-bit dual core with about half the single-core performance of an Intel Core i7-10700, for example an Intel Core i5/i7 of the 4th generation or an AMD Ryzen 1000 from about 3 GHz | four cores for video recording |
| Memory | 4 GB | |
| Disk space | 10 MB plus your media | |
| Screen | 1920 x 1080 at 100 % display scaling | 1920 x 1200 or more |
| Sound, game controllers | optional: without a sound device the emulator runs silently | |

The values come from measurements on an Intel Core i7-10700 under Windows 11: the SVI-328 uses about 28 % (with the SVI-608M and the SVI-806 about 47 %) of one processor core, video recording adds about one more core spread over two threads, and the program needs 25 to 35 MB of memory, about 140 MB while recording. The minimum processor is derived from these figures; the emulator was not tried on such a PC.

The window has a fixed size in screen pixels: 1140 x 949 pixels, 1820 x 949 pixels with the 80-column panel of the SVI-806 including frame, menu and status bar. A display scaling above 100 % makes frame, menu and status bar larger but not the picture, so the screen needs a few more lines. The setup needs the .NET Framework 4, which is part of Windows 10 and 11. The N editions of Windows lack Media Foundation; video recording needs the Media Feature Pack there.

## 3. The Window

- **Picture:** the TV picture, four times enlarged, with the border colour around it. With an SVI-806 80-column card fitted, the window grows by a green panel on the right that shows the 80-column screen.
- **Title bar:** the effective CPU clock, measured once per second (for example `SVI-328 | CPU@3,58MHz`). Values around 3.58 MHz mean the emulation runs in real time.
- **Status bar** (below the picture), from left to right:
  - joystick assignment, for example `Joystick 1: arrows + C | Joystick 2: off (F11: on)`;
  - video standard, `PAL 50 Hz` or `NTSC 60 Hz`;
  - module file and cassette file; while the tape motor runs, the block, bytes, percentage and tape time (`Loading: ...`) or the bytes recorded (`Saving: ...`);
  - with an expander: model, drives (file name, `empty` for empty, `new (no file)` for a freshly formatted disk without a file, `[WP]` for write protection, `(motor)` while the drive turns), hard disk and COM port;
  - `Audio: off` when no audio device could be opened;
  - `*` when a cassette recording is not yet saved.

**F12** plays a short PSG test tone.

**?** at the right end of the menu bar shows this manual as a web page in the PC's default browser. The emulator reads `MANUAL.md` next to the program and writes the page to the Windows temporary folder.

## 4. Keyboard

The PC keyboard layout follows the language (**System > English / Deutsch**): with English, the default, the keys are read as a US PC keyboard; with German as a German PC keyboard. Type as usual: each character is placed on the SVI key that produces it, together with SVI Shift or a GRAPH key where the SVI needs it. The PC Shift key acts as the SVI Shift key, also on its own: BASIC shows the F6-F10 labels while it is held. For characters the PC layout decides; while a character that the SVI types without Shift is held, Shift is taken back.

Keys without a character:

| PC key | SVI key |
| --- | --- |
| Cursor keys, Space, Tab, Backspace, Enter, Esc, Caps Lock | the same key |
| F1-F5 | F1-F5 |
| F6-F10 | Shift+F1 to Shift+F5 |
| Home | CLS |
| Insert / Delete | INS / DEL |
| End | STOP |
| Page Up / Page Down | SELECT / PRINT |
| Left Ctrl | CTRL |
| Right Ctrl | right GRAPH |
| Left Alt | left GRAPH |
| Numeric keypad | SVI numeric keypad |

F11 and F12 have their own functions in the emulator (keyboard joystick on/off, test tone).

## 5. Joysticks

The menu **Joystick > Port 1** and **Port 2** sets what drives each SVI joystick port:

| Item | Meaning |
| --- | --- |
| **off** | No joystick. All keys act on the SVI keyboard. This is the default. |
| **Keyboard** | Port 1: cursor keys, **C** as fire. Port 2: **W A S D**, **Z** as fire (with the German interface the same key, labelled **Y** on a German keyboard). The keys count by their position on the keyboard. These keys then act on the joystick only, as on the real machine, where keyboard and joysticks are separate. |
| **PC game controller 1 / 2** | A joystick or gamepad connected to the PC. Stick or point-of-view hat give the directions, any button is fire. Port 1 uses the first controller Windows reports, port 2 the second. A controller connected later is found within a second. |

**F11** switches the keyboard joystick on and off. After the start it is off (`F11: off` in the status bar), so that loading commands such as `BLOAD"CAS:",R` can be typed; press F11 to play and again to type. Switching releases all held keys.

## 6. System Menu

| Item | Meaning |
| --- | --- |
| **Reset** | Restarts the machine as after switching it on: the RAM is cleared and the CPU starts again from the BASIC ROM; module, cassette, expander and disks stay in place. |
| **New disk...** | Makes a writable copy of a disk image: the first dialog chooses the template (it opens in the `Original` folder of the disk media folder), the second asks for the name of the copy. |
| **Load state...** | Loads a snapshot (`*.sst`, dialog in `Documents\SVI-328\STATE`): the machine continues exactly where the snapshot was taken, for example in a game without its tape load. A snapshot holds the plain SVI-328, so loading it disconnects the expander and switches to the snapshot's video standard; BASIC ROM, module and cassette stay in place. |
| **Save state...** | Saves the running machine as a snapshot: CPU, RAM, video chip with its video RAM, sound chip and PPI. Module, cassette and expander are not part of it. |
| **Record video...** | Records the picture and the sound into an MP4 file (H.264 video, AAC sound) until **Stop video**; see [Video Recording](#video-recording) below. |
| **Stop video** | Ends the recording and finishes the file. |
| **PAL (TMS9929A, 50 Hz)** | European machine: 313 lines, 50 pictures per second. |
| **NTSC (TMS9918A, 60 Hz)** | US machine: 262 lines, 60 pictures per second. |
| **English** / **Deutsch** | Language of menus, status bar and messages, and the PC keyboard layout: English with a US keyboard (the default) or German with a German keyboard. The switch takes effect at once and is kept in the settings file. |

Switching the video standard is like switching on the other machine: it restarts, while BASIC ROM, module, cassette and expander stay in place. Software that counts in pictures runs five sixths as fast on the PAL machine as on the NTSC machine, as it did on European SVI-328s. On a 60 Hz PC monitor the 60 pictures per second of the NTSC machine also move more evenly than 50.

### Video Recording

**Record video...** suggests a file name with date and time in the `VIDEO` media folder (see [Media Folders and Settings](#15-media-folders-and-settings)). The video shows the window picture as it is on screen, 1124 x 868 pixels (1804 x 868 including the SVI-806 panel when it is shown at the start of the recording), one picture per emulated field (50.16 per second on PAL, 59.92 on NTSC), with the sound of the machine, also when the PC has no sound device. Picture and sound follow the emulated time: the video plays at the speed of the real machine even when the emulation stuttered while recording, and when the PC cannot encode fast enough, the emulation slows down instead of losing pictures. The status bar shows `REC` with the recorded time. Switching the video standard, also by loading a snapshot of the other standard, ends the recording. Quitting the emulator finishes the file. The data rate follows the picture: a still screen takes a few kilobytes per second, a moving one more.

## 7. Cassette

The **Cassette** menu:

| Item | Meaning |
| --- | --- |
| **Open image...** | Inserts a CAS image (`*.cas`). |
| **Rewind** | Rewinds the tape. |
| **Eject** | Ejects the tape. |
| **New recording** | Inserts an empty tape for recording. |
| **Stop recording and save...** | Saves the recording as a CAS file; the name is suggested from the recorded file header. |

The tape runs only while the program switches the cassette motor on, exactly as on the real machine; loading takes the original time (about 180 bytes per second, a 50 KiB game about 4.5 minutes).

How to load depends on the file type, which the first block of a file records:

| File type | Load with |
| --- | --- |
| BASIC program (saved with `CSAVE`) | `CLOAD` then `RUN`, or `RUN"CAS:"` |
| BASIC program as ASCII text (saved with `SAVE"CAS:name"`) | `LOAD"CAS:name"` then `RUN` |
| Machine code (saved with `BSAVE`) | `BLOAD"CAS:",R` |

To record, choose **New recording**, save in BASIC (for example `CSAVE"NAME"`), then choose **Stop recording and save...**. An unsaved recording is confirmed before it is discarded or the window closes.

## 8. Modules

The **Module** menu inserts a module image (**Insert module...**, `*.bin` or `*.rom`) or removes it (**Remove module**). Both restart the machine as after switching it on. The BASIC ROM starts a module by itself when it begins with the bytes `F3h 31h`; otherwise BASIC starts as usual.

## 9. Expanders and Cards

The **Expander** menu connects an expander to the expansion connector:

| Item | Meaning |
| --- | --- |
| **No expander** | Nothing connected (the plain SVI-328). |
| **SVI-601 Super Expander (7 slots)** | Seven slots SK1-SK7 for interface cartridges. SK6 takes only the SVI-801 disk controller; the other cards go into any other slot. Drives and printer port come from the SVI-801 and SVI-802 cards. |
| **SVI-602 Mini Expander (1 slot)** | One slot for one interface cartridge, for example the SVI-801 disk controller or an SVI-807. |
| **SVI-605B** | Super Expander with four card slots, drives A and B and a Centronics printer port. |
| **SVI-605B with hard disk** | SVI-605B with drive A only and, in place of drive B, a 10 MB hard disk on a built-in SASI host adapter; all four slots stay free. It has no boot card and always starts from the floppy. |
| **SVI-608M (hard disk)** | Super Expander with hard disk: five slots, drive A and a Centronics port. It comes with its factory cards: SVI-806 80 columns (slot 1), SASI host adapter (slot 2) and boot card (slot 3). |
| **SVI-609 Master Station** | The Network Expander of the SVI-609: the SVI-608M expander with all five slots fitted at the factory: SVI-806 80 columns (slot 1), SVI-805 RS-232 (slot 2), SVI-807 64 KB RAM S2+S3 (slot 3), SASI host adapter (slot 4) and LAN card (slot 5). The LAN card starts the machine like the boot card and carries a second printer port, which CP/M 2.27 uses; its network controller is not connected yet, so the network does not work. |

Changing the expander or a card restarts the machine. Without an expander the items below are grayed. Otherwise only the slots of the model are active (SVI-601 1-7, SVI-602 1, SVI-605B and SVI-605B with hard disk 1-4, SVI-608M and SVI-609 1-5); the drives only as far as present (SVI-601 and SVI-602 A and B with an SVI-801, SVI-605B A and B, SVI-605B with hard disk, SVI-608M and SVI-609 A); the hard disk only on the SVI-605B with hard disk or with a SASI host adapter; and the printer folder only with a printer port (an SVI-802, SVI-605B, SVI-608M or SVI-609).

**Slot 1-7** chooses the card in each slot; the title of a slot that holds a card is shown in bold, so the fit is visible without opening its menu:

| Card | Meaning |
| --- | --- |
| **empty** | Empty slot. |
| **SVI-801 disk controller** | Floppy disk controller with two single-sided SVI-902 drives A and B (SVI-601 and SVI-602 only; on the SVI-601 in SK6). |
| **SVI-802 Centronics printer** | Printer port; output goes to the printer folder as with the expanders' built-in port (SVI-601 and SVI-602 only). |
| **SVI-805 RS-232** | Serial port, see [RS-232](#13-rs-232). |
| **SVI-806 80 columns** | 80-column card; its screen appears in the green panel. |
| **SVI-807 64 KB RAM**, S2+S3 | RAM banks 22 and 31: 128 KiB in all. |
| **SVI-807 64 KB RAM**, only S2 | RAM bank 22. |
| **SVI-807 64 KB RAM**, S3+S4 | RAM banks 31 and 32; together with a second card set to S2 this gives 144 KiB. |
| **LAN-HD-01 hard disk adapter** | SASI host adapter for the hard disk (SVI-608M and SVI-609 only). |
| **LAN-IF-01 boot card** | Boot card that starts the SVI-608M from floppy or hard disk (SVI-608M only). |
| **LAN-IF-01 LAN card** | LAN card of the SVI-609: boot card with printer port (SVI-609 only). |

A choice is grayed when it would clash with another slot or does not fit the model: an SVI-801, SVI-802, SVI-805, SVI-806, SASI adapter, boot card or LAN card can be fitted only once, and two SVI-807 cards must not answer the same RAM bank.

## 10. Disks

**Expander > Drive A / B** inserts a disk image (**Insert disk...**), ejects it (**Eject**) or switches its write protection (**Write protection**).

- SVI disk images (`*.dsk`) have 40 cylinders, single-sided (172,032 bytes) or double-sided (346,112 bytes).
- Changes are written back to the image file once a second and on eject or exit.
- A read-only file is inserted write-protected. To work with such an original, make a writable copy with **System > New disk...**.
- Formatting in an empty drive works as with a real blank disk: when software formats or writes to an empty drive, the expander inserts a blank disk (single-sided on the SVI-605B, double-sided on the SVI-605B with hard disk, the SVI-608M and the SVI-609). It has no file yet (`new (no file)`); on eject, on changing the expander and on exit the emulator asks whether to save it.
- The BASIC ROM boots from drive A by itself when a disk is inserted at reset.

## 11. Hard Disk

The SVI-608M, the SVI-609 and the SVI-605B with hard disk have a Seagate ST-212 (10 MB) on a WD1002-SHD controller. **Expander > Hard disk**:

| Item | Meaning |
| --- | --- |
| **Choose image...** | Attaches a hard disk image (`*.hdd` or `*.img`, sectors of 256 bytes, at most 10,027,008 bytes). |
| **New hard disk...** | Saves a blank hard disk under a new name and attaches it. CP/M 2.27 uses it without formatting. |
| **Remove** | Detaches the hard disk. |

Changes are written back once a second and on exit; a read-only image keeps its changes in memory only (`[read-only]`).

The hard disk is used by CP/M 2.27 for the SVI-608M; Disk BASIC and the BASIC ROM do not use it. The SVI-605B with hard disk has no boot card: it always starts CP/M 2.27 from the floppy, which then uses the hard disk as drives B: to E:. The SVI-608M and the SVI-609 can also start from the hard disk:

1. Choose **SVI-608M (hard disk)** or **SVI-609 Master Station**, insert the CP/M 2.27 system disk in drive A and create a hard disk with **New hard disk...**.
2. **System > Reset**: the boot card (on the SVI-609 the LAN card) starts CP/M from the floppy (`Booted From Floppy Disk`).
3. Type `MSYSGEN` and answer with a capital **H**: the system is written to the hard disk (`Function complete.`).
4. Eject the floppy (**Drive A > Eject**) and reset: the machine starts from the hard disk (`Booted From Hard Disk`).

CP/M 2.27 divides the hard disk into four drives, and the drive letters depend on where it started:

| Drive | Started from floppy | Started from hard disk |
| --- | --- | --- |
| Floppy | A: | E: |
| Hard disk, area 1 | B: | A: |
| Hard disk, area 2 | C: | B: |
| Hard disk, area 3 | D: | C: |
| Hard disk, area 4 | E: | D: |

After a start from the hard disk, menu, status bar and dialogs show the floppy as drive E.

## 12. Printer

The SVI-605B, SVI-608M and SVI-609 have a Centronics printer port; the LAN card of the SVI-609 carries a second printer port, which CP/M 2.27 finds and uses instead. On the SVI-608M CP/M 2.27 prints through the Centronics port. The emulator writes whatever is printed to text files: each print job becomes a file `SVI328_YYYYMMDD_HHMMSS.txt`, and a pause of three seconds starts a new file. **Expander > Printer folder...** chooses the folder; by default it is `Documents\SVI-328\PRINT`.

In BASIC, `LPRINT` and `LLIST` print.

## 13. RS-232

With an SVI-805 card fitted, **Expander > RS-232 (SVI-805)** (grayed without one) connects it to one of the PC's COM ports (the list is refreshed each time the menu opens) or to none. The card runs at the speeds the software programs (9600 baud with the usual settings); data and the modem lines RTS, DTR, CTS, DSR and DCD pass between the card and the COM port. A terminal program on another computer can be connected through a null-modem cable.

## 14. Software Notes

**Disk BASIC.** Insert a Disk BASIC disk in drive A and reset. Disk BASIC addresses the drives as `1:` and `2:`: `SAVE"1:NAME"`, `LOAD"1:NAME"`, `FILES` (drive 1) and `FILES 2`. Without a drive number the commands use the cassette. The program `FORMAT` on the Disk BASIC disk formats a disk; in an empty drive the expander provides a blank disk. With an SVI-806, `WIDTH 80` switches the screen to the 80-column card.

**CP/M 2.24** (SVI-605B). Insert the CP/M 2.24 system disk in drive A and reset; the banner reports `Spectravideo CP/M-80 Revision 2.24` and the prompt `A>` appears. `DIR`, `STAT`, `PIP` and the other standard programs are on the disk; `FORMAT` formats a disk in A: or B:. With an SVI-806 fitted, the CP/M console appears on the 80-column screen.

**CP/M 2.27** (SVI-608M, SVI-605B with hard disk). Use the double-sided system disk; the drive of both expanders is double-sided and CP/M 2.27 fills both sides (326 KB). See [Hard Disk](#11-hard-disk) for `MSYSGEN` and the drive letters. With the factory SVI-806, the console appears on the 80-column screen only; the TV picture stays empty.

**Games.** Many games want a joystick: set **Joystick > Port 1** to **Keyboard** or to a game controller before starting them, and use F11 when you need to type. A game written for 60 Hz runs at its intended speed on the NTSC machine.

## 15. Media Folders and Settings

The emulator keeps its media in `Documents\SVI-328` and creates the folders at its first start; the file dialogs start there:

| Folder | Contents |
| --- | --- |
| `Documents\SVI-328\CAS` | cassette images |
| `Documents\SVI-328\MOD` | module images |
| `Documents\SVI-328\DSK` | disk images; write-protected originals in `DSK\Original`, where the setup puts the blank disks `LEER_BASIC.DSK` (Disk BASIC) and `LEER_CPM.DSK` (CP/M) |
| `Documents\SVI-328\HDD` | hard disk images |
| `Documents\SVI-328\PRINT` | printer output |
| `Documents\SVI-328\ROM` | ROM images (see [Starting the Emulator](#2-starting-the-emulator)) |
| `Documents\SVI-328\STATE` | snapshots (`*.sst`) |
| `Documents\SVI-328\VIDEO` | video recordings (`*.mp4`) |

The emulator keeps its configuration in `svi328.ini` next to the program and updates it on every change:

| Key | Meaning |
| --- | --- |
| `language` | `en` (English) or `de` (German) |
| `video` | `PAL` or `NTSC` |
| `joystick1`, `joystick2` | `off`, `keyboard` or `controller` |
| `expander` | `1` when an expander is connected |
| `model` | `601`, `602`, `605B`, `605BHD` (SVI-605B with hard disk), `608M` or `609` |
| `slot1` ... `slot7` | card in each slot |
| `diskA`, `diskB` | disk image paths |
| `hdd` | hard disk image path |
| `comPort` | COM port of the SVI-805 |
| `printerDirectory` | folder for printer output |

Deleting `svi328.ini` returns the emulator to its defaults: PAL, no expander, joysticks off.

## 16. Accuracy

The emulator reproduces the timing of the real machine rather than just its results:

- The CPU is clocked T-state by T-state; the VDP runs on the same 21.477 MHz master clock, six master clocks per CPU T-state.
- The VDP builds the picture line by line as the beam moves, so mid-screen changes of colours or registers appear where the program makes them.
- CPU access to video RAM follows the TMS9918A/9929A: the CPU gets a memory cycle only in the chip's access windows. When software writes faster than the windows allow, the chip loses bytes, and the emulator does the same. The windows follow TI's original documents: one CPU window per 32 pixels while the picture is drawn, plus the sprite scan cycles of a line once its sprite scan has ended, and free access during vertical refresh and with the screen blanked.
- Cassette, disk and hard disk run at their original speeds through the original signals and controller commands.

Not modelled: analog picture and sound circuits, electrical timing of the chips, and the cassette recorder's mechanics.

## 17. Troubleshooting

| Symptom | Cause and remedy |
| --- | --- |
| The first start of a new `svi328.exe` takes 15 seconds or more | The virus scanner checks the new program; later starts are fast. |
| `Audio: off` in the status bar | No audio device could be opened, for example because another program holds it exclusively. The emulator retries every second. |
| `Game controller ... (not found)` | No game controller at that position; connect one, it is found within a second. |
| Typing in a game moves the joystick | The keyboard joystick is on; press F11 or set the port to **off**. |
| A disk cannot be written | The image file is read-only (`[WP]`); make a copy with **System > New disk...**. |
| CP/M 2.27 reports read or write errors on a full disk | A single-sided image is used; CP/M 2.27 needs the double-sided system disk. |
| The 80-column panel stays black | The SVI-806 needs `svi806.rom`; check that it lies next to the program or in `svi3x8\SVI806\rom`. |
| A message that a saved disk or hard disk was not found | The file named in `svi328.ini` was moved or deleted; insert it again from the menu. |

## Appendix A: Technical Reference

This appendix describes the emulated hardware at register level: memory map, I/O ports, every chip, the expansion bus, the expanders, the cards and the file formats. It is what the emulator implements; the developer guide in the source code describes the programming interfaces.

### A.1 Clocks and Interrupts

| Signal | Frequency | Derivation |
| --- | --- | --- |
| Master clock | 21,477,270 Hz | crystal |
| Z80 CPU | 3,579,545 Hz | master / 6, no wait states |
| VDP | 5,369,318 dots/s | master / 4; 342 dots per line, 1,368 master clocks per line |
| AY-3-8910 | 1,789,773 Hz | CPU clock / 2 |
| MC6845 (SVI-806) | 2,000,000 Hz character clock | on the card |
| INS8250 (SVI-805) | 3.072 MHz crystal | on the card |
| FD1793 | clocked with the CPU clock, timed as a 1 MHz 5.25" controller | on the disk unit |

| Video standard | Lines | Active display from line | Master clocks per field | CPU T-states per field | Fields per second |
| --- | --- | --- | --- | --- | --- |
| PAL (TMS9929A) | 313 | 64 | 428,184 | 71,364 | 50.16 |
| NTSC (TMS9918A) | 262 | 40 | 358,416 | 59,736 | 59.92 |

The Z80 runs in interrupt mode 1 (BASIC ROM, CP/M). Its INT line combines the VDP frame interrupt with /INT of the expansion bus (SVI-805, printer PIA of the SVI-609 LAN card).

### A.2 Memory Map and Bank Selection

The 64 KiB address space consists of a lower half (`0000h`-`7FFFh`) and an upper half (`8000h`-`FFFFh`). PSG port B (register 15) selects what each half shows; all bits are active low:

| Bit | Signal | Effect |
| --- | --- | --- |
| 0 | /CART | cartridge bank 11 in `0000h`-`7FFFh` instead of the BASIC ROM |
| 1 | /BK21 | RAM bank 21 in `0000h`-`7FFFh` (internal) |
| 2 | /BK22 | RAM bank 22 in `8000h`-`FFFFh` (expansion bus only) |
| 3 | /BK31 | RAM bank 31 in `0000h`-`7FFFh` (expansion bus only) |
| 4 | /BK32 | RAM bank 32 in `8000h`-`FFFFh` (expansion bus only) |
| 5 | CAPS | Caps Lock LED |
| 6 | /ROMEN0 | with /CART: cartridge bank 12 in `8000h`-`BFFFh` |
| 7 | /ROMEN1 | with /CART: cartridge bank 12 in `C000h`-`FFFFh` |

| Bank | Content | Address range |
| --- | --- | --- |
| 1 | BASIC ROM 1.11, 32 KiB | `0000h`-`7FFFh` (default) |
| 2 | internal RAM, 32 KiB | `8000h`-`FFFFh` (default) |
| 11 | module, first 32 KiB | `0000h`-`7FFFh` |
| 12 | module, bytes `8000h`-`FFFFh` | `8000h`-`BFFFh` / `C000h`-`FFFFh` |
| 21 | internal RAM, 32 KiB | `0000h`-`7FFFh` |
| 22, 31, 32 | RAM on SVI-807 cards | as above |

- Internally, /CART or /BK21 alone selects its bank; neither or both leave the BASIC ROM.
- /BK22, /BK31 and /BK32 reach only the expansion bus. The BASIC ROM and RAM bank 2 answer every cycle outside the cartridge banks and bank 21 until a card pulls /ROMDIS (lower half) or /RAMDIS (upper half); then the card's data count, and RAM bank 2 is not written.
- A selected bank that no card provides leaves the BASIC ROM or RAM bank 2 visible.
- While PSG register 7 makes port B an input, the register reads `FFh` and the default banks apply.
- Missing module bytes read `FFh`; module banks are read-only.

### A.3 I/O Map

The machine decodes ports `80h` and above itself; every I/O cycle below `80h` goes to the expansion bus. Ports without a device read `FFh`.

| Port | R/W | Device | Function |
| --- | --- | --- | --- |
| `10h` | W | Centronics (SVI-605B, SVI-608M, SVI-802) | printer data |
| `11h` | W | Centronics | bit 0 /STROBE |
| `12h` | R | Centronics | bit 0 BUSY (`FEh` while the printer is ready) |
| `28h`-`2Fh` | R/W | SVI-805 | INS8250 registers 0-7 |
| `30h`-`33h` | R/W | disk unit (SVI-605B, SVI-608M, SVI-801) | FD1793 command/status, track, sector, data |
| `34h` | R | disk unit | bit 7 INTRQ, bit 6 DRQ |
| `34h` | W | disk unit | bit 0 select drive A, bit 1 select drive B, bit 2 motor A, bit 3 motor B |
| `38h` | W | disk unit | bit 0 density (1 = FM, 0 = MFM), bit 1 side |
| `40h` | W | LAN-HD-01 | SASI bus reset |
| `41h` | W | LAN-HD-01 | data out with automatic ACK (in bus free: selection ID) |
| `42h` | R | LAN-HD-01 | data in with automatic ACK |
| `43h` | W | LAN-HD-01 | SEL |
| `44h` | W | LAN-HD-01 | further bus control, without effect |
| `46h` | R | LAN-HD-01 | bit 0 BSY, 1 REQ, 2 MSG, 3 C/D, 4 I/O, 5 target connected |
| `50h` | W | SVI-806 | MC6845 register select |
| `51h` | R/W | SVI-806 | MC6845 data |
| `58h` | W | SVI-806 | 0: video RAM off; other values: video RAM at `F000h`-`FFFFh` |
| `60h`-`63h` | R/W | LAN card (SVI-609) | printer port, MC6821 PRA/DDRA, CRA, PRB/DDRB, CRB |
| `6Fh` | W | LAN-IF-01 boot card, LAN card | switch the boot EPROM off until the next reset |
| `80h` / `81h` | W | VDP | data / control |
| `84h` / `85h` | R | VDP | data / status |
| `88h` | W | PSG | register select |
| `8Ch` | W | PSG | data |
| `90h` | R | PSG | data |
| `94h`-`97h` | W | PPI | ports A, B, C, control |
| `98h`-`9Bh` | R | PPI | ports A, B, C, control |

The sources differ on port `34h` bits 2 and 3: svi-328.dev reads them as side and motor, MAME and openMSX as motor A and motor B. The emulator follows MAME and openMSX; the BASIC ROM disk boot, Disk BASIC and CP/M run with it.

### A.4 Keyboard, Joysticks and Cassette Lines

| Unit | Bits |
| --- | --- |
| PSG port A (input) | bits 0-3 joystick 1 up, down, left, right; bits 4-7 joystick 2 (active low) |
| PPI port A (input) | bit 4 trigger joystick 1, bit 5 trigger joystick 2 (active low), bit 6 cassette ready (reads 0), bit 7 cassette read data |
| PPI port B (input) | keyboard row selected by port C bits 0-3 (active low) |
| PPI port C (output) | bits 0-3 keyboard row, bit 4 cassette motor (0 = on), bit 5 cassette write data, bit 6 cassette audio, bit 7 keyboard click |

Keyboard matrix (row = PPI port C bits 0-3, bit = PPI port B):

| Row | Bit 0 | Bit 1 | Bit 2 | Bit 3 | Bit 4 | Bit 5 | Bit 6 | Bit 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| 1 | 8 | 9 | : | ' | , | = | . | / |
| 2 | - | A | B | C | D | E | F | G |
| 3 | H | I | J | K | L | M | N | O |
| 4 | P | Q | R | S | T | U | V | W |
| 5 | X | Y | Z | [ | \ | ] | BS | Up |
| 6 | Shift | Ctrl | left Graph | right Graph | Esc | Stop | Return | Left |
| 7 | F1 | F2 | F3 | F4 | F5 | CLS | Ins | Down |
| 8 | Space | Tab | Del | Caps Lock | Select | Print | – | Right |
| 9 | keypad 0 | keypad 1 | keypad 2 | keypad 3 | keypad 4 | keypad 5 | keypad 6 | keypad 7 |
| 10 | keypad 8 | keypad 9 | keypad + | keypad - | keypad * | keypad / | keypad . | keypad , |

The BASIC writes text as ASCII minus `20h` into the VDP name table; its font starts with the space character.

### A.5 Chips

#### Z80 CPU

- T-state-stepped core (floooh/chips) with memory, I/O, refresh, HALT, WAIT, BUSREQ and the interrupt modes IM0, IM1 and IM2.
- The machine services memory and I/O in the same T-state; there are no wait states and no M1 wait cycle.

#### TMS9929A / TMS9918A Video Display Processor

The PAL machine has the TMS9929A, the NTSC machine the TMS9918A; both share one design (TI data manual), only the line count and the display position differ.

- 16 KiB VRAM, registers R0-R7, status register; Graphics I, Graphics II, Text and Multicolor modes and the undocumented mixed modes (Text or Multicolor with M3, the "bogus" modes with 40 groups of 4 foreground and 2 background pixels).
- CPU interface: two control writes set a VRAM address (second byte `40h` + high bits for writing) or a register (second byte `80h` + register number). Reading status clears the frame flag and the interrupt.
- The picture is built line by line as the beam advances; register values are latched at line boundaries. Graphics groups fetch name, colour and pattern 10, 6 and 4 dots before the group, text groups name and pattern 8 and 4 dots before. The backdrop colour (R7) is recorded for every dot of every line, so border colour changes within a line appear.
- Sprites: Y checks as staged raster events, then attribute and pattern fetches; up to four sprites per line, fifth-sprite flag, collision flag, Y code 208 ends the list, 8- and 16-pixel sprites, magnification.
- CPU VRAM access: a request executes at the next access slot of the current mode, at the earliest 28 master clocks after the request; a later request replaces one still pending (the earlier byte is lost, as on the chip). In Graphics I and II every fourth memory cycle after a character's fetches serves the CPU, the other three the sprite Y scan; once the scan of a line has stopped (Y = 208 or the sprites for the next line found) its remaining Y cycles serve the CPU as well. During the 4.3 ms of vertical refresh and with the screen blanked the CPU does not wait for slots, only for the setup delay.
- Output: palette indices 0-15 in a 256 x 192 picture plus the border raster.

#### AY-3-8910 Programmable Sound Generator

- 16 registers with the AY-3-8910 read masks; three tone channels, a noise generator (17-bit LFSR), a 16-step envelope generator with the AY-3-8910 envelope shapes, per-channel tone/noise mixer and fixed or envelope volume.
- Two I/O ports. Register 7 bit 6 sets port A, bit 7 port B as input (0) or output (1). An output port reads its register ANDed with the external levels (open-collector outputs): a joystick contact still reads 0 while software has made port A an output, as the game CHESS does.
- Port A reads the joysticks, port B selects the memory banks (A.2).
- The three channels are mixed with the keyboard click into 44.1 kHz mono sound.

#### 8255 PPI

- Mode 0, Mode 1 and Mode 2 for port A, Mode 0 and Mode 1 for port B, bit set/reset for port C, handshakes and their interrupt enables. Reading the control register returns `FFh`.
- The SVI-328 uses Mode 0: port A and B inputs, port C output (A.4).

#### FD1793 Floppy Disk Controller

- Registers: command/status, track, sector, data; outputs INTRQ and DRQ; side and density come from port `38h`.
- Type I: RESTORE (steps out until TR00, at most 255 steps), SEEK, STEP, STEP IN, STEP OUT; update flag, head load flag, verify (30 ms settling, then an ID field with the track register value within five revolutions, otherwise seek error); step rates 6, 12, 20 and 30 ms.
- Type II: READ SECTOR and WRITE SECTOR, single and multiple, 30 ms delay flag, side compare; record not found after five revolutions, write protect, lost data when DRQ is not served. Data runs at 32 µs per byte in MFM and 64 µs in FM; the two CRC bytes pass before INTRQ.
- Type III: READ ADDRESS (six ID bytes, track into the sector register), READ TRACK (one revolution, 6250 MFM or 3125 FM bytes with gaps, marks and CRCs), WRITE TRACK (`F5h`/`F6h`/`F7h` conventions; the ID and data fields found become the formatted sectors).
- Type IV: FORCE INTERRUPT immediately, on the index pulse, on ready or on not ready.
- Status bits per command type; NOT READY, WRITE PROTECT, TR00 and INDEX follow the drive. Reading status clears INTRQ (not after an immediate interrupt); data register access clears DRQ. The head unloads after 15 idle revolutions; master reset sets the sector register to 1 and runs RESTORE.
- Drives: 300 rpm, 2 ms index pulse, cylinders 0-41; a drive is ready with a disk inserted and its motor on.

#### WD1002-SHD Winchester Controller

- SASI target ID 0 with one Seagate ST-212: 306 cylinders, 4 heads, 32 sectors of 256 bytes (10,027,008 bytes).
- Sequence: selection with ID bit 0, Command phase with a six-byte command block, Data In or Data Out as the command needs, Status byte (bit 1 error, bits 5-7 LUN), message byte `00h`, bus free. A bus reset returns to bus free.
- Phase values at port `46h`: Command `0Bh`, Data Out `03h`, Data In `13h`, Status `1Bh`, Message In `1Fh`.
- Commands: Test Drive Ready, Restore, Request Status (four bytes: bit 7 address valid plus error code, LUN and 21-bit address), Format Drive, Check Track Format, Format Track, Format Bad Track, Read and Write (21-bit sector address, block count 0 = 256), Seek, Set Parameters (8 bytes, for both drives; bytes 1-5 of the command block are not used), Return Burst Error Length, Format Alternate Track, Write/Read Sector Buffer, RAM/Drive/Controller Diagnostic, Read/Write Long (sector plus four ECC bytes).
- Error codes: `04h` drive not ready (no image, or LUN other than 0), `20h` invalid command, `21h` address beyond the capacity set by the parameters. Power-up parameters are those of the ST-506 (153 cylinders, 4 heads).

#### INS8250 UART

- Registers 0-7: receiver buffer / transmitter holding (divisor latch LSB with DLAB), interrupt enable (divisor MSB with DLAB), interrupt identification, line control, modem control, line status, modem status, scratch. No FIFO.
- Baud rate = crystal / 16 / divisor (3.072 MHz, divisor 20 = 9600 baud); a character takes one frame time of start bit, 5-8 data bits, parity and 1, 1.5 or 2 stop bits.
- Interrupts by priority: line status (overrun, parity, framing, break), received data, transmitter holding register empty, modem status.
- Modem lines DTR, RTS, OUT1, OUT2 out, CTS, DSR, RI, DCD in with delta bits; loopback mode.

#### MC6845 CRT Controller

- Address register for R0-R17 and data register; registers keep only their defined bits; R14-R17 read back, the others read `00h`; R16/R17 (light pen) are read-only.
- Geometry: R1 characters per row, R6 displayed rows, R9 + 1 raster lines per row, start address R12/R13, cursor address R14/R15. A field lasts (R0 + 1) x ((R4 + 1) x (R9 + 1) + R5) character clocks.
- Cursor between raster lines R10 bits 0-4 and R11; R10 bits 5-6 select steady, off, blinking every 16 or every 32 fields.

#### MC6821 PIA

- Register select 0-3: PRA or DDRA, CRA, PRB or DDRB, CRB; control register bit 2 chooses the peripheral register (1) or the direction register (0).
- C1 inputs set flag bit 7 on the edge chosen by bit 1 and interrupt with bit 0; C2 as input sets flag bit 6 on the edge chosen by bit 4 and interrupts with bit 3.
- C2 as output: manual (bit 4 = 1, level from bit 3) or handshake (bit 4 = 0: CA2 low on a PRA read, CB2 low on a PRB write, high again on the next C1 edge or right after the access).
- Reading a peripheral register clears both flags of its side; reset clears all registers.

#### Z8530 SCC

The Z8530 is part of the LAN card of the SVI-609; it is emulated as a component but not connected to the card yet, because its ports on the card are not documented.

- Two channels A and B with a control and a data port each. With pointer 0 a control write is WR0: bits 0-2 select the register of the next control access (plus 8 with the Point High command, bits 3-5 = 001), bits 3-5 are a command (Reset Ext/Status Interrupts, Send Abort, Enable Int on Next Rx Character, Reset Tx Int Pending, Error Reset, Reset Highest IUS), bits 6-7 a CRC reset code (Rx CRC checker, Tx CRC generator, Tx Underrun/EOM latch). After the next access the pointer returns to 0.
- NMOS register images: RR4-RR7 read as RR0-RR3, RR9 as RR13, RR11 as RR15, RR14 as RR10. WR2 (vector) and WR9 (master interrupt control and resets) exist once; RR3 (interrupt pending) only in channel A.
- Reset values: RR0 `44h`, RR1 `07h`, RR15 `F8h`, WR4 bit 2 set (asynchronous), WR11 `08h`; WR9 `40h`/`80h`/`C0h` reset channel B, channel A or the whole chip.
- Asynchronous: 5-8 bits per character (five-or-less coding on transmit), parity (passed in the data below 8 bits), 1, 1.5 or 2 stop bits, framing error, break with a null character at its end; unused receive bits read as 1. Clock x1, x16, x32 or x64.
- SDLC: opening and closing flags, address search on WR6 with broadcast `FFh`, CRC-CCITT preset by WR10 bit 7 and sent inverted, the two CRC bytes passed into the FIFO, end of frame with CRC result and residue 011 in RR1, Tx Underrun/EOM latch with CRC, flag or abort on underrun, Send Abort. The CRC generator is preset by software before each frame.
- Receive FIFO of three characters with overrun on the last place; parity and overrun errors latched until Error Reset. Receive interrupt modes: off, first character or special condition (locks the FIFO on a special condition until Error Reset), every character, special condition only.
- Baud rate generator: period 2 x (time constant + 2) clocks of PCLK or RTxC; zero count as Ext/Status source. Clock sources RTxC, TRxC or baud rate generator; local loopback, auto echo and auto enables (/CTS for the transmitter, /DCD for the receiver).
- Ext/Status: zero count, /DCD, sync/hunt, /CTS, Tx Underrun/EOM and break/abort, enabled in WR15; a change closes all latches, Reset Ext/Status opens them, a change that persists interrupts again; both edges of break/abort interrupt.
- Interrupts: priority Rx A, Tx A, Ext/Status A, Rx B, Tx B, Ext/Status B, each with pending and under-service bit; /INT with MIE; the acknowledge returns WR2, with VIS modified in bits 1-3 (Status Low) or 6-4 (Status High); RR2 of channel B is always modified, `011` when nothing is pending.
- Not modelled: monosync, bisync, external sync, SDLC loop mode, DPLL, NRZI/FM coding and DMA requests.

#### Printer

- A rising /STROBE edge appends the data latch to the output once; status bit 1 BUSY is clear, the printer is always ready.
- The frontend writes each print job to a text file; a pause of three seconds starts a new file.

### A.6 Expansion Bus and Expanders

The 50-pin expansion connector carries the address and data bus, memory and I/O cycles below port `80h`, /BK21, /BK22, /BK31, /BK32, /ROMDIS, /RAMDIS, /INT, CPUCLK and /RST.

- **Combination:** In an expander every card sees every memory and I/O cycle, CPUCLK and /RST. Data lines combine as on an open bus: the AND of all cards that drive, `FFh` when none does. /ROMDIS, /RAMDIS and /INT are wired-OR.
- **RAM bank 2 stand-in:** An SVI-807 that answers bank 02 stands in for RAM bank 2 and, like the internal RAM, keeps off the bus while another card pulls /RAMDIS (the SVI-806 video RAM at `F000h`-`FFFFh`).

| Expander | Slots | Built in |
| --- | --- | --- |
| SVI-601 Super Expander | 7 (SK1-SK7) | nothing; SK6 takes only the SVI-801 disk controller |
| SVI-602 Mini Expander | 1 | nothing; the slot passes every signal straight through, empty = open bus |
| SVI-605B Super Expander | 4 | Centronics port (`10h`-`12h`), disk unit (`30h`-`38h`) with drives A and B |
| SVI-605B with hard disk | 4 | Centronics port, disk unit with drive A only (double-sided), LAN-HD-01 SASI host adapter (`40h`-`46h`) with WD1002-SHD and ST-212, built in; no boot ROM |
| SVI-608M Super Expander | 5 | Centronics port, disk unit with drive A only (double-sided); factory cards SVI-806 (slot 1), LAN-HD-01 (slot 2), LAN-IF-01 boot card (slot 3) |
| SVI-609 Network Expander | 5 | as the SVI-608M; factory cards SVI-806 (slot 1), SVI-805 (slot 2), SVI-807 S2+S3 (slot 3), LAN-HD-01 (slot 4), LAN card (slot 5) |

- **SVI-605B/SVI-608M/SVI-609 main board:** FD1793 and Centronics port as on the SVI-801 and SVI-802; selecting a drive the board does not have selects none.
- **Printer port of the SVI-609 LAN card:** MC6821; the SVI-608M has none. Port A carries the data inverted, CA2 drives /STROBE through an inverter, /ACK goes to CA1, BUSY to PB0 (other port B lines high), the PIA interrupt to /INT. CP/M 2.27 initialises it with `00h` to `61h`/`63h`/`62h`, `FFh` to `60h` and `34h` to `63h`/`61h`, and detects it by switching CRB bit 2 between DDRB and PRB and comparing `62h`; without it CP/M 2.27 prints through the Centronics port.
- **Not emulated:** the cards SVI-701/SVI-827 (modem), SVI-809 (LAN station) and SVI-820 (I/O), and the network of the SVI-609 LAN card (Z8530 SCC, RS-422 drivers and station DIP switches; their ports and the BOND-NET firmware are not documented).
- **Formatting an empty drive:** WRITE SECTOR or WRITE TRACK to a selected drive without a disk first inserts a blank SVI disk (`E5h`; single-sided on the SVI-605B and SVI-801, double-sided on the SVI-605B with hard disk, the SVI-608M and the SVI-609). Reading an empty drive still fails.

### A.7 Cards

| Card | Ports / memory | Function |
| --- | --- | --- |
| SVI-801 disk controller | `30h`-`38h` | disk unit with two single-sided SVI-902 drives; SVI-601 (SK6) and SVI-602 only |
| SVI-802 Centronics | `10h`-`12h` | printer port as on the expanders |
| SVI-803 16 KiB RAM | `8000h`-`BFFFh` | made for the SVI-318; without /RAMDIS, so on the SVI-328 RAM bank 2 answers the same addresses (not offered in the frontend) |
| SVI-805 RS-232 | `28h`-`2Fh`, /INT | INS8250, 3.072 MHz; TXD, DTR, RTS out, RXD, CTS, DSR, DCD in (RI not connected); connected to a Windows COM port |
| SVI-806 80 columns | `50h`, `51h`, `58h`; `F000h`-`FFFFh` | MC6845 at 2 MHz, 2 KiB video RAM mirrored every 2 KiB in `F000h`-`FFFFh` (/RAMDIS) while port `58h` holds a value other than 0; 4 KiB character ROM with 16 lines per character, code `21h` = `A` |
| SVI-807 64 KiB RAM | banks 21, 22, 31, 32, 02 | DIP switches S1-S5 select the banks (factory S2+S3); the lower 32 KiB serve banks 21/31 with /ROMDIS, the upper 32 KiB banks 22/32/02 with /RAMDIS. S1 stays off on the SVI-328 (bank 21 is internal). One card S2+S3 = 128 KiB, a second card S3+S4 next to one with S2 = 144 KiB |
| LAN-HD-01 SASI host adapter | `40h`-`46h` | WD1002-SHD with the ST-212 (A.3) |
| LAN-IF-01 boot card | `0000h`-`7FFFh` read, `6Fh` | 4 KiB EPROM "NETROM 1.0": after /RST it asserts /ROMDIS and supplies the EPROM in place of the BASIC ROM, mirrored every 4 KiB, reads only; a write to `6Fh` switches it off until the next reset |
| LAN card (SVI-609) | `0000h`-`7FFFh` read, `60h`-`63h`, `6Fh`, /INT | fully fitted LAN-IF-01: boot logic and EPROM as on the boot card (the original LAN EPROM is not available; the emulator uses `svi609.rom`), MC6821 printer port (A.6). The Z8530 SCC, RS-422 drivers and station DIP switches 1-7 (8 loopback) are not connected |

**Start of the SVI-608M and SVI-609:** The boot EPROM copies itself to `8000h`, writes to `6Fh`, looks for the SASI target and starts from floppy drive A when a disk answers there, otherwise it loads LBA 0-1 of the hard disk to `C100h`. Without a hard disk it restarts endlessly.

**CP/M 2.27 drive layout:** The hard disk forms four drives. Started from floppy, the floppy is A: and the hard disk B:-E:; started from the hard disk, the hard disk is A:-D: (the same four areas in the same order) and the floppy E:.

### A.8 Boot Sequence and File Formats

**BASIC ROM routines.** The BASIC ROM 1.11 drives the expander ports itself: the printer routine at `3915h` (used by `LPRINT` and `LLIST`, accesses at `1461h`, `3922h` and `3938h`) waits while port `12h` bit 0 (BUSY) is 1, writes the byte to `10h` and pulses /STROBE with `00h` and `FFh` on `11h`; the disk boot at `79DCh` (called from `7C1Fh`, port accesses from `79E0h`) uses `30h`, `34h` and `38h`.

**BASIC ROM start.** At power-on the BASIC ROM selects only /CART, reads `0000h`/`0001h` and jumps to `0000h` when it finds `F3h 31h` (`DI`, `LD SP,nn`), so a module starts with RAM bank 2 in the upper half. Otherwise it restores the banks and tests the FD1793 through the sector register; with a controller present it restores both drives, waits for the index pulse (about 2 s without a disk), reads track 0, sector 1 of drive A in FM to `C100h` and jumps there when the status ANDed with `9Ch` is 0. Otherwise BASIC starts.

**Module images** (`*.rom`, `*.bin`): plain ROM images up to 64 KiB without a header. The first 32 KiB form bank 11, the rest bank 12; bytes beyond a shorter image read `FFh`.

**Cassette images** (`*.cas`): a sequence of blocks, each starting with 16 x `55h` and `7Fh`. The first block of a file holds 10 type bytes (`D3h` BASIC, `D0h` binary, `EAh` ASCII) and 6 name characters.

- Playback drives PPI port A bit 7 while PPI port C bit 4 holds the motor on. The signal has 4800 samples per second: a 1 bit is one period of 2400 Hz, a 0 bit one period of 1200 Hz, most significant bit first. Each block is 0.25 s of silence, one 1 bit, 199 x `55h`, `7Fh`, then every data byte preceded by a 0 bit. About 180 data bytes per second.
- Recording reads PPI port C bit 5 while the motor runs. The BASIC writes half periods of about 650 T-states for a 1 bit and 1300 or more for a 0 bit; two equal half periods form a bit, a block starts after `55h 7Fh`, data bytes follow with their 0 bit, and each motor-on section becomes one block with the 16 x `55h` + `7Fh` header.

**Disk images** (`*.dsk`): 40 cylinders, single-sided (172,032 bytes) or double-sided (346,112 bytes), stored cylinder by cylinder with side 0 before side 1.

| Track | Encoding | Sectors |
| --- | --- | --- |
| cylinder 0, side 0 | FM | 18 x 128 bytes |
| all others | MFM | 17 x 256 bytes |

A track can be read only in its own density. WRITE TRACK with this layout writes the sector data; other layouts are not stored. A blank disk holds `E5h`. CP/M 2.27 on the SVI-608M uses both sides of 40 cylinders (326 KB, 34 records per track, 3 reserved tracks): first side 0, cylinders 0-39, then side 1 from cylinder 0 again.

**Hard disk images** (`*.hdd`, `*.img`): a flat sequence of 256-byte sectors in logical order (LBA x 256). A full ST-212 has 10,027,008 bytes; shorter images (for example 9,502,720 bytes, 1160 tracks) are accepted, read `E5h` beyond their end and grow when written there. A new hard disk holds `E5h`.

**Snapshots** (`*.sst`): CPU, RAM banks 2 and 21, VDP with VRAM, PSG and PPI of the plain machine, with its video standard; not the ROM, module, cassette or expansion devices.

**ROM images:**

| File | Size | SHA1 | Content |
| --- | --- | --- | --- |
| `svi111.rom` | 32 KiB | `10349ce675f6d6d47f0976e39cb7188eba858d89` | BASIC 1.11 (`SV extended BASIC version 1.1`) |
| `svi806.rom` | 4 KiB | `ed45cb0e9bd18a9d7bd74f87e620f016a7ae840f` | SVI-806 character ROM, English |
| `svi609.rom` | 4 KiB | `73f9717e0439d03976a23c1765b6ad114076e029` | LAN-IF-01 boot EPROM "NETROM 1.0" |

## Appendix B: Sources

| Source | Used for |
| --- | --- |
| Spectravideo, *Service Manual SVI-318/328*, vol. 1 (rigpix.com) | SVI-601 slots SK1-SK7 and SK6 for the SVI-801, SVI-802 ports, disk ports, memory |
| Texas Instruments, *TMS9918A/TMS9928A/TMS9929A Video Display Processors Data Manual*, Nov. 1982 (bitsavers.org: `components/ti/TMS9900/TMS9918A_TMS9928A_TMS9929A_Video_Display_Processors_Data_Manual_Nov82.pdf`) | VDP registers, modes, sprites, CPU access windows (section 2.1.5, table 2-2) |
| Texas Instruments, *9918 Master Timing*, 1978, Karl Guttag archive (spatula-city.org/~im14u2c/vdp-99xx) | memory cycle sequence per line |
| US patent 4,243,984 (patents.google.com/patent/US4243984A) | CPU access slots, sprite scan stop |
| Western Digital, *WD1002-SHD OEM Manual*, Aug. 1984 | SASI phases and commands, error codes |
| MSX Wiki: *Spectravideo SV-605* (msx.org/wiki/Spectravideo_SV-605); MSX Resource Center forum *SVI-605B HDD* | versions of the SVI-605 including the SVI-605B with ST-212 and LAN-HD-01; hard disk controller without ROM |
| svi-328.dev | SVI-608M port plan, boot EPROM, HDD protocol, CP/M 2.27 disk image |
| MSX Wiki: *Spectravideo SVI-605*, *SVI-608M*, *SVI-609*, *SVI-3x8 Memory Banks* (msx.org/wiki) | expanders, cards, memory banks |
| Generation-MSX: *SVI-605B* | expander contents |
| MAME source: `src/devices/bus/svi3x8`, `src/mame/svi/svi318.cpp` | expansion bus, /ROMDIS and /RAMDIS, card ports, MC6845 clock, INS8250 crystal |
| openMSX source: `SVIFDC`, `SVIPrinterPort`, `Spectravideo_SVI-328_superexpanded.xml`, `unicodemap.svi`, `svi2dmk` | disk and printer ports, disk image layout, keyboard character table |
| blueMSX | SVI-806 character ROMs (SHA1 as in MAME) |
| TOSEC *Spectravideo SVI 318-328 - CAS* | cassette image format |
| floooh/chips (github.com/floooh/chips) | Z80 core |
| Spectravideo CP/M BIOS 2.24 (`SVI328BIOS224.ASM`), Digital Research CP/M 2.2 source (Unofficial CP/M Web Site, `cpm2-plm.zip`) | CP/M 2.24 system disk |
| Dave Dunfield, Disk BASIC 1.0 disk image | Disk BASIC |
| Disassembly of the BASIC ROM 1.11 and of the CP/M 2.27 BIOS (own work) | disk boot, printer routines, printer PIA |
| Spectravideo, *SVI Local Area Network*, brochure C-307, 1984 (Electric Adventures archive, github.com/tony-cruise/ElectricAdventuresArchive) | SVI-609 Master Station and its factory cards, network data |
| MSX Resource Center forum: *[SVI] SVI-809 Network Card*, *[SVI] Hard drive controller* (2026) | LAN-IF-01 board, boot logic, DIP switches; printer PIA only on the SVI-609 LAN card |
| Zilog, *SCC/ESCC User Manual* UM010902-0609 (zilog.com) | Z8530 registers, reset values, interrupts, asynchronous and SDLC operation |
