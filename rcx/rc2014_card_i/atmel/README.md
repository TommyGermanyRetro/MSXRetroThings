# MSXMCP4822 - ATMEGA8 firmware for Card I

## MSXMCP4822.ino

Runs on the card's ATMEGA8 and drives the two MCP4822 dual 12 bit D/A converters over SPI (8 MHz),
exposed to the RC2014 bus as a tiny peripheral with 4 registers (selected by 2 address lines,
`/WAIT` held low via a 74123 monostable while each access is handled):

- **Register 0 (SELECT)** - write selects the channel 0...3 for the other registers; read returns
  the channel (bit 1..0) and its configuration (bit 5..4).
- **Register 1 (CONFIG)** - write sets the configuration of the selected channel (bit 0 = gain x2,
  bit 1 = shutdown), sends it to the converter together with the current value and updates the
  outputs; read returns the configuration.
- **Register 2 (VALUE LOW)** - write stores bit 7..0 of the value; read returns them.
- **Register 3 (VALUE HIGH)** - write takes bit 11..8 of the value (bit 3..0) and starts the SPI
  transfer to the selected channel. Bit 6 (HOLD) = 0 pulses /LDAC afterwards, so all outputs
  update; HOLD = 1 leaves the output unchanged until the next pulse. Read returns bit 11..8.

Both converters share /LDAC: writing several channels with HOLD = 1 and the last one with
HOLD = 0 changes all outputs at the same moment. The MCP4822 cannot be read back, so all reads
return the values the ATMEGA8 last sent. After reset all channels are set to value 0, gain x2,
active.

This is the firmware counterpart to the ROM's `DAC INIT/SET/HOLD/CONF/GET` commands (see the
card's [README.md](../README.md#command-reference)) - each of those ends up as a few raw register
accesses following exactly this register layout.

## Build and fuses

Board package [MiniCore](https://github.com/MCUdude/MiniCore), ATmega8 with external 16 MHz
crystal. Libraries: `SPI`, `elapsedMillis`.

```sh
arduino-cli compile --fqbn MiniCore:avr:8:clock=16MHz_external --export-binaries MSXMCP4822
```

Fuses, once per chip: lfuse `0xFF` (external crystal, BOD off), hfuse `0xC9` (CKOPT programmed:
full-swing oscillator, needed for 16 MHz - with CKOPT unprogrammed the datasheet allows crystals
only up to 8 MHz). Flash via the ISP connector SV101, e.g. with an AVRISP mkII:

```sh
avrdude -p m8 -c avrispmkii -P usb -B 8 -U lfuse:w:0xFF:m -U hfuse:w:0xC9:m
avrdude -p m8 -c avrispmkii -P usb -B 8 -U flash:w:build/MiniCore.avr.8/MSXMCP4822.ino.hex:i
```

Unplug the ISP cable afterwards and pull jumper JP101 if the interrupt output is not used.
