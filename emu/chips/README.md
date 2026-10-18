# chips

Chip components of the emulators, independent of any machine: each library models one chip and has its own tests in
`tests`. All four emulators use them: [SVI-728](../svi728/README.md),
[VG-8235](../vg8235/README.md), [SVI-318](../svi318/README.md) and
[SVI-328](../svi328/README.md).

The solutions of the machines build the chips, each into its own `Build` folder, and their `run_tests.ps1` runs the
tests. [Common.props](Common.props) holds the shared compiler settings (MSVC v143, C++17/C11,
`/W4 /permissive- /utf-8`, static runtime library), the include paths of the chips and the output folders.

| Component | Contents |
| --- | --- |
| [AY8910/](AY8910/README.md) | AY-3-8910 (and YM2149): three tone channels, joysticks (port A), memory banks (port B) |
| [FD1793/](FD1793/README.md) | FD1793 floppy disk controller (5.25", 1 MHz) |
| [INS8250/](INS8250/README.md) | INS8250 UART of the SVI-805 RS-232 card |
| [MC6821/](MC6821/README.md) | MC6821 PIA of the printer port on the SVI-609 LAN card |
| [MC6845/](MC6845/README.md) | MC6845 CRT controller of the SVI-806 80-column card |
| [PPI8255/](PPI8255/README.md) | Intel 8255A: keyboard matrix, cassette control, keyboard click |
| [PRINTER/](PRINTER/README.md) | Printer on the Centronics port |
| [RP5C01/](RP5C01/README.md) | Ricoh RP5C01: clock with calendar, alarm and 12/24-hour block, 2 x 13 nibbles of CMOS RAM, image for the CMOS file (VG-8235) |
| [TMS9918A/](TMS9918A/README.md) | NTSC VDP (US machine, 60 Hz) |
| [TMS9929A/](TMS9929A/README.md) | PAL VDP (European machine, 50 Hz) |
| [TMS99X8/](TMS99X8/README.md) | Shared logic of the TI VDPs: master clock, timed VRAM access, live raster output |
| [V9938/](V9938/README.md) | Yamaha V9938 with 128 KiB VRAM: ports `98h`-`9Bh`, registers R#0-R#23/R#32-R#46, status S#0-S#9, palette, PAL/NTSC time base with 192/212 lines, frame and line interrupt, line-by-line output (512 x 212) for TEXT 1/2, MULTICOLOR, GRAPHIC 1-7 with sprite modes 1 and 2, page flipping through EO and R#13, interlace flag, command unit with all commands and logical operations (fixed duration per step) (VG-8235) |
| [WD1002SHD/](WD1002SHD/README.md) | WD1002-SHD hard disk controller (SASI) with ST-212 |
| [WD2793/](WD2793/README.md) | Western Digital WD2793 (all command types, status table, MFM timing) and 3.5" drive with `.dsk` images (320/360/640/720 KB) (VG-8235) |
| [Z80CPU/](Z80CPU/README.md) | Z80 clocked by T-state (core from floooh/chips) |
| [Z8530/](Z8530/README.md) | Z8530 SCC (NMOS) of the SVI-609 LAN card: asynchronous and SDLC, baud rate generator, interrupt daisy chain; not yet connected to the card |
| [VIDEOREC/](VIDEOREC/README.md) | Not a chip: MP4 video recording (H.264 and AAC through Media Foundation) of picture and sound for the frontends of all four emulators |
