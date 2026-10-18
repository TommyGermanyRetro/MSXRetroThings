# Z8530

Z8530 is a C++17 static library modeling the Zilog Z8530 (NMOS) Serial Communications Controller: two full-duplex channels A and B for asynchronous and SDLC operation, each with baud rate generator, modem lines and a share of the interrupt daisy chain. It sits on the LAN card of the SVI-609 Master Station (see [LANIF01](../../svi3x8/LANIF01/README.md)); there it is not connected yet, because its ports on the card are not documented.

## Functionality

- **Register access** as on the Z85X30: control and data port per channel. With pointer 0 a control write is WR0 (register select bits 0-2, Point High for WR8-WR15, the commands Reset Ext/Status Interrupts, Send Abort, Enable Int on Next Rx Character, Reset Tx Int Pending, Error Reset, Reset Highest IUS, the CRC reset codes); after the next access the pointer returns to 0. NMOS images: RR4-RR7 = RR0-RR3, RR9 = RR13, RR11 = RR15, RR14 = RR10. WR2 and WR9 exist once; RR3 only in channel A; RR15 returns bits 0 and 2 as 0.
- **Resets:** hardware reset, channel reset A and B and Force Hardware Reset by WR9, with the register values of the reset table of the manual (RR0 `44h`, RR1 `07h`, RR15 `F8h`, WR4 bit 2, WR11 `08h`, modem outputs inactive).
- **Asynchronous:** 5, 6, 7 or 8 bits (five-or-less coding on transmit), parity even or odd (passed in the data below 8 bits), 1, 1.5 or 2 stop bits, clock x1/x16/x32/x64, framing error, break with the null character at its end, unused receive bits read as 1, Sync Character Load Inhibit.
- **SDLC:** opening flag before the first byte, address search on WR6 with broadcast `FFh` and the four-bit range, CRC-CCITT preset by WR10 bit 7, sent inverted and checked against `0001110100001111b`, the two CRC bytes passed into the receive FIFO, end of frame with CRC result and residue `011` in RR1, Tx Underrun/EOM latch (Reset Tx Underrun/EOM Latch) with CRC and flag, or abort and flag (WR10 bit 2), on underrun, Send Abort, hunt after enabling the receiver, an abort and Enter Hunt. The CRC generator is preset by software (Reset Tx CRC) before each frame, as on the NMOS part.
- **Receiver:** three-character FIFO, overrun on its last place; parity and overrun errors latched until Error Reset; receive interrupt modes off, first character or special condition (Enable Int on Next Rx Character re-arms, a special condition locks the FIFO exit until Error Reset), every character or special condition, special condition only.
- **Transmitter:** one-byte buffer, Tx interrupt when the buffer empties and after the CRC, Reset Tx Int Pending, all sent, send break, auto enables (/CTS starts the transmitter, /DCD the receiver), local loopback and auto echo.
- **Clocks:** transmit clock from the RTxC pin, the TRxC pin or the baud rate generator (period 2 x (time constant + 2) clocks of PCLK or RTxC); zero count as Ext/Status source. One `tick()` is one PCLK cycle; a character takes its bit count times the bit time.
- **Ext/Status:** zero count, /DCD, sync/hunt, /CTS, Tx Underrun/EOM and break/abort with the enables of WR15 and WR1 bit 0. A change closes all latches (RR0 then shows the latched state of the enabled bits), Reset Ext/Status opens them and a change that persists interrupts again; Tx Underrun/EOM and zero count interrupt on their rising edge, break/abort on both edges even while latched.
- **Interrupts:** six sources in the priority Rx A, Tx A, Ext/Status A, Rx B, Tx B, Ext/Status B with interrupt pending (RR3) and under service; /INT with MIE; the acknowledge cycle sets the under-service bit and returns WR2, with VIS modified in bits 1-3 or (Status High) in bits 6-4; NV leaves the bus undriven; Reset Highest IUS; RR2 of channel B is always modified, `011` when nothing is pending.
- **Line:** bit coding is not modelled; the line carries symbols (character with parity bit and stop bit, flag, abort, start and end of break) between `takeTransmitted()` and `receive()`.
- **Not modelled:** monosync, bisync, external sync, SDLC loop mode, DPLL, NRZI/FM coding, Wait/Request and DMA.

## Interface and Integration

[Z8530.hpp](include/Z8530/Z8530.hpp) exposes `z8530::Z8530` with `readControl()`, `writeControl()`, `readData()`, `writeData()` per channel (`z8530::ChannelA`, `z8530::ChannelB`), `tick()`, `interruptRequest()`, `interruptAcknowledge()`, the line side `hasTransmitted()`, `takeTransmitted()`, `receive()` with `z8530::LineSymbol`, the modem lines `setCts()`, `setDcd()`, `dtr()`, `rts()` and the pin clocks `setRtxcPeriod()`, `setTrxcPeriod()`.

```cpp
#include <Z8530/Z8530.hpp>

z8530::Z8530 scc;
scc.writeControl(z8530::ChannelA, 4);      // pointer to WR4
scc.writeControl(z8530::ChannelA, 0x44);   // x16, 1 stop bit, no parity
// ... WR3, WR5, WR11-WR14
scc.writeData(z8530::ChannelA, 'A');
scc.tick();
```

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `z8530` | Static library | `z8530.lib` |
| `z8530_tests` | Console application | `z8530_tests.exe` |

## Tests

[z8530_tests.cpp](tests/z8530_tests.cpp) follows the Zilog *SCC/ESCC User Manual* (UM010902-0609): the reset values and register images, the pointer, RR2 with status low and high, channel reset and the modem outputs; an asynchronous character through local loopback in 640 PCLK cycles at time constant 0 and x16, a one-bit character of the five-or-less coding; parity in D7 and the latched parity error, unused bits as 1, framing error, the FIFO with overrun, break; interrupt priority, vectors, IUS and Reset Highest IUS, the first-character mode with Enable Int on Next Rx Character and the locked FIFO, Tx interrupt and Reset Tx Int Pending, the Ext/Status latches with a persisting change, No Vector and MIE; zero count; SDLC frames from channel A to channel B with flags, CRC, end of frame, address search, broadcast, a CRC error, an abort and Send Abort; auto enables with /CTS.
