# MSXRetroThings
<i>Some nerdy hard- and software for MSX machines</i>

When I was a child, my first computer was a <a href="https://www.msx.org/wiki/Category:SVI-3x8">SV 318 MKII</a> with a cassette recorder. But not SPECTRON or ARMOURED ASSAULT were my favorites, the expansion port and the table of its pins in the manual took my fully interest.

Unfortunatelly, I had no real clou to connect the signals to my Z80 PIO in a way that it works so I lost the fun on playing around with the hardware.

Now, 40 years later, I got a <a href="https://www.msx.org/wiki/Spectravideo_SVI-728">SVI 728</a> and a  <a href="https://www.ebay.de/sch/i.html?item=332817640567&rt=nc&_trksid=p4429486.m145687.l2562&_ssn=fractal2000">memory mapper / sd card cartridge</a> from the bay and also a <a href="https://www.8bits4ever.net/product-page/sxe-msx2-fpga-computer">SX-E MSX2+</a> and started over. 

I had a lot of chips in my basket like Z80 PIO, 8255, 8254, SJA1000, PCF8584 etc. The results are several MSX cartridges, ROM code and RC2014 cards which I would like to show you here as retro and nerdy stuff for own developments or just for gambling.

Some of the software stuff comes from KONAMIMAN, especially the UNAPI structure and also the memory allocation in basic for working areas in RAM. Please have a look at his <a href="https://www.konamiman.com/msx/msx-e.html">git</a> for further information.

Have fun and stay healthy,

Thomas


# MSXRetroThings - The XIO cartridge

<b>The XIO cartridge contains these functions:</b>

+ Switched IO based on the idea of ASCII to achive 256 additional IO locations
+ 8 channel programable interrupt controller 8259 with fallen edge sensitive inputs
+ 8 channel daisy chain IM2 emulator
+ BASIC and UNAPI commands to controll the functionallities of the cartridge in on board BIOS ROM

<b>Impressions:</b>

XIO cartridge used in a SX-E MSX2+ with adapter to RC2014 backplane

![XIO system](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/xio/pcb/msx_io_expander_system.png)

XIO cartridge 

![XIO cartridge](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/xio/pcb/msx_io_expander_card.png)

Boot message from BIOS ROM

![XIO boot](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/xio/pcb/msx_io_expander_boot.png)

<b>Hardware:</b>

The schematics of XIO is strictly build up in non smd method to enable people without special tools to rebuild the pcb. But it uses GAL chips for the switched IO decoder, the glue decoder for controll signals and the address decoder.

![XIO schematics](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/xio/pcb/msx_io_expander.png)

The XIO OUT/IN functions work without installing the XIO card. If the XIO card is installed correctly, the green LED shows that alle 16 interrupt channels are ready to use. A blinking yellow LED shows activities to the switched IO addresses. It signals, that the switched IO is selected via &H40 of the original MSX IO bus. The red LED shows a working supply.


<b>Software:</b>

The BIOS for the card functions is provided in the on board ROM (EEPROM). It contains the additional BASIC commands and an instance of UNAPI base on the approach of KONAMIMAN. Please have a look at his page to get a deeper impression on how it works.

Commands implemented in ROM:

+ <i><b>XIO ?</i></b>

  Open the help information. It works independently from XIO INIT
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO ?</td>
    <td>_XIO ?</td>
		<td>1</td>
  </tr>
	</table>
  
+ <i><b>XIO OUT</i></b>

  Send byte to swithched IO port. It works independently from XIO INIT
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO OUT</td>
    <td>_XIO OUT(ADDR,DATA)</td>
		<td>2</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>IO addr 0...255</td>
    <td>byte or variable</td>
		<td>C</td>
  </tr>
	<tr>
    <td>PARA 2</td>
    <td>IO data 0...255</td>
    <td>byte or variable</td>
		<td>B</td>
  </tr>
	</table>

+ <i><b>XIO INP</i></b>

  Get byte from switched IO port. It works independently from XIO INIT
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO INP</td>
    <td>_XIO INP(ADDR,DATA)</td>
		<td>3</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>IO addr 0...255</td>
    <td>byte or variable</td>
		<td>C</td>
  </tr>
	<tr>
    <td>PARA 2</td>
    <td>IO data 0...255</td>
    <td>variable</td>
		<td>DE as varptr</td>
  </tr>
	</table>
  
