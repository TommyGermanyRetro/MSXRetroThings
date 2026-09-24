# xio_io - XIO_IO_EXPANDER UNAPI/EXTBIOS access (SDCC/MSXgl)

Precompiled SDCC library (`xio_io.h`/`xio_io.s`, assembled with `sdasz80` into `xio_io.rel` and
archived with `sdar` into `xio_io.lib`) providing `XIO_IO_EXPANDER` UNAPI/EXTBIOS access: EXTBIO
discovery followed by a segment-aware `CALSLT` dispatch for every call (ROM-slot only - there is
no mapped-RAM driver for `XIO_IO_EXPANDER`). Covers every `XIOROM.ASM` entry point except its
BASIC-only interactive help menu.

## Discovery

- `Xio_Discover()` - locates the driver via EXTBIO; on success sets `g_XSlot`/`g_XSeg`
  (`0xFF` for a ROM-resident driver)/`g_XCount` (implementation count from that call).
- `Xio_GetInfo()` - reads driver/API identification into `g_XRomVersion`/`g_XApiVersion`
  (high byte = primary, low byte = secondary version) and `g_XApiInfo` (zero-terminated ID
  string).

## Init/deinit

- `Xio_Init(u16 workarea, u16 intflags)` - binds a 97-byte scratch work area and a 2-byte
  interrupt-flag word (both caller-supplied, must be fixed page-3 addresses if any interrupt
  channel will be used - the MSX BIOS forces page 0 to system ROM while dispatching a hardware
  interrupt).
- `Xio_IsInit()` / `Xio_Deinit()` - query/tear down the current binding.

## Raw I/O

- `Xio_Out(u8 addr, u8 data)` / `Xio_Inp(u8 addr)` - direct byte read/write to the IO-Expander
  bus.

## Interrupt channels

- `Xio_SetAddr(u8 channel, u16 addr)` / `Xio_GetAddr(u8 channel, u16* outAddr)` - register/read
  the service routine address for one of the 8 interrupt channels (0..7); the low byte of
  `intflags` carries PIC channel bits, the high byte carries IM2 channel bits.
- `Xio_SetMask(u16 mask)` / `Xio_GetMask(u16* outMask)` - enable/read which channels are active,
  one bit per channel (bit 0..7 = PIC channels 0..7 in the mask's low byte, bit 8..15 = IM2
  channels 0..7 in the high byte).

## Rebuilding

```sh
cd MSXgl/lib/xio_io
sdasz80 -o -l -s xio_io.s
sdar -rc xio_io.lib xio_io.rel
```
