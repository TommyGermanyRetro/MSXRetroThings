# MSXRetroThings
<i>Some nerdy hard- and software for MSX machines</i>

When I was a child, my first computer was a <a href="https://www.msx.org/wiki/Category:SVI-3x8">SV 318 MKII</a> with a cassette recorder. But not SPECTRON or ARMOURED ASSAULT were my favorites, the expansion port and the table of its pins in the manual took my fully interest.

Unfortunatelly, I had no real clou to connect the signals to my Z80 PIO in a way that it works so I lost the fun on playing around with the hardware.

Now, 40 years later, I got a <a href="https://www.msx.org/wiki/Spectravideo_SVI-728">SVI 728</a> and a  <a href="https://www.ebay.de/sch/i.html?item=332817640567&rt=nc&_trksid=p4429486.m145687.l2562&_ssn=fractal2000">memory mapper / sd card cartridge</a> from the bay and also a <a href="https://www.8bits4ever.net/product-page/sxe-msx2-fpga-computer">SX-E MSX2+</a> and started over.

I had a lot of chips in my basket like Z80 PIO, 8255, 8254, SJA1000, PCF8584 etc. The results are several MSX cartridges, ROM code and <a href="https://rc2014.co.uk/">RC2014 cards</a> which I would like to show you here as retro and nerdy stuff for own developments or just for gambling.

Some of the software stuff comes from KONAMIMAN, especially the UNAPI structure and also the memory allocation in basic for working areas in RAM. Please have a look at his <a href="https://www.konamiman.com/msx/msx-e.html">git</a> for further information.

Have fun and stay healthy,

Thomas


## 1. MSXRetroThings - The XIO cartridge

#### <b>1.1 The XIO cartridge contains these functions:</b>

+ Switched IO based on the idea of ASCII to achive 256 additional IO locations
+ 8 channel programable interrupt controller 8259 with fallen edge sensitive inputs
+ 8 channel daisy chain IM2 emulator
+ BASIC and UNAPI commands to controll the functionallities of the cartridge in on board BIOS ROM

#### <b>1.2 Impressions:</b>

XIO cartridge used in a SX-E MSX2+ with adapter to RC2014 backplane

![XIO system](xio/pcb/msx_io_expander_system.png)

XIO cartridge

![XIO cartridge](xio/pcb/msx_io_expander_card.png)

Boot message from BIOS ROM

![XIO boot](xio/pcb/msx_io_expander_boot.png)

#### <b>1.3 Hardware:</b>

The schematics of XIO is strictly build up in non smd method to enable people without special tools to rebuild the pcb. But it uses GAL chips for the switched IO decoder, the glue decoder for controll signals and the address decoder.

![XIO schematics](xio/pcb/msx_io_expander.png)

![XIO system](rcx/rc2014_adapter/pcb/rc2014_adapter.png)

The XIO OUT/IN functions work without installing the XIO card. If the XIO card is installed correctly, the green LED shows that alle 16 interrupt channels are ready to use. A blinking yellow LED shows activities to the switched IO addresses. It signals, that the switched IO is selected via &H40 of the original MSX IO bus. The red LED shows a working supply.

#### <b>1.4 Software:</b>

The BIOS for the card functions is provided in the on board ROM (EEPROM). It contains the additional BASIC commands and an instance of UNAPI base on the approach of KONAMIMAN. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>XIO ?</i></b>

  Open the help information. It works independently from XIO INIT

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO ? | _XIO ? | 1 |

+ <i><b>XIO OUT</i></b>

  Send byte to swithched IO port. It works independently from XIO INIT

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO OUT | _XIO OUT(ADDR,DATA) | 2 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | IO data 0...255 | byte or variable | B |

+ <i><b>XIO INP</i></b>

  Get byte from switched IO port. It works independently from XIO INIT

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO INP | _XIO INP(ADDR,DATA) | 3 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | IO data 0...255 | variable | DE as varptr |

+ <i><b>XIO INIT</i></b>

  Install XIO card. The variable handed over as parameter contains the status of all 16 interrupt channels.

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO INIT | _XIO INIT(CHANNEL) | 4 |
| PARA 1 | status | variable | DE as varptr |
| PARA 2 | Working area in RAM | automatic by BIOS | HL |

