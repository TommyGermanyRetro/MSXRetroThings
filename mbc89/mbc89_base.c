//***************************************************************************************************
//* SOURCE - mbc89_base.c - PC-/CS2 registration and config-channel state machine                    *
//***************************************************************************************************
//* Datei: mbc89_base.c                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 17/09/26                                                                                 *
//***************************************************************************************************
#include "mbc89_base.h"
#include "mbc89_can.h"
#include "mbc89_com.h"
#include "mbc89_sim.h"
#include "mbc89_store.h"
#include "rcx_io.h"

//===================================================================================================
// CAN COMMAND / LOC-ID CONSTANTS
//===================================================================================================

#define CAN_ID_SYSTEM_BEFEHLE           0x00
#define CAN_ID_SYSTEM_BEFEHLE_RESPONSE  0x01
#define CAN_ID_PING                     0x30
#define CAN_ID_PING_RESPONSE            0x31
#define CAN_ID_KONFIG                   0x3A
#define CAN_ID_KONFIG_RESPONSE          0x3B
#define CAN_ID_ZUBEHOER_SCHALTEN        0x16

#define CAN_ID_SYSTEM_BEFEHLE_SUB_MW    0x0B

#define CAN_LOCID_MBCAN                 0x18

// Second byte of the 0x18xx PC Loc-ID.
#define PC_DB_H       0x00
#define PC_DB_M       0x01
#define PC_DB_L       0x02
#define PC_KENNER     0x03
#define PC_NEU        0x04
#define PC_NEU_DATA   0x05
#define MD_NEU_DATA   0x06
#define PC_RESET      0x07
#define PC_MD_DEL     0x08
#define PC_ALIVE      0x09
#define MD_ALIVE      0x0A
#define PC_ARRAY      0x0B
#define MD_ARRAY      0x0C
#define PC_ARRAY_DATA 0x0D
#define MD_ARRAY_DATA 0x0E

// Sentinel PC_ARRAY_DATA indices (outside the real array's 0..ARR_TOTAL-1 range).
#define MCAN_EEPROM 0xFFFD
#define MCAN_READ   0xFFFE
#define MCAN_WRITE  0xFFFF

#define MBC_COM_ANZMESSWERTE   2
#define MBC_COM_ANZKONFIG      4
#define MBC_COM_CS2PAKETE      5

// This module always has a fixed CS2 device Kennung - the Maerklin-specified Connect6021
// identification, never assigned by ParaCenter like a generic module's Kennung.
#define MCAN_KENNER_FIXED_H 0x00
#define MCAN_KENNER_FIXED_L 0x20

//===================================================================================================
// SYSTEM ARRAY LAYOUT (Plug/Voltage/Temperature live in mbc89_sim.c's g_SimArr and are read from
// there directly, not duplicated here)
//===================================================================================================

#define O_SNR    1	// 8 bytes
#define O_ART    9	// 4 bytes
#define O_SW     13	// 3 bytes (ASCII digits, e.g. '1','0','0' = V1.00)
#define O_HW     16	// 6 bytes
#define O_NAME   22	// 20 bytes
#define MBC_INFO_START  O_SNR
#define MBC_INFO_ENDE   41	// O_NAME + 20 - 1

#define O_UID    42	// 4 bytes (GUID)
#define O_DB     46	// 12 bytes
#define O_KN_H   58
#define O_KN_L   59
#define O_CS2GER 62	// KN block byte 4

#define ARR_SIZE 63

static u8 s_Arr[ARR_SIZE];

// PC_ARRAY_DATA (ParaCenter's byte-by-byte systemarray read/write) exposes a unified view of the
// systemarray's 0..69 range: 0..62 from s_Arr[] above, 63..69 (Plug/Voltage/Temperature) from
// mbc89_sim.c's g_SimArr[]. Beyond that, the loco DB (MBC_89_S_LOKS below) and 5 module-specific
// config fields (MBC_89_S_SW/MAPPING/BASIS/I2C below) are bridged too; every other index is
// silently ignored.
#define ARR_TOTAL 70

// Absolute systemarray offset where the loco database starts. SW Major/Minor occupy the 2 bytes
// right before it (MBC_89_S_SW). ParaCenter's "Lokeigenschaften" dialog and its module-properties
// "Stammdaten" dialog both address the systemarray at these fixed absolute offsets, independent of
// how this port lays out its own loco array internally.
#define MBC_89_S_SW   328	// 2 bytes, SW Major/Minor
#define MBC_89_S_LOKS 330

// Absolute systemarray offsets for 3 more module-specific config fields (Zentralentyp-Mapping,
// Keyboard-Basisadresse, I2C-Bus-Takt) that ParaCenter's "Stammdaten" dialog reads/writes via the
// same PC_ARRAY_DATA byte protocol - a separate path from the CS2 config-channel protocol
// (McCom_Cs2IndexParameterExtern() below). These sit past a reserved per-keyboard protocol block
// (not bridged - out of this port's scope) that follows the loco database.
#define MBC_89_S_MAPPING 1205
#define MBC_89_S_BASIS   1206
#define MBC_89_S_I2C     1207

