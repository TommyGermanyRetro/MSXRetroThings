# SVI-728

SVI728 is a C++17 SVI-728/MSX emulator with a static machine library, a Windows frontend, a default BIOS/BASIC ROM and automated tests. It combines the sibling Z80CPU, PPI8255, TMS99X8 (with TMS9918A and TMS9929A), AY8910 and PRINTER libraries.

## Functionality

- A 64 KiB CPU address space, 64 KiB RAM, a 32 KiB main ROM with mirroring and cartridge ROM loading in primary slots 2 and 3. PPI port A selects primary slots for the four 16 KiB pages.
- A T-state-stepped Z80 CPU with the MSX wait state on every M1 cycle, 8255 keyboard/slot control, AY-3-8910 sound/GPIO and live raster output of the TMS9929A (PAL, the default) or the TMS9918A (NTSC).
- Keyboard matrix translation, two joystick inputs and the PPI port-C bit-7 one-bit sound output.
- MSX CAS playback and recording through emulated motor, write and read pins, without BIOS hooks or direct tape-to-RAM copying.
- A printer device on ports `0x90`-`0x97`, capturing bytes on the rising strobe edge.
- A Windows frontend with keyboard input, cassette and printer file dialogs, and 44.1 kHz mono PCM output.

## Interface and Integration

The public machine API is in [SVI728.hpp](include/SVI728/SVI728.hpp). `loadMainRom()` and `loadSlotRom()` supply ROM images; `readMemory()`/`writeMemory()` and `readIo()`/`writeIo()` expose machine accesses. `tick()` advances one CPU T-state and the intervening VDP master clocks. Keyboard setters accept active-low matrix values or individual pressed keys; `setJoystickPorts()` supplies joystick input values.

This self-contained host function accepts ROM bytes, checks loading and advances one PAL field. The surrounding application is responsible for reading the ROM file and presenting video/audio:

```cpp
#include <SVI728/SVI728.hpp>

bool runOneField(const std::vector<std::uint8_t>& rom)
{
	svi728::SVI728 machine;
	if (!machine.loadMainRom(rom)) {
		return false;
	}
	for (std::uint32_t tick = 0; tick < svi728::CpuTicksPerPalField; ++tick) {
		static_cast<void>(machine.tick());
	}
	return true;
}
```

Keep the machine instance across fields for normal operation. `video()`, `sound()` and `printer()` expose the VDP, PSG and printer device; `printer().output()` returns the captured bytes and `clearOutput()` clears them. `ram()`, `primarySlotRegister()` and `oneBitSoundLevel()` provide inspection. Cassette integration uses `loadCassette()`, `rewindCassette()`, `ejectCassette()`, `cassetteState()`, `startCassetteRecording()`, `stopCassetteRecording()` and `cassetteImage()`. A host calls `markCassetteSaved()` only after successful persistence.

For integration into another Visual Studio solution, retain the sibling directory layout, add [svi728.vcxproj](svi728.vcxproj) and the component projects `z80bus`, `ppi8255`, `tms99x8`, `tms9918a`, `tms9929a`, `ay8910`, `cartridge` and `printer`, and reference them from the host project. Add the `include` directories of SVI728 and the components to the host's include directories. The Windows frontend additionally links the system libraries `winmm` and `comdlg32`.

### I/O Mapping

| Port | Function |
| --- | --- |
| `0x98`, `0x99` | VDP data and control/status; mirrored over `0x98`-`0x9B` |
| `0x90`, even mirrors through `0x96` | Printer status on read (bit 1 clear means ready); bit 0 controls strobe on write |
| `0x91`, odd mirrors through `0x97` | Printer data output |
| `0xA0` | PSG register selection |
| `0xA1`, `0xA2` | PSG data write and read |
| `0xA8` | PPI port A: primary-slot selection |
| `0xA9` | PPI port B: keyboard matrix input |
| `0xAA` | PPI port C: keyboard row, cassette control and one-bit audio |
| `0xAB` | PPI mode control and BSR |

Joystick selection uses PSG register 15 bit 6, with input read through register 14. As on every MSX, PSG port A is an input and port B an output whatever register 7 bits 6-7 say. Cassette input occupies PSG port A bit 7 while the lower joystick bits are preserved.

### Timing and Video

The shared 21,477,270 Hz master clock supplies six ticks per Z80 T-state and four per VDP dot. A PAL field contains 313 lines, 428,184 master ticks, 107,046 dots and exactly 71,364 CPU T-states, approximately 50.16 fields per second. An NTSC field contains 262 lines, 358,416 master ticks and 59,736 CPU T-states, approximately 59.92 fields per second; `cpuTicksPerField()`, `vdpMasterTicksPerField()` and `fieldRateHz()` report the fitted chip. The PSG clock is approximately half the CPU clock.

