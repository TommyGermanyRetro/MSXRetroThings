# msx

Shared components of the MSX emulators [SVI-728](../svi728/README.md) (MSX1) and
[VG-8235](../vg8235/README.md) (MSX2) that are not chips. The chips are in
[chips](../chips/README.md), the components of the Spectravideo SVI-318/328 in
[svi3x8](../svi3x8/README.md).

The solutions of the machines (`Development\SVI728.sln`, `Development\VG8235.sln`) build the components, each into its
own `Build` folder, and their `run_tests.ps1` runs the tests. [Common.props](Common.props) sets the include paths and
builds on the settings of `chips`.

| Component | Contents |
| --- | --- |
| [CARTRIDGE/](CARTRIDGE/README.md) | Cartridge slot: ROM modules without mapper and ASCII8, ASCII16, Konami and Konami SCC (without SCC sound), mapper detection |
| [USBSLOT/](USBSLOT/README.md) | USB2SLOT board: real cartridges in a module slot, bus cycles over USB |
