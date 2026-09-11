# Card A (I2C/RTC) - BASIC test programs

## RTCLCD.ASC

Reads the on-board PCF8583 RTC, optionally lets the user change date/time interactively, then
writes both to a SEED I2C LCD:

- Discovers/installs the `RCX_INTERFACE` UNAPI driver, asks for the card's IO address.
- `GET TIME`/`GET DATE` read the current time and date, printed to the screen.
- Interactively asks whether to change date and/or time (validated `DD/MM/YY`/`HH:MM:SS` input),
  writes any change back via `SET TIME`/`SET DATE`.
- Runs the HD44780 SEED LCD init sequence (function set, clear display, display/cursor/blink on)
  and writes the (possibly just-changed) date on line 1, time on line 2, via `I2C WRB`.

## RTCLCDI.ASC

Same as `RTCLCD.ASC`, plus interrupt-driven display updates: installs the XIO driver alongside
RCX, registers a PIC channel, and refreshes the LCD's date/time automatically on every interrupt
instead of only once at startup.
