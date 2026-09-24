//***************************************************************************************************
//* SOURCE - mbc89_can.c - CAN adapter: logical Mbc89Can array <-> RCX/SJA1000 PeliCAN buffer        *
//***************************************************************************************************
//* Datei: mbc89_can.c                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 17/09/26                                                                                 *
//***************************************************************************************************
#include "mbc89_can.h"

//---------------------------------------------------------------------------------------------------
// PRIO/ID/HASH_H/HASH_L, taken byte-for-byte, form the 29-bit Maerklin CAN identifier as a plain
// big-endian value (PRIO in the top byte, only its low bits meaningful) - derived from
// Mbc89Can::sendMcp2515()'s own SID/EID composition, which collapses to exactly this when the
// MCP2515-specific register split is undone. The SJA1000 PeliCAN extended-frame ID registers
// split that same 29-bit value differently: ID1=bits28-21, ID2=bits20-13, ID3=bits12-5, ID4's top
// 5 bits=bits4-0 (bit2=RTR, bits1-0 reserved/0).
void McanEncode(const u8* msg, u16* rcxBuf)
{
	u32 extId = ((u32)msg[MCAN_PRIO] << 24) | ((u32)msg[MCAN_ID] << 16)
	          | ((u32)msg[MCAN_HASH_H] << 8) | (u32)msg[MCAN_HASH_L];

	rcxBuf[0] = (u16)(0x80 | (msg[MCAN_DLC] & 0x0F));	// FrameInfo: FF=1 (extended), RTR=0, DLC
	rcxBuf[1] = (u16)((extId >> 21) & 0xFF);		// ID1
	rcxBuf[2] = (u16)((extId >> 13) & 0xFF);		// ID2
	rcxBuf[3] = (u16)((extId >> 5) & 0xFF);		// ID3
	rcxBuf[4] = (u16)((extId & 0x1F) << 3);		// ID4 (RTR=0, reserved bits=0)

	for (u8 i = 0; i < 8; i++)
		rcxBuf[5 + i] = msg[MCAN_D0 + i];
}

//---------------------------------------------------------------------------------------------------
void McanDecode(const u16* rcxBuf, u8* msg)
{
	u8 dlc = (u8)(rcxBuf[0] & 0x0F);
	u32 extId = ((u32)(rcxBuf[1] & 0xFF) << 21) | ((u32)(rcxBuf[2] & 0xFF) << 13)
	          | ((u32)(rcxBuf[3] & 0xFF) << 5) | (u32)(((rcxBuf[4] & 0xFF) >> 3) & 0x1F);

	msg[MCAN_PRIO]   = (u8)((extId >> 24) & 0xFF);
	msg[MCAN_ID]     = (u8)((extId >> 16) & 0xFF);
	msg[MCAN_HASH_H] = (u8)((extId >> 8) & 0xFF);
	msg[MCAN_HASH_L] = (u8)(extId & 0xFF);
	msg[MCAN_DLC]    = dlc;

	for (u8 i = 0; i < 8; i++)
		msg[MCAN_D0 + i] = (i < dlc) ? (u8)(rcxBuf[5 + i] & 0xFF) : 0;
}
