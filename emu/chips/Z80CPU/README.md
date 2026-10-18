# Z80Bus

Z80Bus is a C++17 static library for integrating a T-state-stepped Z80 CPU into a software machine. The host supplies memory, I/O devices, interrupt signals and scheduling.

## Functionality

- `z80bus::Z80Bus::tick()` advances one Z80 T-state and returns address, data and control signals.
- Outputs expose M1, memory and I/O requests, read/write, HALT, refresh, BUSACK, interrupt enable output and RETI.
- Inputs provide data-bus drive, INT, NMI, WAIT, reset, BUSREQ and interrupt enable input.
- The core supports memory and I/O cycles, refresh, HALT, WAIT and interrupt modes IM0, IM1 and IM2.
- `reset()` resets the CPU; `instructionComplete()` identifies the core's instruction-fetch boundary, without treating prefixes as complete instructions.

## Interface and Integration

The public API is in [Z80Bus.hpp](include/Z80Bus/Z80Bus.hpp). Signal booleans describe logical assertion, not voltage: `true` means an active-low signal is asserted. Service the outputs from the **previous** tick before advancing the CPU again. Drive read data only when a memory read, I/O read or interrupt acknowledge requires it.

This host-dependent example assumes `memory`, `readIo(address)` and `writeIo(address, data)` belong to the machine. Populate memory with a program before running it. The `0xFF` interrupt response is a placeholder; use the opcode or vector required by the machine.

```cpp
#include <Z80Bus/Z80Bus.hpp>

z80bus::Z80Bus cpu;
z80bus::Inputs inputs{};
z80bus::Outputs pins{};

for (;;) {
    inputs.driveDataBus = false;
    if (pins.ioRequest && pins.m1) {
        inputs.dataBus = 0xFF;
        inputs.driveDataBus = true;
    } else if (pins.memoryRequest && pins.read) {
        inputs.dataBus = memory[pins.address];
        inputs.driveDataBus = true;
    } else if (pins.ioRequest && pins.read) {
        inputs.dataBus = readIo(pins.address);
        inputs.driveDataBus = true;
    }
    if (pins.memoryRequest && pins.write) {
        memory[pins.address] = pins.dataBus;
    } else if (pins.ioRequest && pins.write) {
        writeIo(pins.address, pins.dataBus);
    }
    pins = cpu.tick(inputs);
}
```

The host also sets `intRequest`, `nmiRequest` and `wait` before each tick and determines how long to run. Device writes observed across several T-states must be handled according to the host device's bus protocol.

For integration into another Visual Studio solution, add [z80bus.vcxproj](z80bus.vcxproj), reference it from the host project and add this project's `include` directory to the host's include directories. There are no external library dependencies or downloads. The locally included [CPU core](third_party/chips/chips/z80.h) comes from [floooh/chips](https://github.com/floooh/chips); its zlib/libpng license and attribution are retained in [LICENSE](third_party/chips/LICENSE) and the header.

## Limits

- Ticks model T-state boundaries, not half-cycles, electrical edges, voltage, contention or setup/hold timing.
- BUSREQ/BUSACK is an adapter approximation: the CPU pauses at an instruction-fetch boundary and reports idle control outputs. Address and data retain their last values; no high-impedance bus release is modeled.
- Reset takes effect immediately through the core reset function, rather than reproducing reset-pin timing.
- Memory mapping, peripheral behavior and interrupt-vector selection are host responsibilities.

## In the SVI-318 and SVI-328

The Z80 runs at 3,579,545 Hz (the 21.477 MHz master clock divided by six) without wait states; the machine advances one T-state per `tick()` and services memory and I/O in the same T-state. Interrupt mode 1 is used by the BASIC ROM; INT combines the VDP frame interrupt with /INT of the expansion bus. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md) for the machine as a whole.

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln), [VG8235.sln](../../vg8235/Development/VG8235.sln), [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-728](../../svi728/Development/run_tests.ps1), [VG-8235](../../vg8235/Development/run_tests.ps1), [SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `z80bus` | Static library | `z80bus.lib` |
| `z80bus_tests` | Console application | `z80bus_tests.exe` |

## Tests

[z80bus_tests.cpp](tests/z80bus_tests.cpp) checks opcode/M1 and refresh cycles, memory writes, HALT, WAIT, maskable interrupt acknowledgement and the documented BUSREQ approximation. These are functional bus checks, not proof of electrical timing accuracy or exhaustive instruction conformance.