+ <i><b>XIO ISINIT</i></b>

  Check if XIO is installed

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO ISINIT | _XIO ISINIT(ISI) | 5 |
| PARA 1 | 0 = not installed, 1 = installed | variable | DE as varptr |

+ <i><b>XIO DEINIT</i></b>

  Uninstall XIO card

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO DEINIT | _XIO DEINIT | 6 |

+ <i><b>XIO SET ADDR</i></b>

  Set address of interrupt routine per channel

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO SET ADDR | _XIO SET ADDR(CHANNEL,ADDR) | 7 |
| PARA 1 | channel | byte or variable | B |
| PARA 2 | addr | word or variable | DE |
| PARA 3 | slot | automatic by BIOS | C |

+ <i><b>XIO SET MASK</i></b>

  Enable or disable interrupt channel. Ch0 = 1, Ch1 = 2, Ch3 = 4, ... Ch15 = 128.

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO SET MASK | _XIO SET MASK(MASK) | 8 |
| PARA 1 | Bit Chx: 0 = disabled, 1 = enabled | word or variable | DE |

+ <i><b>XIO GET ADDR</i></b>

  Get address of interrupt address of selected channel

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO GET ADDR | _XIO GET ADDR(CHANNEL,ADDR) | 9 |
| PARA 1 | channel | byte or variable | B |
| PARA 2 | addr | variable | DE as varptr |
| PARA 3 | slot | automatic by BIOS | C |

+ <i><b>XIO GET MASK</i></b>

  Get current mask of enabled/disabled interrupt channels

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | XIO GET MASK | _XIO GET MASK(MASK) | 10 |
| PARA 1 | Bit Chx: 0 = disabled, 1 = enabled | variable | DE as varptr |


## 2. MSXRetroThings - The RCX ROM cartridge and RC2014 cards

#### <b>2.1 The RCX ROM cartridge contains these functions:</b>

+ Up to 16 RC2014 cards installable with used IO address control and 16 bytes of free RAM each card

+ RC2014 card with PCF8584 I2C Controller (3 ports via Seeed) and PCF8583 battery buffered RTC

+ RC2014 card with SJA1000 CAN controller (single use only)

+ RC2014 card with 8255A PIO

+ RC2014 card with 8254A CTR

+ RC2014 card with 4x SPI bus up to 2 MHz via ATMEGA8

+ RC2014 card with ADS1220 4 channel 24 bit A/D converter via ATMEGA8

+ RC2014 card with MCP4822 4 channel 12 bit D/A converter via ATMEGA8

+ RC2014 card SC103 with Z80 PIO

+ RC2014 card SC725 with Z80 SIO and Z80 CTC

+ BASIC and UNAPI commands to controll the functions of the cartridge in separate BIOS ROM cartridge

The schematics of RC2014 cards are strictly build up in non smd method to enable people without special tools to rebuild the pcb.

#### <b>2.2 Impressions:</b>

Slot ROM

![RCX system](rcx/rc2014_rom+ram/pcb/msx_rom_slot.png)

Boot message from BIOS ROM

![RCX boot](rcx/rc2014_rom+ram/pcb/rcx_boot.png)

#### <b>2.3 Hardware:</b>

![RCX schematics](rcx/rc2014_rom+ram/pcb/msx_rom.png)

#### <b>2.4 Software:</b>

The BIOS for the card functions is provided in the on board ROM (EEPROM). It contains the additional BASIC commands and an instance of UNAPI base on the approach of KONAMIMAN. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM for steering and controlling of the cards functions:

+ <i><b>RCX ?</i></b>

  Open the help information. It works independently from RCX INIT

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | RCX ? | _RCX ? | 1 |

+ <i><b>RCX INIT</i></b>

  Install RCX card. Clears table for 16 RC2014 cards and allocates RAM

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | RCX INIT | _RCX INIT | 2 |
| PARA 1 | Working area in RAM | automatic by BIOS | HL |

+ <i><b>RCX ISINIT</i></b>

  Check if RCX is installed

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | RCX ISINIT | _RCX ISINIT(ISI) | 3 |
| PARA 1 | 0 = not installed, 1 = installed | variable | DE as varptr |

+ <i><b>RCX DEINIT</i></b>

  Uninstall RCX card

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | RCX DEINIT | _RCX DEINIT | 4 |

