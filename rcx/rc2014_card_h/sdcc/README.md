# Card H (SC725) - C/SDCC test programs

C ports of `../com/COMTST` and `../com/COMHS` with MSXgl and the `rcx_io` library (`../../../lib/rcx_io/`): `Com_Init` (UNAPI 68), `Com_Discover` (EXTBIO device 8) and one wrapper per RS-232C entry (`Com_Ini`, `Com_Open`, `Com_Stat`, `Com_GetChr`, `Com_SndChr`, `Com_Close`, `Com_Eof`, `Com_Loc`, `Com_Lof`, `Com_Backup`, `Com_SndBrk`, `Com_Dtr`, `Com_SetChn`). COMHS uses all entries except SNDBRK. Work with the ROM cartridge and with `RCXRAM.COM`. Channels in `s_Chans`.

Both programs end with `DOS_Exit0()` (`Bios_Exit()` switches the screen mode and clears the results) and set their counters in `main` (the MSXgl DOS crt0 does not zero statics).

## Build

Copy the folder to `MSXgl/projects/<name>/` (the MSXgl build tool does not accept spaces in the path) with `MSXgl/lib/xio_io/`, `rcx_io/` and `dostools/` present, then:

```text
../../tools/build/Node/node.exe ../../engine/script/js/build.js
```
