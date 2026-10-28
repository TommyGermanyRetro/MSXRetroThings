# XIO cartridge - command reference

The XIO cartridge provides 256 additional switched IO addresses and an 8259 PIC with an 8-channel daisy-chain Z80 IM2 emulator. Its ROM adds BASIC `CALL` commands and an UNAPI/EXTBIOS interface; the SDCC library `xio_io` bridges to the same functions.

## Command reference

Commands implemented in ROM/LIB:

+ <i><b>XIO ?</i></b>

  Prints the list of available XIO commands directly to the screen. Works independently from XIO INIT, so it is always safe to call even if the card was never installed - useful as a quick "is this ROM extension actually loaded" check.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO ? | _XIO ? | 1 | - (BASIC-only, not exposed) |

+ <i><b>XIO OUT</i></b>

  Sends a single raw byte to one of the 256 additional switched IO addresses the XIO address decoder provides. Works independently from XIO INIT and the whole interrupt/PIC subsystem - a plain digital output, nothing more.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO OUT | _XIO OUT() | 2 | Xio_Out() |
| PARA 1 | IO addr 0...255 | byte or variable | C | byte or variable |
| PARA 2 | IO data 0...255 | byte or variable | B | byte or variable |

+ <i><b>XIO INP</i></b>

  Reads a single raw byte back from one of the 256 switched IO addresses. Like XIO OUT, this bypasses XIO INIT and the interrupt system entirely - a plain digital input.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO INP | _XIO INP() | 3 | Xio_Inp() |
| PARA 1 | IO addr 0...255 | byte or variable | C | byte or variable |
| PARA 2 | IO data 0...255 | variable | DE as varptr | return value (byte) |

+ <i><b>XIO INIT</i></b>

  Installs the XIO card: allocates the BIOS work area and hooks the standard IM2/PIC interrupt dispatcher. The status variable is not read once at install time - the ROM keeps writing into it on every interrupt for the rest of the program's run, one bit per channel (0..15), so a caller can just poll the variable instead of writing its own interrupt handler.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO INIT | _XIO INIT() | 4 | Xio_Init() |
| PARA 1 | status | variable | DE as varptr | word (intflags) |
| PARA 2 | Working area in RAM | automatic by BIOS | HL | word (workarea) |

+ <i><b>XIO ISINIT</i></b>

  Checks whether XIO is already installed, without touching anything. The usual pattern is ISINIT, then DEINIT if it reports 1, then INIT - so a program never ends up stacking a second installation on top of an old one from a previous run.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO ISINIT | _XIO ISINIT() | 5 | Xio_IsInit() |
| PARA 1 | 0 = not installed, 1 = installed | variable | DE as varptr | return value (byte) |

+ <i><b>XIO DEINIT</i></b>

  Uninstalls the XIO card: unhooks the interrupt dispatcher and releases the BIOS work area again, so a program can clean up before it exits.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO DEINIT | _XIO DEINIT | 6 | Xio_Deinit() |

+ <i><b>XIO SET ADDR</i></b>

  Registers a custom Z80 machine code service routine for one of the 16 IM2/PIC channels. Once set, the standard dispatcher calls this routine directly whenever that channel's interrupt fires, instead of only updating the passive status bit that XIO INIT's variable exposes - use this when a channel needs to react immediately rather than being polled.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO SET ADDR | _XIO SET ADDR() | 7 | Xio_SetAddr() |
| PARA 1 | channel | byte or variable | B | byte or variable |
| PARA 2 | addr | word or variable | DE | word or variable |
| PARA 3 | slot | automatic by BIOS | C | - |

+ <i><b>XIO SET MASK</i></b>

  Enables or disables interrupt channels via a 16-bit bitmask (bit N = channel N). All channels start out disabled after XIO INIT, so this must be called at least once before any channel actually generates interrupts - by convention it is called last, only once the rest of a program's setup (card init, custom SET ADDR routines, etc.) has already completed.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO SET MASK | _XIO SET MASK() | 8 | Xio_SetMask() |
| PARA 1 | Bit Chx: 0 = disabled, 1 = enabled | word or variable | DE | word or variable |

+ <i><b>XIO GET ADDR</i></b>

  Reads back which service routine address is currently registered for a given channel - the counterpart to XIO SET ADDR, mainly useful for diagnostics.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO GET ADDR | _XIO GET ADDR() | 9 | Xio_GetAddr() |
| PARA 1 | channel | byte or variable | B | byte or variable |
| PARA 2 | addr | variable | DE as varptr | pointer (word) |
| PARA 3 | slot | automatic by BIOS | C | - |

+ <i><b>XIO GET MASK</i></b>

  Reads back the current 16-bit enable/disable bitmask - the counterpart to XIO SET MASK.

|  | Definition | BASIC | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | XIO GET MASK | _XIO GET MASK() | 10 | Xio_GetMask() |
| PARA 1 | Bit Chx: 0 = disabled, 1 = enabled | variable | DE as varptr | pointer (word) |

## Folders

+ [rom/](rom/README.md) - ROM source and image
+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [gal/](gal/) - GAL equations
+ [pcb/](pcb/) - schematics, PCB, Gerber and drill files, bill of materials

[↑ Main README](../README.md#1-msxretrothings---the-xio-cartridge)
