# SVI3X8

SVI3X8 is the header-only interface of the 50-pin expansion connector shared by the Spectravideo SVI-318 and SVI-328. The machine drives it, and every expander and card in this folder implements it; both solutions ([SVI318.sln](../../svi318/Development/SVI318.sln), [SVI328.sln](../../svi328/Development/SVI328.sln)) use it, so the expanders and cards are the same for both machines.

## Functionality

- [ExpansionPort.hpp](include/SVI3X8/ExpansionPort.hpp): `svi3x8::ExpansionPort`, the device side of the connector: /RST (`reset()`), memory cycles with the bank lines /BK21, /BK22, /BK31 and /BK32 (`readMemory()`, `writeMemory()`), I/O cycles below port `80h` (`readIo()`, `writeIo()`), CPUCLK (`tick()`, one Z80 T-state) and /INT (`interruptRequest()`).
- `svi3x8::ExpansionMemoryCycle`: the data a device drives (`FFh` when none does), /ROMDIS, /RAMDIS and the stand-in for RAM bank 2 (`ramBank2`), which gives way to /RAMDIS like the internal RAM.
- `svi3x8::CpuClockHz`: CPUCLK, 3,579,545 Hz.

## Interface and Integration

The folder is on the include path of every project through [Common.props](../Common.props) (`$(Svi3x8Include)`); there is no library to link.

```cpp
#include <SVI3X8/ExpansionPort.hpp>

class MyCard final : public svi3x8::ExpansionPort {
    // reset(), readMemory(), writeMemory(), readIo(), writeIo(), tick(), interruptRequest()
};
```

The machine connects one device with `connectExpansion()`; an expander combines its cards (see [SVI605B](../SVI605B/README.md)).
