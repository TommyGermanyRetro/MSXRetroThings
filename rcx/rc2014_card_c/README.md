# Card C - RC2014 card with 8255A PIO

RC2014 card with a 8255 PIO. Up to 16 cards are possible. The interrupt pins of PORT C can be assigned to one of the XIO PIC interrupt channels.

## Command reference

Commands implemented in ROM:

+ <i><b>PPI INIT</i></b>

  Registers the card's IO base address in the RCX table. Must be called once per card before any of the other PPI commands below will find it.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI INIT | _PPI INIT() | 23 | Ppi_Init() |

*) ROM only

+ <i><b>PPI A RD</i></b>

  Reads the current byte at 82C55 Port A, in whatever mode it is currently configured for via PPI CTRL.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI A RD | _PPI A RD() | 24 | Ppi_ReadA() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to read | variable | DE as varptr | return value (byte) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 0 | - |

*) ROM only

+ <i><b>PPI A WR</i></b>

  Writes a byte to 82C55 Port A.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI A WR | _PPI A WR() | 25 | Ppi_WriteA() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 1 | - |

*) ROM only

+ <i><b>PPI B RD</i></b>

  Reads the current byte at 82C55 Port B.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI B RD | _PPI B RD() | 26 | Ppi_ReadB() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to read | variable | DE as varptr | return value (byte) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 2 | - |

*) ROM only

+ <i><b>PPI B WR</i></b>

  Writes a byte to 82C55 Port B.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI B WR | _PPI B WR() | 27 | Ppi_WriteB() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 3 | - |

*) ROM only

+ <i><b>PPI C RD</i></b>

  Reads the current byte at 82C55 Port C.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI C RD | _PPI C RD() | 28 | Ppi_ReadC() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to read | variable | DE as varptr | return value (byte) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 4 | - |

*) ROM only

+ <i><b>PPI C WR</i></b>

  Writes a byte to 82C55 Port C.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI C WR | _PPI C WR() | 29 | Ppi_WriteC() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 5 | - |

*) ROM only

+ <i><b>PIO CTRL</i></b>

  Writes the 82C55's mode-control word - selects Mode 0/1/2 independently for the two port groups and sets each port's direction, matching the 82C55 datasheet's control word format directly.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI CTRL | _PPI CTRL() | 30 | Ppi_Ctrl() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 6 | - |

*) ROM only

+ <i><b>PPI SET</i></b>

  Sets a single bit (0..7) of Port C using the 82C55's built-in bit-set/reset (BSR) mode - a single-instruction toggle without a separate read-modify-write, typically used for the interrupt-enable/handshake bits Port C carries in Mode 1/2.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI SET | _PPI SET() | 31 | Ppi_SetBit() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Bit to set | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 7 | - |

*) ROM only

+ <i><b>PPI RESET</i></b>

  Clears a single bit (0..7) of Port C via the same BSR mechanism as PPI SET.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PPI RESET | _PPI RESET() | 32 | Ppi_ResetBit() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Bit to reset | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 8 | - |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [pcb/](pcb/) - schematics and PCB

[↑ Main README](../../README.md#c-rc2014-card-with-8255a-pio)
