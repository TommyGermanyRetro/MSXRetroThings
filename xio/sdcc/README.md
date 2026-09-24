# XIO cartridge - C/SDCC port (MSXgl)

C ports of the BASIC/COM XIO-Expander tools, built with SDCC via the
[MSXgl](https://github.com/aoineko-fr/MSXgl) library instead of hand-written Z80 assembler.

MSXgl has no built-in support for a project-specific UNAPI driver like `XIO_IO_EXPANDER`, so
[`../../lib/xio_io/`](../../lib/xio_io/) is a small hand-written, precompiled SDCC library
(`xio_io.h`/`xio_io.s`, assembled with `sdasz80` into `xio_io.rel` and archived with `sdar` into
`xio_io.lib`) that performs the EXTBIO discovery and the segment-aware `CALSLT` dispatch - a
complete library covering every `XIOROM.ASM` entry point except its BASIC-only interactive help
menu. Plain MSX-DOS console I/O and print/input helpers (`Dos_ReadLine`, `Dos_PrintHex2`,
`Dos_AskDecimal`, etc.) live in the separate [`../../lib/dostools/`](../../lib/dostools/) instead.

## XIOIM2/XIOIM2.c

Same Z80 PIO IM2 interrupt test as `XIOIM2.ASC`/`XIOIM2.ASM`: installs a Z80 machine code service
routine via `XIO SET ADDR` on the chosen IM2 channel, toggles PA0, reports which channel fired
with a running per-channel counter at a fixed screen position. IO address and IM2 channel (0..7)
are asked for interactively, same as the BASIC original.

## XIOPIC/XIOPIC.c

Same PIC interrupt test as `XIOPIC.ASC`/`XIOPIC.ASM`: installs a Z80 machine code service routine
via `XIO SET ADDR` on PIC channel 0, enables all 8 PIC channels via `XIO SET MASK`, reports
whichever fires with a running per-channel counter - no UNAPI parameters or interactive prompts
needed beyond discovery, since the interrupt source is external hardware, not configurable here.

## XIOSLT/XIOSLT.c

Same BIOS-only slot work area dump as `XIOSLT.ASC`/`XIOSLT.ASM`: asks for a main slot number,
dumps the 8-byte MSX BIOS slot work area (`SLTWRK`) for that slot, then follows the pointer
stored at its bytes 6+7 and dumps 96 (`&H60`) further bytes from there. Calls no `Xio_*` function
at all and needs no EXTBIO discovery - as generic/UNAPI-free as the BASIC original; links only
against [`../../lib/dostools/`](../../lib/dostools/), for its `Dos_ReadLine`/`Dos_PrintHex2`
helpers (plain MSX-DOS console I/O, unrelated to the XIO chip).

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../lib/xio_io/`'s contents into
`MSXgl/lib/xio_io/`, `../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and a tool's
folder (e.g. `XIOIM2/`) into `MSXgl/projects/XIOIM2/` - all paths must stay space-free, a
requirement of the MSXgl build tool itself. Each tool's `project_config.js` already references the
libraries it needs with the matching relative paths, e.g.:

```js
CompileOpt = "-I../../lib/xio_io -I../../lib/dostools";                              // header lookup
AddLibs = [ "../../lib/xio_io/xio_io.lib", "../../lib/dostools/dostools.lib" ];      // linked archives
```

`AddLibs` is a small addition to MSXgl's own build script (`engine/script/js/build.js`), not part
of stock MSXgl - it appends a prebuilt `.lib` archive to the SDCC link command line *after* the
project's own compiled sources, which a plain `LinkOpt` entry could not do (an archive listed
before the code that needs it is not pulled in by a single left-to-right link pass).

```sh
cd MSXgl/lib/xio_io && sdasz80 -o -l -s xio_io.s && sdar -rc xio_io.lib xio_io.rel   # only if xio_io.s changed
cd MSXgl/projects/XIOIM2 && node ../../engine/script/js/build.js   # or XIOPIC/XIOSLT, etc.
```

Prebuilt `.rel`/`.lib` files for both libraries and each tool's `.com` are included so both the
libraries and the tools can be used without rebuilding.
