//***************************************************************************************************
//* SOURCE - mbc89_store.c - File-based persistence (EEPROM replacement) for mbc89_base/mbc89_com     *
//***************************************************************************************************
//* Datei: mbc89_store.c                                                                              *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************
#include "mbc89_store.h"
#include "mbc89_base.h"
#include "mbc89_com.h"
#include "dos.h"
#include "memory.h"

//===================================================================================================
// FILE FORMAT
//===================================================================================================
// MBC89.CFG, one fixed 128-byte sequential record (only 42 bytes meaningful, rest reserved/zero):
//   0-3   GUiD (4 bytes)
//   4     CS2-Geraetegruppe
//   5-16  PC-Datenbanknummer (12 bytes)
//   17-18 Connect6021 SW Major/Minor
//   19    Zentralentyp-Mapping
//   20    Keyboard-Basisadressblock
//   21    I2C-Takt
//   22-41 Modulname (20 bytes, O_NAME)
//   42-127 reserved

#define STORE_RECORD_SIZE 128

#define O_GUID    0
#define O_CS2GER  4
#define O_DB      5
#define O_SW      17
#define O_MAPPING 19
#define O_BASIS   20
#define O_I2C     21
#define O_NAME    22

// MSX-DOS 1 disk I/O must not transfer data through page 1 (0x4000-0x7FFF) - DISKROM occupies that
// page during the actual transfer, so a page-1 buffer gets silently overwritten/misread mid-transfer.
// A plain static here would end up wherever SDCC's linker places program data, which lands above
// 0x4000 once the .COM file is large enough - fixed page-3 addresses avoid this entirely, right after
// mbc89_can.h's MCAN_RX_BUF (0xC650-0xC669).
#define STORE_FCB 0xC66A	// DOS_FCB struct
#define STORE_BUF 0xC6A0	// 128-byte transfer buffer

__at(STORE_FCB) DOS_FCB s_File;
__at(STORE_BUF) u8      s_Buf[STORE_RECORD_SIZE];

//---------------------------------------------------------------------------------------------------
static void OpenFile(void)
{
	Mem_Set(0, &s_File, sizeof(DOS_FCB));
	Mem_Copy("MBC89   CFG", &s_File.Name, 11);
}

//---------------------------------------------------------------------------------------------------
// Raw BDOS DISK RESET (function 0Dh, no parameters/results). RAM survives a warm restart on this
// hardware (only a real power-cycle clears it) - if MSX-DOS's own internal disk-buffer state is
// among what survives, a warm-restarted program could see stale, pre-write data on its very first
// read even though the file on disk is already correct. Resets the transfer address back to its
// default (0x80) as a side effect, so the caller must set it again afterward. Call once at the start
// of both Store_Load() and Store_Save(), before touching s_File.
static void DiskReset(void)
{
__asm
	push	ix
	ld		c, #DOS_FUNC_DSKRST
	call	BDOS
	pop		ix
__endasm;
}

//---------------------------------------------------------------------------------------------------
void Store_Load(void)
{
	DiskReset();
	OpenFile();

	// The coupled CAN interrupt can fire at any point during this FCB session - disk I/O on MSX is
	// not reentrant-safe against that, so the whole session runs with interrupts masked.
	DisableInterrupt();

	if (DOS_OpenFCB(&s_File) != DOS_ERR_NONE)
	{
		EnableInterrupt();
		Store_Save();
		return;
	}

	DOS_SetTransferAddr(s_Buf);
	if (DOS_SequentialReadFCB(&s_File) == DOS_ERR_NONE)
	{
		McBase_SetPersist(&s_Buf[O_GUID], s_Buf[O_CS2GER], &s_Buf[O_DB], &s_Buf[O_NAME]);
		McCom_SetPersist(&s_Buf[O_SW], s_Buf[O_MAPPING], s_Buf[O_BASIS], s_Buf[O_I2C]);
	}

	DOS_CloseFCB(&s_File);
	EnableInterrupt();
}

//---------------------------------------------------------------------------------------------------
void Store_Save(void)
{
	Mem_Set(0, s_Buf, STORE_RECORD_SIZE);

	McBase_GetPersist(&s_Buf[O_GUID], &s_Buf[O_CS2GER], &s_Buf[O_DB], &s_Buf[O_NAME]);
	McCom_GetPersist(&s_Buf[O_SW], &s_Buf[O_MAPPING], &s_Buf[O_BASIS], &s_Buf[O_I2C]);

	DiskReset();
	OpenFile();

	// Same reasoning as Store_Load() - the whole FCB session (Open/Create through Close) runs with
	// interrupts masked.
	DisableInterrupt();

	// DOS_CreateFCB() (F_MAKE) deletes and recreates an already-existing file, reallocating it to a
	// new disk cluster every time - only used here on the very first save (file does not exist yet).
	// Every later save opens the existing file instead and overwrites its already-allocated record in
	// place, so a value change never moves the file to a new cluster.
	if (DOS_OpenFCB(&s_File) != DOS_ERR_NONE)
	{
		OpenFile();
		DOS_CreateFCB(&s_File);
	}

	DOS_SetTransferAddr(s_Buf);
	DOS_SequentialWriteFCB(&s_File);

	DOS_CloseFCB(&s_File);
	EnableInterrupt();
}
