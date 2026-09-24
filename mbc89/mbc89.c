//***************************************************************************************************
//* HEADER - mbc89.c - MSX/SDCC mbc89 (Connect6021-CAN-Interface) main program                        *
//***************************************************************************************************
//* Reads the startup configuration from MSX-DOS 2 environment variables (IO/INT/KEYB/C80F), shows a   *
//* splash screen and a boot info screen (startup config, persisted config, discovery/init results),   *
//* then discovers and initializes the XIO_IO_EXPANDER and RCX_INTERFACE UNAPI drivers and the CAN     *
//* adapter, before handing control to the GUI (mbc89_gui.c).                                          *
//***************************************************************************************************
//* Datei: mbc89.c                                                                                    *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************

#include "msxgl.h"
#include "bios_var.h"
#include "dos.h"
#include "xio_io.h"
#include "rcx_io.h"
#include "dostools.h"
#include "mbc89_sim.h"
#include "mbc89_can.h"
#include "mbc89_base.h"
#include "mbc89_com.h"
#include "mbc89_gui.h"
#include "mbc89_store.h"

#include "font/font_mgl_sample6.h"

//===================================================================================================
// DATA
//===================================================================================================

// Acceptance code & mask (ACR0-3/AMR0-3) - CAN INIT only reads the even (low-byte) offsets, the
// high bytes are never read. Targeted (not accept-all): PRIO is fully wildcarded (the protocol
// specifies that prio bits are not evaluated), HASH is fully wildcarded (this module filters only on
// the GUiD bytes in D0-D7), RTR is fixed to 0 (this protocol never uses RTR frames). Within the 8-bit
// MCAN_ID byte itself (straddling PeliCAN ID1's bottom 3 bits and ID2's top 5 bits), only bits 7 and
// 0 are fixed to 0, bits 6-1 are wildcarded - this accepts every even MCAN_ID below 0x80. Bit 0 fixed
// to 0 excludes every _RESPONSE echo of this module's own IDs (0x01/0x31/0x3B/0x45 are all odd).
static const u16 s_AccBuf[8] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x00FB, 0x00F7, 0x00FF, 0x00FB };

// Fixed page-3 addresses - CALSLT switches page 1 during Can_Tx()/Can_Rx(), so a regular static
// buffer here would become unreachable once the .COM file grows large enough for SDCC's linker to
// place it above 0x4000.
__at(MCAN_TX_BUF) u16 g_CanTxBuf[MCAN_MSG_LEN];
__at(MCAN_RX_BUF) u16 g_CanRxBuf[MCAN_MSG_LEN];

static u8 s_Row;	// next free VDP text row for status output

// CAN card IO address / PIC interrupt channel, read once in main() from the IO/INT environment
// variables. File-scope, not local to main(). g_CanAddr is NOT static (unlike canChan/intAddr) -
// declared extern in mbc89_can.h, every module's Can_Check()/Can_Rx()/Can_Tx() call takes it as an
// explicit parameter.
u8 g_CanAddr;
static u8 canChan;
static u16 intAddr;

// Status text as file-scope static const, not inline literals inside main() (SDCC allocates inline
// literal arguments in the context of that one function rather than as independent, stable
// module-level symbols).
static const c8 s_MsgTitle[]      = "MBC89 - MSX Connect6021 port";
static const c8 s_MsgXioFound[]   = "XIO_IO_EXPANDER found.";
static const c8 s_MsgRcxFound[]   = "RCX_INTERFACE found.";
static const c8 s_MsgXioInitOk[]  = "XIO INIT ok.";
static const c8 s_MsgRcxInitOk[]  = "RCX INIT ok.";
static const c8 s_MsgCanInitOk[]  = "CAN INIT ok.";
static const c8 s_MsgSetAddrOk[]  = "XIO SET ADDR ok.";
static const c8 s_MsgSetMaskOk[]  = "XIO SET MASK ok.";
static const c8 s_MsgIntCoupled[] = "Interrupt coupled.";
static const c8 s_MsgRcxMissing[] = "RCX_INTERFACE not found.";
static const c8 s_MsgXioMissing[] = "XIO_IO_EXPANDER not found.";
static const c8 s_MsgXioInitFail[]= "XIO INIT failed.";
static const c8 s_MsgRcxInitFail[]= "RCX INIT failed.";
static const c8 s_MsgCanInitFail[]= "CAN INIT failed.";
static const c8 s_MsgSetAddrFail[]= "XIO SET ADDR failed.";
static const c8 s_MsgSetMaskFail[]= "XIO SET MASK failed.";

