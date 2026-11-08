# Card B - RC2014 card SC103 with Z80 PIO

The original RC2014 card SC103 (Small Computer Central): Z80 PIO with two 8-bit bidirectional ports A and B, each with two handshake lines. The RCX ROM contains BASIC and UNAPI commands for the card; interrupts use the PIO's own IM2 vector through an XIO Z80 interrupt channel (XIO cartridge needed for interrupt operation only).

## Hardware setup

The IO base address is set with the 6-way DIP switch SW1: the address is the sum of the switches in the Off position (Up = Off, Down = On). Any multiple of 4 from 0 to 252; the card uses the base address and the next three addresses.

| Switch | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| Value | 128 | 64 | 32 | 16 | 8 | 4 |
| `&H68` (recommended) | On | Off | Off | On | Off | On |

A second card is recommended at `&H6C`.

| Connector / jumper | Function |
| --- | --- |
| P2 (2x13 pins) | ports A and B for a ribbon cable |
| P3 (1x29 pins) | ports A and B for Dupont wires, pin 15 GND |
| P4 (1x2 pins) | IEI / IEO for a daisy chain with Dupont wires |
| JP1, JP2 open | IEI/IEO only on P4, IEI pulled high on the card |
| JP1, JP2 fitted | IEI/IEO on the RC2014 bus (needs a backplane with the chain, e.g. SC107) |

## Command reference

Commands implemented in ROM:

+ <i><b>PIO INIT</i></b>

  Registers the card's IO base address in the RCX table. Must be called once per card before any of the other PIO commands below will find it.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PIO INIT | _PIO INIT() | 16 | Pio_Init() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |

*) ROM only

+ <i><b>PIO A RD</i></b>

  Reads the current byte at Z80 PIO Port A's data register, in whatever mode Port A is currently configured for via PIO A CTRL.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PIO A RD | _PIO A RD() | 17 | Pio_ReadA() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to read | variable | DE as varptr | return value (byte) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 0 | - |

*) ROM only

+ <i><b>PIO A WR</i></b>

  Writes a byte to Z80 PIO Port A's data register.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PIO A WR | _PIO A WR() | 18 | Pio_WriteA() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 1 | - |

*) ROM only

+ <i><b>PIO B RD</i></b>

  Reads the current byte at Z80 PIO Port B's data register.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PIO B RD | _PIO B RD() | 19 | Pio_ReadB() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to read | variable | DE as varptr | return value (byte) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 2 | - |

*) ROM only

+ <i><b>PIO B WR</i></b>

  Writes a byte to Z80 PIO Port B's data register.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PIO B WR | _PIO B WR() | 20 | Pio_WriteB() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 3 | - |

*) ROM only

+ <i><b>PIO A CTRL</i></b>

  Writes a raw control byte to Port A's mode/control register - selects the port's operating mode (0..3), direction, interrupt vector and interrupt mask bits, matching the Z80 PIO datasheet's control word format directly.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PIO A CTRL | _PIO A CTRL() | 21 | Pio_CtrlA() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 4 | - |

*) ROM only

+ <i><b>PIO B CTRL</i></b>

  Writes a raw control byte to Port B's mode/control register, same format as PIO A CTRL.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | PIO B CTRL | _PIO B CTRL() | 22 | Pio_CtrlB() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to write | byte or variable | D | byte or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 5 | - |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [pcb/](pcb/) - schematics and PCB

[↑ Main README](../../README.md#b-rc2014-card-sc103-with-z80-pio)
