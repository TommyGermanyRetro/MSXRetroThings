# Card I (2x MCP4822, ATMEGA8) - .COM tool

UNAPI/EXTBIOS port of `../basic/DAC.ASC` - calls `RCX_INTERFACE` directly via EXTBIO discovery,
no BASIC interpreter involved. Works with the ROM cartridge and with `RCXRAM.COM`. IO address is a
command-line parameter (`/IO:<0..252>`), e.g. `DAC /IO:32`.

## DAC.COM

Same tests as `DAC.ASC`: `DAC SET`/`DAC GET` and `DAC HOLD`/`DAC GET` readback on all channels,
all four outputs set at once via HOLD, and `DAC CONF` with gain x1, shutdown and gain x2 (key
press between the steps to measure). The banner starts with `DAC (ASM)`.
