# Card E (SJA1000 CAN) - BASIC test program

## SJA1000.ASC

Full CAN bus example for a Maerklin CS3 bus: installs `XIO_IO_EXPANDER` and `RCX_INTERFACE`,
sets the SJA1000's acceptance code/mask and PIC interrupt channel, then registers the ROM's own
CAN interrupt service routine via `XIO SET ADDR` (`CAN GET ADDR`) - no custom Z80 interrupt code
needed. Receives and decodes incoming CAN messages interrupt-driven, and can send messages back
onto the bus, all shown on screen (`<SPACE>` to exit, cleans up via `CAN`/`XIO DEINIT`).