As on every MSX, the wait generator inserts one wait state into each M1 cycle: every opcode fetch and every prefix byte (CB, DD, ED, FD) takes one T-state longer, interrupt acknowledge cycles are unaffected. `OUT (n),A` therefore takes 12 T-states, `OUTI` 18 and `OTIR` 23 per byte. This spacing decides whether VRAM writes reach the VDP access slots in time.

The Windows frontend schedules fields with QueryPerformanceCounter using the master-clock-derived period of the fitted VDP, not a rounded 50 or 60 Hz CPU budget. **Settings > PAL (TMS9929A, 50 Hz)** and **NTSC (TMS9918A, 60 Hz)** switch the machine like switching on the other model: ROM, modules and cassette stay, the machine restarts, and `svi728.ini` keeps the choice (`video=`); the status bar shows it. Keyboard messages are serviced every 256 CPU ticks. The window border is drawn from the VDP backdrop raster, 12.5 dots and lines beyond each edge of the active display, so register 7 changes during a field appear as horizontal stripes as on a real display, for example the colour bars of loaders that change the backdrop while reading tape. Audio sample counts use a separate remainder accumulator; three AY channels and timed PC7 transitions are mixed and DC-filtered for WinMM playback. If the audio device cannot be opened or a block cannot be queued, the frontend closes the device, keeps emulating silently, shows `Audio: off` in the status bar and retries opening the device every second. The window title shows the effective CPU clock, the emulated T-states per second of wall time measured once per second (for example `SVI-728 | CPU@3,58MHz`); the status bar below the picture shows the joystick keys, inserted cartridges, cassette and printer folder.

VDP data-port accesses use mode-dependent VRAM slots, read-ahead, pending-request replacement and slot-bound pointer advancement. The 4.3 ms vertical-refresh window removes normal slot waiting but retains VDP setup delay; refresh does not change VRAM contents. Raster registers latch at line boundaries. Graphics name/color/pattern fetches occur 10/6/4 dots before each group, text name/pattern fetches 8/4 dots before each group; sprite Y checks and attribute/pattern fetches are staged. Normal Z80 I/O cycles are used without artificial VDP WAIT states. See the [PAL VDP README](../../../chips/TMS9929A/README.md) for the component contract and limitations.

## Windows Operation

Build the `svi728_frontend` project, the solution's startup project. Its output is `svi728.exe`, also copied into the `Build` folder of the project (next to `Development`) after each frontend build. From the project folder, launch it with:

```powershell
.\Build\svi728.exe
```

The frontend loads `SVI-MSX-BIOS+Basic.bin` from its own directory (installed) or, started as `Build\svi728.exe`, the default ROM in `Development\SVI728\rom`. Another ROM file can be given as a command-line argument:

```powershell
.\Build\svi728.exe .\Development\SVI728\rom\SVI-MSX-BIOS+Basic.bin
```

**System > Save state...** writes a snapshot of the running machine (`.sst`, by default in `2100 - MSX - MEDIEN\STATE`), **Load state...** continues from one; a snapshot of the other video standard switches the machine first. A snapshot holds CPU, PPI, VDP, PSG, RAM, slot selection and the bank registers of the modules; the ROM, the module images, the cassette and the printer stay as they are, so a snapshot loads only with the same modules inserted. `-state` continues from a snapshot at start:

```powershell
.\Build\svi728.exe -state "..\2100 - MSX - MEDIEN\STATE\Game.sst"
```

The keyboard mapping translates Set-1 scan codes of a US PC keyboard (English interface) or a German PC keyboard (German interface) to the US-labeled MSX matrix; keys without a character: F6-F10 = Shift+F1-F5, Home = HOME, End = STOP, Page Up = SELECT, left Ctrl = CTRL, left Alt = GRAPH, right Ctrl = CODE, and the PC Shift key acts as MSX Shift, also on its own (BASIC then shows the F6-F10 labels). `Ctrl`+`End` sends MSX `CTRL`+`STOP`. **Joystick > Port 1 / 2** chooses off (default), the keyboard (port 1: arrow keys with `C`/`V`, port 2: `W`/`A`/`S`/`D` with `Z`/`X`, the two keys left of `C`, labelled `Y`/`X` on a German keyboard; the keys count by their scan codes; **F11** switches the keyboard joystick on and off, these keys then act on the joystick only) or a PC game controller; `svi728.ini` keeps the choice. `F12` plays a short AY diagnostic sequence. Frontend menu labels are currently German; the cassette actions below are described in English. The [user manual](../../Distribution/MANUAL.md) describes the operation.