//---------------------------------------------------------------------------------------------------
static u8 ReadArrByte(u16 index)
{
	return (index < ARR_SIZE) ? s_Arr[index] : g_SimArr[index - ARR_SIZE];
}

// Returns TRUE only if the byte actually changed - a valid-but-identical rewrite is not a real
// change and should not trigger a persist.
static bool WriteArrByte(u16 index, u8 value)
{
	if (index < ARR_SIZE)
	{
		if (s_Arr[index] == value) return FALSE;
		s_Arr[index] = value;
	}
	else
	{
		if (g_SimArr[index - ARR_SIZE] == value) return FALSE;
		g_SimArr[index - ARR_SIZE] = value;
	}
	return TRUE;
}

//===================================================================================================
// CS2 CONFIG-CHANNEL TABLE
//===================================================================================================

#define IDX_0     0
#define IDX_1_1   1
#define IDX_1_2   2
#define IDX_2_1   3
#define IDX_2_2   4
#define IDX_2_3   5
#define IDX_2_4   6

#define INDEX_0_ART_NR  11
#define INDEX_0_TYP     26
#define INDEX_0_SERNR   4

#define INDEX_2_1_SERNRDS  15
#define INDEX_2_2_GU_1     15
#define INDEX_2_2_GU_2     17
#define INDEX_2_2_GU_3     18
#define INDEX_2_2_GU_4     22
#define INDEX_2_3_FW_1     13
#define INDEX_2_3_FW_2     15
#define INDEX_2_3_FW_3     16
#define INDEX_2_4_HW_1     13
#define INDEX_2_4_HW_2     14
#define INDEX_2_4_HW_3     16
#define INDEX_2_4_HW_4     17
#define INDEX_2_4_HW_5     19
#define INDEX_2_4_HW_6     20

