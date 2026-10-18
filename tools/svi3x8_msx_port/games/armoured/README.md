# Armoured Assault for MSX

MSX version of the two-player tank game Armoured Assault (Spectravideo, 1983) for the SVI-318/328. The
recipe builds an MSX cassette and a plain 16 KiB cartridge (page 1, `4000h`-`7FFFh`, no mapper) from the
original cassette `Armoured Assaut (1983)(Spectravideo).cas` (file name as in the TOSEC set). The cassette
version loads with `BLOAD"CAS:",R` on the SVI-728 and other MSX computers with 64 KB of RAM (about 2.5
minutes at 1200 baud).

```powershell
python port.py armoured --cas "Armoured Assaut (1983)(Spectravideo).cas"
```

## Parts

| File | Content |
| --- | --- |
| [recipe.py](recipe.py) | Takes the memory image from the cassette, applies the patch table, builds cassette and ROM |
| [ARHELP.ASM](ARHELP.ASM) | Helper block in page 0: interrupt entry as in the SVI ROM, SVI keyboard rows from the MSX matrix (`SVIROW`), both joysticks (`SVIR14`) and both triggers (`SVIPA`) |
| [../../templates/BOOT.ASM](../../templates/BOOT.ASM) | Boot code at `B800h` of the cassette version: copies the game to `C000h`, switches all four pages to the RAM slot of page 3 (secondary slot included), installs the helper block at `0038h`, selects joystick port 1 and jumps to `C000h` |
| [../../templates/ROM16.ASM](../../templates/ROM16.ASM) | Cartridge startup at `4010h`: copies the embedded game image to `C000h`, switches to the RAM slot of page 3 and starts the game |

**Memory image:** the `GAME` block of the cassette holds the image the SVI loader copies to `C000h`-`F7FFh`:
block offsets `0000h`-`2FFFh` become `C000h`-`EFFFh`, offsets `3080h`-`387Fh` become `F000h`-`F7FFh`.

**Hardware changes** (patch table, every site with its expected original bytes):

| SVI-318/328 | MSX |
| --- | --- |
| VDP `OUT (80h)`/`OUT (81h)`, `IN (84h)`/`IN (85h)`, `LD C,80h`/`LD C,81h` for `OUT (C)` and `OUTI` | `98h`/`99h` |
| PSG `OUT (88h)`, `OUT (8Ch)`, `IN (90h)`, `LD C,88h` | `A0h`, `A1h`, `A2h` |
| PPI control word `OUT (97h)` | removed, the MSX PPI stays in its BIOS mode |
| keyboard `OUT (96h),A` + `IN A,(99h)` | `CALL 0040h` (`SVIROW`) |
| both joysticks from PSG register 14 (`C123h`) | `CALL 0046h` (`SVIR14`): reads MSX ports 1 and 2 one after the other, joystick 1 in bits 0-3, joystick 2 in bits 4-7 |
| both triggers from PPI port A (`C13Ah`) | `CALL 0049h` (`SVIPA`): trigger 1 in bit 4, trigger 2 in bit 5, stored in `F8C5h` |
| `LD SP,0000h` | `LD SP,FFFEh`, so that the stack does not write the secondary slot register `FFFFh` |

The game calls no ROM routines. It runs in the VDP interrupt: the entry at `0038h` saves HL, DE, BC and AF
and calls `C085h`, exactly as the SVI interrupt routine calls the hook `FE7Dh` set by the game. The MSX BIOS
is not used after the start.

**Controls:** digits `1`-`8` choose the game in the menu. Player 1 drives with joystick 1 or the cursor keys
and fires with trigger A of joystick 1 or the space bar. Player 2 uses joystick 2 with trigger A. `ESC`
pauses, `CTRL`+`STOP` returns to the menu.
