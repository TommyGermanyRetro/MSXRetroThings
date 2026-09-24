# Card F (SPI, ATMEGA8) - .COM tool

UNAPI/EXTBIOS port of `../basic/MCP23S17.ASC` - calls `RCX_INTERFACE` directly via EXTBIO
discovery, no BASIC interpreter involved. IO address and SPI bus number are command-line
parameters (`/IO:<0..252> /BUS:<0..3>`) instead of a fixed/interactive setup.

## MCP23S17.COM

Same MCP23S17 GPIO-expander loopback test as `MCP23S17.ASC`: Port A output, Port B input,
incrementing byte written and read back endlessly.
