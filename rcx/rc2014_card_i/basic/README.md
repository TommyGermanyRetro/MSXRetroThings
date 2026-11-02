# Card I (2x MCP4822, ATMEGA8) - BASIC test programs

Both programs ask for the card's IO address (0..252, multiple of 4) and run the same tests. The
voltages are checked with a meter on X101 (pin 2..5 against pin 6).

## MCP4822.ASC

Direct register access via `_XIO OUT`/`_XIO INP`, no RCX needed:

- **Test A** - SELECT and CONFIG readback for every channel and configuration 0...3.
- **Test B** - VALUE LOW/HIGH readback with 0/1365/2730/4095; odd steps are written with HOLD,
  bit 6 must not come back.
- **Test C** - all outputs 0 V, then 1/2/3/4 V with HOLD (outputs must stay at 0 V), then
  channel 3 again without HOLD: all four outputs change at once.
- **Test D** - channel 0 with gain x1 (0.500 V), shutdown (0 V), gain x2 again (1.000 V).

Prints `ALL READBACK CHECKS PASSED` or the number of failures, then allows manual input of channel
and value.

## DAC.ASC

The same tests via the RCX commands `_DAC INIT/SET/HOLD/CONF/GET`, see the card's
[README.md](../README.md#command-reference).
