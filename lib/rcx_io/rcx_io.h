//***************************************************************************************************
//* HEADER - rcx_io.h - UNAPI/EXTBIOS access to RCX_INTERFACE                                       *
//***************************************************************************************************
//* UNAPI/EXTBIOS access to RCX_INTERFACE, exposed to C (see rcx_io.s for the implementation).       *
//* Covers every FN_CORE entry RCXROM.ASM exposes except FN_CORE1 (BASIC's own interactive "RCX ?"   *
//* help menu, which has no meaning outside the BASIC command line - same exclusion xio_io.s makes   *
//* for XIOROM.ASM's analogous FN_CORE1).                                                            *
//*                                                                                                   *
//* Register contracts below were cross-checked against RCXROM.ASM's actual shared-dispatcher code    *
//* (COREPIO/COREPPI/CORETIM), not just its per-function EXTBIOS doc comments - those turned out to   *
//* disagree with the real code in two places (TIMER write/ctrl/gate functions actually take a real   *
//* 16-bit DE value, not just a byte in D as CORE35's own comment claims; PPI SET/RESET BIT both      *
//* take D=Byte, not DE=varptr as CORE31's own comment claims) - see rcx_io.s for the exact evidence. *
//*                                                                                                   *
//* Card-address/bus/chip parameters (I2C bus address, I2C chip address, PIO/PPI/TIMER/CAN/SPI/ADS    *
//* base address, SPI bus number) are passed explicitly on EVERY call, exactly matching the BASIC/    *
//* UNAPI original - up to 16 cards of most types can be installed simultaneously (SJA1000 excepted,  *
//* single-use only), so nothing about which card an operation targets may be cached client-side.     *
//* Functions needing more than sdcccall(1)'s 2-argument limit use sdcccall(0) (stack-based, see the  *
//* per-function comments in rcx_io.s for the exact stack layout each one expects).                   *
//***************************************************************************************************
//* Datei: rcx_io.h                                                                                  *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 23/09/26                                                                                *
//***************************************************************************************************
#pragma once

#include "core.h"

// Fixed page-3 address for Rcx_Init()'s RAM work area - must not overlap xio_io.h's XIO_WORKAREA
// (0xC000, 102 bytes), XIO_INTFLAGS (0xC100, 2 bytes) or XIO_ISRCODE (0xC200, reserved for a
// custom ISR stub in other XIO-based projects). Size verified against RCXROM.ASM's own X_HENDE
// marker (0x336 = 822 bytes), not the previously-assumed 816 bytes.
#define RCXWORKAREA	0xC300	// 822 bytes (0x0336), 0xC300-0xC635

//===================================================================================================
// DISCOVERY / INFO
//===================================================================================================

// Discovered driver location (valid only after a successful Rcx_Discover())
extern u8 g_RSlot;
extern u8 g_RSeg;		// 0xFF for a ROM-resident driver, else the mapped RAM segment
extern u8 g_RCount;		// implementation count from the last Rcx_Discover() call

// Driver/API identification (valid only after a successful Rcx_GetInfo())
extern u16 g_RRomVersion;	// driver implementation version, high=P/low=S
extern u16 g_RApiVersion;	// UNAPI version supported, high=P/low=S
extern const c8* g_RApiInfo;	// zero-terminated driver ID string

bool Rcx_Discover(void);
bool Rcx_GetInfo(void);	// FN_INFO (0) - does not require Rcx_Init() first

//===================================================================================================
// RCX CORE (CORE2-5)
//===================================================================================================

u8   Rcx_IsInit(void);				// CORE3
bool Rcx_Init(u16 workarea);			// CORE2
void Rcx_Deinit(void);				// CORE4
bool Rcx_GetTable(u16* outAddr);		// CORE5 - RCX table of installed cards

//===================================================================================================
// I2C / PCF8584 (CORE6-11)
//===================================================================================================

void I2c_Init(u8 addr);				// CORE6 - registers the card in the RCX table
void I2c_Write(u8 addr, u8 chip, u8 data) __sdcccall(0);		// CORE7
void I2c_Wrb(u8 addr, u8 chip, u8 len, void* data) __sdcccall(0);	// CORE8
u8   I2c_Read(u8 addr, u8 chip);					// CORE9
void I2c_Rdb(u8 addr, u8 chip, u8 len, void* data) __sdcccall(0);	// CORE10
bool I2c_Reset(u8 addr);						// CORE11

//===================================================================================================
// RTC / PCF8583 (CORE12-15) - addr is the I2C bus address, same as the I2C group above
//===================================================================================================

void Rtc_SetTime(u8 addr, void* time);			// CORE12
void Rtc_GetTime(u8 addr, void* time);			// CORE13
void Rtc_SetDate(u8 addr, void* date);			// CORE14
void Rtc_GetDate(u8 addr, void* date);			// CORE15

//===================================================================================================
// Z80 PIO (CORE16-22)
//===================================================================================================