// Error messages for a missing/invalid startup environment variable. DOS_StringOutput()'s own "$"
// termination convention (BDOS function 9).
static const c8 s_MsgEnvIo[]   = "SET IO=0..254 (CAN card IO address) missing or invalid.\r\n$";
static const c8 s_MsgEnvInt[]  = "SET INT=0..7 (PIC interrupt channel) missing or invalid.\r\n$";
static const c8 s_MsgEnvKeyb[] = "SET KEYB=0..16 (Keyboard-6040 count) missing or invalid.\r\n$";
static const c8 s_MsgEnvC80f[] = "SET C80F=0..8 (Control80f count) missing or invalid.\r\n$";

// Number of simulated Keyboard-6040/Control80f panel instances (0..KEYBOARD_COUNT_MAX/
// 0..C80F_COUNT_MAX), read from the KEYB/C80F environment variables. Shared with mbc89_com.c and
// mbc89_gui.c via extern declarations in their own headers.
u8 g_KeyboardCount;
u8 g_C80fCount;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Prints one status line on the VDP text screen and advances to the next row.
static void Status(const c8* text)
{
	Print_SetPosition(0, s_Row++);
	Print_DrawText(text);
}

//---------------------------------------------------------------------------------------------------
// Waits for ESC via a plain busy-wait poll of the keyboard matrix.
static void WaitEsc(void)
{
	while (!Keyboard_IsKeyPressed(KEY_ESC));
}

//---------------------------------------------------------------------------------------------------
// Prints a status line, waits for ESC, then exits.
static void Fail(const c8* text)
{
	Status(text);
	WaitEsc();
	Bios_Exit(0);
}

//---------------------------------------------------------------------------------------------------
// Reads an MSX-DOS 2 environment variable (BDOS function 6Bh/GENV). name arrives in HL, buffer in DE
// (SDCC's parameter-passing convention for this argument shape) - matches GENV's own register
// convention exactly (C=function number, HL=name, DE=buffer, B=buffer size), no register swap
// needed. B=64 matches every caller's actual buf[64]. Local to this file rather than dos.c/dos.h,
// matching this file's own convention of keeping small helpers (Status/WaitEsc/Fail above) as
// file-local statics. Return: BDOS error code (0 = found).
static u8 DOS_GetEnv(const c8* name, c8* buffer)
{
	name;	// HL
	buffer;	// DE
__asm
	push	ix
	ld		b, #64
	ld		c, #DOS_FUNC_GENV
	call	BDOS
	pop		ix
__endasm;
	// return A
}

//---------------------------------------------------------------------------------------------------
// Reads env var <name> via DOS_GetEnv(), parses it as a plain unsigned decimal ASCIIZ string (no
// sign, no whitespace), range-checks 0..maxVal. Missing, empty, non-digit or out-of-range -> FALSE.
static bool GetEnvDecimal(const c8* name, u16 maxVal, u8* out)
{
	c8 buf[64];
	u16 val = 0;
	u8 i;

	if (DOS_GetEnv(name, buf) != 0) return FALSE;
	if (buf[0] == 0) return FALSE;
	for (i = 0; buf[i] != 0; i++)
	{
		if ((buf[i] < '0') || (buf[i] > '9')) return FALSE;
		val = (u16)(val * 10 + (buf[i] - '0'));
		if (val > 999) return FALSE;
	}
	if (val > maxVal) return FALSE;
	*out = (u8)val;
	return TRUE;
}

