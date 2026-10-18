# Card F - RC2014 card with 4x SPI bus up to 2 MHz via ATMEGA8

RC2014 card with 4 SPI busses selectable via software with baudrate up to 2 MHz. Up to 16 SPI cards are possible. The interrupt pin of the ATMEGA8 can be assigned to one of the XIO PIC interrupt channels.

## Command reference

Commands implemented in ROM:

+ <i><b>SPI INIT</i></b>

  Registers the card's IO base address in the RCX table. Must be called once per card before any of the other SPI commands below will find it.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SPI INIT | _SPI INIT() | 50 | Spi_Init() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |

*) ROM only

+ <i><b>SPI WR</i></b>

  Writes a single byte on one of the card's 4 independently selectable SPI busses.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SPI WR | _SPI WR() | 51 | Spi_Write() |
| PARA 1 | IO addr 0...252 | byte or variable | E | byte or variable |
| PARA 2 | SPI BUS 0..3 | byte or variable | C | byte or variable |
| PARA 3 | Byte to write | byte or variable | B | byte or variable |

*) ROM only

+ <i><b>SPI WRB</i></b>

  Writes an arbitrary-length block of bytes on one SPI bus in a single call, instead of looping single SPI WR calls.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SPI WRB | _SPI WRB() | 52 | Spi_Wrb() |
| PARA 1 | IO addr 0...252 | byte or variable | H | byte or variable |
| PARA 2 | SPI BUS 0..3 | byte or variable | C | byte or variable |
| PARA 3 | Length of array | byte or variable | B | byte or variable |
| PARA 4 | Data to write | array index 0 | DE as varptr | pointer |

*) ROM only

+ <i><b>SPI RD</i></b>

  Reads a single byte back from one SPI bus.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SPI RD | _SPI RD() | 53 | Spi_Read() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | SPI BUS 0..3 | byte or variable | B | byte or variable |
| PARA 3 | Byte to read | variable | DE as varptr | return value (byte) |

*) ROM only

+ <i><b>SPI RDB</i></b>

  Reads an arbitrary-length block of bytes back from one SPI bus in a single call.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SPI RDB | _SPI RDB() | 54 | Spi_Rdb() |
| PARA 1 | IO addr 0...252 | byte or variable | H | byte or variable |
| PARA 2 | SPI BUS 0..3 | byte or variable | C | byte or variable |
| PARA 3 | Length of array | byte or variable | B | byte or variable |
| PARA 4 | Data to read | array index 0 | DE as varptr | pointer |

*) ROM only

+ <i><b>SPI RDH</i></b>

  Reads an array of bytes to RC2014 SPI card after sending a header for register selection.

	Both data is handled by one arry. LEN_WR signals length of header beginning with index 0 of the array. LEN_RD starts at index LEN_WR for data to read. Be sure to have a suitable definition for the array.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SPI RDH | _SPI RDH() | 55 | Spi_Rdh() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | SPI BUS 0..3 | byte or variable | B | byte or variable |
| PARA 3 | Length of array to write | byte or variable | H | word high byte or variable |
| PARA 4 | Length of array to read | byte or variable | L | word low byte or variable |
| PARA 5 | Data to read | array index 0 | DE as varptr | pointer |

*) ROM only

+ <i><b>SPI MODE</i></b>

  Sets clock polarity/phase (Mode 0..3) and clock frequency independently for one of the 4 SPI busses, so different chips on different busses can each run at the timing they need. See the ATMEGA8 firmware's datasheet reference for further information.

	MOD (PARA 3):

	0 = SPI_MODE_0, 1 = SPI_MODE_1, 2 = SPI_MODE_2, 3 = SPI_MODE_3

	FQZ (PARA 4):

	0 = 2 MHz, 1 = 1 MHz, 2 = 500 kHz, 3 = 250 kHz

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SPI MODE | _SPI MODE() | 56 | Spi_Mode() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | SPI BUS 0..3 | byte or variable | B | byte or variable |
| PARA 3 | Mode | byte or variable | D | byte or variable |
| PARA 4 | Bus frequency | byte or variable | E | byte or variable |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [atmel/](atmel/README.md) - ATMEGA8 firmware
+ [pcb/](pcb/) - schematics and PCB

[↑ Main README](../../README.md#f-rc2014-card-with-4x-spi-bus-up-to-2-mhz-via-atmega8)
