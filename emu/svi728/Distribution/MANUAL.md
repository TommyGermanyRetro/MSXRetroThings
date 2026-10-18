# SVI-728 Emulator – User Manual

This manual describes the Spectravideo SVI-728 emulator `svi728.exe`: what it emulates, how to start it, and how to use the keyboard, joysticks, cassettes, modules and the printer. The SVI-728 is Spectravideo's MSX1 computer. The program's menus and messages are in English; **System > Deutsch** switches them to German and **System > English** back, and the choice is kept in the settings. This manual names the English menu items.

## Contents

1. [What Is Emulated](#1-what-is-emulated)
2. [Starting the Emulator](#2-starting-the-emulator)
3. [The Window](#3-the-window)
4. [Keyboard](#4-keyboard)
5. [Joysticks](#5-joysticks)
6. [System Menu](#6-system-menu)
7. [Cassette](#7-cassette)
8. [Modules](#8-modules)
9. [Printer](#9-printer)
10. [Files and Settings](#10-files-and-settings)
11. [Accuracy](#11-accuracy)
12. [Troubleshooting](#12-troubleshooting)

Appendices: [A. Technical Reference](#appendix-a-technical-reference), [B. Sources](#appendix-b-sources)

## 1. What Is Emulated

| Part | Emulation |
| --- | --- |
| Machine | Spectravideo SVI-728 (MSX1) with 64 KiB RAM in slot 1 and MSX BIOS with MSX BASIC 1.0 (`SVI-MSX-BIOS+Basic.bin`) in slot 0 |
| CPU | Z80 at 3.58 MHz, stepped T-state by T-state, with the MSX wait state on every M1 cycle |
| Video | TMS9929A (PAL, 50 Hz, European machine) or TMS9918A (NTSC, 60 Hz), with raster-exact output and the VDP's VRAM access timing |
| Sound | AY-3-8910 (three channels) and the one-bit keyboard click, 44.1 kHz mono |
| Keyboard | MSX keyboard matrix including the numeric keypad, typed on a US PC keyboard (German PC keyboard with the German interface) |
| Joysticks | Two joystick ports with two triggers each, driven by the PC keyboard or by PC game controllers |
| Cassette | MSX CAS images, playback and recording through the real tape signals at 1200 baud |
| Modules | Slots 2 and 3: plain ROM modules and MegaROMs with the ASCII8, ASCII16, Konami and Konami SCC mappers (without SCC sound) |
| Printer | MSX printer port; output goes to text files |

The emulator models the hardware, not the software: BASIC and games run from their original ROM, cassette and module images without patches.

## 2. Starting the Emulator

The emulator needs the MSX BIOS and BASIC, `SVI-MSX-BIOS+Basic.bin` (32 KiB). It looks for the file, by name and size, in the ROM folder `Documents\SVI-728\ROM`, then next to the program, where the setup with ROMs places it. The setup without ROMs (`SVI728-Setup-NoRoms.exe`) creates the empty ROM folder: place your own image there and start the emulator again. Without the file the emulator shows the ROM folder and quits.

**Where to find the ROM images.** The emulator does not come with them in its public setup. Files with the same content (same SHA1) are known under these names; the addresses are given as text on purpose:

| File | SHA1 | Known as |
| --- | --- | --- |
| `SVI-MSX-BIOS+Basic.bin` | `ea6a82cf8c6e65eb30b98755c8577cde8d9186c0` | openMSX machine `Spectravideo_SVI-728`: `svi-728_basic-bios1.rom` |

A different BIOS can be given on the command line, and `-state <file.sst>` starts with a snapshot:

```powershell
.\svi728.exe
.\svi728.exe D:\path\to\bios.rom
.\svi728.exe -state "%USERPROFILE%\Documents\SVI-728\STATE\Game.sst"
```

The machine starts like a switched-on SVI-728: the screen shows `MSX BASIC version 1.0` and `Ok`. The video standard, the modules and the printer folder of the last session are restored from `svi728.ini`.

### System Requirements

| | Minimum | Recommended |
| --- | --- | --- |
| Windows | Windows 10 or Windows 11, 64-bit | Windows 11, 64-bit |
| Processor | 64-bit dual core with about half the single-core performance of an Intel Core i7-10700, for example an Intel Core i5/i7 of the 4th generation or an AMD Ryzen 1000 from about 3 GHz | four cores for video recording |
| Memory | 4 GB | |
| Disk space | 10 MB plus your media | |
| Screen | 1920 x 1080 at 100 % display scaling | 1920 x 1200 or more |
| Sound, game controllers | optional: without a sound device the emulator runs silently | |

The values come from measurements on an Intel Core i7-10700 under Windows 11: the SVI-728 uses about 26 % of one processor core, video recording adds about one more core spread over two threads, and the program needs 25 to 35 MB of memory, about 140 MB while recording. The minimum processor is derived from these figures; the emulator was not tried on such a PC.

The window has a fixed size in screen pixels: 1140 x 949 pixels including frame, menu and status bar. A display scaling above 100 % makes frame, menu and status bar larger but not the picture, so the screen needs a few more lines. The setup needs the .NET Framework 4, which is part of Windows 10 and 11. The N editions of Windows lack Media Foundation; video recording needs the Media Feature Pack there.

## 3. The Window

- **Picture:** the TV picture, four times enlarged, with the border colour around it. Border colour changes during a picture appear where the program makes them, for example the stripes of tape loaders.
- **Title bar:** the effective CPU clock, measured once per second (for example `SVI-728 | CPU@3,58MHz`). Values around 3.58 MHz mean the emulation runs in real time.
- **Status bar** (below the picture), from left to right:
  - joystick assignment, for example `Joystick 1: arrows + C/V | Joystick 2: off (F11: on)`;
  - inserted modules with file name and mapper;
  - cassette file, or `CAS: recording` while recording;
  - printer folder;
  - video standard, `PAL 50 Hz` or `NTSC 60 Hz`;
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
| **Reset** | Restarts the machine as after switching it on: the RAM is cleared and the CPU starts again from the BIOS; modules and cassette stay in place. |
| **PAL (TMS9929A, 50 Hz)** | European machine: 313 lines, 50 pictures per second. This is the default. |
| **NTSC (TMS9918A, 60 Hz)** | 262 lines, 60 pictures per second. |
| **Load state...** | Loads a snapshot (`*.sst`): the machine continues exactly where the snapshot was taken, for example in a game without its tape load. A snapshot of the other video standard switches the machine first. |
| **Save state...** | Saves the running machine as a snapshot: CPU, PPI, video chip with its video RAM, sound chip, RAM, slot selection and the bank registers of the modules. |
| **Record video...** | Records the picture and the sound into an MP4 file (H.264 video, AAC sound) until **Stop video**; see [Video Recording](#video-recording) below. |
| **Stop video** | Ends the recording and finishes the file. |
| **English** / **Deutsch** | Language of menus, status bar and messages, and the PC keyboard layout: English with a US keyboard (the default) or German with a German keyboard. The switch takes effect at once and is kept in the settings file. |

Switching the video standard is like switching on the other machine: it restarts, while the BIOS, the modules and the cassette stay in place. Software that counts in pictures runs five sixths as fast on the PAL machine as on the NTSC machine.

The snapshot dialogs start in the `STATE` media folder (see [Files and Settings](#10-files-and-settings)). A snapshot does not contain the BIOS, the module images, the cassette or the printer; it loads only with the same modules inserted, otherwise the emulator reports that it does not fit and the running machine stays as it was.

### Video Recording

**Record video...** suggests a file name with date and time in the `VIDEO` media folder (see [Files and Settings](#10-files-and-settings)). The video shows the window picture as it is on screen, 1124 x 868 pixels, one picture per emulated field (50.16 per second on PAL, 59.92 on NTSC), with the sound of the machine, also when the PC has no sound device. Picture and sound follow the emulated time: the video plays at the speed of the real machine even when the emulation stuttered while recording, and when the PC cannot encode fast enough, the emulation slows down instead of losing pictures. The status bar shows `REC` with the recorded time. Switching the video standard, also by loading a snapshot of the other standard, ends the recording. Quitting the emulator finishes the file. The data rate follows the picture: a still screen takes a few kilobytes per second, a moving one more.

## 7. Cassette

The **Cassette** menu:

| Item | Meaning |
| --- | --- |
| **Open image...** | Inserts an MSX CAS image (`*.cas`, up to 16 MiB). |
| **Rewind** | Rewinds the tape. |
| **Eject** | Ejects the tape. |
| **New recording** | Inserts an empty tape for recording. |
| **Stop recording and save...** | Saves the recording as a CAS file; the name is suggested from the recorded file header. |

The tape runs only while the program switches the cassette motor on, exactly as on the real machine; reset stops the motor without rewinding. Loading takes the original time: at 1200 baud about 110 data bytes per second plus the pause and leader before each block, so a 50 KiB program takes about nine minutes.

How to load depends on the file type:

| File type | Load with |
| --- | --- |
| BASIC program (saved with `CSAVE`) | `CLOAD` then `RUN`, or `RUN"CAS:"` |
| BASIC program as ASCII text (saved with `SAVE"CAS:name"`) | `LOAD"CAS:name"` then `RUN` |
| Machine code (saved with `BSAVE`) | `BLOAD"CAS:",R` |

To record, choose **New recording**, save in BASIC (for example `CSAVE"NAME"`, `CSAVE"NAME",2` for 2400 baud, `SAVE"CAS:NAME",A` or `BSAVE"CAS:NAME",&HC100,&HC1FF`), then choose **Stop recording and save...**. An unsaved recording is confirmed before it is discarded or the window closes. SVI-318/328 tapes, WAV files and turbo tape formats are not supported.

## 8. Modules

The **Modules** menu has the same items for slot 2 and slot 3:

| Item | Meaning |
| --- | --- |
| **Slot n: insert (detect mapper)...** | Inserts a module image (`*.rom`); the emulator detects the mapper. |
| **Slot n: insert as** | Inserts a module image with a fixed mapper: no mapper, ASCII8, ASCII16, Konami or Konami SCC. Use it when the automatic detection guesses wrong. |
| **Slot n: remove** | Removes the module. |

Inserting and removing restart the machine as after switching it on. Images up to 64 KiB count as plain ROMs; larger images are MegaROMs, whose mapper the emulator guesses from the bank switching writes in the image. The SCC sound chip of Konami SCC modules is not emulated, so their music is missing.

## 9. Printer

`LPRINT`, `LLIST`, the BIOS printer calls and direct output to the printer ports write to text files. By default they go to `Documents\SVI-728\PRINT`; **Printer > Output folder...** chooses another folder, which is kept in `svi728.ini`.

Each `LLIST` and each `LPRINT` in direct mode becomes its own file; a running BASIC program writes all its output into one file, which closes when BASIC returns to direct mode. File names carry the local time (`YYYY-MM-DD_HH-MM-SS-mmm.txt`).

## 10. Files and Settings

| File or folder | Contents |
| --- | --- |
| `svi728.ini` | settings next to the program: `language` (`en` or `de`), `video` (`PAL` or `NTSC`), `joystick1`/`joystick2` (`off`, `keyboard` or `controller`), `printerDirectory`, `slot2`/`slot3` (mapper number and path of the module) |
| `Documents\SVI-728\CAS` | cassette images |
| `Documents\SVI-728\CARD` | module images |
| `Documents\SVI-728\ROM` | ROM image (see [Starting the Emulator](#2-starting-the-emulator)) |
| `Documents\SVI-728\STATE` | snapshots |
| `Documents\SVI-728\VIDEO` | video recordings (`*.mp4`) |
| `Documents\SVI-728\PRINT` | printer output |

The emulator creates the folders in `Documents\SVI-728` at its first start (the setup creates them as well); the cassette, module, snapshot and video dialogs start there. Uninstalling leaves them and their contents in place. Deleting `svi728.ini` returns the emulator to its defaults: PAL, joysticks off, no modules, printer output in `Documents\SVI-728\PRINT`.

## 11. Accuracy

The emulator reproduces the timing of the real machine rather than just its results:

- The CPU is clocked T-state by T-state, with the MSX wait state on every opcode fetch; the VDP runs on the same 21.477 MHz master clock, six master clocks per CPU T-state.
- The VDP builds the picture line by line as the beam moves, so mid-screen changes of colours or registers appear where the program makes them.
- CPU access to video RAM follows the TMS9918A/9929A: the CPU gets a memory cycle only in the chip's access windows. When software writes faster than the windows allow, the chip loses bytes, and the emulator does the same. The windows follow TI's original documents: one CPU window per 32 pixels while the picture is drawn, plus the sprite scan cycles of a line once its sprite scan has ended, and free access during vertical refresh and with the screen blanked.
- As on every MSX, PSG port A is an input (joysticks, cassette) and port B an output, whatever the program writes to the direction bits.
- The cassette runs at its original speed through the original signals.

Not modelled: analog picture and sound circuits, electrical timing of the chips, the SCC sound chip and the cassette recorder's mechanics.

## 12. Troubleshooting

| Symptom | Cause and remedy |
| --- | --- |
| The first start of a new `svi728.exe` takes 15 seconds or more | The virus scanner checks the new program; later starts are fast. |
| `The SVI-728 ROM (SVI-MSX-BIOS+Basic.bin, 32 KiB) is missing.` | The file is in none of the places named in [Starting the Emulator](#2-starting-the-emulator), or is not 32 KiB. |
| Typing in a game moves the joystick | The keyboard joystick is on; press F11 or set the port to **off**. |
| `Game controller ... (not found)` | No game controller at that position; connect one, it is found within a second. |
| `Audio: off` in the status bar | No audio device could be opened, for example because another program holds it exclusively. The emulator retries every second. |
| A MegaROM crashes or shows garbage | The automatic mapper detection guessed wrong; insert it again with **insert as** and the right mapper. |
| A snapshot does not load | It was saved with other modules inserted, or by another version of the emulator. |
| A module saved in `svi728.ini` is not restored | The file was moved or deleted; insert it again from the menu. |

## Appendix A: Technical Reference

This appendix describes the emulated hardware at register level: clocks, slots, I/O ports, every chip and the file formats. It is what the emulator implements; the developer guide in the source code describes the programming interfaces.

### A.1 Clocks and Interrupts

| Signal | Frequency | Derivation |
| --- | --- | --- |
| Master clock | 21,477,270 Hz | crystal |
| Z80 CPU | 3,579,545 Hz | master / 6; one wait state in every M1 cycle |
| VDP | 5,369,318 dots/s | master / 4; 342 dots per line, 1,368 master clocks per line |
| AY-3-8910 | 1,789,773 Hz | CPU clock / 2 |

| Video standard | Lines | Active display from line | Master clocks per field | CPU T-states per field | Fields per second |
| --- | --- | --- | --- | --- | --- |
| PAL (TMS9929A) | 313 | 64 | 428,184 | 71,364 | 50.16 |
| NTSC (TMS9918A) | 262 | 40 | 358,416 | 59,736 | 59.92 |

The Z80 runs in interrupt mode 1; INT is the VDP frame interrupt.

**M1 wait state:** every opcode fetch and every prefix byte (`CBh`, `DDh`, `EDh`, `FDh`) takes one T-state longer; interrupt acknowledge cycles are not affected. `OUT (n),A` takes 12 T-states, `OUTI` 18 and `OTIR` 23 per byte. This spacing decides whether VRAM writes reach the VDP access slots in time.

### A.2 Slots and Memory

The address space has four 16 KiB pages. PPI port A selects the primary slot of each page (bits 0-1 page 0 at `0000h`, bits 2-3 page 1, bits 4-5 page 2, bits 6-7 page 3). The SVI-728 has no secondary slots.

| Slot | Content |
| --- | --- |
| 0 | MSX BIOS with MSX BASIC 1.0 (32 KiB), mirrored: `8000h`-`FFFFh` show the same 32 KiB |
| 1 | 64 KiB RAM |
| 2 | module slot 2 |
| 3 | module slot 3 |

### A.3 I/O Map

| Port | R/W | Device | Function |
| --- | --- | --- | --- |
| `90h`, `92h`, `94h`, `96h` | R | printer | status: bit 1 BUSY (`FDh`, always ready) |
| `90h`, `92h`, `94h`, `96h` | W | printer | bit 0 /STROBE; a rising edge prints the latched byte |
| `91h`, `93h`, `95h`, `97h` | W | printer | data |
| `98h`, `9Ah` | R/W | VDP | VRAM data |
| `99h`, `9Bh` | R/W | VDP | control (write) / status (read) |
| `A0h` | W | PSG | register select |
| `A1h` | W | PSG | data |
| `A2h` | R | PSG | data |
| `A8h` | R/W | PPI | port A: primary slot select |
| `A9h` | R | PPI | port B: keyboard row |
| `AAh` | R/W | PPI | port C: keyboard row, cassette, click |
| `ABh` | W | PPI | mode control and bit set/reset |

### A.4 PPI, PSG Ports and Keyboard

| Unit | Bits |
| --- | --- |
| PPI port A (output) | primary slot select (A.2) |
| PPI port B (input) | keyboard row selected by port C bits 0-3 (active low) |
| PPI port C (output) | bits 0-3 keyboard row, bit 4 cassette motor (0 = on), bit 5 cassette write data, bit 6 Caps LED, bit 7 keyboard click |
| PSG port A (input) | bits 0-3 up, down, left, right, bit 4 trigger A, bit 5 trigger B of the selected joystick (active low), bit 6 reads 1, bit 7 cassette read data |
| PSG port B (output) | bit 6 selects joystick port 1 (0) or 2 (1); the other bits (joystick output pins, Kana LED) have no effect |

As on every MSX, PSG port A is always an input and port B always an output, whatever register 7 bits 6-7 say.

Keyboard matrix of the international SVI-728 (row = PPI port C bits 0-3, bit = PPI port B):

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

#### TMS9929A / TMS9918A Video Display Processor

The PAL machine (default) has the TMS9929A, the NTSC machine the TMS9918A; both share one design (TI data manual), only the line count and the display position differ.

- 16 KiB VRAM, registers R0-R7, status register; Graphics I, Graphics II, Text and Multicolor modes and the undocumented mixed modes (Text or Multicolor with M3, the "bogus" modes with 40 groups of 4 foreground and 2 background pixels).
- CPU interface: two control writes set a VRAM address (second byte `40h` + high bits for writing) or a register (second byte `80h` + register number). Reading status clears the frame flag and the interrupt.
- The picture is built line by line as the beam advances; register values are latched at line boundaries. Graphics groups fetch name, colour and pattern 10, 6 and 4 dots before the group, text groups name and pattern 8 and 4 dots before. The backdrop colour (R7) is recorded for every dot of every line, so border colour changes within a line appear.
- Sprites: Y checks as staged raster events, then attribute and pattern fetches; up to four sprites per line, fifth-sprite flag, collision flag, Y code 208 ends the list, 8- and 16-pixel sprites, magnification.
- CPU VRAM access: a request executes at the next access slot of the current mode, at the earliest 28 master clocks after the request; a later request replaces one still pending (the earlier byte is lost, as on the chip). In Graphics I and II every fourth memory cycle after a character's fetches serves the CPU, the other three the sprite Y scan; once the scan of a line has stopped (Y = 208 or the sprites for the next line found) its remaining Y cycles serve the CPU as well. During the 4.3 ms of vertical refresh and with the screen blanked the CPU does not wait for slots, only for the setup delay.
- Output: palette indices 0-15 in a 256 x 192 picture plus the border raster.

#### AY-3-8910 Programmable Sound Generator

- 16 registers with the AY-3-8910 read masks; three tone channels, a noise generator (17-bit LFSR), a 16-step envelope generator with the AY-3-8910 envelope shapes, per-channel tone/noise mixer and fixed or envelope volume.
- Fixed port directions (A.4). The three channels are mixed with the keyboard click into 44.1 kHz mono sound.

#### 8255 PPI

- Mode 0, Mode 1 and Mode 2 for port A, Mode 0 and Mode 1 for port B, bit set/reset for port C, handshakes and interrupt enables. The machine uses Mode 0 (A.4); bit set/reset writes reach the cassette pins.

#### Printer

- A rising /STROBE edge appends the data latch to the output once; status bit 1 BUSY is clear, the printer is always ready.
- The frontend writes each `LLIST`, each direct-mode `LPRINT` and each BASIC program run to its own text file.

### A.6 Module Mappers

| Mapper | Banks | Bank registers |
| --- | --- | --- |
| none | plain ROM up to 64 KiB, placed by its `AB` header (page 2 for BASIC ROMs with a page 2 entry, otherwise page 1; `0000h` above 48 KiB) | – |
| ASCII8 | 4 x 8 KiB at `4000h`/`6000h`/`8000h`/`A000h` | `6000h`, `6800h`, `7000h`, `7800h` |
| ASCII16 | 2 x 16 KiB at `4000h`/`8000h` | `6000h`-`67FFh`, `7000h`-`77FFh` |
| Konami | 8 KiB at `6000h`/`8000h`/`A000h`, `4000h` fixed to bank 0 | `6000h`, `8000h`, `A000h` |
| Konami SCC | 4 x 8 KiB at `4000h`-`BFFFh` | `5000h`, `7000h`, `9000h`, `B000h` (each `x000h`-`x7FFh`) |

Bank numbers wrap at the ROM size; reset selects bank 0 (banks 0-3 for the Konami mappers); unmapped regions read `FFh`. Images up to 64 KiB count as plain ROMs; for larger ones the emulator counts the `LD (nnnn),A` instructions that write to bank registers (ties go to ASCII16, then ASCII8, Konami, Konami SCC).

### A.7 File Formats

**Cassette images** (`*.cas`, MSX format): decoded tape bytes. A block starts with the marker `1F A6 DE BA CC 13 7D 74` on an offset divisible by eight. A file header is ten equal type bytes (`D3h` tokenized BASIC, `EAh` ASCII, `D0h` binary) and six name characters; a binary data block starts with start, end and execution address. Playback generates the MSX BIOS signal at 1200 baud: a start bit 0, eight data bits least significant first, two stop bits 1; a 0 is one 1200 Hz period, a 1 two 2400 Hz periods. Before a file header come 2 s of silence and 8,000 one bits, before a data block 1 s and 2,000 one bits. PPI port C bit 4 runs the motor, bit 5 is the write signal, PSG port A bit 7 the read signal. Recording decodes the written edges at 1200 or 2400 baud; each new leader starts a block.

**Snapshots** (`*.sst`): CPU, PPI, VDP with VRAM, PSG, RAM, slot selection and the bank registers of the modules, with the video standard; not the ROM, module images, cassette or printer.

**ROM image:**

| File | Size | SHA1 | Content |
| --- | --- | --- | --- |
| `SVI-MSX-BIOS+Basic.bin` | 32 KiB | `ea6a82cf8c6e65eb30b98755c8577cde8d9186c0` | MSX BIOS with MSX BASIC 1.0 (`MSX BASIC version 1.0`) |

## Appendix B: Sources

| Source | Used for |
| --- | --- |
| Texas Instruments, *TMS9918A/TMS9928A/TMS9929A Video Display Processors Data Manual*, Nov. 1982 (bitsavers.org: `components/ti/TMS9900/TMS9918A_TMS9928A_TMS9929A_Video_Display_Processors_Data_Manual_Nov82.pdf`) | VDP registers, modes, sprites, CPU access windows (section 2.1.5, table 2-2) |
| Texas Instruments, *9918 Master Timing*, 1978, Karl Guttag archive (spatula-city.org/~im14u2c/vdp-99xx) | memory cycle sequence per line |
| US patent 4,243,984 (patents.google.com/patent/US4243984A) | CPU access slots, sprite scan stop |
| MSX Assembly Page (map.grauw.nl): *I/O ports*, *BIOS calls*, *System variables* | port assignment, cassette pins, BIOS tape and printer calls |
| MSX Wiki (msx.org/wiki): *Emulation related file formats*, *CSAVE*, *BSAVE* | CAS format and file headers |
| openMSX `CasImage.cc` | comparison of the cassette encoding |
| openMSX machine definition of the Spectravideo SVI-728 and its mapper detection | comparison only; no code taken |
| floooh/chips (github.com/floooh/chips) | Z80 core |
