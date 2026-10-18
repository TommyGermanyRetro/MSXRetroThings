# MSX CAS and Cassette Emulation

## Sources

- [MSX Wiki: Emulation related file formats](https://www.msx.org/wiki/Emulation_related_file_formats): CAS marker, 8-byte alignment, missing timing information and format limits.
- [MSX Assembly Page: I/O ports](https://map.grauw.nl/resources/msx_io_ports.php): 8255 PC4/PC5 and PSG port A bit 7.
- [MSX Assembly Page: BIOS calls](https://map.grauw.nl/resources/msxbios.php): TAPION, TAPIN, TAPIOF, TAPOON, TAPOUT, TAPOOF and STMOTR.
- [MSX Assembly Page: System variables](https://map.grauw.nl/resources/msxsystemvars.php): CS120, CS240, LOW, HIGH and HEADER.
- [MSX Wiki: CSAVE](https://www.msx.org/wiki/CSAVE): tokenized BASIC programs, six-character tape file name, 1200/2400 baud.
- [MSX Wiki: BSAVE](https://www.msx.org/wiki/BSAVE): binary memory areas and start, end and execution address.
- [openMSX: CasImage.cc](https://raw.githubusercontent.com/openMSX/openMSX/master/src/cassette/CasImage.cc): cross-check of the 8N2 FSK coding and the file type detection; no code taken from it. openMSX plays back at an accelerated rate; this emulation rebuilds a conservative 1200 baud.

## File Layout

A CAS file holds decoded tape bytes, not audio samples. A block starts with:

```text
1F A6 DE BA CC 13 7D 74
```

The marker is recognized only at offsets divisible by eight. The payload follows up to the next aligned marker or the
end of the file. An image can hold several files and several data blocks. Markers are not bytes to send on the tape:
they stand for the leader before a block.

Normal BASIC files have a header of ten equal type bytes and six file name bytes:

| Type bytes | Contents |
| --- | --- |
| `D3` ten times | Tokenized BASIC, CSAVE/CLOAD |
| `EA` ten times | ASCII, SAVE/LOAD and sequential files |
| `D0` ten times | Binary data, BSAVE/BLOAD |

A separate data block follows. A binary data block starts with three little-endian words for the start, end and
execution address, followed by the memory bytes. ASCII can have several data blocks; there `1A` marks the end of the
file. The emulation does not interpret the payload as BASIC or a binary program; that is left to the ROM or the loaded
machine code.

CAS stores neither the baud rate, nor exact pauses, nor the difference between long and short leaders. The data cannot
contain an aligned marker sequence without being mistaken for a new block. Other codings and copy protection signals
cannot be represented in CAS without loss. SVI-318/328 CAS is a different format despite the same extension; the
SVI-728 is an MSX machine.

## CAS to Pins

`Cassette.hpp` recognizes aligned markers and rebuilds a signal in emulated CPU clocks. A file header, the first block or
one recognized by its ten type bytes, gets two seconds of pause and 8000 one bits; a data block one second and 2000 one
bits. This is a reconstruction convention, not an original timing read from the CAS.

Every data byte is sent as a zero start bit, eight data bits LSB first and two one stop bits. At 1200 baud a zero is one
full 1200 Hz period, a one two full 2400 Hz periods; both bit values last equally long. PSG port A bit 7 receives the
current level, while the lower bits stay with the joystick.

8255 port C bit 4 switches the motor, active low; a motor pin that is not driven counts as off. Motor off stops the tape
position and the pulse phase. Bit 5 is the write level. Direct port C writes and 8255 BSR commands both reach these pins.
The PSG direction bits are respected: R7 bit 6 = 0 enables input A.

## Pins to CAS

`CassetteRecorder.hpp` measures the time between PC5 edges only while the motor runs and recording is on. A leader of
short half waves calibrates the pulse length, so both 1200 and 2400 baud are decoded. A zero consists of two long half
waves, a one of four short ones. The decoder checks the start bit, eight data bits and both stop bits before it takes a
byte.

Motor off or the end of recording closes a signal section. Newly recognized leaders create new CAS blocks. Before a
further marker the previous block is padded with zero bytes to a multiple of eight; these padding bytes are a compromise
of the CAS format and remain part of the previous payload. The last block length need not be divisible by eight. BASIC
knows its file length or terminator; custom machine-code readers must know the amount of their payload themselves.

Empty, faulty or truncated recordings are not exported. A marker sequence inside the aligned payload is rejected as not
representable. Recording has a budget of 8 million edges per motor section and a CAS limit of 16 MiB; beyond that an
error is reported instead of silently cutting data.

There is no BIOS interception: BASIC and machine code produce the same hardware edges. Machine code with direct port
access works as long as it uses the MSX BIOS FSK coding; arbitrary turbo or alternative formats are not guaranteed as
CAS.

## Operation and Persistence

1. Choose **Cassette > New recording**.
2. Run the BASIC save command or the machine-code writer and wait until it has finished.
3. **Stop recording and save...** opens the save dialog.
4. To read it again, the recording is at the start of the tape afterwards; or insert a `.cas` with **Open image...**.

The recording stays in memory when the dialog is cancelled or writing the file fails. It is written first to a new
temporary file in the target folder; only after successful writing, flushing and closing is the target file replaced,
so earlier errors never damage the original. Before unsaved data is discarded, the emulator asks.

## Verification

- Synthetic CAS write pin sequences: 1200/2400 baud, several byte values, a truncated byte and a marker collision.
- Original SVI BIOS through Z80, 8255 and PSG: TAPION/TAPIN/TAPIOF and TAPOON/TAPOUT/TAPOOF, byte-exact export of file
  and data block at both write speeds, then reading back through the BIOS.
- Fully booted BASIC: CSAVE, NEW and CLOAD restore the complete tokenized program; BSAVE, clearing with POKE and BLOAD
  restore a binary memory area byte for byte.
