# Card A (I2C/RTC) - .COM tools

UNAPI/EXTBIOS ports of the BASIC test programs (see `../basic/`) - call `RCX_INTERFACE` directly
via EXTBIO discovery, no BASIC interpreter involved.

## RTCLCD.COM

Same behaviour as `RTCLCD.ASC`: discovers `RCX_INTERFACE`, reads/optionally changes the PCF8583
RTC, writes date and time to the SEED I2C LCD. IO address is a command-line parameter
(`/IO:<0..254>`) instead of an interactive prompt.

## RTCLCDI.COM

Same as `RTCLCD.COM`, plus interrupt-driven LCD refresh: also discovers `XIO_IO_EXPANDER`,
registers a PIC channel (`/INT:<0..7>`), and keeps the display updated automatically on every
interrupt while the program runs (`<SPACE>` to exit).
