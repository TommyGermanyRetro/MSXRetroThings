# RC2014 cartridge - card type G (ADS1220 ADC) - C/SDCC port (MSXgl)

## ADS1220/ADS1220.c

C/SDCC port of [`../com/ADS1220/ADS1220.ASM`](../com/ADS1220/ADS1220.ASM), built with
[MSXgl](https://github.com/aoineko-fr/MSXgl): runs the same TEST A/B/C/D/F/G as the BASIC
original, but via `FN_CORE2/4/57..62` directly through EXTBIO discovery, no BASIC interpreter
involved. `TEST A` cycles all 12 mode/channel types (start conversion, select channel, read+decode
the 4-byte MSX SNG result); `TEST B`/`C` check command/status readback; `TEST D` checks a reserved
command does not change state; `TEST F` checks RESULT auto-increment/wrap; `TEST G` compares the
convenience `ADS GET` command against `TEST A`'s individual-command path with a tolerance instead
of an exact match. Includes a pure-C float decode/print/tolerance-compare subsystem (no ROM call
involved) translating the ADS1220's raw 4-byte result format. Links against
[`../../../lib/rcx_io/`](../../../lib/rcx_io/) and
[`../../../lib/dostools/`](../../../lib/dostools/) only - calls no `Xio_*` function.

The ADS1220 base address is read from `/IO:<0..252>` on the command line; missing or out of range
prints a usage message and exits.

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/rcx_io/`'s contents
into `MSXgl/lib/rcx_io/`, `../../../lib/dostools/`'s contents into `MSXgl/lib/dostools/`, and this
folder into `MSXgl/projects/ADS1220/` - all paths must stay space-free, a requirement of the
MSXgl build tool itself.

```sh
cd MSXgl/projects/ADS1220 && node ../../engine/script/js/build.js
```

Prebuilt `ADS1220.com` is included so the tool can be used without rebuilding.
