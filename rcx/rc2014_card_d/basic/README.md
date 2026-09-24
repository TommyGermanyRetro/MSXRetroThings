# Card D (82C54 Timer) - BASIC test programs

## 8254.ASC

Timer cascade test: Timer 0 and Timer 1 run in Mode 2 (rate generator), Timer 2 in Mode 0, all
three permanently gated on. Timer 0's output is externally wired to Timer 1's clock input, and
Timer 1's output to Timer 2's, forming a divider chain - Timer 2's live counter value is read
back and shown as hex on screen in an endless loop.

## 8254I.ASC

Same timer cascade as `8254.ASC`, plus Timer 1's output additionally wired to a PIC interrupt
channel: reports "TIMER1 fired" with a running interrupt count every time that edge is detected,
while the live Timer 2 counter display keeps running in its own screen area without scrolling.