static u8 s_Cs2Index[7][40] =
{
	// IDX_0 - device info
	{   2,4,0x00,0x00, 0x00,0x00,0x00,0x00,
	    'M','B','C', 0x00,0x00,0x00,0x00, ' ',
	    'M','B','C','A','N',' ','m','b',
	    'c','-', 0x00,0x00, 0x00, 0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// IDX_1_1 - voltage (raw value = 50*Volt-500)
	{   1,0x00,0xc3,0xd2,0x31,0x08,0x00,0x00,
	    0x00,0x32,0x01,0x2c,0x02,0x58,0x03,0x52,
	    'V','O','L','T',0x00,'1','0','.',
	    '0','0',0x00,'2','7','.','0','0',
	    0x00,'V',0x00,0x00,0x00,0x00,0x00,0x00
	},
	// IDX_1_2 - temperature
	{   2,0x00,0x0c,0x08,0xf0,0xc0,0x00,0x00,
	    0x00,0x7D,0x00,0x96,0x00,0xaf,0x00,0xc8,
	    'T','E','M','P',0x00,'0','.','0',
	    0x00,'8','0','.','0',0x00,'C',0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// IDX_2_1 - serial number
	{   3, 0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	    'S','/','N',':',0x00,'0','x',0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// IDX_2_2 - GUiD
	{   4, 0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	    'G','U','i','D',':',0x00,'#',0x00,
	    '-',0x00,0x00,'-','0','x',0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// IDX_2_3 - firmware
	{   5, 0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	    'F','W',':',0x00,'#',0x00,'.',0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	},
	// IDX_2_4 - hardware
	{   6, 0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	    'H','W',':',0x00,'#',0x00,0x00,'.',
	    0x00,0x00,'.',0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	}
};

//===================================================================================================
// CAN MESSAGE TEMPLATES
//===================================================================================================

#define MSG_INFO             0	// PC registration walk (MD_NEU_DATA)
#define MSG_ACK_MODUL        1	// PC_ALIVE ack (MD_ALIVE)
#define MSG_CS2_0x30_R       2	// CS2 ping response
#define MSG_CS2_0x3A_R_8     3	// CS2 config-channel data packet
#define MSG_CS2_0x3A_R_6     4	// CS2 config-channel end marker
#define MSG_CS2_0x00_R_8_0x0B 5	// CS2 messwert response
#define MSG_ACK_IOREQUEST     6	// PC_ARRAY ack (MD_ARRAY)
#define MSG_ACK_IOACT         7	// PC_ARRAY_DATA per-byte ack (MD_ARRAY_DATA)
#define MSG_COUNT             8

static u8 s_CanMsg[MSG_COUNT][MCAN_MSG_LEN] =
{
	{0x00,CAN_ID_ZUBEHOER_SCHALTEN,0x03,0x00,0x08,0x00,0x00,CAN_LOCID_MBCAN,MD_NEU_DATA,0x00,0x00,0x00,0x00},
	{0x00,CAN_ID_ZUBEHOER_SCHALTEN,0x03,0x00,0x08,0x00,0x00,CAN_LOCID_MBCAN,MD_ALIVE,0x00,0x00,0x00,0x00},
	{0x00,CAN_ID_PING_RESPONSE,0x03,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0xaa,0xaa},
	{0x00,CAN_ID_KONFIG_RESPONSE,0x03,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x00,CAN_ID_KONFIG_RESPONSE,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x00,CAN_ID_SYSTEM_BEFEHLE_RESPONSE,0x03,0x00,0x08,0x00,0x00,0x00,0x00,CAN_ID_SYSTEM_BEFEHLE_SUB_MW,0x00,0x00,0x00},
	{0x00,CAN_ID_ZUBEHOER_SCHALTEN,0x03,0x00,0x08,0x00,0x00,CAN_LOCID_MBCAN,MD_ARRAY,0x00,0x00,0x00,0x00},
	{0x00,CAN_ID_ZUBEHOER_SCHALTEN,0x03,0x00,0x08,0x00,0x00,CAN_LOCID_MBCAN,MD_ARRAY_DATA,0x00,0x00,0x00,0x00}
};

//===================================================================================================
// PROTOCOL STATE
//===================================================================================================

static bool s_InitDone;
static bool s_InitStartup;
static bool s_NewPhase;
static u8   s_Step;
static u8   s_SysIndex;

static bool s_CsInit;
static bool s_CsStartup;
static bool s_CsConnect;
static bool s_CsAutoMode;

static bool s_DbH, s_DbM, s_DbL, s_DbValid;
static u8   s_Db[12];

static bool s_IoTransfer;

// Set TRUE by the PC_ARRAY_DATA handler whenever a byte write during the current transfer actually
// changed something (not just a valid-but-identical rewrite) - checked and cleared at the
// MCAN_EEPROM commit, which only persists when this is TRUE.
static bool s_PcArrayDirty;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// 4-bit hex value -> uppercase ASCII.
static u8 Hex2Ascii(u8 digit)
{
	return (digit < 10) ? (u8)('0' + digit) : (u8)('A' - 10 + digit);
}

//---------------------------------------------------------------------------------------------------
// Default module name for an unconfigured module: "0x" + 16 hex ASCII digits of the 8-byte serial
// number (O_SNR), terminated by 0x81 0x81. Call once from McBase_Init(), after O_SNR is set -
// overwritten by Store_Load() if a persisted (or ParaCenter-assigned) name exists on disk.
static void NameFromSerial(void)
{
	u8 p = O_NAME;

	s_Arr[p++] = '0';
	s_Arr[p]   = 'x';

	for (u8 i = 0; i < 8; i++)
	{
		p++;
		s_Arr[p] = Hex2Ascii((s_Arr[O_SNR + i] & 0xF0) >> 4);
		p++;
		s_Arr[p] = Hex2Ascii(s_Arr[O_SNR + i] & 0x0F);
	}

	p++;
	s_Arr[p++] = 0x81;
	s_Arr[p]   = 0x81;
}

//---------------------------------------------------------------------------------------------------
static void SendMsg(const u8* msg)
{
	McanEncode(msg, g_CanTxBuf);
	Can_Tx(g_CanAddr, g_CanTxBuf);
}

//---------------------------------------------------------------------------------------------------
static void ModuleFlagsReset(void)
{
	s_InitDone = FALSE;
	s_InitStartup = FALSE;
	s_NewPhase = FALSE;
	s_Step = 0;
	s_SysIndex = 0;

	s_CsInit = FALSE;
	s_CsStartup = TRUE;
	s_CsConnect = FALSE;
	s_CsAutoMode = FALSE;

	s_DbH = FALSE; s_DbM = FALSE; s_DbL = FALSE; s_DbValid = FALSE;

	s_IoTransfer = FALSE;
}

//---------------------------------------------------------------------------------------------------
// Clears database number and GUiD (module drops out of the PC database) and resets all flags.
static void ModuleBlank(void)
{
	bool wasBlank = (s_Arr[O_UID] == 0xFF);

	for (u8 i = 0; i < 12; i++) { s_Db[i] = 0; s_Arr[O_DB + i] = '0'; }
	for (u8 i = 0; i < 4; i++) s_Arr[O_UID + i] = 0xFF;

	ModuleFlagsReset();

	// Persist the "unregistered" state too - without this, a module deleted in ParaCenter would
	// come back with its old GUiD/DB after the next restart. Only on a genuine transition, not on a
	// repeated delete of an already-blank module.
	if (!wasBlank) Store_Save();
}

//---------------------------------------------------------------------------------------------------
// Computes the CS2 HASH value and fills every field derived from it - GUiD/Kennung/HASH on the
// message templates, hardware version and GUiD into the config-channel table.
static void HashInit(void)
{
	u16 uidH = (u16)(((u16)s_Arr[O_CS2GER] << 8) | s_Arr[O_UID + 1]);
	u16 uidL = (u16)(((u16)s_Arr[O_UID + 2] << 8) | s_Arr[O_UID + 3]);
	u16 uidHashG = uidH ^ uidL;
	u16 uidHashH = (uidHashG >> 8) & 0x00FF;
	u16 uidHashL = (u16)((uidHashG & 0x00FF) << 8);
	u16 uidHash  = (uidHashL | uidHashH) | 0x0300;

	for (u8 i = 0; i < MSG_COUNT; i++)
	{
		s_CanMsg[i][MCAN_HASH_H] = (u8)((uidHash & 0xFF00) >> 8);
		s_CanMsg[i][MCAN_HASH_L] = (u8)(uidHash & 0x00FF);
	}

	s_CanMsg[MSG_CS2_0x30_R][MCAN_D0] = s_Arr[O_CS2GER];
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D1] = s_Arr[O_UID + 1];
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D2] = s_Arr[O_UID + 2];
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D3] = s_Arr[O_UID + 3];
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D4] = (u8)(s_Arr[O_SW] - '0');
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D5] = (u8)((s_Arr[O_SW + 1] - '0') * 10 + (s_Arr[O_SW + 2] - '0'));

	s_Arr[O_KN_H] = MCAN_KENNER_FIXED_H;
	s_Arr[O_KN_L] = MCAN_KENNER_FIXED_L;

	s_CanMsg[MSG_CS2_0x30_R][MCAN_D6] = s_Arr[O_KN_H];
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D7] = s_Arr[O_KN_L];

	s_Cs2Index[IDX_2_4][INDEX_2_4_HW_1] = s_Arr[O_HW];
	s_Cs2Index[IDX_2_4][INDEX_2_4_HW_2] = s_Arr[O_HW + 1];
	s_Cs2Index[IDX_2_4][INDEX_2_4_HW_3] = s_Arr[O_HW + 2];
	s_Cs2Index[IDX_2_4][INDEX_2_4_HW_4] = s_Arr[O_HW + 3];
	s_Cs2Index[IDX_2_4][INDEX_2_4_HW_5] = s_Arr[O_HW + 4];
	s_Cs2Index[IDX_2_4][INDEX_2_4_HW_6] = s_Arr[O_HW + 5];

	u16 sernum = (u16)(s_Arr[O_UID + 3] + 1);
	s_Cs2Index[IDX_0][INDEX_0_SERNR + 2] = (u8)((sernum & 0xFF00) >> 8);
	s_Cs2Index[IDX_0][INDEX_0_SERNR + 3] = (u8)(sernum & 0xFF);

	s_Cs2Index[IDX_2_2][INDEX_2_2_GU_1] = s_Arr[O_UID];
	s_Cs2Index[IDX_2_2][INDEX_2_2_GU_2] = s_Arr[O_UID + 1];
	s_Cs2Index[IDX_2_2][INDEX_2_2_GU_3] = s_Arr[O_UID + 2];
	s_Cs2Index[IDX_2_2][INDEX_2_2_GU_4]     = Hex2Ascii((s_Arr[O_UID + 3] & 0xF0) >> 4);
	s_Cs2Index[IDX_2_2][INDEX_2_2_GU_4 + 1] = Hex2Ascii(s_Arr[O_UID + 3] & 0x0F);

	// Config-channel end marker (0x3A_R_6) and messwert response (0x00_R_8_0x0B) also carry the
	// GUiD in D0-D3 - the CSx uses this to correlate the "walk complete" signal with the device
	// that sent it. Without it, the terminator arrives with D0-D3=0x00 and the CSx never advances
	// past channel 0.
	s_CanMsg[MSG_CS2_0x3A_R_6][MCAN_D0] = s_Arr[O_CS2GER];
	s_CanMsg[MSG_CS2_0x3A_R_6][MCAN_D1] = s_Arr[O_UID + 1];
	s_CanMsg[MSG_CS2_0x3A_R_6][MCAN_D2] = s_Arr[O_UID + 2];
	s_CanMsg[MSG_CS2_0x3A_R_6][MCAN_D3] = s_Arr[O_UID + 3];

	s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D0] = s_Arr[O_CS2GER];
	s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D1] = s_Arr[O_UID + 1];
	s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D2] = s_Arr[O_UID + 2];
	s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D3] = s_Arr[O_UID + 3];

	// mbc89_com.c's own message templates (Stopp/Go, Konfig-Ok) share the same GUiD/HASH.
	McCom_HashInit(s_Arr[O_CS2GER], s_Arr[O_UID + 1], s_Arr[O_UID + 2], s_Arr[O_UID + 3],
	               s_CanMsg[MSG_CS2_0x30_R][MCAN_HASH_H], s_CanMsg[MSG_CS2_0x30_R][MCAN_HASH_L]);
}