+ <i><b>RCX TABLE</i></b>

  Returns the beginning of the RCX table in RAM for card infos

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | RCX TABLE | _RCX TABLE(ADDR) | 5 |
| PARA 1 | ADDR of RCX table | variable | DE as varptr |

## A) RC2014 card with PCF8584 I2C Controller (3 ports via Seeed) and PCF8583 battery buffered RTC:

This RC2014 card contains an I2C bus controller PCF8584 and a PCF8583 RTC buffered by battery. The interrupt pins of both chips can be assigned to one of the XIO PIC interrupt channels.
Up to 16 cards of this type are possible.

#### <b>A.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_a/pcb/msx_rc2014_i2c_base.png)

RC2014 card on backplane with I2C LCD from Seeed

![RCX system](rcx/rc2014_card_a/pcb/msx_rc2014_base_seeed_lcd.png)

RC2014 card on backplane contacted via BASIC to get TIME and DATE

![RCX system](rcx/rc2014_card_a/pcb/msx_rc2014_i2c_rtc_prompt.png)

#### <b>A.2 Hardware:</b>

![RCX schematics](rcx/rc2014_card_a/pcb/rc2014_i2c_base.png)

#### <b>A.3 Software:</b>

The BIOS for the card functions is provided in the RCX slot ROM. It contains the additional BASIC commands (`I2C INIT/WR/WRB/RD/RDB/RESET`, `SET/GET TIME/DATE`) and an instance of UNAPI based on the approach of KONAMIMAN, so the same functions are callable directly via UNAPI/EXTBIOS from a `.COM` program without the BASIC interpreter. Test software exists both as interactive BASIC programs and as `.COM` tools, including a variant that keeps the SEED I2C LCD updated on every PIC interrupt and one that stays resident in memory (MemMan TSR) to keep updating the display in the background. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>I2C INIT</i></b>

  Initialises RC2014 I2C base card at IO address 0..255

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | I2C INIT | _I2C INIT(ADDR) | 6 |
| PARA 1 | IO addr 0...255 | byte or variable | C |

+ <i><b>I2C WR</i></b>

  Sends a byte via I2C bus to special chip

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | I2C WR | _I2C WR(BUS,CHIP,DATA) | 7 |
| PARA 1 | IO bus addr 0...255 | byte or variable | E |
| PARA 2 | I2C addr 0...254 | byte or variable | C |
| PARA 3 | Data | byte or variable | B |

+ <i><b>I2C WRB</i></b>

  Sends a block of bytes via I2C bus to special chip

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | I2C WRB | _I2C WRB(BUS,CHIP,LEN,DATA) | 8 |
| PARA 1 | IO bus addr 0...255 | byte or variable | H |
| PARA 2 | I2C addr 0...254 | byte or variable | C |
| PARA 3 | Length | byte or variable | B |
| PARA 4 | Data | array index 0 | DE as varptr |

+ <i><b>I2C RD</i></b>

  Reads a bytes via I2C bus from special chip

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | I2C RD | _I2C RD(BUS,CHIP,DATA) | 9 |
| PARA 1 | IO bus addr 0...255 | byte or variable | C |
| PARA 2 | I2C addr 0...254 | byte or variable | B |
| PARA 3 | Data | variable | DE as varptr |

+ <i><b>I2C RDB</i></b>

  Reads a block of bytes via I2C bus from special chip

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | I2C RDB | _I2C RDB(BUS,CHIP,LEN,DATA) | 10 |
| PARA 1 | IO bus addr 0...255 | byte or variable | H |
| PARA 2 | I2C addr 0...254 | byte or variable | C |
| PARA 3 | Length | byte or variable | B |
| PARA 4 | Data | array index 0 | DE as varptr |

+ <i><b>I2C RESET</i></b>

  Resets the I2C bus at IO address 0..255

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | I2C RESET | _I2C RESET(ADDR) | 11 |
| PARA 1 | IO addr 0...255 | byte or variable | C |

+ <i><b>SET TIME</i></b>

  Sets the TIME for on board PCF8583

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SET TIME | _SET TIME RD(BUS,TIME) | 12 |
| PARA 1 | IO bus addr 0...255 | byte or variable | C |
| PARA 2 | TIME | String variable len = 8 | DE as varptr |

