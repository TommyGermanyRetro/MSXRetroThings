# PPI8255

PPI8255 is a standalone C++17 static library modeling the Intel 8255A programmable peripheral interface. It connects a software CPU's register accesses to parallel ports, latches and handshake signals.

## Functionality

- Four register addresses: `0` for port A, `1` for port B, `2` for port C and `3` for mode control or bit set/reset (BSR).
- Mode 0, Mode 1 and bidirectional Mode 2 for port A; Mode 0 and Mode 1 for port B.
- Port direction control, output latches and BSR access to PC0-PC7.
- Mode 1/2 handshakes for strobe, acknowledge, input/output buffer status and interrupt requests.
- BSR writes to reserved handshake bits control the corresponding internal INTE interrupt enables.
- Effective port outputs and per-bit drive masks for host-side pin integration.

## Interface and Integration

The public API is in [PPI8255.hpp](include/PPI8255/PPI8255.hpp). Use `write(address, value)` and `read(address)` for CPU accesses, `setInputs()` for external port and handshake state, and `outputs()` for driven values, masks and status. `controlWord()` exposes the mode word for inspection; it is not CPU-visible control-register readback. `isMode2A()` identifies bidirectional port-A mode.

This self-contained function configures Mode 0 with ports A/C as outputs and port B as input, then reads an external port-B value:

```cpp
#include <PPI8255/PPI8255.hpp>

std::uint8_t readParallelInput()
{
	ppi8255::PPI8255 chip;
	chip.write(3, 0x82);
	ppi8255::Inputs inputs{};
	inputs.portB = 0x5A;
	chip.setInputs(inputs);
	chip.write(0, 0xA5);
	return chip.read(1);
}
```

Handshake `*Active` fields are logical assertions: `true` represents the asserted low level of STB or ACK. Call `setInputs()` on each relevant transition so the model observes handshake edges. `outputBufferFullA/B` is `true` when the active-low OBF output is asserted. `*DriveMask` identifies driven bits; it does not describe electrical current or voltage. The host connects `interruptA/B` to its interrupt controller as needed.

For integration into another Visual Studio solution, add [ppi8255.vcxproj](ppi8255.vcxproj), reference it from the host project and add this project's `include` directory to the host's include directories. There are no external library dependencies.

## Limits

- The model implements register and handshake behavior, not electrical levels, propagation delays, setup/hold timing or metastability.
- Handshake events depend on input transitions supplied by the host; subcycle behavior between those transitions is not simulated.
- Reading register address `3` returns `0xFF`; the 8255A does not provide defined CPU control-word readback.
- Port-C bits assigned to handshakes report handshake state instead of the general port-C latch.

## In the SVI-318 and SVI-328

The PPI is written at `94h`-`97h` and read at `98h`-`9Bh` (ports A, B, C and control). Port A reads the joystick triggers (bit 4 joystick 1, bit 5 joystick 2) and the cassette input (bit 7); port B reads the keyboard row selected by port C bits 0-3. Port C bit 4 switches the cassette motor (0 = on), bit 5 is the cassette write data, bit 6 the cassette audio output and bit 7 the keyboard click. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln), [VG8235.sln](../../vg8235/Development/VG8235.sln), [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-728](../../svi728/Development/run_tests.ps1), [VG-8235](../../vg8235/Development/run_tests.ps1), [SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `ppi8255` | Static library | `ppi8255.lib` |
| `ppi8255_tests` | Console application | `ppi8255_tests.exe` |

## Tests

[ppi8255_tests.cpp](tests/ppi8255_tests.cpp) checks Mode-0 outputs, directions and latches, BSR behavior, Mode-1 and Mode-2 handshakes and interrupt states. These tests exercise functional port behavior, not electrical timing.
