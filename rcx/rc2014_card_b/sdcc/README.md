# RC2014 cartridge - card type B (Z80 PIO) - C/SDCC port (MSXgl)

## Z80PIO/Z80PIO.c

C/SDCC port of [`../com/Z80PIO/Z80PIO.ASM`](../com/Z80PIO/Z80PIO.ASM), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): RCX INIT, PIO INIT, Port A MODE 0 (byte output),
Port B MODE 1 (byte input), then writes an incrementing byte to Port A and reads Port B back in a
loop - via `FN_CORE2/3/4/16/17/18/21/22` directly through EXTBIO discovery, no BASIC interpreter
involved. Uses RC2014 SC103 as DUT. Links against [`../../../lib/rcx_io/`](../../../lib/rcx_io/)
and [`../../../lib/dostools/`](../../../lib/dostools/) only - calls no `Xio_*` function.

The PIO base address is read from `/IO:<0..252>` on the command line; missing or out of range
prints a usage message and exits (no interactive fallback), matching `Z80PIO.ASM`'s own
command-line-only behavior.

## Z80PIOI/Z80PIOI.c

C/SDCC port of [`../com/Z80PIOI/Z80PIOI.ASM`](../com/Z80PIOI/Z80PIOI.ASM): installs a Z80 ISR stub
via IM2 (`XIO SET ADDR`, channel `INT+8`) at a fixed page-3 address, toggles PA0, and reports which
PIO channel triggered the interrupt (the IM2 flag word's **high** byte, bound via `XIO INIT` and
updated by the ROM on every IM2 interrupt, independent of the custom ISR). PIO registers themselves
go through the RCX PIO library functions, not raw port I/O. Discovers **both** `RCX_INTERFACE` and
`XIO_IO_EXPANDER` separately (two independent UNAPI drivers, own slot/segment/entry each), so it
links against `rcx_io.lib`, `xio_io.lib`, and `dostools.lib`.

The PIO base address and IM2 channel are read from `/IO:<0..252>` and `/INT:<0..7>` on the command
line; missing or out of range prints a usage message and exits, matching `Z80PIOI.ASM`'s own
command-line-only behavior.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/xio_io/`'s contents into `MSXgl/lib/xio_io/`,
`../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and either tool's subfolder
(`Z80PIO/` or `Z80PIOI/`) into the matching `MSXgl/projects/<Name>/` - all paths must stay
space-free, a requirement of the MSXgl build tool itself. `Z80PIOI` additionally needs `"memory"`
added to `project_config.js`'s `LibModules` (for `Mem_Copy`) - already set in the shipped
`project_config.js`.

```sh
cd MSXgl/projects/Z80PIO && node ../../engine/script/js/build.js
cd MSXgl/projects/Z80PIOI && node ../../engine/script/js/build.js
```

Prebuilt `.com` files are included so either tool can be used without rebuilding.