+ <i><b>GET TIME</i></b>

  Gets the TIME from on board PCF8583

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | GET TIME | _GET TIME RD(BUS,TIME) | 13 |
| PARA 1 | IO bus addr 0...255 | byte or variable | C |
| PARA 2 | TIME | String variable len = 8 | DE as varptr |

+ <i><b>SET DATE</i></b>

  Sets the DATE for on board PCF8583

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SET DATE | _SET DATE RD(BUS,DATE) | 14 |
| PARA 1 | IO bus addr 0...255 | byte or variable | C |
| PARA 2 | DATE | String variable len = 8 | DE as varptr |

+ <i><b>GET DATE</i></b>

  Gets the DATE from on board PCF8583

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | GET DATE | _GET DATE RD(BUS,DATE) | 15 |
| PARA 1 | IO bus addr 0...255 | byte or variable | C |
| PARA 2 | DATE | String variable len = 8 | DE as varptr |

## B) RC2014 card SC103 with Z80 PIO

The ROM of RCX contains BASIC and UNAPI commands to steer SC103 from original RC2014 manufactor.

#### <b>B.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_b/pcb/rc2014_sc103_board.png)

#### <b>B.2 Hardware:</b>

Please visit RC2014 homepage for further information

#### <b>B.3 Software:</b>

The BIOS for the card functions is provided in the RCX slot ROM. It contains the additional BASIC commands (`PIO INIT`, `PIO A/B RD/WR`, `PIO A/B CTRL`) and an instance of UNAPI based on the approach of KONAMIMAN, so the same functions are callable directly via UNAPI/EXTBIOS from a `.COM` program without the BASIC interpreter. A second test program drives Port B in the Z80 PIO's own IM2 interrupt mode, with a small Z80 machine code service routine registered through `XIO SET ADDR` that reports which channel fired. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>PIO INIT</i></b>

  Initialises RC2014 PIO card SC103 at IO address 0..255

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PIO INIT | _PIO INIT(ADDR) | 16 |
| PARA 1 | IO addr 0...255 | byte or variable | C |

+ <i><b>PIO A RD</i></b>

  Reads byte from RC2014 PIO card SC103 port A

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PIO A RD | _PIO A RD(ADDR,VALUE) | 17 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 0 |

+ <i><b>PIO A WR</i></b>

  Writes byte to RC2014 PIO card SC103 port A

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PIO A WR | _PIO A WR(ADDR,VALUE) | 18 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 1 |

+ <i><b>PIO B RD</i></b>

  Reads byte from RC2014 PIO card SC103 port B

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PIO B RD | _PIO B RD(ADDR,VALUE) | 19 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 2 |

+ <i><b>PIO B WR</i></b>

  Writes byte to RC2014 PIO card SC103 port B

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PIO B WR | _PIO B WR(ADDR,VALUE) | 20 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 3 |

+ <i><b>PIO A CTRL</i></b>

  Writes byte to RC2014 PIO card SC103 port A ctrl register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PIO A CTRL | _PIO A CTRL(ADDR,VALUE) | 21 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 4 |

+ <i><b>PIO B CTRL</i></b>

  Writes byte to RC2014 PIO card SC103 port B ctrl register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PIO B CTRL | _PIO B CTRL(ADDR,VALUE) | 22 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 5 |

## C) RC2014 card with 8255A PIO

RC2014 card with a 8255 PIO. Up to 16 cards are possible. The interrupt pins of PORT C can be assigned to one of the XIO PIC interrupt channels.

#### <b>C.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_c/pcb/rc2014_8255_board.png)

#### <b>C.2 Hardware:</b>

![RCX schematics](rcx/rc2014_card_c/pcb/rc2014_8255.png)

#### <b>C.3 Software:</b>

The BIOS for the card functions is provided in the RCX slot ROM. It contains the additional BASIC commands (`PPI INIT`, `PPI A/B/C RD/WR`, `PPI CTRL`, `PPI SET/RESET`) and an instance of UNAPI based on the approach of KONAMIMAN, so the same functions are callable directly via UNAPI/EXTBIOS from a `.COM` program without the BASIC interpreter. A second test program runs Port B in Mode 1 (strobed input), where the 82C55 itself raises `INTR B` on PC0 - reported through XIO's standard interrupt handler, no custom service routine needed. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>PPI INIT</i></b>

  Initialises RC2014 PIO card with 8255A at IO address 0..255

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI INIT | _PPI INIT(ADDR) | 23 |

