# RC2014 cartridge - C/SDCC port (MSXgl)

## RCXSLT/RCXSLT.c

C/SDCC port of `../basic/RCXSLT.ASC`, built with [MSXgl](https://github.com/aoineko-fr/MSXgl):
asks for a main slot number, dumps the 8-byte MSX BIOS slot work area (`SLTWRK`) for that slot,
then follows the pointer stored at its bytes 6+7 and dumps 96 (`&H60`) further bytes from there.
Calls no `Xio_*`/RCX function at all and needs no EXTBIO discovery - as generic/UNAPI-free as the
BASIC original. Links against [`../../../lib/dostools/`](../../../lib/dostools/) only for its
`Dos_ReadLine`/`Dos_PrintHex2` helpers (plain MSX-DOS console I/O, unrelated to either UNAPI
driver).

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy `../../../lib/dostools/`'s contents
into `MSXgl/lib/dostools/` and this folder into `MSXgl/projects/RCXSLT/` - both paths must stay
space-free, a requirement of the MSXgl build tool itself.

```sh
cd MSXgl/projects/RCXSLT && node ../../engine/script/js/build.js
```

Prebuilt `RCXSLT.com` is included so the tool can be used without rebuilding.
