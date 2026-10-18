# INS8250

INS8250 is a C++17 static library modeling the National Semiconductor INS8250 UART without FIFO. A host clocks it in its own clock and moves whole characters on the serial side, each taking one frame time on the line.

## Functionality

- Registers 0-7: receiver buffer / transmitter holding (divisor latch LSB with DLAB), interrupt enable (divisor latch MSB with DLAB), interrupt identification, line control, modem control, line status, modem status, scratch.
- Baud rate `crystalHz / 16 / divisor`; frame time from start bit, 5-8 data bits, parity and 1, 1.5 or 2 stop bits.
- Transmitter with holding and shift register: THRE when the holding register is free, TEMT when the last character has left.
- Receiver: a character reaches the receiver buffer one frame after it starts; an unread character causes an overrun.
- Interrupts by priority: line status (overrun, parity, framing, break), received data, transmitter holding empty, modem status. Enabling the transmitter interrupt with an empty holding register raises it at once; reading IIR clears it.
- Modem lines: DTR, RTS, OUT1, OUT2 out; CTS, DSR, RI, DCD in with delta bits (RI on its trailing edge).
- Loopback mode: sent characters return to the receiver, RTS/DTR/OUT1/OUT2 appear as CTS/DSR/RI/DCD, the outputs stay inactive.

## Interface and Integration

[INS8250.hpp](include/INS8250/INS8250.hpp) exposes `ins8250::INS8250(hostClockHz, crystalHz)` with `read()`, `write()`, `tick()` and `interruptRequest()`. On the serial side `takeTransmitted()` returns a character that has left the transmitter, `readyToReceive()` tells whether the next character may start and `receive()` starts it; `setModemInputs()`, `modemOutputs()`, `lineSettings()` and `breakActive()` connect the host's line.

## In the SVI-318 and SVI-328

The RS-232 card SVI-805 of the [SVI-605B](../../svi3x8/SVI605B/README.md) places the UART at `28h`-`2Fh` with a 3.072 MHz crystal (divisor 20 = 9600 baud) and its interrupt on /INT; the frontend connects it to a Windows COM port. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `ins8250` | Static library | `ins8250.lib` |
| `ins8250_tests` | Console application | `ins8250_tests.exe` |

## Tests

[ins8250_tests.cpp](tests/ins8250_tests.cpp) checks reset state, divisor latch and line settings, transmit timing over two characters, receive timing with interrupt and overrun, interrupt priorities and clearing, modem status deltas, and loopback.