+ <i><b>PPI A RD</i></b>

  Reads byte from RC2014 PIO card with 8255A port A

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI A RD | _PPI A RD(ADDR,VALUE) | 24 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 0 |

+ <i><b>PPI A WR</i></b>

  Writes byte to RC2014 PIO card with 8255A port A

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI A WR | _PPI A WR(ADDR,VALUE) | 25 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 1 |

+ <i><b>PPI B RD</i></b>

  Reads byte from RC2014 PIO card with 8255A port B

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI B RD | _PPI B RD(ADDR,VALUE) | 26 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 2 |

+ <i><b>PPI B WR</i></b>

  Writes byte to RC2014 PIO card with 8255A port B

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI B WR | _PPI B WR(ADDR,VALUE) | 27 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 3 |

+ <i><b>PPI C RD</i></b>

  Reads byte from RC2014 PIO card with 8255A port C

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI C RD | _PPI C RD(ADDR,VALUE) | 28 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 4 |

+ <i><b>PPI C WR</i></b>

  Writes byte to RC2014 PIO card with 8255A port C

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI C WR | _PPI C WR(ADDR,VALUE) | 29 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 5 |

+ <i><b>PIO CTRL</i></b>

  Writes byte to RC2014 PIO card with 8255A ctrl register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI CTRL | _PPI CTRL(ADDR,VALUE) | 30 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to write | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 6 |

+ <i><b>PPI SET</i></b>

  Sets bit on RC2014 PIO card with 8255A port C

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI SET | _PPI SET(ADDR,VALUE) | 31 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Bit to set | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 7 |

+ <i><b>PPI RESET</i></b>

  Resets bit on RC2014 PIO card with 8255A port C

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | PPI RESET | _PPI RESET(ADDR,VALUE) | 32 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Bit to reset | byte or variable | D |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 8 |

## D) RC2014 card with 8254A CTR

RC2014 card with a 8254 CTR. Up to 16 cards are possible. The interrupt pins of TIMER 0 and 1 can be assigned to one of the XIO PIC interrupt channels. This module does not work with internal clock if 8253 (max. 1 MHz) is used.

#### <b>D.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_d/pcb/rc2014_8254_board.png)

#### <b>D.2 Hardware:</b>

![RCX schematics](rcx/rc2014_card_d/pcb/rc2014_8254.png)

#### <b>D.3 Software:</b>

The BIOS for the card functions is provided in the RCX slot ROM. It contains the additional BASIC commands (`TIMER INIT`, `TIMER 0/1/2 RD/WR`, `TIMER CTRL`, `TIMER SET/RESET GATE`) and an instance of UNAPI based on the approach of KONAMIMAN, so the same functions are callable directly via UNAPI/EXTBIOS from a `.COM` program without the BASIC interpreter. A second test program wires one of the counter outputs to a PIC interrupt channel and keeps a running interrupt count on screen. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>TIMER INIT</i></b>

  Initialises RC2014 TIMER card with 8254A at IO address 0..255

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER INIT | _TIMER INIT(ADDR) | 33 |
| PARA 1 | IO addr 0...255 | byte or variable | C |

+ <i><b>TIMER 0 RD</i></b>

  Reads word from RC2014 TIMER card with 8254A TIMER 0

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER 0 RD | _TIMER 0 RD(ADDR,VALUE) | 34 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Word to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 0 |

+ <i><b>TIMER 0 WR</i></b>

  Writes word to RC2014 TIMER card with 8254A TIMER 0

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER 0 WR | _TIMER 0 WR(ADDR,VALUE) | 35 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Word to write | word or variable | DE |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 1 |

+ <i><b>TIMER 1 RD</i></b>

  Reads word from RC2014 TIMER card with 8254A TIMER 1

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER 1 RD | _TIMER 1 RD(ADDR,VALUE) | 36 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Word to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 2 |

+ <i><b>TIMER 1 WR</i></b>

  Writes word to RC2014 TIMER card with 8254A TIMER 1

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER 1 WR | _TIMER 1 WR(ADDR,VALUE) | 37 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Word to write | word or variable | DE |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 3 |

