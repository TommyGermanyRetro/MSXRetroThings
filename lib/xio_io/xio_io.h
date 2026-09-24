//***************************************************************************************************
//* HEADER - xio_io.h - UNAPI/EXTBIOS access to XIO_IO_EXPANDER                                     *
//***************************************************************************************************
//* UNAPI/EXTBIOS access to XIO_IO_EXPANDER, exposed to C (see xio_io.s for the implementation).    *
//***************************************************************************************************
//* Datei: xio_io.h                                                                                 *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
//***************************************************************************************************
//* VERSION: 17/09/26                                                                               *
//***************************************************************************************************
#pragma once

#include "core.h"

// Fixed page-3 addresses, same convention as the other XIO tools in this
// project: the MSX BIOS forces page 0 to system ROM while a hardware
// interrupt is being dispatched, so the workarea/flag word/ISR code must
// live outside this program's own transient image.
#define XIO_WORKAREA	0xC000	// 102-byte scratch area required by XIO INIT (verified against
					// XIOROM.ASM's own X_HENDE marker, 0x66 - not the 97 bytes
					// previously assumed here)
#define XIO_INTFLAGS	0xC100	// 2-byte flag word; high byte = per-channel
					// IM2 pending bits, updated by the ROM on
					// every IM2 interrupt
#define XIO_ISRCODE	0xC200	// ISR stub target - reserved for projects that register a custom
					// ISR stub here (RTCLCDI/XIOIM2/XIOPIC); rcx_io.h's own
					// RCXWORKAREA starts at 0xC300 to avoid colliding with this

// Discovered driver location (valid only after a successful Xio_Discover())
extern u8 g_XSlot;
extern u8 g_XSeg;		// 0xFF for a ROM-resident driver
extern u8 g_XCount;		// implementation count from the last Xio_Discover() call

// Driver/API identification (valid only after a successful Xio_GetInfo())
extern u16 g_XRomVersion;	// driver implementation version, high=P/low=S
extern u16 g_XApiVersion;	// UNAPI version supported, high=P/low=S
extern const c8* g_XApiInfo;	// zero-terminated driver ID string

bool Xio_Discover(void);
bool Xio_GetInfo(void);
u8   Xio_IsInit(void);
void Xio_Deinit(void);
bool Xio_Init(u16 workarea, u16 intflags);
void Xio_Out(u8 addr, u8 data);
u8   Xio_Inp(u8 addr);
bool Xio_SetAddr(u8 channel, u16 addr);
bool Xio_SetMask(u16 mask);
bool Xio_GetAddr(u8 channel, u16* outAddr);
void Xio_GetMask(u16* outMask);
