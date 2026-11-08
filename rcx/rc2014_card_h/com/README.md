# Card H (SC725) - .COM test programs

Same tests as `../basic/COMTST.ASC` and `../basic/COMHS.ASC` under MSX-DOS. They install `XIO_IO_EXPANDER` and `RCX_INTERFACE` through UNAPI (`XIO INIT`, `RCX INIT`, `COM INIT` = function 68), get the RS-232C jump table through `EXTBIO` device 8 and call its entries with `CALSLT`. Work with the ROM cartridge and with `RCXRAM.COM`. Channels in `CHANS` (count, then 0 = port A, 1 = port B). Assembled with pasmo.

| Program | Test |
| --- | --- |
| `COMTST/COMTST.ASM` | no handshake: 256 byte values at 1200, 9600, 19200 baud |
| `COMHS/COMHS.ASM` | CTS-RTS: RTS stops the sender before the 123 character buffer overflows, then the rest of 200 characters |

Memory: XIO work area `#0C000`, XIO interrupt variable `#0C100`, RCX work area `#0C200`-`#0C695`, INIT parameters `#0C6A0`, EXTBIO entries `#0C6B0`, receive buffer `#09000`.
