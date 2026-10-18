# Spectron for MSX

MSX version of Spectron (Spectravideo, 1983) for the SVI-318/328. The recipe builds an MSX cassette and a
plain 16 KiB cartridge (page 1, `4000h`-`7FFFh`, no mapper) from the original cassette
`Spectron (1983)(Spectravideo).cas`. The cassette version loads with `BLOAD"CAS:",R` on the SVI-728 and
other MSX computers with 64 KB of RAM (about 2.5 minutes at 1200 baud).

```powershell
python port.py spectron --cas "Spectron (1983)(Spectravideo).cas"
```

## Parts

| File | Content |
| --- | --- |
| [recipe.py](recipe.py) | Takes the memory image from the cassette, applies the patch table, builds cassette and ROM |
| [SPHELP.ASM](SPHELP.ASM) | Helper block in page 0: interrupt entry as in the SVI ROM, SVI keyboard rows from the MSX matrix (`SVIROW`), trigger of joystick 1 (`SVITRIG`) |
| [../../templates/BOOT.ASM](../../templates/BOOT.ASM) | Boot code at `B800h` of the cassette version: copies the game to `C000h`, switches all four pages to the RAM slot of page 3 (secondary slot included), installs the helper block at `0038h`, selects joystick port 1 and jumps to `C000h` |
| [../../templates/ROM16.ASM](../../templates/ROM16.ASM) | Cartridge startup at `4010h`: copies the embedded game image to `C000h`, switches to the RAM slot of page 3 and starts the game; afterwards the game runs at `C000h` independently of the cartridge slot |

**Memory image:** the `GAME` block of the cassette holds the image the SVI loader copies to `C000h`-`F7FFh`:
block offsets `0000h`-`2FFFh` become `C000h`-`EFFFh`, offsets `3080h`-`387Fh` become `F000h`-`F7FFh`.

**Hardware changes** (patch table, every site with its expected original bytes):

| SVI-318/328 | MSX |
| --- | --- |
| VDP `OUT (80h)`/`OUT (81h)`, `IN (84h)`/`IN (85h)`, `LD C,81h` | `98h`/`99h` |
| PSG `OUT (88h)`, `OUT (8Ch)`, `IN (90h)` | `A0h`, `A1h`, `A2h`; joystick 1 through PSG register 14 as on the SVI |
| PPI control word `OUT (97h)` | removed, the MSX PPI stays in its BIOS mode |
| keyboard `OUT (96h),A` + `IN A,(99h)` | `CALL 0040h` (`SVIROW`) |
| trigger `IN A,(98h)` + `BIT 4,A` | `CALL 0043h` (`SVITRIG`) |
| `LD SP,0000h` | `LD SP,FFFEh`, so that the stack does not write the secondary slot register `FFFFh` |

The game calls no ROM routines. It runs entirely in the VDP interrupt: the entry at `0038h` saves HL, DE,
BC and AF and calls `C52Dh`, exactly as the SVI interrupt routine calls the hook `FE7Dh` set by the game. The
MSX BIOS is not used after the start.

**Controls:** keys as on the SVI-328 on the MSX keys of the same name: digits `1`-`8` choose difficulty and
number of players in the menu, cursor left/right moves, space fires, `ESC` pauses, `CTRL`+`STOP` returns to
the menu. Joystick 1 with trigger A works as well.