//---------------------------------------------------------------------------------------------------
void McBase_SetPingSw(u8 major, u8 minor)
{
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D4] = major;
	s_CanMsg[MSG_CS2_0x30_R][MCAN_D5] = minor;
}

//---------------------------------------------------------------------------------------------------
void McBase_GetPersist(u8* guid4, u8* cs2Ger, u8* db12, u8* name20)
{
	for (u8 i = 0; i < 4; i++) guid4[i] = s_Arr[O_UID + i];
	*cs2Ger = s_Arr[O_CS2GER];
	for (u8 i = 0; i < 12; i++) db12[i] = s_Arr[O_DB + i];
	for (u8 i = 0; i < 20; i++) name20[i] = s_Arr[O_NAME + i];
}

//---------------------------------------------------------------------------------------------------
void McBase_SetPersist(const u8* guid4, u8 cs2Ger, const u8* db12, const u8* name20)
{
	for (u8 i = 0; i < 4; i++) s_Arr[O_UID + i] = guid4[i];
	s_Arr[O_CS2GER] = cs2Ger;
	for (u8 i = 0; i < 12; i++) { s_Arr[O_DB + i] = db12[i]; s_Db[i] = db12[i]; }
	for (u8 i = 0; i < 20; i++) s_Arr[O_NAME + i] = name20[i];

	// A loaded GUiD is either still the "unregistered" sentinel (0xFF x4, McBase_Init()'s own
	// default - HashInit() below is harmless either way) or a real one from a prior PC registration -
	// either way HashInit() must run so every GUiD-derived template (ping/config-channel/ack
	// messages) reflects the loaded value instead of McBase_Init()'s blank defaults.
	HashInit();
}

