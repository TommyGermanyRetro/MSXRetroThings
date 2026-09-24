# rcx_io - RCX_INTERFACE UNAPI/EXTBIOS access (SDCC/MSXgl)

Precompiled SDCC library (`rcx_io.h`/`rcx_io.s`, assembled with `sdasz80` into `rcx_io.rel` and
archived with `sdar` into `rcx_io.lib`) providing `RCX_INTERFACE` UNAPI/EXTBIOS access: dual
dispatch per call (`SEG=0xFF` -> direct `CALSLT` for a ROM cartridge, `SEG<>0xFF` -> a
self-modifying-trampoline `CALL` through the mapped-RAM driver's own RAM helper, restoring the C
caller's `IX`/`IY` afterward either way). Covers the full 62-function `RCX_INTERFACE` surface
(`FN_CORE2`-`62`, i.e. every function except `FN_CORE1`'s BASIC-only help menu), grouped by
peripheral. Card address (and, for I2C, the chip address; for SPI, the bus number) is passed
explicitly on **every** call, matching the BASIC/UNAPI original exactly - up to 16 cards of most
types can be installed at once, so nothing about which card an operation targets is cached
anywhere in this library.

Functions with 3 or more logical parameters exceed `sdcccall(1)`'s 2-argument register limit and
use `__sdcccall(0)` instead (a real stack-based call - see each function below): `I2c_Write`/
`I2c_Wrb`/`I2c_Rdb`, `Can_Write`, `Spi_Write`/`Spi_Wrb`/`Spi_Rdb`/`Spi_Rdh`/`Spi_Mode`, `Ads_Get`.
Every other function fits `sdcccall(1)`'s default register convention.

## Discovery

- `Rcx_Discover(void)` - locates the driver via EXTBIO; on success sets `g_RSlot`/`g_RSeg`
  (`0xFF` for a ROM-resident driver, else the mapped-RAM segment)/`g_RCount` (implementation
  count from that call).
- `Rcx_GetInfo(void)` - `FN_INFO` (0), does not require `Rcx_Init()` first; sets
  `g_RRomVersion`/`g_RApiVersion` (high byte = primary, low byte = secondary version) and
  `g_RApiInfo` (zero-terminated driver ID string).

## RCX core (FN_CORE2-5)

- `Rcx_Init(u16 workarea)` - binds a caller-supplied RAM work area for the driver's own
  bookkeeping.
- `Rcx_IsInit(void)` - queries whether the driver is currently initialized.
- `Rcx_Deinit(void)` - tears down the current binding.
- `Rcx_GetTable(u16* outAddr)` - address of the RCX table of installed cards.

## I2C / PCF8584 (FN_CORE6-11)

- `I2c_Init(u8 addr)` - registers the card's IO base address in the RCX table.
- `I2c_Write(u8 addr, u8 chip, u8 data)` - writes one byte to `chip` on the bus at `addr`.
- `I2c_Wrb(u8 addr, u8 chip, u8 len, void* data)` - writes a `len`-byte buffer to `chip`.
- `I2c_Read(u8 addr, u8 chip)` - reads one byte from `chip`.
- `I2c_Rdb(u8 addr, u8 chip, u8 len, void* data)` - reads a `len`-byte buffer from `chip`.
- `I2c_Reset(u8 addr)` - resets the I2C bus.

## RTC / PCF8583 (FN_CORE12-15)

`addr` is the same I2C bus address as the group above; date/time are 8-byte ASCII buffers
(`DD/MM/YY`, `HH:MM:SS`).

- `Rtc_SetTime(u8 addr, void* time)` / `Rtc_GetTime(u8 addr, void* time)` - write/read the time.
- `Rtc_SetDate(u8 addr, void* date)` / `Rtc_GetDate(u8 addr, void* date)` - write/read the date.

## Z80 PIO (FN_CORE16-22)

- `Pio_Init(u8 addr)` - registers the card's IO base address in the RCX table.
- `Pio_ReadA(u8 addr)` / `Pio_WriteA(u8 addr, u8 value)` - read/write Port A data.
- `Pio_ReadB(u8 addr)` / `Pio_WriteB(u8 addr, u8 value)` - read/write Port B data.
- `Pio_CtrlA(u8 addr, u8 value)` / `Pio_CtrlB(u8 addr, u8 value)` - write Port A/B's
  mode/direction control byte.

## 82C55 PPI (FN_CORE23-32)

- `Ppi_Init(u8 addr)` - registers the card's IO base address in the RCX table.
- `Ppi_ReadA(u8 addr)` / `Ppi_WriteA(u8 addr, u8 value)` - read/write Port A data.
- `Ppi_ReadB(u8 addr)` / `Ppi_WriteB(u8 addr, u8 value)` - read/write Port B data.
- `Ppi_ReadC(u8 addr)` / `Ppi_WriteC(u8 addr, u8 value)` - read/write Port C data.
- `Ppi_Ctrl(u8 addr, u8 value)` - writes the PPI mode-control byte.
- `Ppi_SetBit(u8 addr, u8 bit)` / `Ppi_ResetBit(u8 addr, u8 bit)` - set/clear a single Port C bit
  (0..7) via the 8255's bit-set/reset mode.

## 82C54 timer (FN_CORE33-42)

- `Timer_Init(u8 addr)` - registers the card's IO base address in the RCX table.
- `Timer_Read0(u8 addr)` / `Timer_Write0(u8 addr, u16 value)` - read/write counter 0's 16-bit
  value.
- `Timer_Read1(u8 addr)` / `Timer_Write1(u8 addr, u16 value)` - read/write counter 1's 16-bit
  value.
- `Timer_Read2(u8 addr)` / `Timer_Write2(u8 addr, u16 value)` - read/write counter 2's 16-bit
  value.
- `Timer_Ctrl(u8 addr, u16 value)` - writes the mode-control word.
- `Timer_SetGate(u8 addr, u16 value)` / `Timer_ResetGate(u8 addr, u16 value)` - set/clear the
  gate input.

## SJA1000 CAN (FN_CORE43-49)

- `Can_Init(u8 addr, void* acc)` - registers the card's IO base address in the RCX table; `acc`
  is an 8-word acceptance code/mask buffer.
- `Can_Write(u8 addr, u8 reg, u8 data)` **sdcccall(0)** / `Can_Read(u8 addr, u8 reg)` - raw
  SJA1000 register write/read.
- `Can_Check(u8 addr)` - returns 1 if a new RX message is pending, 0 otherwise.
- `Can_Rx(u8 addr, void* data)` / `Can_Tx(u8 addr, void* data)` - receive/send a 13-word CAN
  message buffer.
- `Can_GetIntAddr(u8 addr)` - address of the driver's own CAN interrupt service routine, meant to
  be registered as an interrupt handler for a chosen channel.

## SPI (FN_CORE50-56)

- `Spi_Init(u8 addr)` - registers the card's IO base address in the RCX table.
- `Spi_Write(u8 addr, u8 bus, u8 data)` **sdcccall(0)** / `Spi_Wrb(u8 addr, u8 bus, u8 len, void*
  data)` **sdcccall(0)** - write one byte / a `len`-byte buffer on `bus`.
- `Spi_Read(u8 addr, u8 bus)` / `Spi_Rdb(u8 addr, u8 bus, u8 len, void* data)` **sdcccall(0)** -
  read one byte / a `len`-byte buffer from `bus`.
- `Spi_Rdh(u8 addr, u8 bus, u16 lenHeaderData, void* data)` **sdcccall(0)** - combined
  header+data transfer; `lenHeaderData` packs the header length in the high byte and the data
  length in the low byte.
- `Spi_Mode(u8 addr, u8 bus, u8 mode, u8 freq)` **sdcccall(0)** - sets the SPI mode and clock
  frequency on `bus`.

## ADS1220 (FN_CORE57-62)

- `Ads_Init(u8 addr)` - registers the card's IO base address in the RCX table.
- `Ads_SetMode(u8 addr, u8 type)` - selects the conversion type (`0..11`).
- `Ads_Cmd(u8 addr, u8 val)` - sends a raw ADS1220 command byte.
- `Ads_GetStatus(u8 addr)` - reads the status byte.
- `Ads_Read(u8 addr)` - reads one raw RESULT byte.
- `Ads_Get(u8 addr, u8 type, void* val)` **sdcccall(0)** - reads a full conversion result into a
  4-byte SNG (MSX single-precision float) for the given `type`.

## Rebuilding

```sh
cd MSXgl/lib/rcx_io
sdasz80 -o -l -s rcx_io.s
sdar -rc rcx_io.lib rcx_io.rel
```
