# VG-8235 Emulator

Emulator of the Philips VG 8235/00 (MSX2, PAL) with V9938, disk, real-time clock, cassette, modules and printer. The project folder has three areas:

| Area | Contents |
| --- | --- |
| [Development/](Development/README.md) | Development: solution `VG8235.sln`, machine and frontend with ROMs and tests, `run_tests.ps1`, installer source (`installer/`, `create_installation.ps1`), function tree (`create_callgraph.ps1` writes `Doxygen_index.html`); details in the [developer guide](Development/README.md) |
| `Build/` | Build: compiler output (`x64\<configuration>`, `obj`) and the startable `vg8235.exe` with `vg8235.ini` and `vg8235.cmos` |
| [Distribution/](Distribution/MANUAL.md) | Distribution: `VG8235-Setup.exe` (with ROM images) and `VG8235-Setup-NoRoms.exe` (without), [INSTALLATION.txt](Distribution/INSTALLATION.txt) and the user manual [MANUAL.md](Distribution/MANUAL.md), which the setup installs |
| [chips](../chips/README.md), [msx](../msx/README.md) | Shared components: the chips (with all emulators) and the cartridge slot (with the SVI-728); built from the solution in `Development` |

## System Requirements

Windows 10 or 11 (64-bit), a 64-bit dual core from about the Intel Core 4th generation or AMD Ryzen 1000 (four cores for video recording), 4 GB of memory and a screen of at least 1920 x 1080 at 100 % scaling. Details and measured values: [MANUAL.md, System Requirements](Distribution/MANUAL.md#system-requirements).

## Quick Start

In a Developer PowerShell in the folder `Development`:

```powershell
msbuild VG8235.sln /p:Configuration=Release /p:Platform=x64   # to Build\x64\Release, program copied to Build
.\run_tests.ps1 -Configuration Release                         # all test programs
.\create_callgraph.ps1                                          # function tree, entry page Doxygen_index.html
.\create_installation.ps1                                       # Distribution\VG8235-Setup.exe (-NoRoms: without ROMs)
..\Build\vg8235.exe                                              # start the emulator
```

Started from `Build`, the emulator takes the ROMs from `Development`, the manual (menu **?**) from `Distribution` and the
media from `2100 - MSX - MEDIEN`. Installed, the manual lies next to the program; media and ROM images are in
`Documents\VG-8235`.
