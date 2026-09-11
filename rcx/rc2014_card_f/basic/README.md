# Card F (SPI, ATMEGA8) - BASIC test program

## MCP23S17.ASC

SPI loopback test against an MCP23S17 GPIO expander on SPI bus 0: configures Port A as output
and Port B as input via `SPI WRB`, then writes an incrementing byte to Port A and reads it back
on Port B via `SPI RDH` (send-header-then-read) in an endless loop, showing both values on
screen. Needs Port A and Port B physically wired together to see the written value echoed back.
IO address is asked for interactively.
