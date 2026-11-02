# RC2014 cartridge - card type I (MCP4822 DAC) - C/SDCC port (MSXgl)

## DAC/DAC.c

C/SDCC port of [`../com/DAC/DAC.ASM`](../com/DAC/DAC.ASM), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): runs the same tests as the BASIC original, but via
`FN_CORE2/3/4/63..67` directly through EXTBIO discovery, no BASIC interpreter involved. `TEST A`
writes and reads back 0/1365/2730/4095 on every channel (`Dac_Set`/`Dac_Get`); `TEST B` does the
same with `Dac_Hold`; `TEST C` sets all four outputs at once (three channels with `Dac_Hold`, the
last with `Dac_Set`); `TEST D` checks gain x1, shutdown and gain x2 with `Dac_Conf`. Links against
[`../../../lib/rcx_io/`](../../../lib/rcx_io/) and
[`../../../lib/dostools/`](../../../lib/dostools/) only - calls no `Xio_*` function. The banner
starts with `DAC (SDCC)`.

The card's base address is read from `/IO:<0..252>` on the command line; missing or out of range
prints a usage message and exits.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and this
folder into `MSXgl/projects/DAC/` - all paths must stay space-free, a requirement of the MSXgl
build tool itself.

```sh
cd MSXgl/projects/DAC && node ../../engine/script/js/build.js
```

Prebuilt `DAC.com` is included so the tool can be used without rebuilding.
