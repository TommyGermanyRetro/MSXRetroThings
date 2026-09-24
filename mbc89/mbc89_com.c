//***************************************************************************************************
//* SOURCE - mbc89_com.c - CONNECT6021 mapping, system stop/go, loco queries, module config channels *
//***************************************************************************************************
//* Datei: mbc89_com.c                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************
#include "mbc89_com.h"
#include "mbc89_can.h"
#include "mbc89_base.h"
#include "mbc89_store.h"
#include "rcx_io.h"

#define CAN_ID_SYSTEM_BEFEHLE       0x00
#define CAN_ID_LOK_GESCHWINDIGKEIT  0x08
#define CAN_ID_LOK_RICHTUNG         0x0A
#define CAN_ID_LOK_FUNKTION         0x0C
#define CAN_ID_ZUBEHOER_SCHALTEN    0x16
#define CAN_ID_CONNECT6021          0x44
#define CAN_ID_CONNECT6021_R        0x45

// MM accessory address base (0x3000 - DCC's 0x3800 is not offered, protocol is hardcoded MM) plus
// this simulated Keyboard's own 16 columns (0..15, direct) plus the configurable Basisadressblock
// (s_Basis, 1..4, KANAL_BASIS).
#define ZUBEHOER_MM_BASE 0x3000
#define MM_SPEED_STEP 68

// Loco address database (80 MM addresses 0-79, MM address 0 = CS2 slot 80) - one 10-byte record
// each. LOKS_ANZAHL/LOKS_ITEMS/LOK_MAPPED are in mbc89_com.h (McCom_LokAt()/mbc89_base.c's
// PC_ARRAY_DATA bridge need them exposed too).

#define LOK_SPEED  0
#define LOK_DIR    1
#define LOK_F0     2
#define LOK_FX     3
#define LOK_ADDR4  4
#define LOK_ADDR3  5
#define LOK_ADDR2  6
#define LOK_ADDR1  7
#define LOK_INFRA  9

#define LOK_MAPPED_ECHT 0x02

// The 5 "classic" CS2 config channels - Kanalnummer (as seen in CAN-ID 0x3A/0x00) and 0-based row
// index within this module's own table (laufendeNr).
#define KANAL_SW_MAJ  7
#define KANAL_SW_MIN  8
#define KANAL_MAPPING 9
#define KANAL_BASIS   10
#define KANAL_I2C     11

#define ROW_SW_MAJ  0
#define ROW_SW_MIN  1
#define ROW_MAPPING 2
#define ROW_BASIS   3
#define ROW_I2C     4

#define SW_OFFSET_H     6
#define SW_OFFSET_L     7
#define MAPPING_OFFSET  3
#define BASIS_OFFSET_H  6
#define BASIS_OFFSET_L  7
#define I2C_OFFSET_H    6
#define I2C_OFFSET_L    7

static u8 s_Loks[LOKS_ANZAHL][LOKS_ITEMS];

// Simulated Connect6021 SW major/minor byte pair - overwrites the CS2 ping response's D4/D5 (see
// McCom_HashInit()) independently of this MSX port's own SW version. Default 1.0.
static u8 s_Sw[2];

// Current system Stopp/Go state as last seen on the bus (own CU6021 button press OR any other
// source, e.g. CS2/ParaCenter) - mbc89_gui.c polls McCom_IsSystemStopped() to keep the CU6021 panel's
// GO-LED in sync regardless of who actually changed it. Default FALSE (running).
static bool s_SystemStopped;