## Printer Output

The printer uses the MSX I/O interface directly, not the 8255 PPI: BASIC checks the ready bit at `0x90`, sends a data byte to `0x91`, then pulses the strobe through bit 0 at `0x90`. The emulated status reports ready (BUSY bit 1 clear). Choose **Printer → Output folder...** once to select where output files are created; the directory is persisted as `printerDirectory` in `svi728.ini`. No warning appears just for starting the emulator or opening the printer menu. If no output directory is configured, a warning appears only after the first actual printer byte is sent, regardless of whether it came from BASIC, a BIOS call or direct I/O; the bytes remain buffered until a directory is selected. Each `LLIST` creates its own timestamped `.txt` file. A direct-mode `LPRINT` or `LPRINT USING` also creates a timestamped file. When a BASIC program is running, its first printer output opens one file, subsequent `LPRINT` statements (including `LPRINT USING`) append to that same file, and it closes when BASIC returns to direct mode. Filenames use local time (`YYYY-MM-DD_HH-MM-SS-mmm.txt`); a numeric suffix prevents overwriting if timestamps collide.

Z80 assembly can print through the standard MSX BIOS entries `LPTOUT` (`CALL 00A5h`) and `OUTDLP` (`CALL 014Dh`), or write ports `91h`/`90h` directly. These bytes use the same automatic timestamp-file handling; calls made by a running BASIC program are kept together until BASIC returns to direct mode.

## Cassette Playback and Recording

### Playback

Use the cassette menu's **open image** action and select an MSX `.cas` file in the Windows file dialog. It is inserted at the beginning and its filename appears in the status bar. Load it with the command appropriate to its contents, for example `CLOAD`, `RUN "CAS:"` or `BLOAD "CAS:",R`. The **rewind** action returns to the beginning; **eject** removes the image. File selection pauses emulation. Canceling selection or selecting an invalid image leaves the current tape unchanged.

While the motor runs, the status bar shows the current block, the data bytes already passed on tape (CAS markers excluded), the percentage and the running tape time, for example `Loading: Block 4/12, 1200/53122 Bytes (2 %), Band 0:33`. During recording it shows the CAS bytes captured and the tape time. At 1200 baud a tape delivers about 110 data bytes per second plus the pause and leader before each block, so a 50 KiB program takes about nine minutes.

Playback generates a 1200-baud MSX BIOS FSK signal with one start bit, eight data bits least-significant-bit first and two stop bits. A zero uses one full 1200 Hz period; a one uses two full 2400 Hz periods. PPI PC4 controls the motor active-low, PC5 supplies cassette write and PSG port A bit 7 supplies cassette read. Port directions and PPI BSR writes are respected. A stopped motor preserves tape position and pulse phase. Reset stops the motor without ejecting or rewinding the tape.

### Recording

Select the cassette menu's **new recording** action, then issue a BASIC command such as:

```text
CSAVE "TEST"
CSAVE "TEST",2
SAVE "CAS:TEXT",A
BSAVE "CAS:BIN",&HC100,&HC103
```

Run the command appropriate to the desired file type. Only after BASIC or assembly code has completed writing, select **finish recording and save** and choose the output `.cas` file. Recording decodes actual PC5 transitions at 1200 or 2400 baud. Assembly programs can use BIOS `TAPOON`/`TAPOUT`/`TAPOOF`, or access the pins directly with the same MSX BIOS encoding. Reading similarly supports `TAPION`/`TAPIN`/`TAPIOF` or direct PSG reads.

Stopping a valid recording rewinds the captured tape for playback with `CLOAD`, `LOAD "CAS:TEXT"` or `BLOAD "CAS:BIN"`. Canceling the save dialog or encountering a save error retains the capture in memory. An asterisk in the title marks unsaved data; opening another image, ejecting, starting a new recording or closing requests confirmation before discarding it. A new recording replaces the in-memory tape but never automatically overwrites the mounted file.

The suggested filename comes from the first recognized six-byte CAS filename header, sanitized for Windows; a `TEST` header suggests `TEST.cas`. Saving uses a temporary file and `MoveFileExW` replacement. Invalid or truncated signals and CAS marker collisions prevent export.

### Format and Capacity

The supported format is MSX CAS with the marker `1F A6 DE BA CC 13 7D 74` on eight-byte boundaries, up to 16 MiB. Recording is limited to eight million edges per motor-on section. CAS contains data blocks rather than original pulse timings: playback reconstructs a two-second pause and 8,000 one bits for the first or recognized file header, otherwise a one-second pause and 2,000 one bits. See [docs/CAS.md](docs/CAS.md) for format and signal details.

