# Cartridge profiles for USB2SLOT

A recipe for running cartridges with their own logic (internal slot expander, bank registers, memory mapper,
MegaRAM, SPI or FIFO windows) fast and correctly over the bridge. It applies to openMSX (`openmsx/USBSlot.cc`,
the extensions, `openmsx/start_usbslot.py`) and to the emulators of this repository (component
[`emu/msx/USBSLOT`](../emu/msx/USBSLOT)).

Worked example: SD Mapper/Megaram 512K (FBLabs, CPLD source `msxsdmapper/CPLD/src`).

## 1. Why profiles are needed

Every memory access that does not come from the cache is a USB round trip (median about 100 µs). The cache may
only be used where reading has no side effect, and what a cartridge shows at an address depends on its registers.
Without a profile the only choices are "cache everything" (wrong data) and "cache nothing" (about 100 times
slower).

A profile therefore describes:

- how the cartridge is recognised without doubt
- which registers switch the memory map (bank, segment, switch)
- which areas have side effects on reading (SPI, FIFO, status, counters) and must never be cached
- which timing conditions apply between accesses (for example the minimum spacing of an SPI port)
- where repeated accesses can be combined into one block

## 2. Get the sources

Use documented facts only, never guess from behaviour:

1. Schematic or CPLD/FPGA source (VHDL, Verilog, JED with equations): address decoding, registers, switches,
   state machines, clocks.
2. Driver or firmware of the cartridge (for example the Nextor driver in a ROM bank): how and how fast the
   software accesses the hardware.
3. Measurements over the bridge (section 5).

SD Mapper: `sdmapper.vhd` (bank register 6000h, switch 6001h, SPI window 4000h-47FFh and 4800h) and `sd.vhd`
(SPI state machine: each read of the data port returns the previous byte and starts the next transfer; 8 SPI
clocks take 16 to 19 clocks; a request during a running transfer is lost).

## 3. Recognise the cartridge without doubt

Detection (`detectBoard()` in the component, `slottest scan` plus `start_usbslot.py` for openMSX) may only assign a
profile when all features match. Otherwise the cartridge is a plain slot expander or cartridge with the standard
rules.

Suitable features, read-only or with registers set back to their reset value right away:

- slot expander present (FFFFh reads back the complement of what was written)
- contents per subslot (ROM "AB", data, empty)
- signatures in a given ROM bank (select the bank briefly, then back to the reset value)
- answers of I/O ports (for example a memory mapper after /RESET: E0h-E3h)

SD Mapper: slot expander, ROM in subslot 1, data in subslot 3, and in ROM bank 7 at 4100h the Nextor driver
header "NEXTOR_DRIVER" with the name "SDMapper Driver" at 4110h (bank selected with 6000h = 7, then 6000h = 0).

## 4. Rules

Available rules (per subslot):

| Rule (component / openMSX XML) | Effect |
| --- | --- |
| `noCache` / `<nocache>` | area is always read on the cartridge |
| `switchAddress` + `noCacheWhenSwitched` / `<nocacheswitch>` | area is uncached only while bit 0 of the last write to the switch address is set |
| `bank*` / `<bank>` | a write to the address selects the ROM bank of a page, one cache image per bank |
| `mapper` / `<mapper>` | memory mapper switched by ports, writes go through to the cache |
| `megaram` / `<megaram>` | MegaRAM with four 8 KB bank registers |
| `spiBlock*` / `<spiblock>` | block read from an SPI window with a fixed clock spacing |
| `romMapper` / `<rommapper>` | ROM mapper of a MegaROM (Konami, Konami SCC with its SCC area, ASCII8, ASCII16), one cache image per 8 KB bank |

Add a new rule only when no existing one fits, and then in all three places at once (component, openMSX,
detection), with a test.

Principles:

- reads with side effects are never done ahead and never as a whole cache line
- writes to uncached areas leave the cache alone
- switch and bank registers are evaluated on write; the write still goes to the cartridge
- a reset of the cartridge sets all switches to their state after /RESET according to the source

