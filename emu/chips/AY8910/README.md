# AY-3-8910

AY8910 is a standalone C++17 static library modeling the General Instrument AY-3-8910 programmable sound generator and, as a model option, the Yamaha YM2149. It provides three separate audio channels and two bidirectional GPIO ports for integration into a software machine.

## Functionality

- Sixteen registers with register selection and AY-3-8910 read/write masks.
- Three tone generators, a noise generator with a 17-bit LFSR and an envelope generator with the envelope shape rules: 16 steps per cycle for the AY-3-8910, 32 steps at twice the step rate for the YM2149.
- YM2149 output through a 32-level logarithmic table; a fixed level L selects step 2L+1.
- Per-channel tone/noise gating and fixed-volume or envelope-controlled output.
- Two 8-bit GPIO ports. Register 7 bit 6 controls port A and bit 7 controls port B: `0` selects input, `1` selects output. The CPU reads the pins: an output port reads its register ANDed with the external levels (open-collector outputs), so a joystick contact still reads 0 while software has switched port A to output, as the SVI game Chess does. `setFixedPortDirections(true)` keeps port A an input and port B an output, as wired in MSX engines; port B then reads its register.
- `render(count, sampleRate)` advances sound generation and returns three independent, nonnegative DAC-level samples stored as `std::int16_t` values.
- A configurable master clock, defaulting to 1,773,400 Hz.

## Interface and Integration

The public API is in [AY8910.hpp](include/AY8910/AY8910.hpp). Use `selectRegister()`, `writeData()` and `readData()` for register access. `writeAddressData(false, value)` selects a register; `writeAddressData(true, value)` writes its data, and `readAddressData()` reads the selected register. `registerValue()` and `selectedRegister()` expose register state.

This self-contained function generates a block of channel-A tone samples; the caller must mix, filter and send the returned channels to an audio device.

```cpp
#include <AY8910/AY8910.hpp>

std::vector<ay8910::Sample> makeToneBlock()
{
	ay8910::AY8910 chip;
	chip.selectRegister(0);
	chip.writeData(100);
	chip.selectRegister(1);
	chip.writeData(0);
	chip.selectRegister(7);
	chip.writeData(0x3E);
	chip.selectRegister(8);
	chip.writeData(15);
	return chip.render(4410, 44100);
}
```

Keep the same chip instance across blocks for continuous playback. The host schedules register writes relative to rendered audio. `setPortInputs(portA, portB)` supplies external GPIO values; `ports()` returns effective port values and drive masks. A zero drive mask means the port is not driving external pins. `reset()` clears chip state while preserving the configured clock.

For integration into another Visual Studio solution, add [ay8910.vcxproj](ay8910.vcxproj), reference it from the host project and add this project's `include` directory to the host's include directories. There are no external library dependencies.

## Limits

- Audio is a functional sample model, not an electrical or transistor simulation.
- The logarithmic volume table is an approximation, not a calibrated response for a particular chip or board.
- Analog mixing, filters, DC removal, channel crosstalk and load resistors belong to the host.
- Register access does not model electrical bus timing. Retaining three channels does not by itself reproduce a machine's analog output circuit.

## In the SVI-318 and SVI-328

The PSG runs at 1,789,773 Hz (half the CPU clock). Port `88h` selects a register, `8Ch` writes data and `90h` reads it. Port A reads the joystick directions (bits 0-3 joystick 1, bits 4-7 joystick 2); port B, an output, selects the memory banks (/CART, /BK21, /BK22, /BK31, /BK32, Caps LED, /ROMEN0, /ROMEN1). The frontend mixes the three channels with the keyboard click into 44.1 kHz mono sound. See the user manuals of the [SVI-318](../../svi318/Distribution/MANUAL.md) and [SVI-328](../../svi328/Distribution/MANUAL.md).

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln), [VG8235.sln](../../vg8235/Development/VG8235.sln), [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-728](../../svi728/Development/run_tests.ps1), [VG-8235](../../vg8235/Development/run_tests.ps1), [SVI-318](../../svi318/Development/run_tests.ps1), [SVI-328](../../svi328/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `ay8910` | Static library | `ay8910.lib` |
| `ay8910_tests` | Console application | `ay8910_tests.exe` |

## Tests

[ay8910_tests.cpp](tests/ay8910_tests.cpp) checks register masks and selection, tone generation and mixer gating, changing noise output, a rising envelope, GPIO directions and drive masks, reset and clock preservation, zero-rate silence and render-block continuity, plus the YM2149 envelope rate and 32 steps, the YM2149 level mapping and fixed port directions. The tests do not exhaustively verify every envelope shape or calibrate analog output.