+ <i><b>TIMER 2 RD</i></b>

  Reads word from RC2014 TIMER card with 8254A TIMER 2

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER 2 RD | _TIMER 2 RD(ADDR,VALUE) | 38 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Word to read | variable | DE as varptr |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 4 |

+ <i><b>TIMER 2 WR</i></b>

  Writes word to RC2014 TIMER card with 8254A TIMER 2

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER 2 WR | _TIMER 2 WR(ADDR,VALUE) | 39 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Word to write | word or variable | DE |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 5 |

+ <i><b>TIMER CTRL</i></b>

  Writes word to RC2014 TIMER card with 8254A CTRL register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER CTRL | _TIMER CTRL(ADDR,VALUE) | 40 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Word to write | word or variable | E (D must be 0) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 6 |

+ <i><b>TIMER SET GATE</i></b>

  Sets internal gate for TIMER 0..2 if jumper is set

	Value 0..7 is allowed

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER SET GATE | _TIMER SET GATE(ADDR,VALUE) | 41 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | GATE 0..3 (0..7) | byte or variable | E (D must be 0) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 7 |

	+ <i><b>TIMER RESET GATE</i></b>

  Resets internal gate for TIMER 0..2 if jumper is set

	Value 0..7 is allowed

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | TIMER RESET GATE | _TIMER RESET GATE(ADDR,VALUE) | 42 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | GATE 0..3 (0..7) | byte or variable | E (D must be 0) |
| PARA 3 | UNAPI ID Level 2 | -/- | B = 8 |

## E) RC2014 card with SJA1000 CAN controller (single use only)

RC2014 card with SJA1000 CAN controller and RJ45 sockets with automatic termination function. The software provides PELICAN Mode and 250 kBit/s.

Only one card is possible due to RAM allocation and interrupt handling. The interrupt pin of the SJA1000 can be assigned to one of the XIO PIC interrupt channels.

#### <b>E.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_e/pcb/rc2014_sja1000_board.png)

#### <b>E.2 Hardware:</b>

![RCX schematics](rcx/rc2014_card_e/pcb/rc2014_sja1000.png)

#### <b>E.3 Software:</b>

The BIOS for the card functions is provided in the RCX slot ROM. It contains the additional BASIC commands (`CAN INIT`, `CAN WR/RD`, `CAN CHECK/RX/TX`, `CAN GET ADDR`) and an instance of UNAPI based on the approach of KONAMIMAN, so the same functions are callable directly via UNAPI/EXTBIOS from a `.COM` program without the BASIC interpreter. The test program receives and sends CAN messages interrupt-driven, e.g. for a Maerklin CS3 bus, by registering the ROM's own CAN service routine (returned by `CAN GET ADDR`) directly through `XIO SET ADDR` - no custom interrupt code needed. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>CAN INIT</i></b>

  Initialises RC2014 SJA1000 CAN card at IO address 0..255. The firmware has a ring buffer of 16 messages.

	ACC is the acceptance code and mask

	Array of INT is needed, only low byte is used, len = 8 (see datasheet for further information)

	Index 0 ACR.0, Index 1 ACR.1, Index 2 ACR.2, Index 3 ACR.3

	Index 4 AMR.0, Index 5 AMR.1, Index 6 AMR.2, Index 7 AMR.3

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | CAN INIT | _CAN INIT(ADDR,ACC) | 43 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | ACC code&mask | array index 0 | DE as varptr |

+ <i><b>CAN WR</i></b>

  Writes byte to SJA1000 register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | CAN WR | _CAN WR GATE(ADDR,REG,DATA) | 44 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Register | byte or variable | D |
| PARA 3 | Byte to write | byte or variable | E |

+ <i><b>CAN RD</i></b>

  Reads byte from SJA1000 register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | CAN RD | _CAN RD GATE(ADDR,REG,DATA) | 45 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Register | byte or variable | D |
| PARA 3 | Byte to read | variable | A contains result |

+ <i><b>CAN CHECK</i></b>

  Checks for new message in ring buffer (boolean)

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | CAN CHECK | _CAN CHECK(ADDR,RMC) | 46 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | 1 = new msg | variable | A contains result |

