# MC6821

MC6821 is a C++17 static library modeling the Motorola MC6821 (Hitachi HD46821) peripheral interface adapter: two 8-bit ports with data direction registers and the control lines CA1/CA2 and CB1/CB2.

## Functionality

- Register select 0-3: PRA or DDRA, CRA, PRB or DDRB, CRB; bit 2 of the control register chooses the peripheral register (1) or the data direction register (0).
- Port pins: output bits from the peripheral register, input bits from outside (`setPortAInputs()`, `setPortBInputs()`).
- C1 inputs: interrupt flag (bit 7) on the active edge chosen by bit 1, interrupt request with bit 0.
- C2 as input: flag (bit 6) on the edge chosen by bit 4, interrupt request with bit 3.
- C2 as output: manual (bit 4 = 1, level from bit 3); handshake (bit 4 = 0): CA2 goes low on a PRA read, CB2 on a PRB write, and returns high on the next active C1 edge (bit 3 = 0) or right after the access (bit 3 = 1).
- Reading a peripheral register clears both flags of its side; bits 6 and 7 of the control registers are read-only. Reset clears all registers.

## Interface and Integration

[MC6821.hpp](include/MC6821/MC6821.hpp) exposes `mc6821::MC6821` with `read()`, `write()`, the input setters `setPortAInputs()`, `setPortBInputs()`, `setCA1()`, `setCB1()`, `setCA2()`, `setCB2()`, and the outputs `portA()`, `portB()`, `ca2()`, `cb2()`, `irqA()`, `irqB()`.

## In the SVI-318 and SVI-328

The printer port on the LAN card of the SVI-609 Master Station ([LANIF01](../../svi3x8/LANIF01/README.md)) is an MC6821 at `60h`-`63h`: port A carries the printer data inverted, CA2 the strobe and CA1 the acknowledge, PB0 reads BUSY; CP/M 2.27 prints through it. The SVI-608M has none. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `mc6821` | Static library | `mc6821.lib` |
| `mc6821_tests` | Console application | `mc6821_tests.exe` |

## Tests

[mc6821_tests.cpp](tests/mc6821_tests.cpp) checks reset, DDR/PR selection and pin read-back, read-only flags, manual C2 output, write-strobe handshake ended by CB1, pulse mode, C1 interrupts with edge and enable, clearing by a PRA read, and a CB2 input interrupt.