//---------------------------------------------------------------------------------------------------
static void GuidInit(void)
{
	s_CanMsg[MSG_ACK_MODUL][MCAN_D4] = s_Arr[O_UID];
	s_CanMsg[MSG_ACK_MODUL][MCAN_D5] = s_Arr[O_UID + 1];
	s_CanMsg[MSG_ACK_MODUL][MCAN_D6] = s_Arr[O_UID + 2];
	s_CanMsg[MSG_ACK_MODUL][MCAN_D7] = s_Arr[O_UID + 3];

	s_CanMsg[MSG_ACK_IOREQUEST][MCAN_D4] = s_Arr[O_UID];
	s_CanMsg[MSG_ACK_IOREQUEST][MCAN_D5] = s_Arr[O_UID + 1];
	s_CanMsg[MSG_ACK_IOREQUEST][MCAN_D6] = s_Arr[O_UID + 2];
	s_CanMsg[MSG_ACK_IOREQUEST][MCAN_D7] = s_Arr[O_UID + 3];
}

//---------------------------------------------------------------------------------------------------
// Evaluates PC communication (Loc-ID 0x18xx): database sync, registration ("Neuanmeldeautomat"),
// PC_ALIVE/PC_MD_DEL. PC_RESET clears state without an actual program restart (no reset-pin/
// software-reset equivalent exists yet on this MSX port). PC_ARRAY(_DATA)/PC_UPGRADE(_DATA)/
// PC_BOOT are out of scope (see mbc89_base.h).
static void CheckPcConnection(const u8* msg, bool isPcGuid)
{
	if (s_DbH && s_DbM && s_DbL)
	{
		s_DbValid = TRUE;

		for (u8 i = 0; i < 12; i++)
		{
			if (s_Arr[O_DB + i] != s_Db[i])
			{
				s_DbValid = FALSE;
				s_Arr[O_DB + i] = s_Db[i];
			}
		}

		if (!s_DbValid) Store_Save();

		if (s_InitDone && !s_DbValid)
		{
			s_Arr[O_UID] = 0xFF; s_Arr[O_UID + 1] = 0xFF; s_Arr[O_UID + 2] = 0xFF; s_Arr[O_UID + 3] = 0xFF;

			s_NewPhase = FALSE;
			s_InitDone = FALSE;
			s_CsAutoMode = FALSE;
		}

		s_DbH = FALSE; s_DbM = FALSE; s_DbL = FALSE; s_DbValid = TRUE;
	}

	switch (msg[MCAN_D3])
	{
		case PC_KENNER:
		{
			if (msg[MCAN_D5] != s_Arr[O_CS2GER])
			{
				s_Arr[O_CS2GER] = msg[MCAN_D5];
				s_Arr[O_UID] = s_Arr[O_CS2GER];
				HashInit();
				Store_Save();
			}

			// mbc-89 always has a fixed Kennung (MCAN_KENNER_FIXED_H/L) - unlike a generic
			// module, ParaCenter's own D6/D7 proposal (msg[MCAN_D6]/msg[MCAN_D7]) is ignored.
			if ((s_Arr[O_KN_H] != MCAN_KENNER_FIXED_H) || (s_Arr[O_KN_L] != MCAN_KENNER_FIXED_L))
			{
				HashInit();
			}
		}
		break;

		case PC_DB_H:
			for (u8 i = 0; i < 4; i++) s_Db[i] = msg[MCAN_D4 + i];
			s_DbH = TRUE;
		break;

		case PC_DB_M:
			for (u8 i = 4; i < 8; i++) s_Db[i] = msg[MCAN_D4 + i - 4];
			s_DbM = TRUE;
		break;

		case PC_DB_L:
			for (u8 i = 8; i < 12; i++) s_Db[i] = msg[MCAN_D4 + i - 8];
			s_DbL = TRUE;
		break;

		case PC_NEU:
			if (!s_InitDone && (msg[MCAN_D7] == 0) && s_DbValid)
			{
				s_NewPhase = TRUE;
				s_Step = 1;
				s_SysIndex = MBC_INFO_START;

				s_CanMsg[MSG_INFO][MCAN_D6] = s_Step;
				s_CanMsg[MSG_INFO][MCAN_D7] = s_Arr[s_SysIndex];

				SendMsg(s_CanMsg[MSG_INFO]);
			}
		break;

		case PC_NEU_DATA:
			if (!s_InitDone && s_NewPhase)
			{
				if ((s_Step == msg[MCAN_D6]) && (s_Arr[s_SysIndex] == msg[MCAN_D7]))
				{
					if (s_SysIndex <= MBC_INFO_ENDE)
					{
						s_Step++;
						s_SysIndex++;

						s_CanMsg[MSG_INFO][MCAN_D6] = s_Step;
						s_CanMsg[MSG_INFO][MCAN_D7] = s_Arr[s_SysIndex];

						SendMsg(s_CanMsg[MSG_INFO]);
					}
					else
					{
						bool guidChanged = (s_Arr[O_UID] != s_Arr[O_CS2GER]) || (s_Arr[O_UID + 1] != s_Arr[O_ART])
						                 || (s_Arr[O_UID + 2] != s_Arr[O_ART + 1]) || (s_Arr[O_UID + 3] != msg[MCAN_D5]);

						s_Arr[O_UID]     = s_Arr[O_CS2GER];
						s_Arr[O_UID + 1] = s_Arr[O_ART];
						s_Arr[O_UID + 2] = s_Arr[O_ART + 1];
						s_Arr[O_UID + 3] = msg[MCAN_D5];

						GuidInit();

						s_InitDone = TRUE;
						s_InitStartup = TRUE;
						s_NewPhase = FALSE;

						HashInit();

						// A re-registration commonly re-derives the SAME GUiD - only persist on an
						// actual change, not on every routine reconnect.
						if (guidChanged) Store_Save();
					}
				}
				else
				{
					s_InitDone = FALSE;
					s_NewPhase = FALSE;
					s_CsAutoMode = FALSE;
					s_Step = 0;
					s_SysIndex = 0;
				}
			}
		break;

		default: break;
	}

	if (s_InitDone && isPcGuid)
	{
		switch (msg[MCAN_D3])
		{
			case PC_RESET:    ModuleFlagsReset(); break;
			case PC_MD_DEL:   ModuleBlank(); break;
			case PC_ALIVE:    SendMsg(s_CanMsg[MSG_ACK_MODUL]); break;
			case PC_ARRAY:    SendMsg(s_CanMsg[MSG_ACK_IOREQUEST]); s_IoTransfer = TRUE; break;
			default: break;
		}
	}
	else if (s_InitDone && s_IoTransfer)
	{
		switch (msg[MCAN_D3])
		{
			case PC_ARRAY:
				s_IoTransfer = FALSE;
			break;

			case PC_ARRAY_DATA:
			{
				u16 index = (u16)(((u16)msg[MCAN_D5] << 8) | msg[MCAN_D6]);

				s_CanMsg[MSG_ACK_IOACT][MCAN_D4] = msg[MCAN_D4];
				s_CanMsg[MSG_ACK_IOACT][MCAN_D5] = msg[MCAN_D5];
				s_CanMsg[MSG_ACK_IOACT][MCAN_D6] = msg[MCAN_D6];
				s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = msg[MCAN_D7];

				if (index < ARR_TOTAL)
				{
					if (msg[MCAN_D4] == 1) { if (WriteArrByte(index, msg[MCAN_D7])) s_PcArrayDirty = TRUE; }
					else s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = ReadArrByte(index);
				}
				else if ((index == MBC_89_S_SW) || (index == MBC_89_S_SW + 1))
				{
					// Bridge to mbc89_com.c's s_Sw[].
					u8 swIndex = (u8)(index - MBC_89_S_SW);
					if (msg[MCAN_D4] == 1) { if (McCom_SetSw(swIndex, msg[MCAN_D7])) s_PcArrayDirty = TRUE; }
					else s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = McCom_GetSw(swIndex);
				}
				else if (index == MBC_89_S_MAPPING)
				{
					if (msg[MCAN_D4] == 1) { if (McCom_SetMapping(msg[MCAN_D7])) s_PcArrayDirty = TRUE; }
					else s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = McCom_GetMapping();
				}
				else if (index == MBC_89_S_BASIS)
				{
					if (msg[MCAN_D4] == 1) { if (McCom_SetBasis(msg[MCAN_D7])) s_PcArrayDirty = TRUE; }
					else s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = McCom_GetBasis();
				}
				else if (index == MBC_89_S_I2C)
				{
					if (msg[MCAN_D4] == 1) { if (McCom_SetI2c(msg[MCAN_D7])) s_PcArrayDirty = TRUE; }
					else s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = McCom_GetI2c();
				}
				else if ((index >= MBC_89_S_LOKS) && (index < (u16)(MBC_89_S_LOKS + LOKS_ANZAHL * LOKS_ITEMS)))
				{
					// Bridge to the module's live loco database (mbc89_com.c's s_Loks[], via
					// McCom_LokAt()) - NOT persisted (see mbc89_store.h), so no dirty-tracking needed.
					u16 rel = (u16)(index - MBC_89_S_LOKS);
					u8* lok = McCom_LokAt((u8)(rel / LOKS_ITEMS));
					u8 byteOff = (u8)(rel % LOKS_ITEMS);
					if (msg[MCAN_D4] == 1) lok[byteOff] = msg[MCAN_D7];
					else if ((byteOff == LOK_MAPPED) && (lok[byteOff] != 0))
						// ParaCenter checks this field for exactly 1, but a CONNECT6021-mapped loco's
						// internal byte is 0x01|LOK_MAPPED_ECHT=0x03 - present it as a clean 0/1
						// without changing the internal ECHT-bit-aware byte this module's own loco
						// control logic still uses.
						s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = 1;
					else s_CanMsg[MSG_ACK_IOACT][MCAN_D7] = lok[byteOff];
				}

				if (((index == MCAN_WRITE) || (index == MCAN_READ)) && (msg[MCAN_D7] == 0))
					s_IoTransfer = FALSE;

				// ParaCenter's "commit to EEPROM" signal after a PC_ARRAY_DATA byte-write sequence
				// (msg[D7] carries a block id, unused here - one file holds everything, so any block
				// id just persists the current state). Only persists when a real change happened
				// since the last commit - a periodic re-sync that rewrites identical bytes must not
				// re-trigger a file write every time.
				if (index == MCAN_EEPROM)
				{
					if (s_PcArrayDirty)
					{
						Store_Save();
						s_PcArrayDirty = FALSE;
					}

					// Refreshes the CS2 ping response's SW fields immediately after a ParaCenter-side
					// SW change, not just after the next GUiD (re)assignment.
					McBase_SetPingSw(McCom_GetSw(0), McCom_GetSw(1));
				}

				SendMsg(s_CanMsg[MSG_ACK_IOACT]);
			}
			break;

			default: break;
		}
	}
}

