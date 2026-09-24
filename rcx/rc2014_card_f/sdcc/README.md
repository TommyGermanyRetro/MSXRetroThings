# RC2014 cartridge - card type F (MCP23S17 SPI GPIO expander) - C/SDCC port (MSXgl)

## MCP23S17/MCP23S17.c

C/SDCC port of [`../com/MCP23S17/MCP23S17.ASM`](../com/MCP23S17/MCP23S17.ASM), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): RCX INIT, SPI INIT/MODE on the card's SPI bus
(`/BUS:`, `0..3`), then a continuous loop that writes an incrementing byte to an MCP23S17 GPIO
expander's GPIO A and reads GPIO B back via SPI WRB/RDH - via `FN_CORE2/50/52/55/56` directly
through EXTBIO discovery, no BASIC interpreter involved. Links against
[`../../../lib/rcx_io/`](../../../lib/rcx_io/) and
[`../../../lib/dostools/`](../../../lib/dostools/) only - calls no `Xio_*` function.

The SPI base address and bus number are read from `/IO:<0..252>` and `/BUS:<0..3>` on the command
line; missing or out of range prints a usage message and exits.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and this
folder into `MSXgl/projects/MCP23S17/` - all paths must stay space-free, a requirement of the
MSXgl build tool itself.

```sh
cd MSXgl/projects/MCP23S17 && node ../../engine/script/js/build.js
```

Prebuilt `MCP23S17.com` is included so the tool can be used without rebuilding.
