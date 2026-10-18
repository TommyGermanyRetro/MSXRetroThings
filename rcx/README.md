# RCX ROM cartridge - command reference

The RCX ROM cartridge manages up to 16 RC2014 cards. Its ROM (or the RAM driver `RCXRAM.COM`) adds BASIC `CALL` commands and an UNAPI/EXTBIOS interface; the SDCC library `rcx_io` bridges to the same functions. This page lists the general RCX commands; the commands of each card are on the card's page.

## Cards

+ [A) RC2014 card with PCF8584 I2C Controller (3 ports via Seeed) and PCF8583 battery buffered RTC](rc2014_card_a/README.md)
+ [B) RC2014 card SC103 with Z80 PIO](rc2014_card_b/README.md)
+ [C) RC2014 card with 8255A PIO](rc2014_card_c/README.md)
+ [D) RC2014 card with 8254A CTR](rc2014_card_d/README.md)
+ [E) RC2014 card with SJA1000 CAN controller (single use only)](rc2014_card_e/README.md)
+ [F) RC2014 card with 4x SPI bus up to 2 MHz via ATMEGA8](rc2014_card_f/README.md)
+ [G) RC2014 card with ADS1220 4 channel 24 bit A/D converter via ATMEGA8](rc2014_card_g/README.md)
+ [H) RC2014 card SC725 with Z80 SIO and Z80 CTC](rc2014_card_h/README.md)
+ [I) RC2014 card with MCP4822 4 channel 12 bit D/A converter via ATMEGA8](rc2014_card_i/README.md)

## Command reference

Commands implemented in ROM/DRV/LIB for steering and controlling of the cards functions:

+ <i><b>RCX ?</i></b>

  Prints the list of available RCX commands directly to the screen. Works independently from RCX INIT, so it is always safe to call even if no card has been registered yet.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | RCX ? | _RCX ? | 1 | - (BASIC-only, not exposed) |

*) ROM only

+ <i><b>RCX INIT</i></b>

  Installs the RCX driver itself: allocates and clears the internal table that tracks which RC2014 card types are registered at which IO address. Every other RCX/card-specific command needs this table to exist, so it must run before any card-type INIT (I2C INIT, PIO INIT, etc.); calling it again while already installed just resets the table.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | RCX INIT | _RCX INIT | 2 | Rcx_Init() |
| PARA 1 | Working area in RAM | automatic by BIOS | HL | word |

*) ROM only

+ <i><b>RCX ISINIT</i></b>

  Checks whether the RCX driver is already installed, without touching anything - same ISINIT/DEINIT/INIT pattern as XIO ISINIT.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | RCX ISINIT | _RCX ISINIT() | 3 | Rcx_IsInit() |
| PARA 1 | 0 = not installed, 1 = installed | variable | DE as varptr | return value (byte) |

*) ROM only

+ <i><b>RCX DEINIT</i></b>

  Uninstalls the RCX driver and frees the card table again.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | RCX DEINIT | _RCX DEINIT | 4 | Rcx_Deinit() |

*) ROM only

+ <i><b>RCX TABLE</i></b>

  Returns the RAM address of the internal per-slot card table directly, mainly useful for diagnostics - inspecting which addresses and card types are currently registered without a dedicated command for each one.

|  | Definition | BASIC* | UNAPI | SDCC |
| --- | --- | --- | --- | --- |
| Syntax | RCX TABLE | _RCX TABLE() | 5 | Rcx_GetTable() |
| PARA 1 | ADDR of RCX table | variable | DE as varptr | pointer (word) |

*) ROM only

## Folders

+ [rc2014_rom+ram/rom/](rc2014_rom+ram/rom/README.md) - ROM source and image
+ [rc2014_rom+ram/drv/](rc2014_rom+ram/drv/README.md) - RAM driver `RCXRAM.COM`
+ [rc2014_rom+ram/pcb/](rc2014_rom+ram/pcb/) - schematics and PCB of the ROM cartridge
+ [rc2014_adapter/pcb/](rc2014_adapter/pcb/) - schematics and PCB of the RC2014 adapter
+ [rcxslt/](rcxslt/) - RCX slot tools (BASIC, `.COM`, SDCC)

[↑ Main README](../README.md#2-msxretrothings---the-rcx-rom-cartridge-and-rc2014-cards)
