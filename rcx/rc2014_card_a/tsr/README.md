# RTCLCDP — Installation on the target machine

A real MemMan 2.4 TSR (background program) that keeps the SEED I2C LCD showing the current time,
updated on every MemMan `H.TIMI` tick, after returning to MSX-DOS.

## One-time requirement: enlarge the MemMan heap

`RTCLCDP.TSR` requests about 1000 bytes from MemMan's page-3 heap on install (RCX/XIO work areas,
RTC/LCD buffers, the resident PIC interrupt routine). **The default MemMan heap is too small for
this** — without the step below, installation fails with
`MemMan HeapAlloc failed (out of page-3 heap)`.

Run once on the target system:

```
A>CFGMMAN
```

1. `2` — Modify MEMMAN.COM
2. In the submenu: `2` — Change heap size
3. Enter a new value: **at least 1200** (some margin above the ~1000 bytes actually needed)
4. `0` — Write MEMMAN to disk

This writes the new heap size permanently into the `MEMMAN.COM` file — it is a one-time step per
`MEMMAN.COM` copy, not something to repeat on every MSX boot.

## Install / start

```
A>MEMMAN _SYSTEM@ TL RTCLCDP.TSR /IO:x /INT:y@
```

(`x` = I2C card IO address, `y` = PIC interrupt channel). The space after `_SYSTEM@` is
intentional — without it, the first character of the next command is lost (this is MemMan's own
`@` command-chaining behaviour, not specific to RTCLCDP).

Plain `MEMMAN.COM` with no command-line argument always warm-boots back into BASIC once
installation is done (documented MemMan behaviour) — the `_SYSTEM@...@` form above avoids that
and stays in MSX-DOS.

`smemman.bat` and `rtclcdp.bat` are small convenience scripts for the two steps above (the
latter uses the example parameters `/IO:0 /INT:0` — adjust to match your actual card address and
interrupt channel).

## Files in this folder

| File | Purpose |
| --- | --- |
| `RTCLCDP.MAC` | source code (M80/Nestor80 syntax) |
| `RTCLCDP.REL` | Nestor80 intermediate output (relocatable) |
| `RTCLCDP.TSR` | finished TSR, installable with `TL` |
| `rel2tsr.py` | replacement for the no-longer-available `LT.COM` (builds `.TSR` from `.REL`) |
| `memman.com` / `memman.bin`, `tl.com`, `cfgmman.com` | the MemMan runtime files this TSR needs |
| `smemman.bat` | starts MemMan in system mode (`memman _system@`) |
| `rtclcdp.bat` | installs the TSR via `TL` with example `/IO:`/`/INT:` values |

## Status

Hardware-confirmed, works end-to-end: installation via `TL`, discovery, reading/displaying/
changing the RTC, initial LCD output, PIC interrupt registration, and — after returning to DOS —
periodic LCD updates while the system keeps running, driven by MemMan's `H.TIMI` hook.
