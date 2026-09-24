//***************************************************************************************************
//* HEADER - mbc89_can.h - CAN adapter: logical Mbc89Can array <-> RCX/SJA1000 PeliCAN buffer        *
//***************************************************************************************************
//* Converts between a flat 13-byte CS2 message layout (PRIO/ID/HASH_H/HASH_L/DLC/D0-D7) and the      *
//* 13-word buffer rcx_io.h's Can_Tx()/Can_Rx() expect - the raw SJA1000 PeliCAN extended-frame TX/RX  *
//* registers (FrameInfo, ID1-4, Data0-7), matching RCXROM.ASM's CORE49 (which writes the 13 bytes    *
//* straight into the SJA1000's TX FIFO with no reinterpretation).                                    *
//***************************************************************************************************
//* Datei: mbc89_can.h                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// Fixed page-3 addresses for the 13-word buffers passed to Can_Tx()/Can_Rx() - REQUIRED, not just
// convention: those calls go through CALSLT (see rcx_io.s's _RcxCall), which switches page 1
// (0x4000-0x7FFF) to the driver's own slot for the duration of the call. A buffer allocated as a
// normal (non-fixed) static ends up wherever SDCC's linker places program data, which lands above
// 0x4000 once the whole .COM file exceeds roughly 16KB - CALSLT would then have the SJA1000 driver
// read/write the WRONG memory (its own ROM content, not our buffer) for the exact duration of the
// call, silently. Placed right after rcx_io.h's RCXWORKAREA (0xC300, 822 bytes, ends 0xC635).
#define MCAN_TX_BUF	0xC636	// 26 bytes (13 x u16), 0xC636-0xC64F
#define MCAN_RX_BUF	0xC650	// 26 bytes (13 x u16), 0xC650-0xC669

// Logical 13-byte CS2 message array index layout.
#define MCAN_MSG_LEN	13
#define MCAN_PRIO	0
#define MCAN_ID		1
#define MCAN_HASH_H	2
#define MCAN_HASH_L	3
#define MCAN_DLC	4
#define MCAN_D0		5
#define MCAN_D1		6
#define MCAN_D2		7
#define MCAN_D3		8
#define MCAN_D4		9
#define MCAN_D5		10
#define MCAN_D6		11
#define MCAN_D7		12

// CAN card IO address, set once by discovery in mbc89.c's main() and reused by every Can_Check()/
// Can_Rx()/Can_Tx()/Can_GetIntAddr() call across every module - each call takes it explicitly.
extern u8 g_CanAddr;

// Defined (__at(MCAN_TX_BUF)) in mbc89.c. Every module's own SendMsg()-style helper (mbc89_base.c,
// mbc89_com.c) must encode into THIS buffer before calling Can_Tx(), not a local/static array of its
// own - a per-file local static could end up above 0x4000 once the .COM file is large enough, which
// is exactly as unreachable during CALSLT as the case this fixed address exists to avoid.
extern __at(MCAN_TX_BUF) u16 g_CanTxBuf[MCAN_MSG_LEN];
// Same reasoning, RX side - defined in mbc89.c, also used directly by mbc89_gui.c's Gui_Run().
extern __at(MCAN_RX_BUF) u16 g_CanRxBuf[MCAN_MSG_LEN];

// Logical CS2 message (PRIO/ID/HASH_H/HASH_L/DLC/D0-D7) -> RCX Can_Tx() buffer (13 words,
// low byte per word: FrameInfo, ID1-4, Data0-7).
void McanEncode(const u8* msg, u16* rcxBuf);

// RCX Can_Rx() buffer (13 words) -> logical message array. Data bytes at or beyond the decoded DLC
// are zeroed.
void McanDecode(const u16* rcxBuf, u8* msg);
