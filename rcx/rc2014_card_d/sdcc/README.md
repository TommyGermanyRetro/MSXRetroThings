# RC2014 cartridge - card type D (82C54 timer) - C/SDCC port (MSXgl)

## 8254/8254.c

C/SDCC port of [`../com/8254/8254.ASM`](../com/8254/8254.ASM), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): RCX INIT, TIMER INIT, TIMER0/TIMER1 both set to
MODE 2 (Rate Generator), TIMER0 preset to `0xFFFF`, TIMER1 to `16`, TIMER2 (MODE 0) preset to
`0xFFFF`, all three gated on once - `OUT0` wired externally to `CLK1`, `OUT1` to `CLK2` - then
reads TIMER2 back in an unconditional loop (no exit condition, matching the ASM original's plain
loop with no key poll) - via `FN_CORE2/33/35/37/38/39/40/41/42` directly through EXTBIO discovery.
Links against [`../../../lib/rcx_io/`](../../../lib/rcx_io/) and
[`../../../lib/dostools/`](../../../lib/dostools/) only - calls no `Xio_*` function.

The timer base address is read from `/IO:<0..251>` on the command line (note the narrower range
than every other card's tools) - missing or out of range prints a usage message and exits.

## 8254I/8254I.c

C/SDCC port of [`../com/8254I/8254I.ASM`](../com/8254I/8254I.ASM): same TIMER0/TIMER1/TIMER2 setup
as `8254`, but `OUT1` is **also** wired to a PIC channel - TIMER2 is read/printed continuously, and
every PIC interrupt (from TIMER1, via the **standard** XIO handler, no custom ISR) prints a
"fired" message plus a running count. Discovers **both** `RCX_INTERFACE` and `XIO_IO_EXPANDER`
separately, so it links against `rcx_io.lib`, `xio_io.lib`, and `dostools.lib`.

The timer base address and PIC interrupt channel are read from `/IO:<0..251>` and `/INT:<0..7>` on
the command line; missing or out of range prints a usage message and exits.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/xio_io/`'s contents into `MSXgl/lib/xio_io/`,
`../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and either tool's subfolder
(`8254/` or `8254I/`) into the matching `MSXgl/projects/<Name>/` - all paths must stay space-free,
a requirement of the MSXgl build tool itself.

```sh
cd MSXgl/projects/8254 && node ../../engine/script/js/build.js
cd MSXgl/projects/8254I && node ../../engine/script/js/build.js
```

Prebuilt `.com` files are included so either tool can be used without rebuilding.
