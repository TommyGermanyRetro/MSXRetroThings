# Card H - RC2014 card SC725 with Z80 SIO and Z80 CTC

The original RC2014 card SC725 (Small Computer Central) used as MSX COM port: Z80 SIO/2 with two asynchronous serial ports A and B, Z80 CTC channel 0/1 as baud rate generator. The RCX ROM contains an MSX RS-232C driver following the "RS-232C Extended BIOS Call Specification" (EXTBIO device 8, multi channel type) with interrupt driven receiving through XIO, the BASIC devices `COM0:`/`COM1:` and the Sony HBI-232 style CALL statements. Card H needs the XIO cartridge (interrupt).

## Hardware setup

Fixed IO addresses: SIO A control/data `&H80`/`&H81`, SIO B `&H82`/`&H83`, CTC channel 0...3 `&H88`...`&H8B`.

| Jumper | Setting | Function |
| --- | --- | --- |
| JP4 | 1-2 | CTC clock = bus CLK (3.579545 MHz) |
| JP5 | 5-6 | SIO A clock = CTC channel 0 |
| JP6 | 5-6 | SIO B clock = CTC channel 1 |
| JP1, JP2 | open | no +5 V on the serial headers |
| JP3 | open | IEI pulled high on the card |

Serial headers P1/S1 (port A) and P2/S2 (port B): 1 GND, 2 /RTS out, 3 +5 V (JP1/JP2), 4 RXD in, 5 TXD out, 6 /CTS in.

Baud rates 110, 300, 600, 1200, 1800, 2000, 2400, 3600, 4800, 7200, 9600, 19200 (error +0.09 % ... -2.9 %). Receive and send rate are always equal (one CTC channel per port); 50 and 75 baud are not possible.

## Command reference

Commands implemented in ROM:

+ <i><b>COM INIT</i></b>

  Registers the SC725 as COM port, resets CTC and SIO and presets both channels with `8N1XHNNN`, 1200 baud, no time out. Without COM INIT the driver does not answer EXTBIO device 8.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | COM INIT | _COM INIT() | 68 | Com_Init() |
| PARA 1 | SIO IO addr | byte or variable | C | byte |

*) ROM only

+ <i><b>COMINI</i></b>

  RS-232C INIT of channel n (`0:` port A, `1:` port B, default 0). All parameters are optional; omitted ones from `8N1XHNNN`, 1200 baud, no time out. The data string replaces the defaults from the left: data bits `5`...`8`, parity `E`/`O`/`I`/`N`, stop bits `1`/`2`/`3`, XON/XOFF `X`/`N`, CTS-RTS `H`/`N`, auto LF receive `A`/`N`, auto LF send `A`/`N`, SI/SO `S`/`N`. Call it before OPEN.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | COMINI | _COMINI() | - | Com_Ini() |
| PARA 1 | "[n:]data" | string | - | 13 byte table |
| PARA 2 | Receive baud | integer or variable | - | in the table |
| PARA 3 | Send baud (default = receive) | integer or variable | - | in the table |
| PARA 4 | Time out 0...255 s (0 = none) | integer or variable | - | in the table |

*) BASIC only

+ <i><b>COMDTR / COMSTAT / COMBREAK</i></b>

  DTR off (0) or on; status word into an integer variable; send break for count character times.

|  | Definition | BASIC* | SDCC |
| --- | --- | --- | --- |
| Syntax | COMDTR | _COMDTR(["n:",]value) | Com_Dtr() |
| Syntax | COMSTAT | _COMSTAT(["n:",]variable) | Com_Stat() |
| Syntax | COMBREAK | _COMBREAK(["n:",]count) | Com_SndBrk() |

*) BASIC only

Status bits (COMSTAT, STAT): 15 buffer overflow, 14 time out, 13 framing error, 12 overrun, 11 parity error, 10 CTRL-STOP, 7 CTS, 2 break, 0 DCD.

+ <i><b>BASIC devices COM0: / COM1:</i></b>

  `OPEN "COM1:" AS #1` (input and output on one file number), `FOR INPUT`, `FOR OUTPUT`; `PRINT #`, `INPUT #`, `LINE INPUT #`, `INPUT$`, `LOC`, `LOF`, `EOF`, `CLOSE`. Receive buffer 123 characters. Time out, receive errors and a failed OPEN give "Device I/O error"; `FOR APPEND` gives "Bad file name", `GET`/`PUT` "Sequential I/O only". String contents must be created after `_XIO INIT`/`_RCX INIT`, the INITs empty the string space.

```basic
_XIO INIT(A)
_RCX INIT
_COM INIT(&H80)
_COMINI("1:8N1NHNNN",9600,,10)
OPEN "COM1:" AS #1
PRINT #1,"HELLO"
A$=INPUT$(LOC(1),#1)
CLOSE #1
```

+ <i><b>RS-232C Extended BIOS (EXTBIO device 8)</i></b>

  `CALL EXTBIO` (`&HFFCA`) with D = 8, E = 0, B:HL = buffer appends slot, table address (low, high) and 0. Each entry is called by inter slot call (`CALSLT`); the program enables interrupts again afterwards. Channel 0 = port A, 1 = port B (SETCHN).

| Entry | Function | Registers | SDCC |
| --- | --- | --- | --- |
| +3 | INIT | B:HL = parameter table; CARRY = illegal | Com_Ini() |
| +6 | OPEN | HL = FCB (&H8000 or higher), C = length 32...254, E = mode 1/2/4; CARRY = error | Com_Open() |
| +9 | STAT | HL = status | Com_Stat() |
| +12 | GETCHR | A = character, SIGN = error, CARRY = EOF | Com_GetChr() |
| +15 | SNDCHR | A = character; CARRY = CTRL-STOP, ZERO = time out | Com_SndChr() |
| +18 | CLOSE | | Com_Close() |
| +21 | EOF | HL = -1, CARRY if next character is EOF | Com_Eof() |
| +24 | LOC | HL = characters in buffer | Com_Loc() |
| +27 | LOF | HL = free characters | Com_Lof() |
| +30 | BACKUP | C = character | Com_Backup() |
| +33 | SNDBRK | DE = count; CARRY = CTRL-STOP | Com_SndBrk() |
| +36 | DTR | A = 0 off, otherwise on | Com_Dtr() |
| +39 | SETCHN | A = channel; CARRY = not present | Com_SetChn() |

  SDCC: `Com_Discover()` finds the table (EXTBIO device 8) after `Com_Init()`.

Implementation notes: the SIO receive interrupt (vector 8, XIO Z80 channel 12) is served by a routine in the RCX work area; the driver registers it at XIO itself on the first OPEN. The SIO does not see the `RETI` through XIO, the routine ends with WR0 `&H38` on port A. The SIO latches CTS/DCD/break in RR0 until WR0 `&H10`; the driver writes `&H10` before reading RR0. The RCX work area is `&H496` bytes; `.COM`/SDCC programs pass a buffer of this size to `RCX INIT`.

With `RCXRAM.COM` (RAM driver) state, interrupt routine and the RS-232C table live at the fixed page-3 addresses `&HCF10`...`&HD11C`.

## Folders

+ [basic/](basic/) - BASIC test programs
+ [com/](com/) - `.COM` test programs (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/) - C/SDCC test programs
+ [pcb/](pcb/) - picture of the card

[↑ Main README](../../README.md#h-rc2014-card-sc725-with-z80-sio-and-z80-ctc)
