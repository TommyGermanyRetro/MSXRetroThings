# Card A - RC2014 card with PCF8584 I2C Controller (3 ports via Seeed) and PCF8583 battery buffered RTC

This RC2014 card contains an I2C bus controller PCF8584 and a PCF8583 RTC buffered by battery. The interrupt pins of both chips can be assigned to one of the XIO PIC interrupt channels.

## Command reference

Commands implemented in ROM:

+ <i><b>I2C INIT</i></b>

  Registers the card's IO base address in the RCX table. Must be called once per card before any of the other I2C/RTC commands below will find it.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | I2C INIT | _I2C INIT() | 6 | I2c_Init() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |

*) ROM only

+ <i><b>I2C WR</i></b>

  Writes a single byte to a specific I2C slave device's address on the bus - the basic building block for talking to anything hanging off the PCF8584 (an LCD, a sensor, another RTC, ...).

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | I2C WR | _I2C WR() | 7 | I2c_Write() |
| PARA 1 | IO bus addr 0...254 | byte or variable | E | byte or variable |
| PARA 2 | I2C addr 0...254 | byte or variable | C | byte or variable |
| PARA 3 | Data | byte or variable | B | byte or variable |

*) ROM only

+ <i><b>I2C WRB</i></b>

  Writes an arbitrary-length block of bytes to a specific I2C slave in one call, instead of looping single I2C WR calls - used e.g. to send a whole HD44780 LCD command/data sequence at once.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | I2C WRB | _I2C WRB() | 8 | I2c_Wrb() |
| PARA 1 | IO bus addr 0...254 | byte or variable | H | byte or variable |
| PARA 2 | I2C addr 0...254 | byte or variable | C | byte or variable |
| PARA 3 | Length | byte or variable | B | byte or variable |
| PARA 4 | Data | array index 0 | DE as varptr | pointer |

*) ROM only

+ <i><b>I2C RD</i></b>

  Reads a single byte back from a specific I2C slave device's address.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | I2C RD | _I2C RD() | 9 | I2c_Read() |
| PARA 1 | IO bus addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | I2C addr 0...254 | byte or variable | B | byte or variable |
| PARA 3 | Data | variable | DE as varptr | return value (byte) |

*) ROM only

+ <i><b>I2C RDB</i></b>

  Reads an arbitrary-length block of bytes back from a specific I2C slave in one call.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | I2C RDB | _I2C RDB() | 10 | I2c_Rdb() |
| PARA 1 | IO bus addr 0...254 | byte or variable | H | byte or variable |
| PARA 2 | I2C addr 0...254 | byte or variable | C | byte or variable |
| PARA 3 | Length | byte or variable | B | byte or variable |
| PARA 4 | Data | array index 0 | DE as varptr | pointer |

*) ROM only

+ <i><b>I2C RESET</i></b>

  Resets the PCF8584 I2C bus controller itself (not a specific slave device) - useful to recover a bus that got stuck, e.g. after a slave was interrupted mid-transfer.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | I2C RESET | _I2C RESET() | 11 | I2c_Reset() |
| PARA 1 | IO addr 0...254 | byte or variable | C | byte or variable |

*) ROM only

+ <i><b>SET TIME</i></b>

  Writes the time-of-day into the on-board PCF8583 RTC, as an 8-character `HH:MM:SS` string.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SET TIME | _SET TIME RD() | 12 | Rtc_SetTime() |
| PARA 1 | IO bus addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | TIME | String variable len = 8 | DE as varptr | pointer (8 bytes) |

*) ROM only

+ <i><b>GET TIME</i></b>

  Reads the current time-of-day back from the PCF8583 RTC, as an 8-character `HH:MM:SS` string.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | GET TIME | _GET TIME RD() | 13 | Rtc_GetTime() |
| PARA 1 | IO bus addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | TIME | String variable len = 8 | DE as varptr | pointer (8 bytes) |

*) ROM only

+ <i><b>SET DATE</i></b>

  Writes the date into the on-board PCF8583 RTC, as an 8-character `DD/MM/YY` string.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | SET DATE | _SET DATE RD() | 14 | Rtc_SetDate() |
| PARA 1 | IO bus addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | DATE | String variable len = 8 | DE as varptr | pointer (8 bytes) |

*) ROM only

+ <i><b>GET DATE</i></b>

  Reads the current date back from the PCF8583 RTC, as an 8-character `DD/MM/YY` string.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | GET DATE | _GET DATE RD() | 15 | Rtc_GetDate() |
| PARA 1 | IO bus addr 0...254 | byte or variable | C | byte or variable |
| PARA 2 | DATE | String variable len = 8 | DE as varptr | pointer (8 bytes) |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [tsr/](tsr/README.md) - MemMan TSR
+ [pcb/](pcb/) - schematics and PCB

[↑ Main README](../../README.md#a-rc2014-card-with-pcf8584-i2c-controller-3-ports-via-seeed-and-pcf8583-battery-buffered-rtc)
