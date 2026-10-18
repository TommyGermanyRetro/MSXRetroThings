# TMS99X8 Library

TMS99X8 is a standalone C++17 static library with the common logic of the Texas Instruments video display processors TMS9918A, TMS9928A and TMS9929A, which share one data manual and one design. It provides CPU-facing ports, master-clock progression, timed VRAM requests and live raster scanout for a host emulator. The chips themselves are separate components on top of it: [TMS9918A](../TMS9918A/README.md) (NTSC) and [TMS9929A](../TMS9929A/README.md) (PAL).

## Functionality

- Two timing models: `Model::Pal` with 313 scanlines, active display from line 64 and 50 fields per second; `Model::Ntsc` with 262 scanlines, active display from line 40 and 60 fields per second.
- 16 KiB of VRAM, eight VDP registers, Graphics I/II, Text and Multicolor modes, sprites and status/interrupt handling.
- A 256 x 192 row-major frame of palette indices `0` through `15`, plus approximate digital YPbPr output.
- A 21,477,270 Hz master clock, four master ticks per dot and 342 dots per scanline: 1,368 ticks per line; 428,184 ticks per PAL field (about 50.16 fields per second) and 358,416 ticks per NTSC field (about 59.92). `fieldRateHz()` reports the nominal integer value `50` or `60`.
- Master-tick, dot and scanline stepping; live `scanoutFrame()` as well as current-state snapshot rendering.
- Buffered CPU VRAM requests with mode-dependent access slots, read-ahead, request replacement and slot-bound address advancement.

## Interface and Integration

[TMS99X8.hpp](include/TMS99X8/TMS99X8.hpp) exposes `tms99x8::TMS99X8`, constructed with a `tms99x8::Model` (PAL by default), together with the shared types `FrameBuffer`, `BackdropRaster`, `Outputs`, `YpbprFrame` and the timing constants. Its destructor is virtual, so a machine can hold the fitted chip through this type. Port `0` accesses VRAM data; port `1` accepts control writes or reads status. Two control writes set a VRAM address or register: write the low address/data byte first, then the high address/operation byte. For a register write, the second byte is `0x80 | registerIndex`; for a VRAM write address, it is `0x40 | addressHigh`.

This self-contained function enables the display, selects a backdrop color and advances one PAL field before copying its live scanout:

```cpp
#include <TMS99X8/TMS99X8.hpp>

tms99x8::FrameBuffer makePalField()
{
	tms99x8::TMS99X8 vdp(tms99x8::Model::Pal);
	vdp.write(1, 0x40);
	vdp.write(1, 0x81);
	vdp.write(1, 0x01);
	vdp.write(1, 0x87);
	for (unsigned tick = 0; tick < 428184; ++tick) {
		vdp.advanceMasterTick();
	}
	return vdp.scanoutFrame();
}
```

The host synchronizes CPU port accesses with `advanceMasterTick()`, or uses `advanceDot()`/`advanceScanline()` when coarser stepping is sufficient. Route `outputs().interruptRequest` to the CPU and convert palette indices to display colors. Reading status acknowledges latched status conditions. `scanoutFrame()` references pixels assembled as the beam advanced; `backdropRaster()` holds the register 7 backdrop colour for every dot of every raster line (342 x 313, indexed `line * 342 + dot`), so a host can draw the border, including colour changes within a line; `renderFrame()` instead renders current state and updates sprite status. `peekVram()` and `pokeVram()` bypass timed CPU ports for debugging. `reset()`, `registerValue()` and `outputs()` support state management and inspection.

Raster register values are latched at line boundaries. Graphics groups fetch name, color and pattern data 10, 6 and 4 dots before the group starts; text groups fetch name and pattern 8 and 4 dots beforehand. Pixels use those buffered bytes. Sprite Y checks are staged raster events, followed by attribute and pattern fetches for the selected line; fetches still open when the evaluation for the next line begins are completed first, so up to four sprites per line are always drawn.

CPU data-port requests execute at mode-dependent VRAM slots, at the earliest 28 master clocks after the request; a later request can replace a pending request. In Graphics I and II each fourth memory cycle after a character's fetches serves the CPU and the other three the sprite Y scan; once the scan of a line has stopped (Y code 208, or the sprites for the next line found), its remaining Y cycles serve the CPU as well (TI, US patent 4,243,984). In the 4.3 ms vertical-refresh window, requests need not wait for normal access slots, but the VDP setup delay still applies. Address advancement accompanies request execution. Refresh does not change emulated VRAM contents and is not simulated as a sequence of ineffective DRAM reads.

For integration into another Visual Studio solution, add [tms99x8.vcxproj](tms99x8.vcxproj) together with the chip project, reference both from the host project and add both `include` directories. The implementation has no external library dependencies.

## Limits

- Analog waveform shape, electrical output and sub-dot internal chip logic are not modeled. YPbPr output is a digital palette approximation.
- The disabled-display Text-mode VRAM slot reference is not fully verified; the documented slot model is not a claim of exhaustive hardware equivalence.
- Debug VRAM helpers and snapshot rendering bypass parts of the timed scanout path. Use live scanout and timed ports when observing raster effects.
- The host must advance time between CPU accesses; calling register/VRAM APIs alone does not advance the raster clock.

## In the SVI-318 and SVI-328

The machine fits one of the two chips built on this library, the PAL [TMS9929A](../TMS9929A/README.md) or the NTSC [TMS9918A](../TMS9918A/README.md). The CPU writes the VDP at `80h` (data) and `81h` (control) and reads it at `84h` (data) and `85h` (status); the frame interrupt goes to the Z80 INT. CPU and VDP share the 21.477 MHz master clock, six master clocks per CPU T-state. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln), [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-728](../../svi728/Development/run_tests.ps1), [SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `tms99x8` | Static library | `tms99x8.lib` |
| `tms99x8_tests` | Console application | `tms99x8_tests.exe` |

## Tests

[tms99x8_tests.cpp](tests/tms99x8_tests.cpp) checks, on the PAL model, the timing and vertical blank, delayed VRAM writes and read-ahead, replacement of pending requests, refresh-window setup delay, a Graphics-I access slot, sprite Y cycles serving the CPU after Y = 208 but not during the scan, line-boundary register latching and staged sprite attribute fetches. Rendering checks cover YPbPr conversion, text margins and sprite suppression, mode-bit combinations, Graphics-II banking, sprite pattern selection with the full register 6 base, 16 x 16 sprite quadrant order in snapshot and timed scanout, four sprites on one line without a terminating entry, 8-bit sprite Y wrap, suppression of the fifth-sprite flag while F is set, sprite status and collision checks on the undisplayed line 192, the TMS99xx mixed modes Text with M3 and Multicolor with M3 (pattern quarters through register 4 only) and the bogus modes (8 border pixels and 40 groups of 4 foreground and 2 background pixels), transparent-sprite collisions, dot-accurate backdrop raster recording, overflow status and preservation of fetched raster groups. These are targeted regressions, not exhaustive verification of every slot sequence, sprite interaction or analog output.
