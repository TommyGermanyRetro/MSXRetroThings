# Card D - RC2014 card with 8254A CTR

RC2014 card with a 8254 CTR. Up to 16 cards are possible. The interrupt pins of TIMER 0 and 1 can be assigned to one of the XIO PIC interrupt channels. This module does not work with internal clock if 8253 (max. 1 MHz) is used.

## Command reference

Commands implemented in ROM:

+ <i><b>TIMER INIT</i></b>

  Registers the card's IO base address in the RCX table. Must be called once per card before any of the other TIMER commands below will find it.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER INIT | _TIMER INIT() | 33 | Timer_Init() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |

*) ROM only

+ <i><b>TIMER 0 RD</i></b>

  Latches and reads the current 16-bit counter value of Timer 0.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER 0 RD | _TIMER 0 RD() | 34 | Timer_Read0() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | Word to read | variable | DE as varptr | return value (word) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 0 | - |

*) ROM only

+ <i><b>TIMER 0 WR</i></b>

  Loads a new 16-bit count/reload value into Timer 0.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER 0 WR | _TIMER 0 WR() | 35 | Timer_Write0() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | Word to write | word or variable | DE | word or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 1 | - |

*) ROM only

+ <i><b>TIMER 1 RD</i></b>

  Latches and reads the current 16-bit counter value of Timer 1.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER 1 RD | _TIMER 1 RD() | 36 | Timer_Read1() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | Word to read | variable | DE as varptr | return value (word) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 2 | - |

*) ROM only

+ <i><b>TIMER 1 WR</i></b>

  Loads a new 16-bit count/reload value into Timer 1.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER 1 WR | _TIMER 1 WR() | 37 | Timer_Write1() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | Word to write | word or variable | DE | word or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 3 | - |

*) ROM only

+ <i><b>TIMER 2 RD</i></b>

  Latches and reads the current 16-bit counter value of Timer 2.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER 2 RD | _TIMER 2 RD() | 38 | Timer_Read2() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | Word to read | variable | DE as varptr | return value (word) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 4 | - |

*) ROM only

+ <i><b>TIMER 2 WR</i></b>

  Loads a new 16-bit count/reload value into Timer 2.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER 2 WR | _TIMER 2 WR() | 39 | Timer_Write2() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | Word to write | word or variable | DE | word or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 5 | - |

*) ROM only

+ <i><b>TIMER CTRL</i></b>

  Writes the 82C54's mode-control word for one of the three counters - selects counting mode 0..5, BCD vs. binary counting and the read/write byte order, matching the 82C54 datasheet's control word format directly.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER CTRL | _TIMER CTRL() | 40 | Timer_Ctrl() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | Word to write | word or variable | E (D must be 0) | word or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 6 | - |

*) ROM only

+ <i><b>TIMER SET GATE</i></b>

  Drives one of the card's jumper-selectable GATE inputs high in software instead of wiring it permanently - only takes effect for a counter whose GATE jumper is actually set to the software-controlled position.

	Value 0..7 is allowed

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER SET GATE | _TIMER SET GATE() | 41 | Timer_SetGate() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | GATE 0..3 (0..7) | byte or variable | E (D must be 0) | word or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 7 | - |

*) ROM only

+ <i><b>TIMER RESET GATE</i></b>

  Drives a jumper-selectable GATE input low again - the counterpart to TIMER SET GATE.

	Value 0..7 is allowed

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | TIMER RESET GATE | _TIMER RESET GATE() | 42 | Timer_ResetGate() |
| PARA 1 | IO addr 0...251 | byte or variable | C | byte or variable |
| PARA 2 | GATE 0..3 (0..7) | byte or variable | E (D must be 0) | word or variable |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 8 | - |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [pcb/](pcb/) - schematics and PCB

[↑ Main README](../../README.md#d-rc2014-card-with-8254a-ctr)
