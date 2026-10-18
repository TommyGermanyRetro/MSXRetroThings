# Donkey Kong for MSX

MSX version of the SVI-318/328 cassette Donkey Kong (Buggy Software), itself a conversion of the
ColecoVision version. The recipe builds a 64 KiB cartridge with ASCII16 mapper from the original cassette
`Donkey Kong (19xx)(Buggy Software).cas`. The game needs 64 KB of RAM.

The mapper is not detected automatically at 64 KiB; insert the ROM as ASCII16 (in the SVI-728 and VG-8235 emulators:
**Modules > Slot n: insert as > ASCII16**).

```powershell
python port.py donkey --cas "Donkey Kong (19xx)(Buggy Software).cas"
```

## Parts

| File | Content |
| --- | --- |
| [recipe.py](recipe.py) | Decrypts the memory image from the cassette, applies the patch table, assembles and writes the ROM; generates `DKROWS.INC` from the keyboard table `KEYMAP` (one routine per SVI row that reads each needed MSX row once) |
| [DKROM.ASM](DKROM.ASM) | Bank 0 with `AB` header; the initialization switches page 2 to the ROM, determines the slot settings and starts the copy code |
| [DKCOPY.ASM](DKCOPY.ASM) | Copy code at `C000h`: copies banks 1-3 into RAM at `0000h`, `4000h` and `8000h`, sets VDP register 1 and the stack as the SVI loader does and jumps to `3745h` |
| [DKHELP.ASM](DKHELP.ASM) | Helper block at `F000h`: SVI keyboard rows from the MSX matrix (`SVIROW`, with a buffer read anew every 32 calls), both joysticks (`SVIJOY`) and triggers (`TRIG4`, `TRIG5`) |

ROM layout: bank 0 startup code, banks 1-3 the game for `0000h`, `4000h` and `8000h`.

**Memory image:** the SVI version loads three BLOAD files to `9000h`, copies them to `0000h`, `4000h` and
`8000h`, decrypts `0000h`-`BFFFh` (`XOR 5Dh`) and jumps to `3745h`. There the game copies `4000h`-`7FFFh` to
`C000h`-`FFFFh`. `0000h`-`3FFFh` is a BIOS replacement for the SVI hardware; input and output are collected
at `E0E5h`-`E76Fh`. The helper block sits at `7000h` in the image and arrives at `F000h` with that copy, an
area the game does not use.

**Hardware changes** (patch table, every site with its expected original bytes):

| SVI-318/328 | MSX |
| --- | --- |
| VDP `OUT (80h)`/`(81h)`, `IN (84h)`/`(85h)`, `LD C,80h`/`84h` for `OUTI`/`INI` | `98h`/`99h` |
| PSG `OUT (88h)`, `OUT (8Ch)`, `IN (90h)` | `A0h`, `A1h`, `A2h` |
| PSG register 7 = `FCh` | `BCh`, port A stays input |
| keyboard `OUT (96h),A` + `IN A,(99h)` | `CALL F000h` (`SVIROW`) |
| joysticks 1 and 2 in PSG register 14 | `CALL F003h` (`SVIJOY`), ports selected through PSG register 15 |
| triggers `IN A,(98h)` + `BIT 4/5,A` | `CALL F006h`/`F009h` (`TRIG4`/`TRIG5`) |
| copy `4000h`-`7FFFh` to `C000h`-`FFFFh` | without the last byte, so that `FFFFh` (secondary slot register) stays untouched |

**Controls:** in the options screen `1`-`8` choose difficulty and number of players. Cursor keys and space
bar or joystick 1 (player 2: joystick 2). As in the SVI version the picture freezes briefly after a hit;
there is no dying animation and no "GAME OVER", after the last life the picture stays.
