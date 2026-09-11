//***********************************************************************************************
//* 4x SPI at ATMEGA8                                                                           *
//***********************************************************************************************
//* This sketch uses an ATMEGA8 as driver for four seperate SPI busses with a speed up to 2 MHz *
//***********************************************************************************************
//* Datei: MSXSPI.ino                                                                           *
//***********************************************************************************************
//* Changes:                                                                                    *
//* V1.0.0 - 12.03.2026 - base version                                                          *
//***********************************************************************************************

#include <Arduino.h>
#include <SPI.h>
#include <elapsedMillis.h>

// Definition of pin usage

#define SA0         (1 << PC0)   
#define SA1         (1 << PC1)
#define SPIBUS0     (1 << PC2)
#define SPIBUS1     (1 << PC3)
#define SPISS       (1 << PB2)
#define SPIINT      (1 << PC5)
#define SRD         (1 << PB0)
#define SPICS       (1 << PB1)
#define IORDY       (1 << PC4)

// Define cycle for hardware wait steering

elapsedMicros waitTime = 0;

// Define SPI controls

long    freq[4]     = {2000000,1000000,500000,250000};
uint8_t mode[4]     = {SPI_MODE0,SPI_MODE1,SPI_MODE2,SPI_MODE3};

uint8_t mMsg[4]     = {0,0,0,0};
uint8_t mMode[4]    = {0,0,0,0};
uint8_t mClk[4]     = {0,0,0,0};
bool    mChange[4]  = {false,false,false,false};

// Define control signals

uint8_t activeBus   = 0;
uint8_t dMode       = 0;
uint8_t dClk        = 0;
uint8_t dBus        = 0;
uint8_t dCmd        = 0;
uint8_t data        = 0;
uint8_t addr        = 0;
uint8_t wr          = 0;

// SETUP()

void setup() {

  // Start SPI with standard settings

  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));

  // PORT D as databus
  // Starts as input

  DDRD = 0;

  // Activate PullUp at PORT C

  PORTC = 0xFF;

  // Control signals

  DDRC &= ~SA0;
  DDRC &= ~SA1;
  DDRC |= SPIBUS0;
  PORTC &= ~SPIBUS0;
  DDRC |= SPIBUS1;
  PORTC &= ~SPIBUS1;
  DDRC |= IORDY;
  PORTC |= IORDY;
  DDRC |= SPIINT;
  PORTC &= ~SPIINT;
  DDRB &= ~SRD;
  DDRB &= ~SPICS;
  DDRB |= SPISS;
  PORTB |= SPISS;
  
}

// LOOP()

void loop() {

  // Check, if SPI is selected
  
  if(PINB & SPICS) {

    // Take over /WAIT

    PORTC &= ~IORDY;

    // Start cycle measuring

    waitTime = 0;

    // Get direction RD = LOW, WR = HIGH

    wr = PINB & SRD;

    // Get address

    addr = PINC & 0x03;

    // Analyse address

    switch(addr){
      
      // SPI-MODE 1  Bits 7..6 = FREQ   Bits 5..4 = MODE   Bits 3..2 = NC   Bits 1..0 = BUS

      case 0: 

              if (wr){

                data = PIND; 

                dBus = data & 0x03;
                dMode = (data & 0x30) >> 4;
                dClk = (data & 0xc0) >> 6;

                if (dMode!=mMode[dBus]){
                  mMode[dBus] = dMode;
                  mChange[dBus] = true;
                }

                if (dClk!=mClk[dBus]){
                  mClk[dBus] = dClk;
                  mChange[dBus] = true;
                }    

              }
              else {

                DDRD = 0xff;  // PORT D output

                data = activeBus;
                data |= (mMode[activeBus] << 4);
                data |= (mClk[activeBus] << 6);
                
                PORTD = data;                

              }      

      break;

      // SPI-MODE 2   Bits 7..6 = CMD   Bits 5..2 = NC   Bits 1..0 = Bus

      case 1:

              if (wr){

                data = PIND; 
           
                dCmd = (data & 0xc0) >> 6;

                // Set active Bus
 
                if (dCmd == 0) activeBus = data & 0x03;
         
              }
              else {

                DDRD = 0xff;  // PORT D output

                data = activeBus;
                
                PORTD = data;                  

              }

      break;

      // SPI /CS on/off
      
      case 2:

              if (wr){

                data = PIND; 

                if (data > 0){
                  PORTB |= SPISS;
                  SPI_CS_Set(activeBus);
                  PORTB &= ~SPISS;
                }
                else{
                  PORTB |= SPISS; 
                }

              }
              else {

                DDRD = 0xff;  // PORT D output

                data = (PINB & SPISS) >> 2;

                PORTD = data;                  

              }      

      break;

      // SPI-TRANSFER

      case 3:

              // SPI Settings changed?
              // Then restart SPI-Bus

              if (mChange[activeBus]){
                SPI.begin();
                SPI.beginTransaction(SPISettings(freq[mClk[activeBus]], MSBFIRST, mode[mMode[activeBus]]));
                mChange[activeBus] = false;
              }

              // Check for RD (0) or WR (1)
              
              if (wr){

                data = PIND; 

                SPI.transfer(data);
 
              }
              else {

                DDRD = 0xff;  // PORT D output
 
                data = SPI.transfer(0);

                PORTD = data;
                
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

// CS routine on/off

void SPI_CS_Set(uint8_t cs){

    switch(cs){
      case 0:   PORTC &= ~SPIBUS0;
                PORTC &= ~SPIBUS1;
      break;
      case 1:   PORTC |= SPIBUS0;
                PORTC &= ~SPIBUS1;
      break;
      case 2:   PORTC &= ~SPIBUS0;
                PORTC |= SPIBUS1;
      break;
      case 3:   PORTC |= SPIBUS0;
                PORTC |= SPIBUS1;
      break;
      default:
                PORTC &= ~SPIBUS0;
                PORTC &= ~SPIBUS1;
      break;
    }

}