//---------------------------------------------------------------------------------------------------
// Evaluates CS2 communication: ping response, config-channel table walk, messwert (Voltage/
// Temperature) query. No response staggering on CAN_ID_PING and no inter-packet delay on the
// config-channel walk - see mbc89_base.h for why both are safe to omit here.
static void CheckCs2Connection(const u8* msg, bool isCs2Guid)
{
	switch (msg[MCAN_ID])
	{
		case CAN_ID_PING:
			SendMsg(s_CanMsg[MSG_CS2_0x30_R]);
			s_CsConnect = TRUE;
		break;

		case CAN_ID_KONFIG:
			if (isCs2Guid)
			{
				u8 idNr = msg[MCAN_D4];
				const u8* sourceArr;

				if (idNr <= (MBC_COM_ANZMESSWERTE + MBC_COM_ANZKONFIG))
				{
					sourceArr = s_Cs2Index[idNr];
				}
				else
				{
					sourceArr = McCom_Cs2IndexExtern((u8)(idNr - (MBC_COM_ANZMESSWERTE + MBC_COM_ANZKONFIG + 1)));
					if (sourceArr == NULL) break;
				}

				for (u8 paket = 0; paket < MBC_COM_CS2PAKETE; paket++)
				{
					for (u8 i = 0; i < 8; i++)
						s_CanMsg[MSG_CS2_0x3A_R_8][MCAN_D0 + i] = sourceArr[i + (paket * 8)];

					s_CanMsg[MSG_CS2_0x3A_R_8][MCAN_HASH_H] = 0x03;
					s_CanMsg[MSG_CS2_0x3A_R_8][MCAN_HASH_L] = (u8)(paket + 1);

					SendMsg(s_CanMsg[MSG_CS2_0x3A_R_8]);

					// Under real bus load (e.g. concurrent MFX Seek traffic) the CSx loses the last of
					// the 5 packets in a burst if they arrive with no gap; Halt() (one VBlank,
					// ~16.7/20ms) stays negligible against the ~10s CS2 ping cycle this whole walk
					// runs inside.
					Halt();
				}

				s_CanMsg[MSG_CS2_0x3A_R_6][MCAN_D4] = idNr;
				s_CanMsg[MSG_CS2_0x3A_R_6][MCAN_D5] = MBC_COM_CS2PAKETE;

				SendMsg(s_CanMsg[MSG_CS2_0x3A_R_6]);

				s_CsInit = TRUE;
				s_CsStartup = TRUE;
				s_CsAutoMode = FALSE;
			}
		break;

		case CAN_ID_SYSTEM_BEFEHLE:
			if (isCs2Guid && (msg[MCAN_D4] == CAN_ID_SYSTEM_BEFEHLE_SUB_MW) && (msg[MCAN_DLC] == 8))
			{
				// Module-specific config-channel write (mbc89_com.c's channels 7..11).
				McCom_Cs2IndexParameterExtern(msg);
			}
			else if (isCs2Guid && (msg[MCAN_D4] == CAN_ID_SYSTEM_BEFEHLE_SUB_MW) && (msg[MCAN_DLC] == 6))
			{
				u16 index;

				if (msg[MCAN_D5] == 1)
				{
					index = (u16)(g_SimArr[1] - '0') * 1000 + (u16)(g_SimArr[2] - '0') * 100 + (u16)(g_SimArr[3] - '0') * 10;
					index = (u16)((index - 1000) / 2);

					s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D5] = IDX_1_1;
					s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D6] = (u8)((index & 0xFF00) >> 8);
					s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D7] = (u8)(index & 0x00FF);

					SendMsg(s_CanMsg[MSG_CS2_0x00_R_8_0x0B]);
				}
				else if (msg[MCAN_D5] == 2)
				{
					index = (u16)(g_SimArr[4] - '0') * 100 + (u16)(g_SimArr[5] - '0') * 10 + (u16)(g_SimArr[6] - '0');
					index = (u16)(index / 4);

					s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D5] = IDX_1_2;
					s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D6] = (u8)((index & 0xFF00) >> 8);
					s_CanMsg[MSG_CS2_0x00_R_8_0x0B][MCAN_D7] = (u8)(index & 0x00FF);

					SendMsg(s_CanMsg[MSG_CS2_0x00_R_8_0x0B]);
				}
			}
		break;

		default: break;
	}
}