+ <i><b>XIO INIT</i></b>

  Install XIO card. The variable handed over as parameter contains the status of all 16 interrupt channels.
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO INIT</td>
    <td>_XIO INIT(CHANNEL)</td>
		<td>4</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>status</td>
    <td>variable</td>
		<td>DE as varptr</td>
  </tr>
	<tr>
    <td>PARA 2</td>
    <td>Working area in RAM</td>
    <td>automatic by BIOS</td>
		<td>HL</td>
  </tr>
	</table>	
 
+ <i><b>XIO ISINIT</i></b>

  Check if XIO is installed
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO ISINIT</td>
    <td>_XIO ISINIT(ISI)</td>
		<td>5</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>0 = not installed, 1 = installed</td>
    <td>variable</td>
		<td>DE as varptr</td>
  </tr>
	</table>	
  
+ <i><b>XIO DEINIT</i></b>

  Uninstall XIO card
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO DEINIT</td>
    <td>_XIO DEINIT</td>
		<td>6</td>
  </tr>
	</table>	
  
+ <i><b>XIO SET ADDR</i></b>

  Set address of interrupt routine per channel
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO SET ADDR</td>
    <td>_XIO SET ADDR(CHANNEL,ADDR)</td>
		<td>7</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>channel</td>
    <td>byte or variable</td>
		<td>B</td>
  </tr>
	<tr>
    <td>PARA 2</td>
    <td>addr</td>
    <td>word or variable</td>
		<td>DE</td>
  </tr>
	<tr>
    <td>PARA 3</td>
    <td>slot</td>
    <td>automatic by BIOS</td>
		<td>C</td>
  </tr>	
	</table>		

+ <i><b>XIO SET MASK</i></b>

  Enable or disable interrupt channel. Ch0 = 1, Ch1 = 2, Ch3 = 4, ... Ch15 = 128.
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO SET MASK</td>
    <td>_XIO SET MASK(MASK)</td>
		<td>8</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>Bit Chx: 0 = disabled, 1 = enabled</td>
    <td>word or variable</td>
		<td>DE</td>
  </tr>
	</table>		
  
+ <i><b>XIO GET ADDR</i></b>

  Get address of interrupt address of selected channel
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO GET ADDR</td>
    <td>_XIO GET ADDR(CHANNEL,ADDR)</td>
		<td>9</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>channel</td>
    <td>byte or variable</td>
		<td>B</td>
  </tr>
	<tr>
    <td>PARA 2</td>
    <td>addr</td>
    <td>variable</td>
		<td>DE as varptr</td>
  </tr>
	<tr>
    <td>PARA 3</td>
    <td>slot</td>
    <td>automatic by BIOS</td>
		<td>C</td>
  </tr>	
	</table>	

+ <i><b>XIO GET MASK</i></b>

  Get current mask of enabled/disabled interrupt channels
	
	<table style="width:100%">
  <tr>
    <th></th>
    <th>Definition</th>
    <th>BASIC</th>
    <th>UNAPI</th>		
  </tr>
  <tr>
    <td>Syntax</td>
    <td>XIO GET MASK</td>
    <td>_XIO GET MASK(MASK)</td>
		<td>10</td>
  </tr>
	<tr>
    <td>PARA 1</td>
    <td>Bit Chx: 0 = disabled, 1 = enabled</td>
    <td>variable</td>
		<td>DE as varptr</td>
  </tr>
	</table>	

	
# MSXRetroThings - The RCX ROM cartridge and RC2014 cards

<b>The RCX ROM cartridge contains these functions:</b>

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

* <b>RC2014 card with PCF8584 I2C Controller (3 ports via Seeed) and PCF8583 battery buffered RTC:<*/b>

This RC2014 card contains an I2C bus controller PCF8584 and a PCF8583 RTC buffered by battery. The interrupt pins of both chips can be assigned to one of the XIO PIC interrupt channels.

<b>Impressions:</b>

RC2014 card on backplane

![RCX system](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/rcx/rc2014_i2c_base/pcb/msx_rc2014_i2c_base.png)

RC2014 card on backplane with I2C LCD from Seeed

![RCX system](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/rcx/rc2014_i2c_base/pcb/msx_rc2014_base_seeed_lcd.png)

RC2014 card on backplane contacted via BASIC to get TIME and DATE

![RCX system](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/rcx/rc2014_i2c_base/pcb/msx_rc2014_i2c_rtc_prompt.png)

<b>Hardware:</b>

The schematics of RC2014 I2C base is strictly build up in non smd method to enable people without special tools to rebuild the pcb.

![XIO schematics](https://github.com/TommyGermanyRetro/MSXRetroThings/blob/main/rcx/rc2014_i2c_base/pcb/rc2014_i2c_base.png)

Details coming soon