bool Pio_Init(u8 addr);			// CORE16 - registers the card in the RCX table
u8   Pio_ReadA(u8 addr);			// CORE17
void Pio_WriteA(u8 addr, u8 value);		// CORE18
u8   Pio_ReadB(u8 addr);			// CORE19
void Pio_WriteB(u8 addr, u8 value);		// CORE20
void Pio_CtrlA(u8 addr, u8 value);		// CORE21
void Pio_CtrlB(u8 addr, u8 value);		// CORE22

//===================================================================================================
// 82C55 PPI (CORE23-32)
//===================================================================================================

bool Ppi_Init(u8 addr);			// CORE23 - registers the card in the RCX table
u8   Ppi_ReadA(u8 addr);			// CORE24
void Ppi_WriteA(u8 addr, u8 value);		// CORE25
u8   Ppi_ReadB(u8 addr);			// CORE26
void Ppi_WriteB(u8 addr, u8 value);		// CORE27
u8   Ppi_ReadC(u8 addr);			// CORE28
void Ppi_WriteC(u8 addr, u8 value);		// CORE29
void Ppi_Ctrl(u8 addr, u8 value);		// CORE30
void Ppi_SetBit(u8 addr, u8 bit);		// CORE31 - bit 0..7
void Ppi_ResetBit(u8 addr, u8 bit);		// CORE32 - bit 0..7

//===================================================================================================
// 82C54 TIMER (CORE33-42)
//===================================================================================================
// Read/write/ctrl/gate all carry a real 16-bit value (verified against
// CORETIMAPR/CORETIMAPW's actual code, not just the per-function EXTBIOS
// doc comments, which disagree with each other here - see rcx_io.s).

bool Timer_Init(u8 addr);			// CORE33 - registers the card in the RCX table
u16  Timer_Read0(u8 addr);			// CORE34
void Timer_Write0(u8 addr, u16 value);		// CORE35
u16  Timer_Read1(u8 addr);			// CORE36
void Timer_Write1(u8 addr, u16 value);		// CORE37
u16  Timer_Read2(u8 addr);			// CORE38
void Timer_Write2(u8 addr, u16 value);		// CORE39
void Timer_Ctrl(u8 addr, u16 value);		// CORE40
void Timer_SetGate(u8 addr, u16 value);	// CORE41
void Timer_ResetGate(u8 addr, u16 value);	// CORE42

//===================================================================================================
// SJA1000 CAN (CORE43-49) - single-card-only in practice (see RCXRAM/RCXROM), addr still passed
// explicitly on every call for consistency with every other card type here.
//===================================================================================================

bool Can_Init(u8 addr, void* acc);		// CORE43
void Can_Write(u8 addr, u8 reg, u8 data) __sdcccall(0);	// CORE44
u8   Can_Read(u8 addr, u8 reg);				// CORE45
u8   Can_Check(u8 addr);					// CORE46 - 1 if a new RX message is pending
void Can_Rx(u8 addr, void* data);				// CORE47
u16  Can_GetIntAddr(u8 addr);					// CORE48 - address of the driver's INT routine
void Can_Tx(u8 addr, void* data);				// CORE49

//===================================================================================================
// SPI (CORE50-56)
//===================================================================================================

bool Spi_Init(u8 addr);			// CORE50 - registers the card in the RCX table
void Spi_Write(u8 addr, u8 bus, u8 data) __sdcccall(0);		// CORE51
void Spi_Wrb(u8 addr, u8 bus, u8 len, void* data) __sdcccall(0);	// CORE52
u8   Spi_Read(u8 addr, u8 bus);					// CORE53
void Spi_Rdb(u8 addr, u8 bus, u8 len, void* data) __sdcccall(0);	// CORE54
// Spi_Rdh: lenHeaderData packs SPI LEN HEADER in the high byte and SPI LEN
// DATA in the low byte, e.g. ((u16)headerLen << 8) | dataLen - matches
// FN_CORE55's H=header/L=data entry registers exactly.
void Spi_Rdh(u8 addr, u8 bus, u16 lenHeaderData, void* data) __sdcccall(0);	// CORE55
void Spi_Mode(u8 addr, u8 bus, u8 mode, u8 freq) __sdcccall(0);		// CORE56

//===================================================================================================
// ADS1220 (CORE57-62)
//===================================================================================================

bool Ads_Init(u8 addr);			// CORE57 - registers the card in the RCX table
void Ads_SetMode(u8 addr, u8 type);		// CORE58 - ADS TYPE 0..11
void Ads_Cmd(u8 addr, u8 val);			// CORE59
u8   Ads_GetStatus(u8 addr);			// CORE60
u8   Ads_Read(u8 addr);			// CORE61 - one RESULT byte
void Ads_Get(u8 addr, u8 type, void* val) __sdcccall(0);	// CORE62 - full conversion into a 4-byte SNG
