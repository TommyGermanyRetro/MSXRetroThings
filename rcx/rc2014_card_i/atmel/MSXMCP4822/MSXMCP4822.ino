//***********************************************************************************************
//* MCP4822 an ATMEGA8                                                                          *
//***********************************************************************************************
//* This sketch drives two MCP4822 dual 12 bit D/A converters (four analog outputs) on a RC2014 *
//* board via SPI                                                                               *
//***********************************************************************************************
//* File:  MSXMCP4822.ino                                                                       *
//***********************************************************************************************
//* Changes:                                                                                    *
//* V1.0.0 - 02.11.2026 - base version                                                          *
//***********************************************************************************************

#include <Arduino.h>
#include <SPI.h>
#include <elapsedMillis.h>

// Definition of pin usage

#define SA0         (1 << PC0)
#define SA1         (1 << PC1)
#define DACCS1      (1 << PC2)
#define DACCS2      (1 << PC3)
#define IORDY       (1 << PC4)
#define SPIINT      (1 << PC5)
#define SRD         (1 << PB0)
#define SPICS       (1 << PB1)
#define DACLDAC     (1 << PB2)

// MCP4822 command word bits (bit 11..0 = value)

#define MCP_CHB     0x8000
#define MCP_GA1     0x2000
#define MCP_ACTIVE  0x1000

// Channel configuration bits (CONFIG register)

#define CONF_GAIN2  0x01
#define CONF_SHDN   0x02

// VALUE HIGH bit: keep /LDAC inactive after the transfer

#define HIGH_HOLD   0x40

// Define cycle for hardware wait steering

elapsedMicros waitTime = 0;

// Define control signals

uint8_t addr        = 0;
uint8_t wr          = 0;
uint8_t data        = 0;
uint8_t index       = 0;

// D/A channels: 0 = IC103 A, 1 = IC103 B, 2 = IC104 A, 3 = IC104 B

uint8_t  dChannel   = 0;
uint8_t  dConf[4]   = {CONF_GAIN2, CONF_GAIN2, CONF_GAIN2, CONF_GAIN2};
uint16_t dValue[4]  = {0, 0, 0, 0};

// Send value and configuration of one channel to its MCP4822 (SPI mode 0, 8 MHz)

static void writeDac(uint8_t channel){
  uint16_t word;
  uint8_t cs;

  word = dValue[channel] & 0x0FFF;
  if (channel & 0x01)                  word |= MCP_CHB;
  if (!(dConf[channel] & CONF_GAIN2))  word |= MCP_GA1;
  if (!(dConf[channel] & CONF_SHDN))   word |= MCP_ACTIVE;

  cs = (channel & 0x02) ? DACCS2 : DACCS1;

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  PORTC &= ~cs;
  SPI.transfer(word >> 8);
  SPI.transfer(word & 0xFF);
  PORTC |= cs;
  SPI.endTransaction();
}

// Transfer the input registers of both MCP4822 to their outputs

static void pulseLdac(){
  PORTB &= ~DACLDAC;
  __asm__ __volatile__ ("nop\n\tnop\n\tnop\n\t");
  PORTB |= DACLDAC;
}

// SETUP

void setup() {

  // MCP4822 control lines inactive before SPI starts (PB2 = SS must be an output for SPI master)

  PORTB |= DACLDAC;
  DDRB |= DACLDAC;
  PORTC |= DACCS1 | DACCS2;
  DDRC |= DACCS1 | DACCS2;

  SPI.begin();

  // All outputs to 0, gain x2, active

  for (index = 0; index < 4; index++){
    writeDac(index);
  }
  pulseLdac();

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
//* Channel 0 = IC103 A (X101.2), 1 = IC103 B (X101.3), 2 = IC104 A (X101.4), 3 = IC104 B       *
//* (X101.5)                                                                                    *
//*                                                                                             *
//* Address 0 - SELECT                                                                          *
//*   WRITE  bit 7..2 = unused (NC)                                                             *
//*          bit 1..0 = channel for addresses 1..3                                              *
//*   READ   bit 7..6 = 0, bit 5..4 = configuration of the channel, bit 3..2 = 0,               *
//*          bit 1..0 = channel                                                                 *
//*                                                                                             *
//* Address 1 - CONFIG                                                                          *
//*   WRITE  bit 7..2 = unused (NC)                                                             *
//*          bit 1    = shutdown   0 = output active, 1 = output off (500 kOhm to GND)          *
//*          bit 0    = gain       0 = x1 (0..2.0475 V), 1 = x2 (0..4.095 V)                    *
//*          sent to the channel with its current value, followed by a /LDAC pulse              *
//*   READ   configuration of the channel                                                       *
//*                                                                                             *
//* Address 2 - VALUE LOW                                                                       *
//*   WRITE  bit 7..0 of the value, stored only                                                 *
//*   READ   bit 7..0 of the value                                                              *
//*                                                                                             *
//* Address 3 - VALUE HIGH                                                                      *
//*   WRITE  bit 7    = unused (NC)                                                             *
//*          bit 6    = HOLD       0 = /LDAC pulse after the transfer (all channels update),    *
//*                                1 = no /LDAC pulse (output follows with the next pulse)      *
//*          bit 5..4 = unused (NC)                                                             *
//*          bit 3..0 = bit 11..8 of the value, starts the transfer to the channel              *
//*   READ   bit 7..4 = 0, bit 3..0 = bit 11..8 of the value                                    *
//*                                                                                             *
//* Start-up: all channels value 0, gain x2 (value = mV), active                                *
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

      // SELECT - Bits 7..2 = NC  Bits 1..0 = Channel

      case 0:

              if (wr){

                data = PIND;
                dChannel = (data & 0x03);

              }
              else {

                DDRD = 0xff;  // PORT D output

                data = dChannel | (dConf[dChannel] << 4);

                PORTD = data;

              }

      break;

      // CONFIG - Bits 7..2 = NC  Bit 1 = Shutdown  Bit 0 = Gain x2

      case 1:

              if (wr){

                data = PIND;
                dConf[dChannel] = (data & (CONF_GAIN2 | CONF_SHDN));

                writeDac(dChannel);
                pulseLdac();

              }
              else {

                DDRD = 0xff;  // PORT D output

                data = dConf[dChannel];

                PORTD = data;

              }

      break;

      // VALUE LOW - Bits 7..0 = Value bits 7..0

      case 2:

              if (wr){

                data = PIND;
                dValue[dChannel] = (dValue[dChannel] & 0x0F00) | data;

              }
              else {

                DDRD = 0xff;  // PORT D output

                data = dValue[dChannel] & 0xFF;

                PORTD = data;

              }

      break;

      // VALUE HIGH - Bit 6 = HOLD  Bits 3..0 = Value bits 11..8

      case 3:

              if (wr){

                data = PIND;
                dValue[dChannel] = (dValue[dChannel] & 0x00FF) | ((uint16_t)(data & 0x0F) << 8);

                writeDac(dChannel);
                if (!(data & HIGH_HOLD)) pulseLdac();

              }
              else {

                DDRD = 0xff;  // PORT D output

                data = dValue[dChannel] >> 8;

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

    // Keep driving the data bus until the read cycle has ended (/RD high again); /WAIT on the
    // bus rises slowly through its pull-up and the CPU takes the data one clock after that

    while (!(PINB & SRD));

    // Port D as Input

    DDRD = 0;
    PORTD = 0;

  }

}
