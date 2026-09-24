//***********************************************************************************************
//* ADS1220 an ATMEGA8                                                                          *
//***********************************************************************************************
//* This sketch uses the ARDIuNO IDE LIB "ADS1220_WE" from Wolfgang Ewald as driver for a four  *
//* channel A/D converter on a RC2014 board                                                     *
//***********************************************************************************************
//* Datei: MSXADS1220.ino                                                                       *
//***********************************************************************************************
//* Changes:                                                                                    *
//* V1.0.0 - 01.09.2026 - base version                                                          *
//***********************************************************************************************

#include <Arduino.h>
#include <SPI.h>
#include <ADS1220_WE.h>
#include <elapsedMillis.h>

// Definition of pin usage

#define ADS1220_DRDY_PIN  A2
#define ADS1220_CS_PIN    10

#define SA0         (1 << PC0)   
#define SA1         (1 << PC1)
#define IORDY       (1 << PC4)
#define SPIINT      (1 << PC5)
#define SRD         (1 << PB0)
#define SPICS       (1 << PB1)

// Create ADS1220 object

ADS1220_WE ads = ADS1220_WE(ADS1220_CS_PIN, ADS1220_DRDY_PIN);

// Define cycle for hardware wait steering

elapsedMicros waitTime = 0;

// Define control signals

uint8_t addr        = 0;
uint8_t wr          = 0;
uint8_t dMeasType   = 0;
uint8_t rChannel    = 0;
uint8_t mChannel    = 0;
uint8_t mCount      = 0;
uint8_t dCmd        = 0;
uint8_t dCnt        = 0;
uint8_t data        = 0;
uint8_t index       = 0;

// A/D result

uint8_t mByte[4][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}};
float res           = 0.0;
float resval        = 0.0;
uint8_t exponent    = 0x46;

// Hard ceiling for resval before it reaches dtostrf(). dtostrf() has no
// notion of fbuf's size and will happily print past it for an extreme
// input -- clamping resval first guarantees the formatted string can
// never exceed "9999999.000" (11 chars), far inside the 32 byte buffer,
// regardless of what getVoltage_mV() ever returns

#define RESVAL_MAX 9999999.0

// Static formatting buffer instead of Arduino String -- the dynamic
// String class fragments/exhausts the ATmega8's 1 KB heap after a
// handful of conversions and silently returns garbage on failed
// allocations (observed on real hardware: indexOf() returning a
// value outside -1/0.. after a few measurements)

char fbuf[32];
char digits[6];
uint8_t dotpos      = 0;
uint8_t digitidx    = 0;

// SETUP

void setup() {

  // Start ADS1220

  if(!ads.init()){
    while(1);
  }

  ads.bypassPGA(true); 

  // PORT D as databus
  // Starts as input

  DDRD = 0;

  // Activate PullUp at PORT C

  PORTC = 0xFF;

  // Control signals

  DDRC &= ~SA0;
  DDRC &= ~SA1;
  DDRC |= IORDY;
  PORTC |= IORDY;
  DDRC |= SPIINT;
  PORTC &= ~SPIINT;
  DDRB &= ~SRD;
  DDRB &= ~SPICS;
  
}

