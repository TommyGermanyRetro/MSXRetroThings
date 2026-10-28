# Card E - RC2014 card with SJA1000 CAN controller (single use only)

RC2014 card with SJA1000 CAN controller and RJ45 sockets with automatic termination function. The software provides PELICAN Mode and 250 kBit/s.

## Command reference

Commands implemented in ROM:

+ <i><b>CAN INIT</i></b>

  Initialises RC2014 SJA1000 CAN card at IO address 0..254. The firmware has a ring buffer of 16 messages.

	ACC is the acceptance code and mask

	Array of INT is needed, only low byte is used, len = 8 (see datasheet for further information)

	Index 0 ACR.0, Index 1 ACR.1, Index 2 ACR.2, Index 3 ACR.3

	Index 4 AMR.0, Index 5 AMR.1, Index 6 AMR.2, Index 7 AMR.3

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | CAN INIT | _CAN INIT() | 43 | Can_Init() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | ACC code&mask | array index 0 | DE as varptr | pointer |

*) ROM only

+ <i><b>CAN WR</i></b>

  Writes a raw byte directly to any SJA1000 internal register - for manual/advanced configuration beyond what CAN INIT already sets up.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | CAN WR | _CAN WR GATE() | 44 | Can_Write() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | Register | byte or variable | D | byte or variable |
| PARA 3 | Byte to write | byte or variable | E | byte or variable |

*) ROM only

+ <i><b>CAN RD</i></b>

  Reads a raw byte directly back from any SJA1000 internal register.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | CAN RD | _CAN RD GATE() | 45 | Can_Read() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | Register | byte or variable | D | byte or variable |
| PARA 3 | Byte to read | variable | A contains result | return value (byte) |

*) ROM only

+ <i><b>CAN CHECK</i></b>

  Non-blocking poll of whether a new message has arrived in the firmware's 16-message ring buffer since the last check - call this from a main loop, or from an interrupt handler registered via CAN GET ADDR/XIO SET ADDR, before bothering to call CAN RX.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | CAN CHECK | _CAN CHECK() | 46 | Can_Check() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | 1 = new msg | variable | A contains result | return value (byte) |

*) ROM only

+ <i><b>CAN RX</i></b>

  Pops the oldest queued message out of the 16-message ring buffer, in raw PELICAN 2.0 frame format.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | CAN RX | _CAN RX() | 47 | Can_Rx() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | New msg (PELICAN 2.0) | array len = 13 | DE as varptr | pointer |

*) ROM only

+ <i><b>CAN GET ADDR</i></b>

  Returns the address of the ROM's own ready-made CAN interrupt service routine, meant to be handed straight to XIO SET ADDR - receiving and queuing CAN messages this way needs no custom Z80 interrupt code at all, only CAN CHECK/RX in the main program to drain the ring buffer XIO fills in the background.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | CAN RX | _CAN GET ADDR() | 48 | Can_GetIntAddr() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | RAM address pointer | variable | DE as varptr | return value (word) |

*) ROM only

+ <i><b>CAN TX</i></b>

  Transmits a new message on the CAN bus, in raw PELICAN 2.0 frame format.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | CAN TX | _CAN TX() | 49 | Can_Tx() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | Message (PELICAN 2.0) | array len = 13 | DE as varptr | pointer |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [pcb/](pcb/) - schematics, PCB, Gerber and drill files, bill of materials

[↑ Main README](../../README.md#e-rc2014-card-with-sja1000-can-controller-single-use-only)