//===================================================================================================
// MAIN LOOP
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Program entry point
void main(void)
{
	// Fixed simulated sensor values reported to the CS3 over CAN. Plugged=TRUE: MBC bus supply,
	// always present in this deployment.
	Sim_SetPlugged(TRUE);
	Sim_SetVoltage(188);      // 18.8 V
	Sim_SetTemperature(235);  // 23.5 degC

	// Startup config: the IO/INT/KEYB/C80F environment variables select the CAN card IO address, the
	// PIC interrupt channel, and the simulated Keyboard-6040/Control80f instance counts. Runs before
	// any VDP_SetMode() - still whatever text state COMMAND.COM left the screen in, so errors go
	// straight to the BDOS console via DOS_StringOutput(). A missing/invalid variable aborts straight
	// back to the DOS prompt - no interactive fallback, this is meant to be fully scriptable. Uses
	// DOS_Exit0() (raw BDOS Terminate), NOT Bios_Exit() - Bios_Exit() forces a VDP mode switch as
	// part of its own "clean exit" routine, which would wipe this error message before it could be
	// read; every other exit point in this file calls WaitEsc() first (a manual pause before that
	// screen-wiping happens) - this is the one path with no VDP mode active yet, so nothing to guard
	// against and no pause needed either.
	if (!GetEnvDecimal("IO", 254, &g_CanAddr))                       { DOS_StringOutput(s_MsgEnvIo);   DOS_Exit0(); }
	if (!GetEnvDecimal("INT", 7, &canChan))                          { DOS_StringOutput(s_MsgEnvInt);  DOS_Exit0(); }
	if (!GetEnvDecimal("KEYB", KEYBOARD_COUNT_MAX, &g_KeyboardCount)) { DOS_StringOutput(s_MsgEnvKeyb); DOS_Exit0(); }
	if (!GetEnvDecimal("C80F", C80F_COUNT_MAX, &g_C80fCount))         { DOS_StringOutput(s_MsgEnvC80f); DOS_Exit0(); }

	// Splash screen - its own standalone SCREEN2 excursion, shown before anything else visible, only
	// reachable once all 4 env vars are valid.
	Gui_ShowSplash();

	// Loads the module identity/persisted config - needed in memory before the boot info screen below
	// can display it, and before any CAN traffic is processed.
	McBase_Init();
	McCom_Init();
	Store_Load();

	// PatternOffset=0 (not 1): this font's CharFirst is 0, so offset 0 keeps Print_*'s pattern-table
	// index equal to the raw ASCII code, matching what BDOS's own console driver writes unshifted.
	VDP_SetMode(VDP_MODE_SCREEN0);
	VDP_ClearVRAM();
	Print_SetTextFont(g_Font_MGL_Sample6, 0);
	Print_SetColor(COLOR_WHITE, COLOR_BLACK);
	s_Row = 0;
	Status(s_MsgTitle);

	// Boot info screen: the 4 startup env vars and the persisted config, followed below by every
	// discovery/init step's own result and a "Loading" progress indicator, all staying on screen
	// together before the GUI takes over.
	{
		u8  guid[4], cs2Ger, db[12], name[21];
		u8  sw[2], mapping, basis, i2c;
		u8  i;

		McBase_GetPersist(guid, &cs2Ger, db, name);
		McCom_GetPersist(sw, &mapping, &basis, &i2c);

		// db[] is 12 raw ASCII decimal digit bytes (not null-terminated - printed byte-by-byte below
		// via %c instead of building a temporary C-string). name[] is up to 20 bytes and may contain
		// non-printable padding bytes - truncated at the first one found, since %s needs a real
		// C-string terminator. name[21] leaves room for the worst case (all 20 bytes printable).
		for (i = 0; i < 20; i++)
		{
			if ((name[i] < 0x20) || (name[i] >= 0x80)) break;
		}
		name[i] = 0;

		Print_SetPosition(0, s_Row++);
		Print_DrawFormat("IO=%u INT=%u KEYB=%u C80F=%u", g_CanAddr, canChan, g_KeyboardCount, g_C80fCount);

		Print_SetPosition(0, s_Row++);
		Print_DrawFormat("GUiD=%2x%2x%2x%2x CS2GER=%2x", guid[0], guid[1], guid[2], guid[3], cs2Ger);

		Print_SetPosition(0, s_Row++);
		Print_DrawFormat("DB=%c%c%c%c%c%c%c%c%c%c%c%c", db[0], db[1], db[2], db[3], db[4], db[5],
		                  db[6], db[7], db[8], db[9], db[10], db[11]);

		Print_SetPosition(0, s_Row++);
		Print_DrawFormat("NAME=%s", name);

		Print_SetPosition(0, s_Row++);
		Print_DrawFormat("SW=%u.%u MAP=%u BASIS=%u I2C=%u", sw[0], sw[1], mapping, basis, i2c);
	}

	// Discovers XIO_IO_EXPANDER, then RCX_INTERFACE, then sets both up, then initializes the CAN
	// adapter and couples the interrupt - every step's result, success or failure, is shown.
	if (!Xio_Discover())
		Fail(s_MsgXioMissing);
	Status(s_MsgXioFound);

	if (!Rcx_Discover() || (g_RCount == 0))
		Fail(s_MsgRcxMissing);
	Status(s_MsgRcxFound);

	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
		Fail(s_MsgXioInitFail);
	Status(s_MsgXioInitOk);

	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init(RCXWORKAREA))
	{
		Status(s_MsgRcxInitFail);
		Xio_Deinit();
		WaitEsc();
		Bios_Exit(0);
	}
	Status(s_MsgRcxInitOk);

	// CAN INIT(CANADDR,ACC).
	if (!Can_Init(g_CanAddr, (void*)s_AccBuf))
	{
		Status(s_MsgCanInitFail);
		Rcx_Deinit();
		Xio_Deinit();
		WaitEsc();
		Bios_Exit(0);
	}
	Status(s_MsgCanInitOk);

	// CAN GET ADDR + XIO SET ADDR + XIO SET MASK - registers RCX ROM's own CAN interrupt service
	// routine.
	intAddr = Can_GetIntAddr(g_CanAddr);

	if (!Xio_SetAddr(canChan, intAddr))
	{
		Status(s_MsgSetAddrFail);
		Rcx_Deinit();
		Xio_Deinit();
		WaitEsc();
		Bios_Exit(0);
	}
	Status(s_MsgSetAddrOk);

	if (!Xio_SetMask((u16)(1 << canChan)))
	{
		Status(s_MsgSetMaskFail);
		Rcx_Deinit();
		Xio_Deinit();
		WaitEsc();
		Bios_Exit(0);
	}
	Status(s_MsgSetMaskOk);
	Status(s_MsgIntCoupled);

	// Holds the screen for ~10s, printing one "." per second, before the GUI takes over - a plain
	// silent wait risks looking like a hang.
	{
		u8  dot;
		u16 target;

		Print_SetPosition(0, s_Row++);
		Print_DrawText("Loading");

		for (dot = 0; dot < 10; dot++)
		{
			target = (u16)(g_JIFFY + 56);
			while (g_JIFFY != target) {}
			Print_DrawChar('.');
		}
	}

	// SCREEN2 GUI takes over for good.
	Gui_Init();
	Gui_Run();

	Rcx_Deinit();
	Xio_Deinit();

	// Gui_Run() reads the keyboard matrix directly for its own panel navigation, but the BIOS's own
	// keyboard interrupt keeps pushing every keypress into the normal KEYBUF ring buffer too - left
	// alone, COMMAND.COM drains that whole backlog right after this program exits, replaying it as if
	// typed at the DOS prompt. Emptied by making the read pointer catch up to the write pointer.
	*(u16*)M_PUTPNT = g_GETPNT;

	Bios_Exit(0);
}
