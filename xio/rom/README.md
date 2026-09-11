# XIOROM - XIO cartridge driver ROM

## XIOROM.ASM / XIOROM.hex

The BIOS extension ROM for the XIO cartridge itself: provides switched IO access to 256
additional IO addresses (`XIO OUT`/`XIO INP`), and drives the on-board 8259 PIC plus an 8-channel
daisy-chain Z80 IM2 emulator (`XIO INIT/ISINIT/DEINIT`, `XIO SET/GET ADDR`, `XIO SET/GET MASK`) -
see the main [README.md](../../README.md) for the complete command reference. Like `RCXROM.ASM`,
it exposes both classic BASIC `CALL` statements and a KONAMIMAN-style UNAPI/EXTBIOS interface.

`XIOROM.hex` is the ready-to-flash EEPROM image built from `XIOROM.ASM`.