## Limits

- Secondary slots are not implemented (the SVI-728 has none); the SCC sound of Konami SCC modules is not emulated.
- SVI-318/328 CAS, WAV, TSX and arbitrary turbo tape encodings are not supported.
- The PAL VDP's disabled-display Text-mode slot reference is not fully verified. Analog video waveform shape and sub-dot internal chip logic are not modeled.
- Sound uses approximate AY levels and software mixing/filtering, not a complete analog board model.
- The graphical frontend is Windows-only and depends on the embedded ROM path unless an alternative is supplied. The library does not provide a host display, file dialog or audio device.

## Build and Test

Requirements: Visual Studio 2022 with **Desktop development with C++** (MSVC v143 toolset and a Windows 10/11 SDK). The solution [SVI728.sln](../SVI728.sln) one directory up contains this project and the sibling [Z80CPU](../../../chips/Z80CPU/README.md), [PPI8255](../../../chips/PPI8255/README.md), [TMS99X8](../../../chips/TMS99X8/README.md), [TMS9918A](../../../chips/TMS9918A/README.md), [TMS9929A](../../../chips/TMS9929A/README.md), [AY8910](../../../chips/AY8910/README.md), [CARTRIDGE](../../../msx/CARTRIDGE/README.md) and [PRINTER](../../../chips/PRINTER/README.md) projects; keep these directories alongside this project. Shared compiler settings are in [Common.props](../Common.props): C++17, C11 for the included CPU core, `/W4 /permissive- /utf-8` with narrowing warning C4244 disabled, static runtime library (`/MTd`, `/MT`) and the configurations `Debug|x64` and `Release|x64`. The solution builds the frontend together with all libraries and tests.

Open the solution in Visual Studio and build it, or run from Developer PowerShell in the solution directory:

```powershell
msbuild SVI728.sln /p:Configuration=Release /p:Platform=x64
.\run_tests.ps1 -Configuration Release
```

To build only the frontend, add `/t:SVI728\svi728_frontend`. Binaries are in `..\Build\x64\<Configuration>`, intermediate files in `..\Build\obj`. Because the runtime library is linked statically, `svi728.exe` and the test executables need no additional runtime DLLs.

| Project | Type | Output |
| --- | --- | --- |
| `svi728` | Static library | `svi728_machine.lib`, machine library |
| `svi728_frontend` | Windows application | `svi728.exe`, also copied to `..\Build` |
| `svi728_tests` | Console application | `svi728_tests.exe` |
| `cassette_bios_tests` | Console application | `cassette_bios_tests.exe`, ROM path as argument |
| `cassette_basic_tests` | Console application | `cassette_basic_tests.exe`, ROM path as argument |

## Test Coverage

[run_tests.ps1](../run_tests.ps1) runs eleven tests: the three machine/cassette tests of this project and `z80bus_tests`, `ppi8255_tests`, `ay8910_tests`, `tms99x8_tests`, `tms9929a_tests`, `tms9918a_tests`, `cartridge_tests` and `printer_tests` from the components. It passes the default ROM `rom/SVI-MSX-BIOS+Basic.bin` to both cassette tests; in Visual Studio the same argument is preset as debugger command argument. Test executables are separate from the frontend.

- [svi728_tests.cpp](tests/svi728_tests.cpp) checks ROM mirroring, primary slots and RAM, cartridge loading and I/O decode, printer port mirrors and byte capture, keyboard translation, PPI/VDP/PSG integration (port A stays an input), PAL and NTSC CPU/VDP field timing, snapshots (exact continuation, module bank registers, refusal without the module, for the other standard or when truncated), CPU bus operation, the M1 wait state for plain, prefixed and block instructions, cassette pulses and pins, recording error handling and filename suggestions.
- [cassette_bios_tests.cpp](tests/cassette_bios_tests.cpp) executes the supplied ROM's tape routines through the Z80, PPI and PSG, verifying reading and recording round trips at both write speeds.
- [cassette_basic_tests.cpp](tests/cassette_basic_tests.cpp) boots BASIC and verifies `CSAVE`/`NEW`/`CLOAD` and `BSAVE`/`POKE`/`BLOAD` using byte-exact comparisons. This integration test can take substantially longer than component tests; its timeout in `run_tests.ps1` is 180 seconds.

These tests verify machine, printer-port capture, BASIC `LLIST`/`LPRINT`/`LPRINT USING` behavior, BIOS printer calls from Z80 assembly (`LPTOUT` and `OUTDLP`), and tape behavior. They do not verify Windows folder-dialog interaction, audio-device operation or exhaustive hardware compatibility.