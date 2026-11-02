# Card I - RC2014 card with MCP4822 4 channel 12 bit D/A converter via ATMEGA8

RC2014 card with 2x MCP4822. Up to 16 cards are possible. The interrupt pin of ATMEGA8 can be assigned to one of the XIO PIC interrupt channels.

The four analog outputs are on the terminal X101: pin 2 = channel 0, pin 3 = channel 1, pin 4 = channel 2, pin 5 = channel 3, pin 1 = +5 V, pin 6 = GND. With gain x2 (default) the range is 0...4.095 V and the value is the output voltage in mV; with gain x1 it is 0...2.0475 V in steps of 0.5 mV.

## Command reference

Commands implemented in ROM:

+ <i><b>DAC INIT</i></b>

  Registers the card's IO base address in the RCX table. Must be called once per card before any of the other DAC commands below will find it.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | DAC INIT | _DAC INIT() | 63 | Dac_Init() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |

*) ROM only

+ <i><b>DAC SET</i></b>

  Writes a value to one channel and updates the outputs immediately. All four outputs share the load signal (/LDAC), so values written before with DAC HOLD are output at the same moment.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | DAC SET | _DAC SET() | 64 | Dac_Set() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Channel 0...3 | byte or variable | B | byte or variable |
| PARA 3 | Value 0...4095 | integer or variable | DE | word or variable |

*) ROM only

+ <i><b>DAC HOLD</i></b>

  Writes a value to one channel without updating the output. The output follows with the next DAC SET or DAC CONF, so several channels can be changed at exactly the same time: write all but the last with DAC HOLD and the last one with DAC SET.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | DAC HOLD | _DAC HOLD() | 65 | Dac_Hold() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Channel 0...3 | byte or variable | B | byte or variable |
| PARA 3 | Value 0...4095 | integer or variable | DE | word or variable |

*) ROM only

+ <i><b>DAC CONF</i></b>

  Sets the configuration of one channel and updates the outputs.

	CFG (PARA 3):

	bit 0 = gain (0 = x1, 0...2.0475 V; 1 = x2, 0...4.095 V), bit 1 = shutdown (output off, 500 kOhm to GND). After reset all channels are set to gain x2, active, value 0.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | DAC CONF | _DAC CONF() | 66 | Dac_Conf() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Channel 0...3 | byte or variable | B | byte or variable |
| PARA 3 | Configuration 0...3 | byte or variable | E | byte or variable |

*) ROM only

+ <i><b>DAC GET</i></b>

  Reads back the last value written to one channel. The MCP4822 itself cannot be read, the value comes from the ATMEGA8.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | DAC GET | _DAC GET() | 67 | Dac_Get() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Channel 0...3 | byte or variable | B | byte or variable |
| PARA 3 | Value 0...4095 | integer variable | DE as varptr | return value (word) |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [atmel/](atmel/README.md) - ATMEGA8 firmware
+ [pcb/](pcb/) - schematics, PCB, Gerber and drill files, bill of materials

[↑ Main README](../../README.md#i-rc2014-card-with-mcp4822-4-channel-12-bit-da-converter-via-atmega8)
