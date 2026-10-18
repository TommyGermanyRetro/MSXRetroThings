# svi3x8

Shared components of the emulators [SVI-318](../svi318/README.md) and
[SVI-328](../svi328/README.md): the interface of the 50-pin expansion connector, the expanders and
the cards with their ROMs. Both machines have the same connector, so every component exists only once. The chips are
in [chips](../chips/README.md).

Nothing is built here: the solutions of the machines (`Development\SVI318.sln`, `Development\SVI328.sln`) build the
components, each into its own `Build` folder. The tests of the expanders and cards boot the machine and are therefore
in each machine's `Development\CardTests`. The SVI-609 is an SVI-328 and is used only by its solution.
[Common.props](Common.props) sets the include paths and builds on the settings of `chips`.

| Component | Contents |
| --- | --- |
| [LANHD01/](LANHD01/README.md) | LAN-HD-01 SASI host adapter of the SVI-608M and SVI-609 (built into the SVI-605B with hard disk) |
| [LANIF01/](LANIF01/README.md) | LAN-IF-01: boot card of the SVI-608M and LAN card of the SVI-609 (boot logic, printer PIA at `60h`-`63h`) with `svi609.rom` |
| [SVI3X8/](SVI3X8/README.md) | Interface of the expansion connector (`svi3x8::ExpansionPort`), shared by the SVI-318 and SVI-328, their expanders and cards; header only |
| [SVI601/](SVI601/README.md) | SVI-601 Super Expander: seven slots, disk through the SVI-801 controller in SK6, printer through the SVI-802 |
| [SVI602/](SVI602/README.md) | SVI-602 Mini Expander: one slot for an interface cartridge, for example the SVI-801 |
| [SVI605B/](SVI605B/README.md) | SVI-605B Super Expander: expander main board (card slots, disk unit from SVI801, Centronics interface from SVI802), four slots and two drives, tests with Disk BASIC and CP/M 2.24 |
| [SVI605BHD/](SVI605BHD/README.md) | SVI-605B with hard disk on the main board of the SVI-605B: four slots, drive A only, built-in SASI adapter LAN-HD-01 with ST-212, no boot ROM; test with CP/M 2.27 |
| [SVI608M/](SVI608M/README.md) | SVI-608M Super Expander on the main board of the SVI-605B: five slots, one drive, boot source detection, factory cards SVI-806, LAN-HD-01 and LAN-IF-01, tests with CP/M 2.27 |
| [SVI609/](SVI609/README.md) | Network Expander of the SVI-609 Master Station (on SVI608M): factory cards SVI-806, SVI-805, SVI-807, LAN-HD-01 and LAN card; test with CP/M 2.27 and printing through the PIA of the LAN card |
| [SVI801/](SVI801/README.md) | SVI-801 disk controller with two SVI-902 drives; disk unit and disk images of the expanders |
| [SVI802/](SVI802/README.md) | SVI-802 Centronics interface; printer port of the expanders |
| [SVI803/](SVI803/README.md) | SVI-803 16 KiB RAM card |
| [SVI805/](SVI805/README.md) | SVI-805 RS-232 card on a Windows COM port |
| [SVI806/](SVI806/README.md) | SVI-806 80-column card with `svi806.rom` |
| [SVI807/](SVI807/README.md) | SVI-807 64 KiB RAM card |