//===================================================================================================
// PUBLIC FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
void McBase_Init(void)
{
	for (u16 i = 0; i < ARR_SIZE; i++) s_Arr[i] = 0;

	// Module identity. Article "8990" (module type 89, first two ASCII digits - ParaCenter parses
	// those to route the module in its UI; byte[2]='9' identifies this MSX build). SW 1.00, HW
	// date-coded YY.MM.DD.
	static const u8 snr[8]  = {0,0,0,0,0,0,0,1};
	static const u8 art[4]  = {'8','9','9','0'};
	static const u8 sw[3]   = {'1','0','0'};
	static const u8 hw[6]   = {'2','6','0','9','1','7'};

	for (u8 i = 0; i < 8; i++) s_Arr[O_SNR + i] = snr[i];
	for (u8 i = 0; i < 4; i++) s_Arr[O_ART + i] = art[i];
	for (u8 i = 0; i < 3; i++) s_Arr[O_SW + i] = sw[i];
	for (u8 i = 0; i < 6; i++) s_Arr[O_HW + i] = hw[i];

	// Name defaults to the serial number - changeable/persistable via ParaCenter, unlike the
	// SNR/ART/SW/HW fields above. Store_Load() below overwrites this default with a persisted or
	// ParaCenter-assigned name if one exists.
	NameFromSerial();

	for (u8 i = 0; i < 4; i++) s_Arr[O_UID + i] = 0xFF;	// unregistered until a PC assigns a GUiD

	// Fixed discovery HASH (0x5f38) a not-yet-registered module answers with, so an unconfigured
	// PC/ParaCenter can find it during PC_NEU/PC_NEU_DATA - HashInit() only runs once a real GUiD
	// exists, so this must be set explicitly here first.
	s_CanMsg[MSG_INFO][MCAN_HASH_H] = 0x5F;
	s_CanMsg[MSG_INFO][MCAN_HASH_L] = 0x38;
	s_CanMsg[MSG_ACK_MODUL][MCAN_HASH_H] = 0x5F;
	s_CanMsg[MSG_ACK_MODUL][MCAN_HASH_L] = 0x38;

	ModuleFlagsReset();

	for (u8 i = 0; i < 4; i++) s_Cs2Index[IDX_0][INDEX_0_ART_NR + i] = s_Arr[O_ART + i];
	for (u8 i = 0; i < 2; i++) s_Cs2Index[IDX_0][INDEX_0_TYP + i] = s_Arr[O_ART + i];

	// ANZKONFIG report = base channels + mbc89_com.c's module-specific channels (4+5=9) - without
	// this the CSx never asks for idNr 7..11.
	s_Cs2Index[IDX_0][1] = MBC_COM_ANZKONFIG + MCCOM_ANZKONFIGMODUL;

	for (u8 i = 0; i < 8; i++)
	{
		s_Cs2Index[IDX_2_1][INDEX_2_1_SERNRDS + (2 * i)]     = Hex2Ascii((s_Arr[O_SNR + i] & 0xF0) >> 4);
		s_Cs2Index[IDX_2_1][INDEX_2_1_SERNRDS + (2 * i) + 1] = Hex2Ascii(s_Arr[O_SNR + i] & 0x0F);
	}

	s_Cs2Index[IDX_2_3][INDEX_2_3_FW_1] = Hex2Ascii(s_Arr[O_SW]     - '0');
	s_Cs2Index[IDX_2_3][INDEX_2_3_FW_2] = Hex2Ascii(s_Arr[O_SW + 1] - '0');
	s_Cs2Index[IDX_2_3][INDEX_2_3_FW_3] = Hex2Ascii(s_Arr[O_SW + 2] - '0');
}