+ <i><b>CAN RX</i></b>

  Reads next message from ring buffer

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | CAN RX | _CAN RX(ADDR,DATA) | 47 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | New msg (PELICAN 2.0) | array len = 13 | DE as varptr |

+ <i><b>CAN GET ADDR</i></b>

  Gets interrupt handling address in RAM for XIO function XIO SET ADDR

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | CAN RX | _CAN GET ADDR(ADDR,INTDATA) | 48 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | RAM address pointer | variable | DE as varptr |

+ <i><b>CAN TX</i></b>

  Writes message to CAN bus

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | CAN TX | _CAN TX(ADDR,DATA) | 49 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Message (PELICAN 2.0) | array len = 13 | DE as varptr |

## F) RC2014 card with 4x SPI bus up to 2 MHz via ATMEGA8

RC2014 card with 4 SPI busses selectable via software with baudrate up to 2 MHz. Up to 16 SPI cards are possible. The interrupt pin of the ATMEGA8 can be assigned to one of the XIO PIC interrupt channels.

#### <b>F.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_f/pcb/rc2014_spi_board.png)

#### <b>F.2 Hardware:</b>

![RCX schematics](rcx/rc2014_card_f/pcb/rc2014_spi.png)

#### <b>F.3 Software:</b>

The BIOS for the card functions is provided in the RCX slot ROM. It contains the additional BASIC commands (`SPI INIT/MODE`, `SPI WR/WRB/RD/RDB/RDH`) and an instance of UNAPI based on the approach of KONAMIMAN, so the same functions are callable directly via UNAPI/EXTBIOS from a `.COM` program without the BASIC interpreter. Up to 4 independent SPI busses are addressable per card; the test program exercises a MCP23S17 GPIO expander as a Port A/B loopback test. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>SPI INIT</i></b>

  Initialises RC2014 SPI card at IO address 0..255

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SPI INIT | _SPI INIT(ADDR) | 50 |
| PARA 1 | IO addr 0...255 | byte or variable | C |

+ <i><b>SPI WR</i></b>

  Writes byte to RC2014 SPI card

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SPI WR | _SPI WR(ADDR,BUS,DATA) | 51 |
| PARA 1 | IO addr 0...255 | byte or variable | E |
| PARA 2 | SPI BUS 0..3 | byte or variable | C |
| PARA 3 | Byte to write | byte or variable | B |

+ <i><b>SPI WRB</i></b>

  Writes an array of bytes to RC2014 SPI card

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SPI WRB | _SPI WRB(ADDR,BUS,LEN,DATA) | 52 |
| PARA 1 | IO addr 0...255 | byte or variable | H |
| PARA 2 | SPI BUS 0..3 | byte or variable | C |
| PARA 3 | Length of array | byte or variable | B |
| PARA 4 | Data to write | array index 0 | DE as varptr |

+ <i><b>SPI RD</i></b>

  Reads byte from RC2014 SPI card

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SPI RD | _SPI RD(ADDR,BUS,DATA) | 53 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | SPI BUS 0..3 | byte or variable | B |
| PARA 3 | Byte to read | variable | DE as varptr |

+ <i><b>SPI RDB</i></b>

  Reads an array of bytes to RC2014 SPI card

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SPI RDB | _SPI RDB(ADDR,BUS,LEN,DATA) | 54 |
| PARA 1 | IO addr 0...255 | byte or variable | H |
| PARA 2 | SPI BUS 0..3 | byte or variable | C |
| PARA 3 | Length of array | byte or variable | B |
| PARA 4 | Data to read | array index 0 | DE as varptr |

+ <i><b>SPI RDH</i></b>

  Reads an array of bytes to RC2014 SPI card after sending a header for register selection.

	Both data is handled by one arry. LEN_WR signals length of header beginning with index 0 of the array. LEN_RD starts at index LEN_WR for data to read. Be sure to have a suitable definition for the array.

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SPI RDH | _SPI RDH(ADDR,BUS,LEN_WR,LEN_RD,DATA) | 55 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | SPI BUS 0..3 | byte or variable | B |
| PARA 3 | Length of array to write | byte or variable | H |
| PARA 4 | Length of array to read | byte or variable | L |
| PARA 5 | Data to read | array index 0 | DE as varptr |

