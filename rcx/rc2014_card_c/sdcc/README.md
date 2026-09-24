# RC2014 cartridge - card type C (82C55 PPI) - C/SDCC port (MSXgl)

## 8255/8255.c

C/SDCC port of [`../com/8255/8255.ASM`](../com/8255/8255.ASM), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): RCX INIT, PPI INIT, PPI CTRL (Mode 0, Port A output,
Port B input, Port C output), then writes an incrementing byte to Port A and reads Port A/B/C back,
toggles a Port C bit via SET/RESET - via `FN_CORE2/3/4/23/24/25/26/28/30/31/32` directly through
EXTBIO discovery, no BASIC interpreter involved. Links against
[`../../../lib/rcx_io/`](../../../lib/rcx_io/) and
[`../../../lib/dostools/`](../../../lib/dostools/) only - calls no `Xio_*` function.

The PPI base address is read from `/IO:<0..252>` on the command line; missing or out of range
prints a usage message and exits, matching `8255.ASM`'s own command-line-only behavior.

## 8255I/8255I.c

C/SDCC port of [`../com/8255I/8255I.ASM`](../com/8255I/8255I.ASM): Mode 0, Port A output, Port C
upper output; Group B is Mode 1 (Port B strobed input) so the 82C55 itself raises INTR B on PC0 -
fed to the PIC via the **standard** XIO handler (no custom ISR). PC4 is toggled each loop pass to
trigger the interrupt when wired to PC2 (STB B). Discovers **both** `RCX_INTERFACE` and
`XIO_IO_EXPANDER` separately, so it links against `rcx_io.lib`, `xio_io.lib`, and `dostools.lib`.

The PPI base address and PIC interrupt channel are read from `/IO:<0..252>` and `/INT:<0..7>` on
the command line; missing or out of range prints a usage message and exits.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/xio_io/`'s contents into `MSXgl/lib/xio_io/`,
`../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and either tool's subfolder
(`8255/` or `8255I/`) into the matching `MSXgl/projects/<Name>/` - all paths must stay space-free,
a requirement of the MSXgl build tool itself.

```sh
cd MSXgl/projects/8255 && node ../../engine/script/js/build.js
cd MSXgl/projects/8255I && node ../../engine/script/js/build.js
```

Prebuilt `.com` files are included so either tool can be used without rebuilding.