//***********************************************************************************************
//* REGISTER MAP - MSX BUS SIDE (address = SA1:SA0, decoded in switch(addr))                    *
//*                                                                                             *
//* Address 0 - MODE                                                                            *
//*   WRITE  bit 7..4 = unused (NC)                                                             *
//*          bit 3..0 = measurement type                                                        *
//*                     0..3  single-ended AIN0..AIN3 vs GND, 4..9 differential AIN0-1/0-2/     *
//*                     0-3/1-2/1-3/2-3, 10..11 differential AIN1-0/AIN3-2, other -> AIN0/GND   *
//*          (sets mChannel = target result buffer for the selected measurement type)           *
//*   READ   always 0 (no readback)                                                             *
//*                                                                                             *
//* Address 1 - CMD                                                                             *
//*   WRITE  bit 7:6 = command   0 = select read channel, 1 = set mCount, 2 = start conversion, *
//*                              3 = unused                                                     *
//*          bit 5:2 = count     only evaluated by command 1 (mCount = bit 3:2)                 *
//*          bit 1:0 = channel   only evaluated by command 0 (rChannel = bit 1:0, mCount -> 0)  *
//*          command 2 (start conversion) ignores bit 5:0, triggers ads.getVoltage_mV() and     *
//*          writes the 4-byte MSX-float result into mByte[mChannel][0..3]                      *
//*   READ   bit 7:4 = 0, bit 3:2 = mCount, bit 1:0 = rChannel                                  *
//*                                                                                             *
//* Address 2 - unused (NC)                                                                     *
//*   WRITE  ignored                                                                            *
//*   READ   always 0                                                                           *
//*                                                                                             *
//* Address 3 - RESULT                                                                          *
//*   WRITE  ignored                                                                            *
//*   READ   mByte[rChannel][mCount], mCount auto-increments after each read, wraps 4 -> 0      *
//*                                                                                             *
//* MSX-float result bytes (mByte[mChannel][0..3], filled by CMD=2 / read back via addr 3)      *
//*   Byte 0  bit 7   = sign                                                                    *
//*           bit 6:0 = exponent (0x40 + decimal-point pos., clamped 0..63, see RESVAL_MAX)     *
//*   Byte 1  bit 7:4 = mantissa digit 1 (BCD), bit 3:0 = mantissa digit 2 (BCD)                *
//*   Byte 2  bit 7:4 = mantissa digit 3 (BCD), bit 3:0 = mantissa digit 4 (BCD)                *
//*   Byte 3  bit 7:4 = mantissa digit 5 (BCD), bit 3:0 = mantissa digit 6 (BCD)                *
//***********************************************************************************************

// LOOP()

