# Card E (SJA1000 CAN) - .COM tool

UNAPI/EXTBIOS port of `../basic/SJA1000.ASC` - calls `RCX_INTERFACE` and `XIO_IO_EXPANDER`
directly via EXTBIO discovery, no BASIC interpreter involved. IO address and PIC channel are
command-line parameters (`/IO:<0..254> /INT:<0..7>`) instead of interactive prompts.

## SJA1000.COM

Same Maerklin-CS3-style CAN example as `SJA1000.ASC`: receives and sends CAN messages
interrupt-driven, registering the ROM's own CAN service routine directly through `XIO SET ADDR`
- no custom interrupt code needed.