+ <i><b>SPI MODE</i></b>

  Sets MODE of the SPI BUS for each channel. See datasheet of ATMEGA8 for further information

	MOD:

	0 = SPI_MODE_0, 1 = SPI_MODE_1, 2 = SPI_MODE_2, 3 = SPI_MODE_3

	FQZ:

	0 = 2 MHz, 1 = 1 MHz, 2 = 500 kHz, 3 = 250 kHz

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | SPI MODE | _SPI MODE(ADDR,BUS,MOD,FQZ) | 56 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | SPI BUS 0..3 | byte or variable | B |
| PARA 3 | Mode | byte or variable | D |
| PARA 4 | Bus frequency | byte or variable | E |

## G) RC2014 card with ADS1220 4 channel 24 bit A/D converter via ATMEGA8

RC2014 card with ADS1220 module. Up to 16 cards are possible. The interrupt pin of ATMEGA8 can be assigned to one of the XIO PIC interrupt channels.

#### <b>G.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_g/pcb/rc2014_ads1220_board.png)

#### <b>G.2 Hardware:</b>

![RCX schematics](rcx/rc2014_card_g/pcb/rc2014_ads1220.png)

#### <b>G.3 Software:</b>

The BIOS for the card functions is provided in the RCX slot ROM. It contains the additional BASIC commands (`ADS INIT/MODE/CMD/STATUS/READ/GET`) and an instance of UNAPI based on the approach of KONAMIMAN, so the same functions are callable directly via UNAPI/EXTBIOS from a `.COM` program without the BASIC interpreter. `ADS GET` automates the whole measurement sequence (set MODE, start conversion, select channel, read all 4 RESULT bytes) into a single native `SNG` float value. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>ADS INIT</i></b>

  Initialises RC2014 ADS1220 card at IO address 0..255

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | ADS INIT | _ADS INIT(BUS) | 57 |
| PARA 1 | IO addr 0...255 | byte or variable | C |

+ <i><b>ADS MODE</i></b>

  Writes the measurement type to the ADS1220 MODE register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | ADS MODE | _ADS MODE(BUS,TYPE) | 58 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Measurement type 0...11 | byte or variable | E |

+ <i><b>ADS CMD</i></b>

  Writes a raw byte to the ADS1220 CMD register

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | ADS CMD | _ADS CMD(BUS,VAL) | 59 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Raw value 0...255 | byte or variable | E |

+ <i><b>ADS STATUS</i></b>

  Reads the ADS1220 CMD register back (rChannel/mCount readback)

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | ADS STATUS | _ADS STATUS(BUS,VAL) | 60 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to read | variable | DE as varptr |

+ <i><b>ADS READ</i></b>

  Reads one raw RESULT byte; mCount auto-increments on the ATMEGA8 side (wraps 4 -> 0)

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | ADS READ | _ADS READ(BUS,VAL) | 61 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Byte to read | variable | DE as varptr |

+ <i><b>ADS GET</i></b>

  Automates a full measurement: sets MODE, starts conversion, selects channel, reads all 4 RESULT bytes into a native SNG variable

|  | Definition | BASIC | UNAPI |
| --- | --- | --- | --- |
| Syntax | ADS GET | _ADS GET(BUS,TYPE,VAL) | 62 |
| PARA 1 | IO addr 0...255 | byte or variable | C |
| PARA 2 | Measurement type 0...11 | byte or variable | B |
| PARA 3 | Result | SNG variable | DE as varptr |

## H) RC2014 card SC725 with Z80 SIO and Z80 CTC

The ROM of RCX contains BASIC and UNAPI commands to steer SC725 from original RC2014 manufactor.

#### <b>H.1 Impressions:</b>

RC2014 card on backplane

![RCX system](rcx/rc2014_card_h/pcb/rc2014_sc725_board.png)

Details coming soon

## I) RC2014 card with MCP4822 4 channel 12 bit D/A converter via ATMEGA8

RC2014 card with 2x MCP4822. Up to 16 cards are possible. The interrupt pin of ATMEGA8 can be assigned to one of the XIO PIC interrupt channels.

#### <b>I.1 Hardware:</b>

![RCX schematics](rcx/rc2014_card_i/pcb/rc2014_mcp4822.png)

Details coming soon




