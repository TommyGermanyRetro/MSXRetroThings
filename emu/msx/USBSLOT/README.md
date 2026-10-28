# USBSLOT

USBSLOT is a C++17 static library that connects an emulated MSX to the USB2SLOT board: a Teensy 4.1 with two MSX
cartridge slots that runs real Z80 bus cycles on real cartridges. The emulated machine forwards the bus cycles of a
module slot to the board, so the CPU reads and writes the real cartridge.

## Functionality

- `openSerial()` opens the board on a COM port, switches it to its binary protocol and checks protocol version 2.
- `Bridge` sends memory read and write cycles for slot 1 or 2 of the board, with /M1 for opcode fetches, and I/O
  read and write cycles; read and I/O cycles report the clocks the cartridge added with /WAIT.
- `Bridge::reset()` wakes the board and pulses /RESET. `Bridge::heartbeat()` keeps the board running; bus cycles
  carry a heartbeat along when the last one is more than 20 ms old, so long stretches of board cycles never let the
  board switch the cartridges off.
- After a failed transfer the connection counts as lost: reads return FFh, nothing is sent any more.
- `CycleTracker` finds the first T-state of a bus cycle. The Z80 repeats a T-state with the same request while /WAIT
  is low (for example the MSX wait state of every M1 cycle); the board must see each cycle only once.
- `Transport` is the interface to the board; tests replace the serial implementation.
- `BoardSlot` is slot 1 or 2 of the board as a module slot of the machine, with a read cache of 8 KiB windows: the
  first read of a 256-byte line fetches the whole line, further reads cost no USB transfer. A write marks only its
  byte as stale (RAM of a Memory Mapper or MegaRAM takes the value at once); a stale byte is read on the board
  again. The cache follows the switching of the cartridge through `Rules`: its own slot expander (FFFFh, answered
  from the register), a ROM bank register, Memory Mapper segments (ports), MegaRAM banks, and areas without cache.
  Every mapper segment, MegaRAM bank and ROM bank has its own window image.
- `detectBoard()` probes the board like the start script of the USB2SLOT project: slot expander, ROM, data or
  empty per (sub)slot, switched I/O ID C0h at port 40h, Memory Mapper answers at FCh-FFh. It returns the rules,
  the I/O ports to forward and a description per slot. Known profile: the SD Mapper/Megaram 512K (Nextor bank
  register at 6000h, SD interface 4000h-4800h without cache, Memory Mapper or MegaRAM in subslot 3).

The serial implementation uses `slotlink.h`, the PC library of the USB2SLOT project, unchanged (Win32 API only).

## Interface and Integration

The public API is in [UsbSlot.hpp](include/USBSLOT/UsbSlot.hpp) and [BoardSlot.hpp](include/USBSLOT/BoardSlot.hpp).
The machine keeps a `CycleTracker` for memory reads, memory writes, I/O reads and I/O writes, feeds them in every
T-state and hands a cycle to the board slot only when it begins. I/O ports that a board slot claims go to the board,
and every board slot on that board follows them:

```cpp
#include <USBSLOT/BoardSlot.hpp>

std::string error;
usbslot::Bridge bridge(usbslot::openSerial("COM16", error));
const auto detection = usbslot::detectBoard(bridge);
usbslot::BoardSlot slot2(bridge, 2, detection.slots[1]);

// per T-state, for an address that the slot selection maps to the board slot
if (readCycle.begins(pins.memoryRequest && pins.read, pins.address)) {
    std::uint32_t waits = 0;
    data = slot2.read(pins.address, pins.m1, waits);
}
// I/O write cycle on a port the board slot claims
if (slot2.claimsPort(port)) {
    bridge.writeIo(port, value, waits);
    slot2.ioWritten(port, value);
}
```

For integration into another Visual Studio solution, add [usbslot.vcxproj](usbslot.vcxproj), reference it from the
host project and add this project's `include` directory to the host's include directories.

## Limits

- Windows only (Win32 serial port).
- Every cycle is a USB round trip of about 100 microseconds; the emulation slows down accordingly while the CPU
  runs code from the board.
- Write cycles are sent without answer; /WAIT of a memory write is not reported.
- Cached reads never reach the board: a cartridge sees /M1 only in areas without cache.
- Cartridges with an unknown expander or mapper run without switching rules; after a bank switch the cache may
  then show stale contents.

## Build and Test

The project is shared by the solutions [SVI728.sln](../../svi728/Development/SVI728.sln) and
[VG8235.sln](../../vg8235/Development/VG8235.sln). Settings
are in [Common.props](../Common.props), on top of the chip settings in
[chips](../../chips/Common.props).

| Project | Type | Output |
| --- | --- | --- |
| `usbslot` | Static library | `usbslot.lib` |
| `usbslot_tests` | Console application | `usbslot_tests.exe` |

## Test Coverage

[usbslot_tests.cpp](tests/usbslot_tests.cpp) uses a recording transport and checks the forwarding of memory and
I/O cycles with slot, address, /M1 and wait clocks, the heartbeat before the /RESET pulse and along with cycles,
the behaviour after a lost connection and the cycle detection of `CycleTracker`. A simulated board (ROM cartridge,
SD Mapper with expander, Nextor banks and 512 KiB RAM as Memory Mapper or MegaRAM) checks the detection, line
fetches, uncached areas, stale bytes, the expander register, ROM banks, mapper segments and MegaRAM banks.
