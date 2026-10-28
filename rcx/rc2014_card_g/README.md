# Card G - RC2014 card with ADS1220 4 channel 24 bit A/D converter via ATMEGA8

RC2014 card with ADS1220 module. Up to 16 cards are possible. The interrupt pin of ATMEGA8 can be assigned to one of the XIO PIC interrupt channels.

## Command reference

Commands implemented in ROM:

+ <i><b>ADS INIT</i></b>

  Registers the card's IO base address in the RCX table. Must be called once per card before any of the other ADS commands below will find it.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | ADS INIT | _ADS INIT() | 57 | Ads_Init() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |

*) ROM only

+ <i><b>ADS MODE</i></b>

  Writes the measurement type to the ADS1220 MODE register

	TYPE (PARA 2):

	0 = SE AIN0-GND, 1 = SE AIN1-GND, 2 = SE AIN2-GND, 3 = SE AIN3-GND, 4 = DE AIN0-AIN1,
	5 = DE AIN0-AIN2, 6 = DE AIN0-AIN3, 7 = DE AIN1-AIN2, 8 = DE AIN1-AIN3, 9 = DE AIN2-AIN3,
	10 = DE AIN1-AIN0, 11 = DE AIN3-AIN2 (SE = single-ended against GND, DE = differential)

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | ADS MODE | _ADS MODE() | 58 | Ads_SetMode() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Measurement type 0...11 | byte or variable | E | byte or variable |

*) ROM only

+ <i><b>ADS CMD</i></b>

  Sends a raw command byte directly to the ADS1220 - low-level access for start-conversion/reset/power-down and anything else not already covered by the higher-level ADS commands.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | ADS CMD | _ADS CMD() | 59 | Ads_Cmd() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Raw value 0...255 | byte or variable | E | byte or variable |

*) ROM only

+ <i><b>ADS STATUS</i></b>

  Reads the ADS1220's own status byte back, decoded by the firmware into the currently selected read-channel (`rChannel`) and result-byte counter (`mCount`) - lets a program confirm what the last ADS CMD actually set, rather than assuming it took effect.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | ADS STATUS | _ADS STATUS() | 60 | Ads_GetStatus() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to read | variable | DE as varptr | return value (byte) |

*) ROM only

+ <i><b>ADS READ</i></b>

  Reads one of the four raw RESULT bytes (sign/exponent byte plus two mantissa bytes) of the current conversion. The firmware's own byte counter (`mCount`) auto-increments on every call and wraps from 4 back to 0, so four consecutive ADS READ calls step through a whole result in order.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | ADS READ | _ADS READ() | 61 | Ads_Read() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Byte to read | variable | DE as varptr | return value (byte) |

*) ROM only

+ <i><b>ADS GET</i></b>

  Automates a full measurement: sets MODE, starts conversion, selects channel, reads all 4 RESULT bytes into a native SNG variable

	TYPE (PARA 2):

	0 = SE AIN0-GND, 1 = SE AIN1-GND, 2 = SE AIN2-GND, 3 = SE AIN3-GND, 4 = DE AIN0-AIN1,
	5 = DE AIN0-AIN2, 6 = DE AIN0-AIN3, 7 = DE AIN1-AIN2, 8 = DE AIN1-AIN3, 9 = DE AIN2-AIN3,
	10 = DE AIN1-AIN0, 11 = DE AIN3-AIN2 (SE = single-ended against GND, DE = differential)

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | ADS GET | _ADS GET() | 62 | Ads_Get() |
| PARA 1 | IO addr 0...252 | byte or variable | C | byte or variable |
| PARA 2 | Measurement type 0...11 | byte or variable | B | byte or variable |
| PARA 3 | Result | SNG variable | DE as varptr | pointer (4 bytes) |

*) ROM only

## Folders

+ [basic/](basic/README.md) - BASIC test programs
+ [com/](com/README.md) - `.COM` tools (UNAPI/EXTBIOS)
+ [sdcc/](sdcc/README.md) - SDCC tools
+ [atmel/](atmel/README.md) - ATMEGA8 firmware
+ [pcb/](pcb/) - schematics, PCB, Gerber and drill files, bill of materials

[↑ Main README](../../README.md#g-rc2014-card-with-ads1220-4-channel-24-bit-ad-converter-via-atmega8)
