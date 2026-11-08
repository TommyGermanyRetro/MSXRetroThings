# Card H (SC725) - BASIC test programs

Need the XIO cartridge and the RCX ROM. The channels to test are listed in a `DATA` line (count, then 0 = port A, 1 = port B). Test setup on the header of each tested port: wire bridge TXD (pin 5) to RXD (pin 4); for `COMHS` also /RTS (pin 2) to /CTS (pin 6).

## COMTST.ASC

No handshake: for each channel at 1200, 9600 and 19200 baud `CALL COMINI("n:8N1NNNNN",baud,,2)`, `OPEN` without `FOR`, all 256 byte values sent in 8 blocks of 32 with `PRINT #` and read back with `INPUT$`. PASS if all blocks match and nothing is left (`LOC` 0).

## COMHS.ASC

CTS-RTS handshake at 9600 baud with `CALL COMINI("n:8N1NHNNN",9600,,1)`: characters are sent one by one without reading. RTS goes off below 16 free characters, so the sender stops with a time out (error 19) after about 108 characters - LOC equals the number sent, the data is complete, no overflow (`CALL COMSTAT`). Then the rest is sent and read back.
