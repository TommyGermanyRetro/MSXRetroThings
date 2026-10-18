# V9938

V9938 is a standalone C++17 static library modeling the Yamaha V9938 MSX-VIDEO display processor with 128 KiB of VRAM. Sources: V9938 MSX-Video Technical Data Book (PDF and the grauw transcription in `9999 - MSX - Dokumentation`); timing details missing from the transcription follow the observable behaviour of openMSX.

## Functionality

- Ports 0-3: VRAM data with read-ahead, control (two-byte register or address sequence, status read through R#15), palette (two bytes per entry, R#16 auto-increment) and indirect register access through R#17.
- Control registers R#0-R#23 and R#32-R#46 with the bit masks of the data book; status registers S#0-S#9.
- 17-bit VRAM address with R#14; in GRAPHIC 6 and 7 CPU, table and command accesses interleave both 64 KiB banks.
- Time base of 1368 master clocks per line, 262 (NTSC) or 313 (PAL) lines latched per field, 192 or 212 display lines, display position through R#18, vertical interrupt (IE0, S#0 F) and line interrupt (IE1, R#19, S#1 FH), HR, VR and EO flags.
- Line-by-line rendering into a 512 x 212 frame of 0x0GRB colours plus one border colour pair per raster line:
  - TEXT 1, TEXT 2 with blinking (R#12, R#13), MULTICOLOR, GRAPHIC 1-3.
  - GRAPHIC 4-7, page selection through R#2, alternating pages through EO and R#13, vertical scroll R#23.
  - Colour 0 shows the backdrop unless R#8 TP is set; GRAPHIC 5 colours even and odd dots separately; GRAPHIC 7 uses GGGRRRBB colours and fixed sprite colours.
- Sprite mode 1 (4 per line) and sprite mode 2 (8 per line, colour per line, EC, CC, IC), fifth/ninth sprite number, collision flag and coordinates.
- Command engine with HMMC, YMMM, HMMM, HMMV, LMMC, LMCM, LMMM, LMMV, LINE, SRCH, PSET, POINT and STOP, the logical operations IMP, AND, OR, EOR, NOT and their T variants, CE/TR/BD flags and the register values after a command. Commands work in GRAPHIC 4-7 only.
- `saveState()` and `loadState()` copy VRAM, registers, palette, timing and command engine for snapshots; data of another size is refused.

## Interface and Integration

The public API is in [V9938.hpp](include/V9938/V9938.hpp). The host decodes the I/O ports and calls `read(port & 3)` and `write(port & 3, value)`. `advanceMasterTick()` advances one 21.477 MHz master clock; the interrupt line is `outputs().interruptRequest`. `frame()`, `frameDisplayLines()` and `borderRaster()` provide the picture; `frameCount()` advances at the start of every field, and `interlaced()`/`secondField()` describe the field for interlaced output. `peekVram()`, `pokeVram()`, `controlRegister()`, `peekStatus()` and `paletteEntry()` give side-effect-free access for debuggers and tests.

```cpp
#include <V9938/V9938.hpp>

void runOneField(v9938::V9938& vdp)
{
    const std::uint32_t field = vdp.frameCount();
    while (vdp.frameCount() == field) vdp.advanceMasterTick();
    const v9938::FrameBuffer& picture = vdp.frame();
    (void)picture;
}
```

For integration into another Visual Studio solution, add [v9938.vcxproj](v9938.vcxproj), reference it from the host project and add this project's `include` directory to the host's include directories. There are no external library dependencies.

## Limits

- CPU VRAM accesses take effect at once; access slots and too-fast accesses are not modeled.
- Commands use a fixed duration per step instead of the real VRAM access windows; display and sprite activity do not slow them down.
- No expansion RAM: MXC, MXS and MXD reads return FFh and writes are lost, as on the Philips VG 8235.
- Not modeled: light pen, mouse, superimpose, digitize, colour bus, horizontal adjust within the picture (R#18 shifts only the timing), YJK modes of later VDPs.

## Build and Test

The project is used by the solution [VG8235.sln](../../vg8235/Development/VG8235.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([VG-8235](../../vg8235/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `v9938` | Static library | `v9938.lib` |
| `v9938_tests` | Console application | `v9938_tests.exe` |

## Test Coverage

[v9938_tests.cpp](tests/v9938_tests.cpp) has 27 test groups: reset state, register and indirect access, VRAM addressing, palette, field timing, vertical and line interrupts, retrace flags, every display mode, both sprite modes, all commands with their logical operations, CPU transfers and command limits.
