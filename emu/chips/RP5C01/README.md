# RP5C01

RP5C01 is a standalone C++17 static library modeling the Ricoh RP5C01 real-time clock with its battery-backed RAM, as used in MSX2 computers. Source: Ricoh Electronic Devices Data Book 1988 (excerpt in `9999 - MSX - Dokumentation`).

## Functionality

- Sixteen 4-bit registers addressed by A3-A0. The mode register (D) selects block 0 (time), block 1 (alarm, 12/24-hour selector, leap-year counter) or the RAM blocks 2 and 3 with 13 nibbles each.
- Unimplemented bits read 0, as listed in the "Address Modes" table. The test (E) and reset (F) registers are write-only.
- Time and calendar in BCD: seconds, minutes, hours in 12-hour (PM in D1 of the 10-hour register) or 24-hour mode, day of week, day, month and year. February has 29 days when the leap-year counter is 0; the counter advances with the year.
- Mode register bit 3 (timer enable) stops the count at the next second. Reset register bit 0 clears the alarm registers, bit 1 clears the divider below one second.
- `reset()` selects block 0 with the timer running and keeps time, alarm and RAM, like the battery-backed chip.
- `image()` and `loadImage()` exchange the 52 nibbles of all four blocks for a CMOS file kept by the host.
- `saveState()` and `loadState()` copy all registers and the time base for snapshots; data of another size is refused.

## Interface and Integration

The public API is in [RP5C01.hpp](include/RP5C01/RP5C01.hpp). The host latches the register number and calls `read(address)` and `write(address, value)` with 4-bit data. `advance(elapsed, clockHz)` feeds the time base with elapsed clocks of any known frequency; one second passes every `clockHz` clocks. `setDateTime()` and `dateTime()` convert between the registers and a `DateTime` in the current 12/24-hour mode.

```cpp
#include <RP5C01/RP5C01.hpp>

rp5c01::RP5C01 rtc;
rtc.setDateTime({26, 10, 10, 6, 14, 30, 0});
rtc.advance(3579545, 3579545);   // one second of a 3.58 MHz CPU clock
rtc.write(13, 0x0A);             // timer on, RAM block 2
rtc.write(0, 0x5);
```

For integration into another Visual Studio solution, add [rp5c01.vcxproj](rp5c01.vcxproj), reference it from the host project and add this project's `include` directory to the host's include directories. There are no external library dependencies.

## Limits

- The alarm and the 16 Hz/1 Hz outputs are stored but not generated; MSX machines leave these pins unconnected.
- The test register is stored without effect.
- Invalid BCD values count like their decimal value; the chip's behaviour for such values is not modeled.

## Build and Test

The project is used by the solution [VG8235.sln](../../vg8235/Development/VG8235.sln). Shared compiler settings are in [Common.props](../Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([VG-8235](../../vg8235/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `rp5c01` | Static library | `rp5c01.lib` |
| `rp5c01_tests` | Console application | `rp5c01_tests.exe` |

## Test Coverage

[rp5c01_tests.cpp](tests/rp5c01_tests.cpp) checks block selection, register masks, write-only registers, reset and image round trips, the year roll-over with the leap-year counter, February in leap and normal years, 30-day months, the 12-hour transitions at noon and midnight, the timer enable, and both reset bits.
