//***********************************************************************************************
//* ADS1220 an ATMEGA8                                                                          *
//***********************************************************************************************
//* This sketch uses the ARDIuNO IDE LIB "ADS1220_WE" from Wolfgang Ewald as driver for a four  *
//* channel A/D converter on a RC2014 board                                                     *
//***********************************************************************************************
//* Datei: MSXADS1220.ino                                                                       *
//***********************************************************************************************
//* Changes:                                                                                    *
//* V1.0.0 - 12.03.2026 - base version                                                          *
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
String anfang;
String ende;
String buffer;
uint8_t iLength     = 0;

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
                    mCount = dCnt;
                  break;

                  case 2: // Start Conversion
                    
                    res = ads.getVoltage_mV();

                    // Transfer Base 2 float to MSX Base 10 float
                    // MSX float unfortunatelly has BCD values as mantisse

                    // Format exponent

                    resval = res;
                    if (resval < 0.0) resval*=(-1.0);
                    exponent = 0x46;
                    if (resval < 100000.0) exponent = 0x45;
                    if (resval < 10000.0) exponent = 0x44;
                    if (resval < 1000.0) exponent = 0x43;
                    if (resval < 100.0) exponent = 0x42;
                    if (resval < 10.0) exponent = 0x41;

                    // Set sign bit
                   
                    if (res < 0.0) exponent|=0x80;

                    // Format mantisse
                   
                    buffer = String(resval,3);

                    iLength = buffer.length();
 
                    if (buffer.indexOf('.') != -1) {
                     anfang = buffer.substring(0,buffer.indexOf('.'));
                     ende = buffer.substring(buffer.indexOf('.')+1,iLength);
                     buffer = anfang+ende;
                    }
                 
                    iLength = buffer.length();

                    for (index = 0;index < (6-iLength); index++) buffer += "0";

                    // Fill 4 byte MSX float buffer to read

                    mByte[mChannel][0] = exponent;
                    mByte[mChannel][1] = (byte(buffer.charAt(0))-0x30)<<4;
                    mByte[mChannel][1] |= (byte(buffer.charAt(1))-0x30);
                    mByte[mChannel][2] = (byte(buffer.charAt(2))-0x30)<<4;
                    mByte[mChannel][2] |= (byte(buffer.charAt(3))-0x30);
                    mByte[mChannel][3] = (byte(buffer.charAt(4))-0x30)<<4;
                    mByte[mChannel][3] |= (byte(buffer.charAt(5))-0x30);

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

