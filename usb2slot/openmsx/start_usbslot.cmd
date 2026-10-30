@echo off
rem *************************************************************************************************
rem * start_usbslot.cmd - detects what is plugged into the bridge and starts the self-built openMSX *
rem *                                                                                               *
rem * Usage:   start_usbslot.cmd [machine] [--port COMx] [--bridge 1|2|12] [openMSX parameters]     *
rem * Default: C-BIOS_MSX2+_JP. Detection and the matching extensions (USBSlotAuto1, USBSlotAuto2,  *
rem * USBSlotAutoIO) are handled by start_usbslot.py; port: --port, USBSLOT_PORT, COM16             *
rem *************************************************************************************************
python "%~dp0start_usbslot.py" %*
