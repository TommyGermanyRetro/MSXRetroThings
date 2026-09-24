# RC2014 cartridge - card type A (I2C+RTC) - C/SDCC port (MSXgl)

## RTCLCD/RTCLCD.c

C/SDCC port of [`../com/RTCLCD/RTCLCD.ASM`](../com/RTCLCD/RTCLCD.ASM) (itself a 1:1 port of
[`../basic/RTCLCD.ASC`](../basic/RTCLCD.ASC)), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): reads/shows the PCF8583 RTC date/time, optionally
lets the user change it, then writes both to the SEED I2C LCD - via `FN_CORE2/3/4/6/8/12..15`
directly through EXTBIO discovery, no BASIC interpreter involved. A real `RCX_INTERFACE` UNAPI
consumer, so it links against [`../../../lib/rcx_io/`](../../../lib/rcx_io/) (`rcx_io.h`/`.s`/
`.rel`/`.lib`, covering the full 62-function `RCX_INTERFACE` surface) as well as
[`../../../lib/dostools/`](../../../lib/dostools/) (`Dos_ReadLine`/`Dos_PrintHex2`/print/input
helpers, unrelated to either UNAPI driver) - both shared, top-level libraries used across the
whole repo. Calls no `Xio_*` function directly, so `xio_io` is not needed here at all.

The IO address is normally read from `/IO:<0..254>` on the command line; if that parameter is
missing or out of range, it falls back to an interactive `INPUT`-style prompt
("I2C card, IO address (0..254): "), matching `RTCLCD.ASC`'s own behavior.

## RTCLCDI/RTCLCDI.c

C/SDCC port of [`../com/RTCLCDI/RTCLCDI.ASM`](../com/RTCLCDI/RTCLCDI.ASM) (itself a 1:1 port of
[`../basic/RTCLCDI.ASC`](../basic/RTCLCDI.ASC)): reads/shows the PCF8583 RTC date/time, optionally
lets the user change it, writes both to the SEED I2C LCD, then keeps the LCD refreshed on every
PIC channel interrupt (the XIO cartridge's standard handler, no custom ISR stub) - the RTC is
re-read and the SEED LCD rewritten each time. Discovers **both** `RCX_INTERFACE` and
`XIO_IO_EXPANDER` separately (two independent UNAPI drivers, own slot/segment/entry each), so it
links against `rcx_io.lib` (RCX/I2C/RTC calls), `xio_io.lib` (the real `Xio_*` PIC-interrupt
calls), and `dostools.lib` (print/input helpers, shared).

The IO address and PIC interrupt channel are normally read from `/IO:<0..254>` and `/INT:<0..7>`
on the command line; either one falls back to an interactive prompt when missing or out of range,
matching `RTCLCDI.ASC`'s own `INPUT` prompts.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/xio_io/`'s contents into `MSXgl/lib/xio_io/`,
`../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and either tool's subfolder
(`RTCLCD/` or `RTCLCDI/`) into the matching `MSXgl/projects/<Name>/` - all paths must stay
space-free, a requirement of the MSXgl build tool itself.

```sh
cd MSXgl/projects/RTCLCD && node ../../engine/script/js/build.js
cd MSXgl/projects/RTCLCDI && node ../../engine/script/js/build.js
```

Prebuilt `.com` files are included so either tool can be used without rebuilding.
