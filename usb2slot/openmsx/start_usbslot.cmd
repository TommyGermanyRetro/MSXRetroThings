@echo off
rem *************************************************************************************************
rem * start_usbslot.cmd - erkennt die Belegung der Bridge und startet den selbst gebauten openMSX   *
rem *                                                                                               *
rem * Aufruf:  start_usbslot.cmd [maschine] [--port COMx] [--bridge 1|2|12] [openMSX-Parameter]     *
rem * Vorgabe: C-BIOS_MSX2+_JP. Die Erkennung und die passenden Erweiterungen (USBSlotAuto1,        *
rem * USBSlotAuto2, USBSlotAutoIO) uebernimmt start_usbslot.py; Port --port, USBSLOT_PORT, COM16    *
rem *************************************************************************************************
python "%~dp0start_usbslot.py" %*
