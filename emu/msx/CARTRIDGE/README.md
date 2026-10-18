# CARTRIDGE

CARTRIDGE is a standalone C++17 static library modeling MSX ROM cartridges with the common bank-switching mappers. The mapper behaviour and the detection rule follow the observable behaviour of openMSX.

## Functionality

| Mapper | Banks | Registers |
| --- | --- | --- |
| None | plain ROM up to 64 KiB, placed by its "AB" header (page 2 for BASIC ROMs with a page 2 entry, page 1 otherwise, #0000 above 48 KiB) | – |
| ASCII8 | 4 × 8 KiB at #4000/#6000/#8000/#A000 | #6000, #6800, #7000, #7800 |
| ASCII16 | 2 × 16 KiB at #4000/#8000 | #6000-#67FF, #7000-#77FF |
| Konami | 8 KiB at #6000/#8000/#A000, #4000 fixed to bank 0 | #6000, #8000, #A000 |
| Konami SCC | 4 × 8 KiB at #4000-#BFFF | #5000, #7000, #9000, #B000 (each #x000-#x7FF) |

- Bank numbers wrap at the ROM size; `reset()` selects the power-on banks (bank 0, or banks 0-3 for the Konami mappers).
- Unmapped regions read FFh.
- `detectMapper()`: ROMs up to 64 KiB are plain. Larger ROMs are classified by counting `LD (nnnn),A` instructions that write to bank registers; ties go to ASCII16, then ASCII8, Konami and Konami SCC.
- `saveState()` and `loadState()` keep the bank registers for snapshots, with mapper, base address and ROM size of the image; `loadState()` refuses data saved with another image or mapper. The ROM itself is not part of the state.

## Interface and Integration

The public API is in [Cartridge.hpp](include/CARTRIDGE/Cartridge.hpp). The host forwards CPU memory reads and writes of the cartridge slot to `read()` and `write()`.

```cpp
#include <CARTRIDGE/Cartridge.hpp>

cartridge::Cartridge slot1;
slot1.insert(image, cartridge::detectMapper(image));
const std::uint8_t value = slot1.read(0x4000);
slot1.write(0x6000, 2);              // ASCII16: bank 2 into #4000-#7FFF
```

For integration into another Visual Studio solution, add [cartridge.vcxproj](cartridge.vcxproj), reference it from the host project and add this project's `include` directory to the host's include directories. There are no external library dependencies.

## Limits

- The detection is a heuristic. Some ROMs need the mapper set by hand; the Nextor kernel ROM (one stray write to #A000) is detected as Konami and must be inserted as ASCII16.
- The SCC sound chip of Konami SCC cartridges is not emulated; only its bank switching is.
- Cartridges with SRAM, other mappers (e.g. ASCII8/16 with SRAM, R-Type, Konami DAC) and extra hardware are not supported.

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln) and [VG8235.sln](../../vg8235/Development/VG8235.sln). Settings are in [Common.props](../Common.props), on top of the chip settings in [chips](../../chips/Common.props): Visual Studio 2022 (MSVC v143), C++17 and C11, `/W4 /permissive- /utf-8` with the narrowing warning C4244 disabled, the static runtime library and the configurations `Debug|x64` and `Release|x64`. Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests ([SVI-728](../../svi728/Development/run_tests.ps1), [VG-8235](../../vg8235/Development/run_tests.ps1)).

| Project | Type | Output |
| --- | --- | --- |
| `cartridge` | Static library | `cartridge.lib` |
| `cartridge_tests` | Console application | `cartridge_tests.exe` |

## Test Coverage

[cartridge_tests.cpp](tests/cartridge_tests.cpp) checks the mapper detection for every mapper, plain ROM placement and limits, the bank registers of ASCII8, ASCII16, Konami and Konami SCC, bank wrapping, reset and empty slots.
