# RC2014 cartridge - card type E (CAN) - C/SDCC port (MSXgl)

## SJA1000/SJA1000.c

C/SDCC port of [`../com/SJA1000/SJA1000.ASM`](../com/SJA1000/SJA1000.ASM) (itself a 1:1 port of
[`../basic/SJA1000.ASC`](../basic/SJA1000.ASC)), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): UNAPI/EXTBIOS driver for the SJA1000 CAN card.
Discovers **both** `RCX_INTERFACE` (CAN functions) and `XIO_IO_EXPANDER` (interrupt) separately,
each through its own discovery/dispatch pair, and registers **RCX ROM's own CAN interrupt service
routine** (the address `CAN GET ADDR` returns) via `XIO SET ADDR` - no ISR bytes to copy/assemble
at all. CAN is not implemented in `RCXRAM.COM`, so
this only works against a real RCX ROM card. Links against
[`../../../lib/rcx_io/`](../../../lib/rcx_io/) (`Rcx_*`/`Can_*` calls),
[`../../../lib/xio_io/`](../../../lib/xio_io/) (real `Xio_*` SET ADDR/SET MASK calls), and
[`../../../lib/dostools/`](../../../lib/dostools/) (print/input helpers). The RCX work area passed
to `Rcx_Init()` is the fixed page-3 address `0xC200` (matching `SJA1000.ASM`'s own `RCXWORKAREA
EQU`), not a program-local buffer: `CAN INIT` writes a real interrupt routine into it, which then
runs from actual hardware-interrupt context, where the MSX BIOS forces page 0 to system ROM.

The IO address and PIC interrupt channel are normally read from `/IO:<0..254>` and `/INT:<0..7>`
on the command line; either one falls back to an interactive prompt when missing or out of range,
matching `SJA1000.ASC`'s own `INPUT` prompts. Main loop: `CAN CHECK` each iteration - on a new
message, reads and prints it as a hex dump; key `1` asks for a target address (`3000..33FF` hex)
and direction (`0`/`1`) and sends it; `ESC` exits.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/xio_io/`'s contents into `MSXgl/lib/xio_io/`,
`../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and this folder into
`MSXgl/projects/SJA1000/` - all paths must stay space-free, a requirement of the MSXgl build tool
itself.

```sh
cd MSXgl/projects/SJA1000 && node ../../engine/script/js/build.js
```

Prebuilt `SJA1000.com` is included so the tool can be used without rebuilding.
