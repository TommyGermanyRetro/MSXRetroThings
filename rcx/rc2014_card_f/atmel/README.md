# MSXSPI - ATMEGA8 firmware for Card F

## MSXSPI.ino

Runs on the card's ATMEGA8 and makes it act as a 4-channel SPI master, addressed by the RC2014
bus as a tiny memory-mapped peripheral with 4 registers (selected by 2 address lines, `/WAIT`
held low via a 74123 monostable while each access is handled):

- **Register 0 (MODE)** - write selects, per SPI bus (0..3), its clock mode (`SPI_MODE0..3`) and
  clock frequency (2 MHz down to 250 kHz); read returns the currently active bus's mode/frequency.
- **Register 1 (CMD)** - write with command 0 selects which of the 4 busses is "active" for the
  next register-2/3 access; read returns the active bus number.
- **Register 2 (/CS)** - write drives the active bus's chip-select line high or low (via two
  bus-select output pins feeding an external 4:1 router); read returns the current `/CS` state.
- **Register 3 (TRANSFER)** - write shifts a byte out over SPI, read shifts a byte in (dummy
  write of `0`); a changed MODE setting is only actually applied to the SPI peripheral lazily,
  right before the next transfer on that bus.

This is the firmware counterpart to the ROM's `SPI INIT/WR/WRB/RD/RDB/RDH/MODE` commands (see the
main [README.md](../../../README.md)) - each of those ends up as one or more raw register
accesses following exactly this Cmd/CS/Transfer sequence.