void loop() {
  
  // Check, if SPI is selected
  
  if (PINB & SPICS) {

    // Take over /WAIT

    PORTC &= ~IORDY;

    // Start cycle measuring

    waitTime = 0;

    // Get direction RD = LOW, WR = HIGH

    wr = PINB & SRD;

    // Get address and analyse

    addr = PINC & 0x03;

    switch(addr){
      
      // MODE - Bits 7..4 = NC  Bits 3..0 = MeasureType

      case 0: 

              if (wr){

                data = PIND;
                dMeasType = (data & 0x0f);
                
                switch(dMeasType){
                  case 0: // SE AIN0 to GND
                    ads.setCompareChannels(ADS1220_MUX_0_AVSS);
                    mChannel = 0;  
                  break;
                  case 1: // SE AIN1 to GND
                    ads.setCompareChannels(ADS1220_MUX_1_AVSS);
                    mChannel = 1;  
                  break;
                  case 2: // SE AIN2 to GND
                    ads.setCompareChannels(ADS1220_MUX_2_AVSS);
                    mChannel = 2;  
                  break;
                  case 3: // SE AIN3 to GND
                    ads.setCompareChannels(ADS1220_MUX_3_AVSS);
                    mChannel = 3;  
                  break;                                    
                  case 4: // DE AIN0 to AIN1
                    ads.setCompareChannels(ADS1220_MUX_0_1);
                    mChannel = 0;  
                  break;
                  case 5: // DE AIN0 to AIN2
                    ads.setCompareChannels(ADS1220_MUX_0_2);
                    mChannel = 0;  
                  break;
                  case 6: // DE AIN0 to AIN3
                    ads.setCompareChannels(ADS1220_MUX_0_3);
                    mChannel = 0;  
                  break;
                  case 7: // DE AIN1 to AIN2
                    ads.setCompareChannels(ADS1220_MUX_1_2);
                    mChannel = 1;  
                  break;  
                  case 8: // DE AIN1 to AIN3
                    ads.setCompareChannels(ADS1220_MUX_1_3);
                    mChannel = 1;  
                  break;
                  case 9: // DE AIN2 to AIN3
                    ads.setCompareChannels(ADS1220_MUX_2_3);
                    mChannel = 2;  
                  break;
                  case 10: // DE AIN1 to AIN0
                    ads.setCompareChannels(ADS1220_MUX_1_0);
                    mChannel = 1;  
                  break;
                  case 11: // DE AIN3 to AIN2
                    ads.setCompareChannels(ADS1220_MUX_3_2);
                    mChannel = 3;  
                  break;                   
                  default:
                    ads.setCompareChannels(ADS1220_MUX_0_AVSS);
                    mChannel = 0;                  
                  break;
                }
              }
              else {

                DDRD = 0xff;  // PORT D output

                data = 0;
                
                PORTD = data;                

              }      

      break;

      // CMD - Bits 7..6 = CMD   Bits 5..2 = NC   Bits 1..0 = Channel

      case 1:

              if (wr){

                data = PIND;

                dCmd = (data & 0xc0) >> 6;
                dCnt = (data & 0x0c) >> 2;

                switch (dCmd){
            
                  case 0: // Set READ Channel
                    rChannel = (data & 0x03);
                    mCount = 0;
                  break;

                  case 1: // Reset mCount
                    mCount = dCnt & 0x03;
                  break;

                  case 2: // Start Conversion

                    res = ads.getVoltage_mV();

                    // Transfer Base 2 float to MSX Base 10 float
                    // MSX float unfortunatelly has BCD values as mantisse

                    resval = res;
                    if (resval < 0.0) resval*=(-1.0);
                    if (resval > RESVAL_MAX) resval = RESVAL_MAX;

                    // Format mantisse (fixed buffer, no heap allocation)

                    dtostrf(resval, 1, 3, fbuf);

                    // Format exponent from rounded buffer (position
                    // of the decimal point)

                    dotpos = 0;
                    while (fbuf[dotpos] != '.' && fbuf[dotpos] != 0) dotpos++;

                    // Hard limit: if dotpos were ever left unclamped, a
                    // large enough value could push exponent past 0x7F
                    // and silently wrap the sign bit (uint8_t overflow)
                    if (dotpos > 63) dotpos = 63;

                    exponent = 0x40 + dotpos;

                    // Set sign bit

                    if (res < 0.0) exponent|=0x80;

                    // Collect the 6 mantissa digits (skip the point),
                    // pad with trailing zeros if fewer than 6 remain

                    digits[0] = '0';
                    digits[1] = '0';
                    digits[2] = '0';
                    digits[3] = '0';
                    digits[4] = '0';
                    digits[5] = '0';

                    digitidx = 0;
                    for (index = 0; fbuf[index] != 0 && digitidx < 6; index++) {
                      if (fbuf[index] != '.') digits[digitidx++] = fbuf[index];
                    }

                    // Fill 4 byte MSX float buffer to read

                    mByte[mChannel][0] = exponent;
                    mByte[mChannel][1] = (byte(digits[0])-0x30)<<4;
                    mByte[mChannel][1] |= (byte(digits[1])-0x30);
                    mByte[mChannel][2] = (byte(digits[2])-0x30)<<4;
                    mByte[mChannel][2] |= (byte(digits[3])-0x30);
                    mByte[mChannel][3] = (byte(digits[4])-0x30)<<4;
                    mByte[mChannel][3] |= (byte(digits[5])-0x30);

                  break;

                  case 3:

                  break;

                  default:
                  break;
                }


         
              }
              else {

                DDRD = 0xff;  // PORT D output
 
                // Return read channel and current read buffer index

                data = rChannel | (mCount<<2);
                
                PORTD = data;                  

              }

      break;

      // NC
      
      case 2:

              if (wr){

                data = PIND; 

              }
              else {

                DDRD = 0xff;  // PORT D output

                data = 0;

                PORTD = data;                  

              }      

      break;

      // Read Results

      case 3:

              // Check for RD (0) or WR (1)
              
              if (wr){

                data = PIND;
 
              }
              else {

                DDRD = 0xff;  // PORT D output

                // Provide byte to MSX bus to be read
 
                PORTD = mByte[rChannel][mCount];
                if(++mCount==4) mCount = 0;
                
              } 

      break;

      default:

      break;
    }

    // Wait for MVFF to end (74123 has 4700 ns as impulse length) if still active

    while(waitTime<=5);
  
    // Release /WAIT to go on in program

    PORTC |= IORDY;

    // Wait 500 ns till /IORQ cycle has ended

    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
  
    // Port D as Input

    DDRD = 0;
    PORTD = 0;

  }

}

