# SVI-328 Emulator

Emulator of the Spectravideo SVI-328 (64 KiB RAM, BASIC 1.11, PAL or NTSC) with cassette, modules, the expanders SVI-601, SVI-602, SVI-605B (also with hard disk), SVI-608M and SVI-609 and their cards. The project folder has three areas:

| Area | Contents |
| --- | --- |
| [Development/](Development/README.md) | Development: solution `SVI328.sln`, machine and frontend with BASIC ROM, tests of the machine and of the cards (`CardTests`), `run_tests.ps1`, installer source (`installer/`, `create_installation.ps1`), function tree (`create_callgraph.ps1` writes `Doxygen_index.html`); details in the [developer guide](Development/README.md) |
| `Build/` | Build: compiler output (`x64\<configuration>`, `obj`) and the startable `svi328.exe` with `svi328.ini` |
| [Distribution/](Distribution/MANUAL.md) | Distribution: `SVI328-Setup.exe` (with ROM images) and `SVI328-Setup-NoRoms.exe` (without), [INSTALLATION.txt](Distribution/INSTALLATION.txt) and the user manual [MANUAL.md](Distribution/MANUAL.md), which the setup installs |
| [svi3x8](../svi3x8/README.md), [chips](../chips/README.md) | Shared with the SVI-318: expansion interface, expanders, cards and card ROMs, and the chips; built from the solution in `Development` |

## System Requirements

Windows 10 or 11 (64-bit), a 64-bit dual core from about the Intel Core 4th generation or AMD Ryzen 1000 (four cores for video recording), 4 GB of memory and a screen of at least 1920 x 1080 at 100 % scaling. Details and measured values: [MANUAL.md, System Requirements](Distribution/MANUAL.md#system-requirements).

## Quick Start

In a Developer PowerShell in the folder `Development`:

```powershell
msbuild SVI328.sln /p:Configuration=Release /p:Platform=x64   # to Build\x64\Release, program copied to Build
.\run_tests.ps1 -Configuration Release                         # all test programs
.\create_callgraph.ps1                                          # function tree, entry page Doxygen_index.html
.\create_installation.ps1                                       # Distribution\SVI328-Setup.exe (-NoRoms: without ROMs)
..\Build\svi328.exe                                              # start the emulator
```

Started from `Build`, the emulator takes the BASIC ROM from `Development` and the card ROMs from `svi3x8`, the manual (menu **?**) from `Distribution` and the
media from `2100 - MSX - MEDIEN`. Installed, the manual lies next to the program; media and ROM images are in
`Documents\SVI-328`.