## 5. Measure

1. Measure time and transfers without a user interface. Wrap the `Transport` of the component (or the link of
   `USBSlot`) and count every transfer by type and address range. The frontend is never faster than the core.
2. Find the busiest address ranges and the instructions that read there (program counter at the read).
3. Read the code at these addresses from the ROM bank (`slottest dump`).
4. Compare with the hardware source: which accesses have side effects, which timing conditions apply.

SD Mapper: with a fixed uncached range 4000h-4800h the Nextor driver code there ran uncached (`call system`:
206 346 round trips, 23.4 s). With `nocacheswitch` it took 31 068 round trips and 6.0 s. `dir` still made about
120 000 single SPI reads from `LD HL,4000h` followed by 512 `LDI` (bank 7 at 5A73h, a second chain for multiple
blocks at 5628h) and took 16.3 s.

## 6. Block reads with clock spacing (protocol 3)

Reads with side effects can be combined when

- it is certain how many consecutive accesses the software will make, and
- the bridge runs them with the spacing the hardware needs.

`MRDNG` (0Eh: slot, address, count, spacing in clocks) reads consecutive addresses with a fixed spacing from the
start of one cycle to the next. The firmware generates the clock itself, so the spacing matches a real machine.
The PC side uses `MRDNG` only with protocol 3 or newer; with older firmware every byte stays a single access.

The trigger must be unambiguous, and the length must never be guessed: every byte read ahead that the software
does not take shifts the data stream. The SD Mapper driver reads with unrolled `LDI` chains: 512 `LDI` for a
sector and 16 `LDI` for a card register (bank 7 at 4BEAh, followed by two `LD A,(HL)` at 4010h for the CRC). A
fixed block "4001h-41FFh" was therefore wrong: it broke off at every register read (54 times during `dir`).

Method: when an `LDI` from the cartridge's ROM reads 4001h through the switched-on SPI port, the emulator counts
the `ED A0` right behind this instruction in the cached ROM. That is how many bytes the software will still read:
511 or 15. Exactly these are fetched with `MRDNG`, 24 clocks apart (one SPI byte takes about 19 clocks according
to `sd.vhd`, `LDI` on an MSX 18 clocks; more spacing is always safe for the SPI state machine). If the instruction
is no `LDI` of this device or the code is not in the cache, the access stays a single read. If the CPU still
deviates (another address in the window, the switch), the buffer is dropped and counted as broken off.

Each emulator finds the running instruction its own way: the component remembers the last instruction fetch
from this slot (the A0h of the `LDI`); openMSX gives the PC, which during the memory access still points to that
A0h.

Result for the SD Mapper (VG-8235 over the bridge, protocol 3): `dir` in the repository's emulator 16.3 s → 5.5 s
(single reads 128 528 → 8 413, 351 blocks, none broken off), in openMSX 14.9 s → 4.0 s; start to the DOS prompt
13.4 s → 10.7 s and 8.8 s → 5.8 s. Emulated time and directory contents unchanged. Write test: write 4096 bytes,
cold start, read back and compare gave 0 differences, so collected writes into the SPI window need no spacing of
their own on this cartridge.

## 7. Check

- tests with a simulated cartridge (`SimulatedBoard` in `emu/msx/USBSLOT/tests/usbslot_tests.cpp`): detection
  with and without the signature, switch, block read, deviation
- measure before and after: the emulated time must stay the same, only the real time drops
- check reading and writing on the cartridge (storage media: copy a file, read it back after a cold start,
  compare), including writes into areas with timing conditions
- the same measurement in openMSX

## 8. Checklist for a new profile

1. Get and keep the sources (section 2).
2. Choose the features for unambiguous detection (section 3).
3. Note registers, switches, windows with side effects and timing conditions from the source.
4. Measure without a profile (section 5): where do the round trips go?
5. Choose the rules; add a new rule in all three places only when needed.
6. Write tests with a simulated cartridge.
7. Measure and check reading and writing on the real cartridge (section 7).
8. Update the READMEs and this document with the new example.