//---------------------------------------------------------------------------------------------------
void McBase_Check(const u8* msg)
{
	bool isPcGuid = (msg[MCAN_D4] == s_Arr[O_UID]) && (msg[MCAN_D5] == s_Arr[O_UID + 1])
	              && (msg[MCAN_D6] == s_Arr[O_UID + 2]) && (msg[MCAN_D7] == s_Arr[O_UID + 3]);

	bool isCs2Guid = (msg[MCAN_D0] == s_Arr[O_UID]) && (msg[MCAN_D1] == s_Arr[O_UID + 1])
	               && (msg[MCAN_D2] == s_Arr[O_UID + 2]) && (msg[MCAN_D3] == s_Arr[O_UID + 3]);

	if ((msg[MCAN_ID] == CAN_ID_ZUBEHOER_SCHALTEN) && (msg[MCAN_D2] == CAN_LOCID_MBCAN))
		CheckPcConnection(msg, isPcGuid);

	if (s_InitDone && ((msg[MCAN_ID] & 0x01) == 0x00))
		CheckCs2Connection(msg, isCs2Guid);
}

//---------------------------------------------------------------------------------------------------
bool McBase_IsRegistered(void)
{
	return s_InitDone;
}

//---------------------------------------------------------------------------------------------------
bool McBase_IsCs2Init(void)
{
	return s_CsInit;
}