// System-Stopp/-Go response templates (0x01, DLC=5) and spontaneous broadcast templates (0x00,
// DLC=5) - PRIO,CAN_ID,HASH_H,HASH_L,DLC,D0-D7.
static u8 s_MsgStop[MCAN_MSG_LEN] =
	{0x00,0x00,0x03,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 s_MsgGo[MCAN_MSG_LEN] =
	{0x00,0x00,0x03,0x00,0x05,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00};
static u8 s_MsgStopResponse[MCAN_MSG_LEN] =
	{0x00,0x01,0x03,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 s_MsgGoResponse[MCAN_MSG_LEN] =
	{0x00,0x01,0x03,0x00,0x05,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00};

// "Konfigwert uebernommen" confirmation (0x01, DLC=7, D4=SUB_MW fixed at 0x0B) - D5/D6 get patched
// per write in McCom_Cs2IndexParameterExtern().
static u8 s_MsgKonfigOk[MCAN_MSG_LEN] =
	{0x00,0x01,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x0b,0x00,0x01,0x00};

// "Zubehoer schalten" template (CAN-ID 0x16, DLC=6, PRIO=0x00,ID=0x16,HASH=0x03/0x00,DLC=6).
// D2/D3=Adresse, D4=Richtung, D5=Strom, filled in per-call by SendZubehoerSchalten().
static u8 s_MsgZubehoerSchalten[MCAN_MSG_LEN] =
	{0x00,0x16,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

// Lok-Speed/-Richtung/-Funktion templates used by ResyncAfterConnect() - HASH_H/L stay on the fixed
// template value (0x03/0x00), never updated: these messages are GUiD-addressed, not HASH-addressed.
static const u8 s_MsgLokSpeed[MCAN_MSG_LEN] =
	{0x00,0x08,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const u8 s_MsgLokDir[MCAN_MSG_LEN] =
	{0x00,0x0a,0x03,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const u8 s_MsgLokFx[MCAN_MSG_LEN] =
	{0x00,0x0c,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

// The 5 module config-channel rows (SW Major/SW Minor/Mapping/Basis/I2C) - the "current value" bytes
// are patched fresh from RAM state on every McCom_Cs2IndexExtern() call (avoids a second RAM copy).
static u8 s_Cs2IndexEx[MCCOM_ANZKONFIGMODUL][40] =
{
	// ROW_SW_MAJ - Slider 1..255
	{   KANAL_SW_MAJ,0x02,0x00,0x01,0x00,0xff,0x00,0x00,
	    'S','W',' ','M','a','j','o','r',
	    ':',0x00,'1',0x00,'2','5','5',0x00,
	    '#',0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// ROW_SW_MIN - Slider 0..255 (unlike SW Major, Minor may legitimately be 0, e.g. "1.0")
	{   KANAL_SW_MIN,0x02,0x00,0x00,0x00,0xff,0x00,0x00,
	    'S','W',' ','M','i','n','o','r',
	    ':',0x00,'0',0x00,'2','5','5',0x00,
	    '#',0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// ROW_MAPPING - Auswahlliste 0=Mapping/1=Mapping&Direkt
	{   KANAL_MAPPING,0x01,0x02,0x00,0x00,0x00,0x00,0x00,
	    'M','a','p','p','i','n','g',':',
	    0x00,'M','a','p','p','i','n','g',
	    0x00,'M','a','p','p','i','n','g',
	    '&','D','i','r','e','k','t',0x00
	},
	// ROW_BASIS - Slider 1..4
	{   KANAL_BASIS,0x02,0x00,0x01,0x00,0x04,0x00,0x00,
	    'B','a','s','i','s',':',0x00,'1',
	    0x00,'4',0x00,'#',0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// ROW_I2C - Slider 40..100
	{   KANAL_I2C,0x02,0x00,0x28,0x00,0x64,0x00,0x00,
	    'I','2','C',':',0x00,'4','0',0x00,
	    '1','0','0',0x00,'k','H','z',0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	}
};

// RAM state for the 5 module config channels (mbc89-specific systemarray fields).
static u8 s_Mapping;	// 0=Mapping, 1=Mapping&Direkt
static u8 s_Basis;	// 1..4
static u8 s_I2c;	// 40..100 kHz

// Cached module GUiD (guid0=Kennung-group byte aka O_CS2GER, guid1..3=O_UID+1..3), refreshed by
// McCom_HashInit() every time mbc89_base.c recomputes it - HandleSystemCommand() needs it to
// recognise a module-specific (as opposed to universal-broadcast) Stopp/Go request.
static u8 s_Guid[4];

//---------------------------------------------------------------------------------------------------
static u8* LokAt(u8 index)
{
	return s_Loks[index];
}

//---------------------------------------------------------------------------------------------------
// Public wrapper around LokAt().
u8* McCom_LokAt(u8 index)
{
	return LokAt(index);
}

//---------------------------------------------------------------------------------------------------
static void SendMsg(const u8* msg)
{
	McanEncode(msg, g_CanTxBuf);
	Can_Tx(g_CanAddr, g_CanTxBuf);
}

//---------------------------------------------------------------------------------------------------
// Also clears a CONNECT6021-mapped loco's MAPPED status (LOK_MAPPED_ECHT set). CONNECT6021 has no
// explicit "unmap this address" message, only fresh (re-)assignments during the DLC=5 walk that
// follows every stream-start - without this, a loco removed on the CS3 side (simply absent from that
// walk) would stay "controllable" here forever. Locos mapped via the separate Zentralentyp/s_Mapping
// config channel (LOK_MAPPED_ECHT clear) are left untouched - an unrelated mechanism, not part of the
// CONNECT6021 stream.
static void ResetLoks(void)
{
	for (u8 i = 0; i < LOKS_ANZAHL; i++)
	{
		u8* lok = LokAt(i);
		lok[LOK_ADDR4] = 0x00;
		lok[LOK_ADDR3] = 0x00;
		lok[LOK_ADDR2] = 0x00;
		lok[LOK_ADDR1] = i;
		if (lok[LOK_MAPPED] & LOK_MAPPED_ECHT) lok[LOK_MAPPED] = 0x00;
	}
	LokAt(0)[LOK_ADDR1] = 80;
}

//---------------------------------------------------------------------------------------------------
void McCom_Init(void)
{
	for (u16 i = 0; i < LOKS_ANZAHL * LOKS_ITEMS; i++) s_Loks[0][i] = 0;
	ResetLoks();

	s_Sw[0] = 1;
	s_Sw[1] = 0;

	s_Mapping = 0;
	s_Basis = 1;
	s_I2c = 100;
	s_SystemStopped = FALSE;
}

//---------------------------------------------------------------------------------------------------
// DLC=6: stream start (D4=D5=0x00, resets the whole database via ResetLoks()) or stream end
// (D4=D5=0xFF, no cuIsConnected-equivalent yet - ResyncAfterConnect() stays dormant regardless).
// DLC=5: assigns a single slot (D4, MM address 0 = slot 80) a GUiD (D0-D3).
static void HandleConnect6021(const u8* msg)
{
	static u8 response[MCAN_MSG_LEN];
	u8 i;

	for (i = 0; i < MCAN_MSG_LEN; i++) response[i] = msg[i];

	if (msg[MCAN_DLC] == 6)
	{
		if ((msg[MCAN_D4] == 0x00) && (msg[MCAN_D5] == 0x00))
			ResetLoks();

		response[MCAN_ID] = CAN_ID_CONNECT6021_R;
		SendMsg(response);
	}
	else if (msg[MCAN_DLC] == 5)
	{
		u8 index = msg[MCAN_D4];
		u8* lok;

		if (index > 80) return;

		lok = LokAt((index == 80) ? 0 : index);
		lok[LOK_ADDR4]  = msg[MCAN_D0];
		lok[LOK_ADDR3]  = msg[MCAN_D1];
		lok[LOK_ADDR2]  = msg[MCAN_D2];
		lok[LOK_ADDR1]  = msg[MCAN_D3];
		lok[LOK_MAPPED] = 0x01 | LOK_MAPPED_ECHT;

		response[MCAN_ID] = CAN_ID_CONNECT6021_R;
		SendMsg(response);

		// Does not call McCom_SyncLokState(index) here: the CS3 reacts to an unsolicited sync burst
		// right after mapping by re-asserting CONNECT6021, which re-triggers this branch, sending
		// another burst, ad infinitum. The panel-takeover sync (mbc89_gui.c's
		// SyncFromLokState() -> McCom_SyncLokState()) is unaffected.
	}
}

//---------------------------------------------------------------------------------------------------
// Gated by McBase_IsRegistered(). Reacts to a universal (D0-D3 all 0) or module-specific GUiD.
static void HandleSystemCommand(const u8* msg)
{
	bool istUniversell;
	bool istModul;

	if (!McBase_IsRegistered()) return;

	istUniversell = (msg[MCAN_D0] == 0x00) && (msg[MCAN_D1] == 0x00)
	             && (msg[MCAN_D2] == 0x00) && (msg[MCAN_D3] == 0x00);

	// D2 is deliberately compared against s_Guid[1] again instead of s_Guid[2] - not a bug to fix.
	istModul = (msg[MCAN_D0] == s_Guid[0]) && (msg[MCAN_D1] == s_Guid[1])
	        && (msg[MCAN_D2] == s_Guid[1]) && (msg[MCAN_D3] == s_Guid[3]);

	if (!istUniversell && !istModul) return;

	if (msg[MCAN_D4] == 0x00)
	{
		s_SystemStopped = TRUE;
		SendMsg(s_MsgStopResponse);
	}
	else if (msg[MCAN_D4] == 0x01)
	{
		s_SystemStopped = FALSE;
		SendMsg(s_MsgGoResponse);
	}
}

//---------------------------------------------------------------------------------------------------
// Answers CS2 loco direction/function/speed queries (CAN-ID 0x0A/0x0C/0x08) from the loco DB. The
// CU-panel always "is present" in this build; infraIsPresent stays FALSE (no track decoder here).
static void HandleLokAbfrage(const u8* msg)
{
	static const bool cuIsPresent = TRUE;
	static const bool infraIsPresent = FALSE;
	u8 canId = msg[MCAN_ID];
	u8 index;

	if (!(cuIsPresent || infraIsPresent)) return;

	if ((canId != CAN_ID_LOK_RICHTUNG) && (canId != CAN_ID_LOK_FUNKTION)
	 && (canId != CAN_ID_LOK_GESCHWINDIGKEIT)) return;

	for (index = 0; index < LOKS_ANZAHL; index++)
	{
		u8* lok = LokAt(index);
		static u8 response[MCAN_MSG_LEN];
		bool addrMatch = (lok[LOK_ADDR4] == msg[MCAN_D0]) && (lok[LOK_ADDR3] == msg[MCAN_D1])
		              && (lok[LOK_ADDR2] == msg[MCAN_D2]) && (lok[LOK_ADDR1] == msg[MCAN_D3])
		              && lok[LOK_MAPPED];

		if (!addrMatch) continue;

		{
			u8 i;
			for (i = 0; i < MCAN_MSG_LEN; i++) response[i] = msg[i];
		}

		if (canId == CAN_ID_LOK_RICHTUNG)
		{
			u8 speed;
			u16 cs2Speed;

			response[MCAN_D4] = lok[LOK_DIR];
			SendMsg(response);

			speed = lok[LOK_SPEED];
			cs2Speed = (u16)(speed * MM_SPEED_STEP); // matches McCom_HandleLocoSpeed()'s linear mapping

			response[MCAN_ID]  = CAN_ID_LOK_GESCHWINDIGKEIT;
			response[MCAN_DLC] = 6;
			response[MCAN_D4]  = (u8)((cs2Speed & 0xFF00) >> 8);
			response[MCAN_D5]  = (u8)(cs2Speed & 0xFF);
			SendMsg(response);
		}
		else if (canId == CAN_ID_LOK_FUNKTION)
		{
			switch (msg[MCAN_D4])
			{
				case 0: response[MCAN_D5] = lok[LOK_F0]; break;
				case 1: response[MCAN_D5] = lok[LOK_FX] & 0x01; break;
				case 2: response[MCAN_D5] = (lok[LOK_FX] & 0x02) >> 1; break;
				case 3: response[MCAN_D5] = (lok[LOK_FX] & 0x04) >> 2; break;
				case 4: response[MCAN_D5] = (lok[LOK_FX] & 0x08) >> 3; break;
				default: break;
			}

			SendMsg(response);
		}
		else if (canId == CAN_ID_LOK_GESCHWINDIGKEIT)
		{
			u8 speed = lok[LOK_SPEED];
			u16 cs2Speed;

			cs2Speed = (u16)(speed * MM_SPEED_STEP); // matches McCom_HandleLocoSpeed()'s linear mapping

			response[MCAN_D4] = (u8)((cs2Speed & 0xFF00) >> 8);
			response[MCAN_D5] = (u8)(cs2Speed & 0xFF);
			SendMsg(response);
		}
	}
}

//---------------------------------------------------------------------------------------------------
// Dormant: cuIsReset/cuIsConnected/cuIsPresent are permanently FALSE here, so the reconnect resync
// walk below never runs.
static void ResyncAfterConnect(void)
{
	static const bool cuIsReset = FALSE;
	static const bool cuIsConnected = FALSE;
	static const bool cuIsPresent = FALSE;
	u8 index;

	if (!(cuIsReset && cuIsConnected && cuIsPresent)) return;

	for (index = 0; index < LOKS_ANZAHL; index++)
	{
		u8* lok = LokAt(index);
		u8 msg[MCAN_MSG_LEN];
		u8 fx;

		if (!lok[LOK_MAPPED]) continue;

		for (fx = 0; fx < MCAN_MSG_LEN; fx++) msg[fx] = s_MsgLokSpeed[fx];
		msg[MCAN_D0] = lok[LOK_ADDR4];
		msg[MCAN_D1] = lok[LOK_ADDR3];
		msg[MCAN_D2] = lok[LOK_ADDR2];
		msg[MCAN_D3] = lok[LOK_ADDR1];
		msg[MCAN_D4] = 0;
		msg[MCAN_D5] = 0;
		SendMsg(msg);
		Halt();

		for (fx = 0; fx < MCAN_MSG_LEN; fx++) msg[fx] = s_MsgLokDir[fx];
		msg[MCAN_D0] = lok[LOK_ADDR4];
		msg[MCAN_D1] = lok[LOK_ADDR3];
		msg[MCAN_D2] = lok[LOK_ADDR2];
		msg[MCAN_D3] = lok[LOK_ADDR1];
		msg[MCAN_D4] = DIR_VORWAERTS;
		SendMsg(msg);
		Halt();

		for (fx = 0; fx < MCAN_MSG_LEN; fx++) msg[fx] = s_MsgLokFx[fx];
		msg[MCAN_D0] = lok[LOK_ADDR4];
		msg[MCAN_D1] = lok[LOK_ADDR3];
		msg[MCAN_D2] = lok[LOK_ADDR2];
		msg[MCAN_D3] = lok[LOK_ADDR1];

		for (fx = 0; fx < 5; fx++)
		{
			msg[MCAN_D4] = fx;
			msg[MCAN_D5] = 0;
			SendMsg(msg);
			Halt();
		}
	}
}

//---------------------------------------------------------------------------------------------------
void McCom_Check(const u8* msg)
{
	u8 canId = msg[MCAN_ID];

	if (canId == CAN_ID_SYSTEM_BEFEHLE)
	{
		if (msg[MCAN_DLC] == 5) HandleSystemCommand(msg);
	}
	else if (canId == CAN_ID_CONNECT6021)
	{
		HandleConnect6021(msg);
	}

	HandleLokAbfrage(msg);
	ResyncAfterConnect();
}

//---------------------------------------------------------------------------------------------------
void McCom_HashInit(u8 guid0, u8 guid1, u8 guid2, u8 guid3, u8 hashH, u8 hashL)
{
	s_Guid[0] = guid0; s_Guid[1] = guid1; s_Guid[2] = guid2; s_Guid[3] = guid3;

	s_MsgStopResponse[MCAN_D0] = guid0;
	s_MsgStopResponse[MCAN_D1] = guid1;
	s_MsgStopResponse[MCAN_D2] = guid2;
	s_MsgStopResponse[MCAN_D3] = guid3;

	s_MsgGoResponse[MCAN_D0] = guid0;
	s_MsgGoResponse[MCAN_D1] = guid1;
	s_MsgGoResponse[MCAN_D2] = guid2;
	s_MsgGoResponse[MCAN_D3] = guid3;

	s_MsgStopResponse[MCAN_HASH_H] = hashH;
	s_MsgStopResponse[MCAN_HASH_L] = hashL;
	s_MsgGoResponse[MCAN_HASH_H]   = hashH;
	s_MsgGoResponse[MCAN_HASH_L]   = hashL;

	// s_MsgStop/s_MsgGo (spontaneous broadcasts) get HASH only - their GUiD stays the universal
	// broadcast 0x00000000, never the module's own.
	s_MsgStop[MCAN_HASH_H] = hashH;
	s_MsgStop[MCAN_HASH_L] = hashL;
	s_MsgGo[MCAN_HASH_H]   = hashH;
	s_MsgGo[MCAN_HASH_L]   = hashL;

	s_MsgKonfigOk[MCAN_D0] = guid0;
	s_MsgKonfigOk[MCAN_D1] = guid1;
	s_MsgKonfigOk[MCAN_D2] = guid2;
	s_MsgKonfigOk[MCAN_D3] = guid3;
	s_MsgKonfigOk[MCAN_HASH_H] = hashH;
	s_MsgKonfigOk[MCAN_HASH_L] = hashL;

	McBase_SetPingSw(s_Sw[0], s_Sw[1]);
}

//---------------------------------------------------------------------------------------------------
const u8* McCom_Cs2IndexExtern(u8 laufendeNr)
{
	if (laufendeNr >= MCCOM_ANZKONFIGMODUL) return NULL;

	if (laufendeNr == ROW_SW_MAJ)
	{
		s_Cs2IndexEx[ROW_SW_MAJ][SW_OFFSET_H] = 0;
		s_Cs2IndexEx[ROW_SW_MAJ][SW_OFFSET_L] = s_Sw[0];
	}
	else if (laufendeNr == ROW_SW_MIN)
	{
		s_Cs2IndexEx[ROW_SW_MIN][SW_OFFSET_H] = 0;
		s_Cs2IndexEx[ROW_SW_MIN][SW_OFFSET_L] = s_Sw[1];
	}
	else if (laufendeNr == ROW_MAPPING)
	{
		s_Cs2IndexEx[ROW_MAPPING][MAPPING_OFFSET] = s_Mapping;
	}
	else if (laufendeNr == ROW_BASIS)
	{
		s_Cs2IndexEx[ROW_BASIS][BASIS_OFFSET_H] = 0;
		s_Cs2IndexEx[ROW_BASIS][BASIS_OFFSET_L] = s_Basis;
	}
	else if (laufendeNr == ROW_I2C)
	{
		s_Cs2IndexEx[ROW_I2C][I2C_OFFSET_H] = 0;
		s_Cs2IndexEx[ROW_I2C][I2C_OFFSET_L] = s_I2c;
	}

	return s_Cs2IndexEx[laufendeNr];
}

//---------------------------------------------------------------------------------------------------
void McCom_Cs2IndexParameterExtern(const u8* msg)
{
	u8  kanal = msg[MCAN_D5];
	u16 wert  = (u16)(((u16) msg[MCAN_D6] << 8) | msg[MCAN_D7]);
	bool gueltig;
	bool geaendert = FALSE;

	// SW Major/Minor also refresh the CS2 ping response's D4/D5 immediately - without this, a changed
	// SW Major/Minor only shows up on the NEXT GUiD (re)assignment (McCom_HashInit()), not on the
	// very next ping.
	if (kanal == KANAL_SW_MAJ)
	{
		gueltig = (wert >= 1) && (wert <= 255);
		if (gueltig && (s_Sw[0] != (u8) wert))
		{
			s_Sw[0] = (u8) wert;
			geaendert = TRUE;
			McBase_SetPingSw(s_Sw[0], s_Sw[1]);
		}
	}
	else if (kanal == KANAL_SW_MIN)
	{
		gueltig = (wert <= 255);
		if (gueltig && (s_Sw[1] != (u8) wert))
		{
			s_Sw[1] = (u8) wert;
			geaendert = TRUE;
			McBase_SetPingSw(s_Sw[0], s_Sw[1]);
		}
	}
	else if (kanal == KANAL_MAPPING)
	{
		gueltig = (wert <= 1);

		if (gueltig && (s_Mapping != (u8) wert))
		{
			u8 index;

			s_Mapping = (u8) wert;
			geaendert = TRUE;

			// A CONNECT6021-mapped loco (LOK_MAPPED_ECHT set) is never touched by a Zentralentyp
			// change - only the still-default locos follow the new setting live.
			for (index = 0; index < LOKS_ANZAHL; index++)
			{
				u8* lok = LokAt(index);
				if (!(lok[LOK_MAPPED] & LOK_MAPPED_ECHT)) lok[LOK_MAPPED] = s_Mapping;
			}
		}
	}
	else if (kanal == KANAL_BASIS)
	{
		gueltig = (wert >= 1) && (wert <= 4);
		if (gueltig && (s_Basis != (u8) wert)) { s_Basis = (u8) wert; geaendert = TRUE; }
	}
	else if (kanal == KANAL_I2C)
	{
		gueltig = (wert >= 40) && (wert <= 100);
		if (gueltig && (s_I2c != (u8) wert)) { s_I2c = (u8) wert; geaendert = TRUE; }
	}
	else
	{
		return;
	}

	s_MsgKonfigOk[MCAN_D5] = kanal;
	s_MsgKonfigOk[MCAN_D6] = gueltig ? 0x01 : 0x00;

	// Persists only on a genuine value change, not on every valid-but-unchanged write - a CS2/CSx
	// re-announces its current config periodically, and writing the same file over and over on every
	// such re-announcement is both unnecessary and, in rapid succession, risks the file itself.
	if (geaendert) Store_Save();

	SendMsg(s_MsgKonfigOk);
}

//---------------------------------------------------------------------------------------------------
void McCom_GetPersist(u8* sw2, u8* mapping, u8* basis, u8* i2c)
{
	sw2[0] = s_Sw[0];
	sw2[1] = s_Sw[1];
	*mapping = s_Mapping;
	*basis = s_Basis;
	*i2c = s_I2c;
}

//---------------------------------------------------------------------------------------------------
void McCom_SetPersist(const u8* sw2, u8 mapping, u8 basis, u8 i2c)
{
	if (sw2[0] >= 1) s_Sw[0] = sw2[0];
	s_Sw[1] = sw2[1];
	McBase_SetPingSw(s_Sw[0], s_Sw[1]);

	if (mapping <= 1) s_Mapping = mapping;
	if ((basis >= 1) && (basis <= 4)) s_Basis = basis;
	if ((i2c >= 40) && (i2c <= 100)) s_I2c = i2c;
}

//---------------------------------------------------------------------------------------------------
u8 McCom_GetSw(u8 index)
{
	return s_Sw[index];
}

//---------------------------------------------------------------------------------------------------
bool McCom_SetSw(u8 index, u8 value)
{
	if ((index == 0) && (value < 1)) return FALSE;	// Major: same "wert >= 1" range as the CS2 path
	if (s_Sw[index] == value) return FALSE;
	s_Sw[index] = value;
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
u8 McCom_GetMapping(void)
{
	return s_Mapping;
}

//---------------------------------------------------------------------------------------------------
bool McCom_SetMapping(u8 value)
{
	if (value > 1) return FALSE;
	if (s_Mapping == value) return FALSE;
	s_Mapping = value;
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
u8 McCom_GetBasis(void)
{
	return s_Basis;
}

//---------------------------------------------------------------------------------------------------
bool McCom_SetBasis(u8 value)
{
	if ((value < 1) || (value > 4)) return FALSE;
	if (s_Basis == value) return FALSE;
	s_Basis = value;
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
u8 McCom_GetI2c(void)
{
	return s_I2c;
}

//---------------------------------------------------------------------------------------------------
bool McCom_SetI2c(u8 value)
{
	if ((value < 40) || (value > 100)) return FALSE;
	if (s_I2c == value) return FALSE;
	s_I2c = value;
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
// Writes D2-D5 into the shared template and sends.
static void SendZubehoerSchalten(u16 addr, u8 richtung, bool strom)
{
	s_MsgZubehoerSchalten[MCAN_D2] = (u8)(addr >> 8);
	s_MsgZubehoerSchalten[MCAN_D3] = (u8)(addr & 0xff);
	s_MsgZubehoerSchalten[MCAN_D4] = richtung;
	s_MsgZubehoerSchalten[MCAN_D5] = strom ? 0x01 : 0x00;
	SendMsg(s_MsgZubehoerSchalten);
}

//---------------------------------------------------------------------------------------------------
// Returns simulated Keyboard instance kbIndex's own MM base accessory address (kbIndex*16 offset per
// instance) - the same formula McCom_DecodeKeyboardEvent() must invert, kept as one shared function
// so both directions can never drift apart.
static u16 KeyboardBaseAddr(u8 kbIndex)
{
	return (u16)(ZUBEHOER_MM_BASE + (u16)(s_Basis - 1) * 256 + (u16)kbIndex * 16);
}

//---------------------------------------------------------------------------------------------------
void McCom_HandleKeyboardEvent(u8 kbIndex, u8 col, bool green)
{
	u16 addr = (u16)(KeyboardBaseAddr(kbIndex) + col);
	SendZubehoerSchalten(addr, green ? 1 : 0, TRUE);
}

//---------------------------------------------------------------------------------------------------
bool McCom_DecodeKeyboardEvent(const u8* msg, u8* kbIndex, u8* col, bool* green)
{
	u16 familyBase, addr, offset;

	if (msg[MCAN_ID] != CAN_ID_ZUBEHOER_SCHALTEN) return FALSE;
	if (msg[MCAN_DLC] != 6) return FALSE; // other 0x16 sub-messages (MD_ALIVE etc.) use DLC=8

	familyBase = KeyboardBaseAddr(0);
	addr = (u16)(((u16)msg[MCAN_D2] << 8) | msg[MCAN_D3]);
	if (addr < familyBase) return FALSE;
	offset = (u16)(addr - familyBase);
	if (offset >= (u16)(g_KeyboardCount * 16)) return FALSE; // runtime count (KEYB env var), not MAX

	*kbIndex = (u8)(offset / 16);
	*col = (u8)(offset % 16);
	*green = (bool)(msg[MCAN_D4] != 0);
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
// The display convention "1..80" maps directly onto the array index EXCEPT for 80, which (like
// HandleConnect6021()'s msg[MCAN_D4]==80 case) is stored at index 0.
static u8* LokAtAddr(u8 addr)
{
	return LokAt((addr == 80) ? 0 : addr);
}

//---------------------------------------------------------------------------------------------------
bool McCom_IsLokControllable(u8 addr)
{
	u8* lok = LokAtAddr(addr);
	return (bool)(lok[LOK_MAPPED] && !lok[LOK_INFRA]);
}

//---------------------------------------------------------------------------------------------------
void McCom_GetLokState(u8 addr, u8* speed, u8* dir, bool* f0, u8* fx)
{
	u8* lok = LokAtAddr(addr);
	*speed = lok[LOK_SPEED];
	*dir   = lok[LOK_DIR];
	*f0    = (bool)(lok[LOK_F0] != 0);
	*fx    = lok[LOK_FX];
}

//---------------------------------------------------------------------------------------------------
// cs2Speed = mmSpeed*MM_SPEED_STEP for all mmSpeed 0..15 - every step is distinct and linear (the
// GUI's speed knob is a purely digital control, so every discrete step should produce a different
// speed). Only sends on an actual value change.
void McCom_HandleLocoSpeed(u8 addr, u8 mmSpeed)
{
	u8* lok = LokAtAddr(addr);
	static u8 msg[MCAN_MSG_LEN];
	u16 cs2Speed;
	u8 i;

	if (!McCom_IsLokControllable(addr)) return;
	if (mmSpeed == lok[LOK_SPEED]) return;

	cs2Speed = (u16)(mmSpeed * MM_SPEED_STEP);

	for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokSpeed[i];
	msg[MCAN_D0] = lok[LOK_ADDR4];
	msg[MCAN_D1] = lok[LOK_ADDR3];
	msg[MCAN_D2] = lok[LOK_ADDR2];
	msg[MCAN_D3] = lok[LOK_ADDR1];
	msg[MCAN_D4] = (u8)((cs2Speed & 0xff00) >> 8);
	msg[MCAN_D5] = (u8)(cs2Speed & 0xff);
	lok[LOK_SPEED] = mmSpeed;

	SendMsg(msg);
}

//---------------------------------------------------------------------------------------------------
// dir: DIR_VORWAERTS (1) or 0 (rueckwaerts).
void McCom_HandleLocoDir(u8 addr, u8 dir)
{
	u8* lok = LokAtAddr(addr);
	static u8 msg[MCAN_MSG_LEN];
	u8 i;

	if (!McCom_IsLokControllable(addr)) return;
	if (dir == lok[LOK_DIR]) return;

	for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokDir[i];
	msg[MCAN_D0] = lok[LOK_ADDR4];
	msg[MCAN_D1] = lok[LOK_ADDR3];
	msg[MCAN_D2] = lok[LOK_ADDR2];
	msg[MCAN_D3] = lok[LOK_ADDR1];
	msg[MCAN_D4] = dir;
	lok[LOK_DIR] = dir;

	SendMsg(msg);
}

//---------------------------------------------------------------------------------------------------
// F0 is its own lok[] field (LOK_F0), not part of the F1-F4 bitmask (LOK_FX), and its CAN message
// uses D4=0 (function index "0") to distinguish it from McCom_HandleLocoFx()'s D4=1..4.
void McCom_HandleLocoF0(u8 addr, bool on)
{
	u8* lok = LokAtAddr(addr);
	static u8 msg[MCAN_MSG_LEN];
	u8 i;
	u8 val = on ? 1 : 0;

	if (!McCom_IsLokControllable(addr)) return;
	if (val == lok[LOK_F0]) return;

	for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokFx[i];
	msg[MCAN_D0] = lok[LOK_ADDR4];
	msg[MCAN_D1] = lok[LOK_ADDR3];
	msg[MCAN_D2] = lok[LOK_ADDR2];
	msg[MCAN_D3] = lok[LOK_ADDR1];
	msg[MCAN_D4] = 0;
	msg[MCAN_D5] = val;
	lok[LOK_F0] = val;

	SendMsg(msg);
}

//---------------------------------------------------------------------------------------------------
// fxIndex 1..4 selects the bit within lok[LOK_FX] (bit0=F1..bit3=F4), D4=fxIndex in the CAN message
// distinguishes it from F0.
void McCom_HandleLocoFx(u8 addr, u8 fxIndex, bool on)
{
	u8* lok = LokAtAddr(addr);
	static u8 msg[MCAN_MSG_LEN];
	u8 i;
	u8 bit = (u8)(1 << (fxIndex - 1));
	bool wasOn = (bool)((lok[LOK_FX] & bit) != 0);

	if (!McCom_IsLokControllable(addr)) return;
	if (on == wasOn) return;

	for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokFx[i];
	msg[MCAN_D0] = lok[LOK_ADDR4];
	msg[MCAN_D1] = lok[LOK_ADDR3];
	msg[MCAN_D2] = lok[LOK_ADDR2];
	msg[MCAN_D3] = lok[LOK_ADDR1];
	msg[MCAN_D4] = fxIndex;
	msg[MCAN_D5] = on ? 1 : 0;
	lok[LOK_FX] = on ? (u8)(lok[LOK_FX] | bit) : (u8)(lok[LOK_FX] & ~bit);

	SendMsg(msg);
}

//---------------------------------------------------------------------------------------------------
// Force-sends the CURRENT speed/dir/F0/F1-F4 state for addr, unlike McCom_HandleLoco*() above, which
// only send on an actual value change - unsuitable for an explicit "announce current state"
// confirmation, since echoing back exactly what was just read would always compare equal and get
// silently skipped. Call whenever mbc89 wants to declare its own view of a loco's state authoritative
// - right after it gets freshly CONNECT6021-mapped (the CS3's own idea of the loco's state may be
// unrelated/stale) and right after a CU6021/Control80f panel takes the loco over
// (mbc89_gui.c's SyncFromLokState()).
void McCom_SyncLokState(u8 addr)
{
	u8* lok = LokAtAddr(addr);
	static u8 msg[MCAN_MSG_LEN];
	u16 cs2Speed;
	u8 i, fxIndex;

	if (!McCom_IsLokControllable(addr)) return;

	cs2Speed = (u16)(lok[LOK_SPEED] * MM_SPEED_STEP);
	for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokSpeed[i];
	msg[MCAN_D0] = lok[LOK_ADDR4];
	msg[MCAN_D1] = lok[LOK_ADDR3];
	msg[MCAN_D2] = lok[LOK_ADDR2];
	msg[MCAN_D3] = lok[LOK_ADDR1];
	msg[MCAN_D4] = (u8)((cs2Speed & 0xff00) >> 8);
	msg[MCAN_D5] = (u8)(cs2Speed & 0xff);
	SendMsg(msg);

	for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokDir[i];
	msg[MCAN_D0] = lok[LOK_ADDR4];
	msg[MCAN_D1] = lok[LOK_ADDR3];
	msg[MCAN_D2] = lok[LOK_ADDR2];
	msg[MCAN_D3] = lok[LOK_ADDR1];
	msg[MCAN_D4] = lok[LOK_DIR];
	SendMsg(msg);

	for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokFx[i];
	msg[MCAN_D0] = lok[LOK_ADDR4];
	msg[MCAN_D1] = lok[LOK_ADDR3];
	msg[MCAN_D2] = lok[LOK_ADDR2];
	msg[MCAN_D3] = lok[LOK_ADDR1];
	msg[MCAN_D4] = 0;
	msg[MCAN_D5] = lok[LOK_F0];
	SendMsg(msg);

	for (fxIndex = 1; fxIndex <= 4; fxIndex++)
	{
		u8 bit = (u8)(1 << (fxIndex - 1));
		for (i = 0; i < MCAN_MSG_LEN; i++) msg[i] = s_MsgLokFx[i];
		msg[MCAN_D0] = lok[LOK_ADDR4];
		msg[MCAN_D1] = lok[LOK_ADDR3];
		msg[MCAN_D2] = lok[LOK_ADDR2];
		msg[MCAN_D3] = lok[LOK_ADDR1];
		msg[MCAN_D4] = fxIndex;
		msg[MCAN_D5] = (lok[LOK_FX] & bit) ? 1 : 0;
		SendMsg(msg);
	}
}

//---------------------------------------------------------------------------------------------------
// Sends the CU6021's own Stopp/Go broadcast (s_MsgStop/s_MsgGo, HASH already set by McCom_HashInit())
// - immediate send on button press, no pulse-timing simulation. Unlike loco control, NOT gated by
// McCom_IsLokControllable() - track power always works.
void McCom_HandleStopGo(bool go)
{
	s_SystemStopped = !go;
	SendMsg(go ? s_MsgGo : s_MsgStop);
}

//---------------------------------------------------------------------------------------------------
// Read-only: current system Stopp/Go state, kept in sync by BOTH McCom_HandleStopGo() (own button
// press) and HandleSystemCommand() (incoming CAN-ID 0x00 from CS2/ParaCenter/elsewhere) - call from
// mbc89_gui.c whenever the CU6021 panel is shown to keep its GO-LED honest regardless of source.
bool McCom_IsSystemStopped(void)
{
	return s_SystemStopped;
}
