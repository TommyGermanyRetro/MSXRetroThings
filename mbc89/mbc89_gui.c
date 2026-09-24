//***************************************************************************************************
//* SOURCE - mbc89_gui.c - GUI-Prozessor: SCREEN2 Keyboard-6040/CU6021/Control80f-Karussell           *
//***************************************************************************************************
//* Datei: mbc89_gui.c                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************
#include "mbc89_gui.h"
#include "mbc89_can.h"
#include "mbc89_base.h"
#include "mbc89_com.h"
#include "rcx_io.h"
#include "content/panel_keyboard.h"
#include "content/panel_cu.h"
#include "content/knob_sprite.h"
#include "content/ui_frame.h"
#include "content/ui_tri_left.h"
#include "content/ui_tri_right.h"
#include "content/panel_splash.h"

//===================================================================================================
// DEFINES
//===================================================================================================

#define KNOB_FRAME_COUNT 27

#define FRAME_SPRITE_INDEX 0
#define KNOB_SPRITE_INDEX  1
#define TRIL_SPRITE_INDEX  2
#define TRIR_SPRITE_INDEX  3
#define FRAME_PATTERN 0
#define KNOB_PATTERN  4 // right after the 1-frame cursor sprite (1*4 pattern slots)
#define TRIL_PATTERN  (KNOB_PATTERN + KNOB_FRAME_COUNT * 4)
#define TRIR_PATTERN  (TRIL_PATTERN + 4)

#define PANEL_KEYBOARD 0
#define PANEL_CU6021   1
#define PANEL_C80F     2

#define TBL_KEYBOARD 0
#define TBL_CU6021   1

// Device inventory - MAX how many of each type the carousel can ever hold (compile-time array
// sizing only). KEYBOARD_COUNT_MAX is defined in mbc89_com.h (shared with
// McCom_DecodeKeyboardEvent()'s address-range check, must never drift out of sync).
// C80F_COUNT_MAX is defined in mbc89_gui.h (shared with mbc89.c's env-var range check, same reason).
// Control Unit 6021 is always exactly one (fixed, matches the real device). The actual runtime counts
// (0..MAX, read from the KEYB/C80F env vars at startup) are g_KeyboardCount/g_C80fCount.
// s_SlotLeftMin/s_SlotRightMax (below, set once in Gui_Init()) are the runtime carousel-navigation
// bounds.
static i8 s_SlotLeftMin;
static i8 s_SlotRightMax;

// --- Keyboard hotspot grid ---
#define KEYBOARD_COUNT 34
#define KEYBOARD_TRI_L 32
#define KEYBOARD_TRI_R 33
static const u8 s_KeyboardX[] = {65,83,101,119,137,155,173,191,65,83,101,119,137,155,173,191,65,83,101,119,137,155,173,191,65,83,101,119,137,155,173,191,20,236};
static const u8 s_KeyboardY[] = {73,73,73,73,73,73,73,73,101,101,101,101,101,101,101,101,129,129,129,129,129,129,129,129,157,157,157,157,157,157,157,157,88,88};
static const u8 s_KeyboardL[] = {32,0,1,2,3,4,5,6,32,8,9,10,11,12,13,14,32,16,17,18,19,20,21,22,32,24,25,26,27,28,29,30,255,15};
static const u8 s_KeyboardR[] = {1,2,3,4,5,6,7,33,9,10,11,12,13,14,15,33,17,18,19,20,21,22,23,33,25,26,27,28,29,30,31,33,8,255};
static const u8 s_KeyboardU[] = {255,255,255,255,255,255,255,255,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,255,255};
static const u8 s_KeyboardD[] = {8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,255,255,255,255,255,255,255,255,255,255};

// --- CU6021 hotspot grid + LED/digit tables. Shared by CU6021 AND Control80f (same physical
// keypad/knob layout, only the background label differs). ---
#define CU6021_COUNT 23
#define CU6021_TRI_L 21
#define CU6021_TRI_R 22
static const u8 s_Cu6021X[] = {65,83,101,137,155,173,191,65,83,101,137,155,173,191,65,83,101,65,83,101,157,20,236};
static const u8 s_Cu6021Y[] = {73,73,73,73,73,73,73,101,101,101,101,101,101,101,129,129,129,157,157,157,160,88,88};
static const u8 s_Cu6021L[] = {21,0,1,2,3,4,5,21,7,8,9,10,11,12,21,14,15,21,21,18,19,255,13};
static const u8 s_Cu6021R[] = {1,2,3,4,5,6,22,8,9,10,11,12,13,22,15,16,22,18,22,20,22,7,255};
static const u8 s_Cu6021U[] = {255,255,255,255,255,255,255,0,1,2,3,4,5,6,7,8,9,14,15,16,12,255,255};
static const u8 s_Cu6021D[] = {7,8,9,10,11,12,13,14,15,16,255,255,255,255,255,18,255,255,255,255,255,255,255};
// Triangles (21/22) and the knob (20) are bridged out of the neighbor graph on purpose. L(17)/F(19)
// likewise unimplemented (see s_DigitForHotspot below).

// Per-table lookup arrays, indexed by s_TableId (TBL_KEYBOARD/TBL_CU6021).
static const u8* const s_HotX[2] = { s_KeyboardX, s_Cu6021X };
static const u8* const s_HotY[2] = { s_KeyboardY, s_Cu6021Y };
static const u8* const s_HotL[2] = { s_KeyboardL, s_Cu6021L };
static const u8* const s_HotR[2] = { s_KeyboardR, s_Cu6021R };
static const u8* const s_HotU[2] = { s_KeyboardU, s_Cu6021U };
static const u8* const s_HotD[2] = { s_KeyboardD, s_Cu6021D };
static const u8 s_TriL[2]           = { KEYBOARD_TRI_L, CU6021_TRI_L };
static const u8 s_TriR[2]           = { KEYBOARD_TRI_R, CU6021_TRI_R };
// Landing hotspot when a triangle switches to a NEW panel: arriving from the panel that was to its
// right (slot index decreased, own right edge) vs. from the one that was to its left (slot increased).
static const u8 s_EnterFromRight[2] = { 15, 13 };
static const u8 s_EnterFromLeft[2]  = { 8, 7 };

static const u8 s_KnobX[KNOB_FRAME_COUNT] = {136,137,137,138,139,140,141,142,143,144,145,147,148,149,150,151,153,154,155,156,157,158,159,160,161,161,162};
static const u8 s_KnobY[KNOB_FRAME_COUNT] = {144,143,143,142,141,140,139,139,138,138,137,137,137,137,137,137,137,138,138,139,139,140,141,142,143,143,144};

// MSXgl 6x8 font glyphs for digits 0-9 - renders the CU6021/Control80f loco-address display at
// runtime.
static const u8 s_FontDigits[10][8] = {
	{56,68,76,84,100,68,56,0}, // 0
	{16,48,80,16,16,16,124,0}, // 1
	{56,68,4,24,32,64,124,0}, // 2
	{56,68,4,24,4,68,56,0}, // 3
	{16,32,72,124,8,8,8,0}, // 4
	{124,64,120,4,4,68,56,0}, // 5
	{56,68,64,120,68,68,56,0}, // 6
	{124,4,8,8,16,16,16,0}, // 7
	{56,68,68,56,68,68,56,0}, // 8
	{56,68,68,60,4,68,56,0}, // 9
};
#define DIGIT_CELL_OFF_ROW0 824
#define DIGIT_CELL_OFF_ROW1 1080

// Same font/shift convention as s_FontDigits above (lowercase glyphs, matching the label's actual
// baked case - "control unit"/"control 80f") - the 5 extra letters needed to draw the CU6021/
// Control80f label's differing suffix word at runtime. Reuses s_FontDigits[8]/[0] for the '8'/'0' in
// "80f" instead of duplicating those two glyphs here.
static const u8 s_FontLetterU[8] = {0,0,72,72,72,72,52,0};
static const u8 s_FontLetterN[8] = {0,0,88,100,68,68,68,0};
static const u8 s_FontLetterI[8] = {16,0,48,16,16,16,56,0};
static const u8 s_FontLetterT[8] = {32,32,120,32,32,36,24,0};
static const u8 s_FontLetterF[8] = {8,20,16,124,16,16,16,0};

// Cell offsets of the 4x2 blank area content/panel_cu.h's generator left in the bottom-left label
// (cols 12-15, rows 21-22 of the 32x24 identity name table, right after the "control " prefix baked
// into the shared image) - see RenderPanelLabel().
#define LABEL_CELL_OFF_ROW0 5472 // (21*32+12)*8
#define LABEL_CELL_OFF_ROW1 5728 // (22*32+12)*8

// Precomputed pattern bytes for the Keyboard slot-number display ("01".."16", keyed by slot-1) - all
// KEYBOARD_COUNT_MAX (16) entries. UpdateSlotDisplay() below also rewrites the matching COLOR bytes
// when applying one of these (the pattern alone is not enough to draw the digit correctly).
static const u8 s_DigitPattern[KEYBOARD_COUNT_MAX][64] = {
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,6,6,30,30,102,102,6,0,0,0,0,0,0,0,0,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,6,6,6,6,6,127,127,0,0,0,0,0,0,224,224,0}, // 01
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,31,31,96,96,0,0,7,0,128,128,96,96,96,96,128,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,7,24,24,96,96,127,127,0,128,0,0,0,0,224,224,0}, // 02
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,31,31,96,96,0,0,7,0,128,128,96,96,96,96,128,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,7,0,0,96,96,31,31,0,128,96,96,96,96,128,128,0}, // 03
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,6,6,24,24,97,97,127,0,0,0,0,0,128,128,224,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,127,1,1,1,1,1,1,0,224,128,128,128,128,128,128,0}, // 04
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,127,127,96,96,127,127,0,0,224,224,0,0,128,128,96,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,0,0,0,96,96,31,31,0,96,96,96,96,96,128,128,0}, // 05
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,31,31,96,96,96,96,127,0,128,128,96,96,0,0,128,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,127,96,96,96,96,31,31,0,128,96,96,96,96,128,128,0}, // 06
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,127,127,0,0,1,1,1,0,224,224,96,96,128,128,128,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,1,6,6,6,6,6,6,0,128,0,0,0,0,0,0,0}, // 07
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,31,31,96,96,96,96,31,0,128,128,96,96,96,96,128,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,31,96,96,96,96,31,31,0,128,96,96,96,96,128,128,0}, // 08
	{0,1,1,6,6,6,6,6,0,248,248,6,6,30,30,102,0,31,31,96,96,96,96,31,0,128,128,96,96,96,96,224,6,7,7,6,6,1,1,0,102,134,134,6,6,248,248,0,31,0,0,96,96,31,31,0,224,96,96,96,96,128,128,0}, // 09
	{0,0,0,1,1,6,6,0,0,96,96,224,224,96,96,96,0,31,31,96,96,97,97,102,0,128,128,96,96,224,224,96,0,0,0,0,0,7,7,0,96,96,96,96,96,254,254,0,102,120,120,96,96,31,31,0,96,96,96,96,96,128,128,0}, // 10
	{0,0,0,1,1,6,6,0,0,96,96,224,224,96,96,96,0,6,6,30,30,102,102,6,0,0,0,0,0,0,0,0,0,0,0,0,0,7,7,0,96,96,96,96,96,254,254,0,6,6,6,6,6,127,127,0,0,0,0,0,0,224,224,0}, // 11
	{0,0,0,1,1,6,6,0,0,96,96,224,224,96,96,96,0,31,31,96,96,0,0,7,0,128,128,96,96,96,96,128,0,0,0,0,0,7,7,0,96,96,96,96,96,254,254,0,7,24,24,96,96,127,127,0,128,0,0,0,0,224,224,0}, // 12
	{0,0,0,1,1,6,6,0,0,96,96,224,224,96,96,96,0,31,31,96,96,0,0,7,0,128,128,96,96,96,96,128,0,0,0,0,0,7,7,0,96,96,96,96,96,254,254,0,7,0,0,96,96,31,31,0,128,96,96,96,96,128,128,0}, // 13
	{0,0,0,1,1,6,6,0,0,96,96,224,224,96,96,96,0,6,6,24,24,97,97,127,0,0,0,0,0,128,128,224,0,0,0,0,0,7,7,0,96,96,96,96,96,254,254,0,127,1,1,1,1,1,1,0,224,128,128,128,128,128,128,0}, // 14
	{0,0,0,1,1,6,6,0,0,96,96,224,224,96,96,96,0,127,127,96,96,127,127,0,0,224,224,0,0,128,128,96,0,0,0,0,0,7,7,0,96,96,96,96,96,254,254,0,0,0,0,96,96,31,31,0,96,96,96,96,96,128,128,0}, // 15
	{0,0,0,1,1,6,6,0,0,96,96,224,224,96,96,96,0,31,31,96,96,96,96,127,0,128,128,96,96,0,0,128,0,0,0,0,0,7,7,0,96,96,96,96,96,254,254,0,127,96,96,96,96,31,31,0,128,96,96,96,96,128,128,0}, // 16
};

static const u16 s_KbLedOffset[112] = {2876,2877,2878,2883,2884,2885,2886,2887,2899,2900,2901,2902,2903,2915,2916,2917,2918,2919,2931,2932,2933,2934,2935,2939,2940,2941,2942,2943,2948,2949,2950,2955,2956,2957,2958,2959,2971,2972,2973,2974,2975,2987,2988,2989,2990,2991,3003,3004,3005,3006,3007,3011,3012,3013,3014,3015,4668,4669,4670,4675,4676,4677,4678,4679,4691,4692,4693,4694,4695,4707,4708,4709,4710,4711,4723,4724,4725,4726,4727,4731,4732,4733,4734,4735,4740,4741,4742,4747,4748,4749,4750,4751,4763,4764,4765,4766,4767,4779,4780,4781,4782,4783,4795,4796,4797,4798,4799,4803,4804,4805,4806,4807};
static const u8  s_KbLedStart[16] = {0,8,13,18,28,36,41,46,56,64,69,74,84,92,97,102};
static const u8  s_KbLedCount[16] = {8,5,5,10,8,5,5,10,8,5,5,10,8,5,5,10};

static const u16 s_LedPatchOffset[41] = {1935,2176,2177,2178,2184,2185,2186,2187,1951,2200,2201,2202,2203,2948,2949,2950,2955,2956,2957,2958,2959,2971,2972,2973,2974,2975,2987,2988,2989,2990,2991,3003,3004,3005,3006,3007,3011,3012,3013,3014,3015};
static const u8  s_LedPatchOnByte[41] = {224,1,1,1,240,240,240,224,56,124,124,124,56,1,1,1,224,240,240,240,224,56,124,124,124,56,14,31,31,31,14,3,7,7,7,3,128,192,192,192,128};
static const u8  s_LedPatchStart[6]   = {0,8,13,21,26,31};
static const u8  s_LedPatchCount[6]   = {8,5,8,5,5,10};
#define LED_F1   0
#define LED_F2   1
#define LED_F3   2
#define LED_F4   3
#define LED_F0   4
#define LED_GO   5

static const u16 s_DirUpOffset[9]   = {857,858,859,860,861,862,868,869,870};
static const u8  s_DirUpOnByte[9]   = {2,7,7,15,15,31,128,128,192};
static const u16 s_DirDownOffset[9] = {1112,1113,1114,1115,1116,1117,1120,1121,1122};
static const u8  s_DirDownOnByte[9] = {31,15,15,7,7,2,192,128,128};

// Maps a CU6021/Control80f hotspot index to the digit it types, or 255 for a non-digit hotspot.
// L(17)/F(19) resolve to 255 too (unimplemented - gate loco vs. accessory addressing, meaningless
// here).
static const u8 s_DigitForHotspot[21] = {1,2,3,255,255,255,255,4,5,6,255,255,255,255,7,8,9,255,0,255,255};
#define HOT_F1   3
#define HOT_F2   4
#define HOT_OFF  5
#define HOT_STOP 6
#define HOT_F3  10
#define HOT_F4  11
#define HOT_F0  12
#define HOT_GO  13

//===================================================================================================
// VARIABLES
//===================================================================================================

static u8  s_UnpackBuf[6144]; // one GM2 pattern OR color bank's worth - reused, never both at once

static i8  s_SlotIndex = 0;   // negative = Keyboard pool, 0 = CU6021, positive = Control80f pool
static u8  s_PanelType = PANEL_CU6021;
static u8  s_TableId   = TBL_CU6021;
static u8  s_SlotNumber = 0;  // 1-based position within the current panel type's own pool
static u8  s_Hotspot = 0;

// Control Unit 6021 / Control 80f live interactive state - one persistent slot PER PHYSICAL DEVICE
// INSTANCE (index 0 = the one CU6021, index 1..C80F_COUNT_MAX = each Control 80f), not per screen -
// switching panels only changes which instance is currently being looked at, per manual 5.3.5 "Betrieb
// mit mehreren Fahrpulten": a locomotive called up on one control panel stays called up there even
// while another panel is on screen - see IsAddressOwnedElsewhere() below. Sized to the compile-time
// MAX (not the runtime g_C80fCount) - array size can't shrink at runtime (SDCC has no VLAs), only the
// logically-used prefix does (see s_SlotRightMax).
#define CU_INSTANCE_COUNT (1 + C80F_COUNT_MAX)
typedef struct
{
	u8   locoAddr;
	u8   digitCount;
	bool f1On;
	bool f2On;
	bool f3On;
	bool f4On;
	bool f0On;
	bool stopped;
	bool forward;
	// Set to s_NextClaimSeq++ every time this instance's address entry freshly completes (digitCount
	// 1->2) - lets IsAddressOwnedElsewhere() implement real first-come-first-served (manual 5.3.5):
	// whichever instance claimed an address EARLIER (lower claimSeq) keeps it; a later duplicate
	// blinks, regardless of instance index (a plain "lower instance index wins" rule would let the
	// CU6021, always instance 0, silently take over an address a Control80f had already claimed).
	u16  claimSeq;
} Cu6021State;
static Cu6021State  s_CuState[CU_INSTANCE_COUNT];
static Cu6021State* s_CurCu = &s_CuState[0]; // instance backing whichever CU6021/C80F panel is shown
static u8 s_CuInstance = 0;                  // that instance's own index into s_CuState[]
static u16 s_NextClaimSeq = 1;               // 0 reserved for "never claimed" (GSINIT-safe default)

// Keyboard 6040: one persistent status bit per column per physical instance - same "state outlives a
// panel switch" reasoning as s_CuState above. Sized to KEYBOARD_COUNT_MAX (compile-time), not the
// runtime g_KeyboardCount - same reasoning as CU_INSTANCE_COUNT above.
static u16  s_KbColStatus[KEYBOARD_COUNT_MAX];
static u16* s_CurKb = &s_KbColStatus[0]; // instance backing whichever Keyboard panel is shown

static u8   s_KnobFrame = 0;
// Authoritative mmSpeed (0..15) - s_KnobFrame (27 sprite positions) is a purely cosmetic function of
// this, never the other way round. 27 does not divide evenly into 16, so deriving mmSpeed FROM the
// knob frame (as before) made some frames round to the same mmSpeed as their neighbour, silently
// swallowing some U/D presses (McCom_HandleLocoSpeed()'s own "unchanged" guard saw no change) even
// though the sprite visibly moved - worst at the low end (2 presses needed to leave mmSpeed 0) and the
// high end (mmSpeed 15/"100%" never actually reached). Tracking mmSpeed directly guarantees every
// press changes it by exactly 1.
static u8   s_Speed = 0;
static bool s_AddrBlinkOn = TRUE;

//===================================================================================================
// HELPER FUNCTIONS - shared
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// MSXimg's own -compress rlep encoder has a real bug for runs >=64 bytes, so this decoder matches a
// separate encoder used to build the content/panel_*.h files, not MSXgl's own (incompatible)
// RLEp_UnpackToRAM().
static void UnpackRLEp(const u8* src, u16 srcSize, u8* dst)
{
	const u8* srcEnd = src + srcSize;
	while (src < srcEnd)
	{
		u8 type = *src >> 6;
		u8 count = (u8)((*src & 0x3F) + 1);
		u8 i;
		src++;
		if (type == 0) // Chunk of zeros
		{
			for (i = 0; i < count; i++) dst[i] = 0;
		}
		else if (type == 1) // Chunk of same byte
		{
			for (i = 0; i < count; i++) dst[i] = *src;
			src++;
		}
		else if (type == 2) // Chunk of same 2 bytes
		{
			count = (u8)(count << 1);
			for (i = 0; i < count; i++) dst[i] = src[i & 1];
			src += 2;
		}
		else // type == 3, uncompressed data
		{
			for (i = 0; i < count; i++) dst[i] = src[i];
			src += count;
		}
		dst += count;
	}
}

//---------------------------------------------------------------------------------------------------
static void ShowPanel(const u8* patterns, u16 patternsSize, const u8* colors, u16 colorsSize)
{
	UnpackRLEp(patterns, patternsSize, s_UnpackBuf);
	VDP_WriteVRAM(s_UnpackBuf, VDP_GetPatternTable_GM2(0), 0, sizeof(s_UnpackBuf));
	UnpackRLEp(colors, colorsSize, s_UnpackBuf);
	VDP_WriteVRAM(s_UnpackBuf, VDP_GetColorTable_GM2(0), 0, sizeof(s_UnpackBuf));
}

//---------------------------------------------------------------------------------------------------
static void ShowCurrentPanel(void)
{
	switch (s_PanelType)
	{
		case PANEL_KEYBOARD: ShowPanel(g_PanelKeyboard_Patterns, sizeof(g_PanelKeyboard_Patterns), g_PanelKeyboard_Colors, sizeof(g_PanelKeyboard_Colors)); break;
		case PANEL_CU6021:
		case PANEL_C80F:     ShowPanel(g_PanelCu_Patterns, sizeof(g_PanelCu_Patterns), g_PanelCu_Colors, sizeof(g_PanelCu_Colors)); break;
		default: break;
	}
}

// Forward declaration - defined below in the CU6021/Control80f section (shared with DrawDigitGlyph()),
// needed here already by RenderPanelLabel().
static void PlotDigitBit(u8* buf, u8 px, u8 py);

//---------------------------------------------------------------------------------------------------
static const u8* GlyphForChar(char c)
{
	switch (c)
	{
		case 'u': return s_FontLetterU;
		case 'n': return s_FontLetterN;
		case 'i': return s_FontLetterI;
		case 't': return s_FontLetterT;
		case 'f': return s_FontLetterF;
		case '8': return s_FontDigits[8];
		case '0': return s_FontDigits[0];
		default:  return NULL;
	}
}

//---------------------------------------------------------------------------------------------------
// Draws the CU6021/Control80f label's suffix word ("unit"/"80f") into content/panel_cu.h's shared
// background - the "control " prefix stays baked into the static image, only this differing word is
// drawn here at runtime, from SwitchToPanel() while the screen is still blanked. Preserves the row21-
// offset-0 divider bar and inverts the drawn bits at the end, since this region uses an inverted
// gray/black color convention.
static void RenderPanelLabel(const char* word)
{
	u8 buf[64];
	u8 i, x0 = 0;
	for (i = 0; i < 64; i++) buf[i] = 0;
	buf[0] = buf[8] = buf[16] = buf[24] = 0xFF;
	for (i = 0; word[i] != 0; i++)
	{
		const u8* g = GlyphForChar(word[i]);
		u8 ry, rx;
		if (g == NULL) continue;
		for (ry = 0; ry < 8; ry++)
		{
			u8 byte = g[ry];
			for (rx = 0; rx < 6; rx++)
				if ((byte >> (7 - rx)) & 1)
					PlotDigitBit(buf, (u8)(x0 + rx), (u8)(ry + 5));
		}
		x0 = (u8)(x0 + 6);
	}
	for (i = 0; i < 64; i++) buf[i] = (u8)~buf[i];
	VDP_WriteVRAM(buf,      VDP_GetPatternTable_GM2(0) + LABEL_CELL_OFF_ROW0, 0, 32);
	VDP_WriteVRAM(buf + 32, VDP_GetPatternTable_GM2(0) + LABEL_CELL_OFF_ROW1, 0, 32);
	VDP_FillVRAM(COLOR_MERGE(COLOR_GRAY, COLOR_BLACK), VDP_GetColorTable_GM2(0) + LABEL_CELL_OFF_ROW0, 0, 32);
	VDP_FillVRAM(COLOR_MERGE(COLOR_GRAY, COLOR_BLACK), VDP_GetColorTable_GM2(0) + LABEL_CELL_OFF_ROW1, 0, 32);
}

//===================================================================================================
// HELPER FUNCTIONS - Keyboard
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Only the COLOR changes (red<->green) - the LED's shape/pattern bits stay exactly as baked, always
// "on", so this pokes the COLOR table only.
static void SetKeyboardLed(u8 col, bool green)
{
	u8 start = s_KbLedStart[col];
	u8 count = s_KbLedCount[col];
	VADDR base = VDP_GetColorTable_GM2(0);
	u8 colorByte = green ? COLOR_MERGE(COLOR_MEDIUM_GREEN, COLOR_BLACK) : COLOR_MERGE(COLOR_MEDIUM_RED, COLOR_BLACK);
	u8 i;
	for (i = 0; i < count; i++)
		VDP_Poke(colorByte, base + s_KbLedOffset[start + i], 0);
}

//---------------------------------------------------------------------------------------------------
static void RefreshKeyboardLeds(void)
{
	u8 col;
	for (col = 0; col < 16; col++)
		SetKeyboardLed(col, (bool)((*s_CurKb >> col) & 1));
}

//---------------------------------------------------------------------------------------------------
// Dispatches a SPACE press on the currently focused Keyboard hotspot (0-31): the top nibble selects
// the group of 8 columns, the bottom nibble selects the local column and red/green half - updates the
// LOCAL LED immediately, then notifies mbc89_com.c so it can send the equivalent CAN-ID 0x16 message.
// While the system is Stopp, no press may change anything (LED or CAN) - matches the real CU6021's
// track-power-off behaviour, same principle as IsLocoControlAllowed()'s Stopp gate for the CU6021/
// C80F panels.
static void HandleKeyboardPress(void)
{
	u8 group;
	if (McCom_IsSystemStopped()) return;
	group = s_Hotspot >> 4;
	u8 local = s_Hotspot & 15;
	bool green = (bool)(local >= 8);
	u8 col = (u8)(group * 8 + (local & 7));
	u16 mask = (u16)(1 << col);
	if (green) *s_CurKb = (u16)(*s_CurKb | mask);
	else       *s_CurKb = (u16)(*s_CurKb & ~mask);
	SetKeyboardLed(col, green);
	McCom_HandleKeyboardEvent((u8)(s_SlotNumber - 1), col, green);
}

//---------------------------------------------------------------------------------------------------
// Incoming CAN-ID 0x16 handling: if the message targets one of the CURRENTLY SHOWN Keyboard
// instance's own 16 columns, sync the LED regardless of source (own press or elsewhere on the bus),
// exactly like the real device. A message for a DIFFERENT keyboard instance updates that instance's
// own s_KbColStatus[] entry silently (via McCom_DecodeKeyboardEvent()'s returned kbIndex) so it's
// correct next time you carousel to it, without touching the screen now.
static void ApplyIncomingKeyboardMessage(const u8* msg)
{
	u8 kbIndex, col;
	bool green;
	u16 mask;
	if (!McCom_DecodeKeyboardEvent(msg, &kbIndex, &col, &green)) return;
	mask = (u16)(1 << col);
	if (green) s_KbColStatus[kbIndex] = (u16)(s_KbColStatus[kbIndex] | mask);
	else       s_KbColStatus[kbIndex] = (u16)(s_KbColStatus[kbIndex] & ~mask);
	if ((s_PanelType == PANEL_KEYBOARD) && (kbIndex == (u8)(s_SlotNumber - 1)))
		SetKeyboardLed(col, green);
}

//===================================================================================================
// HELPER FUNCTIONS - CU6021 / Control80f
//===================================================================================================

//---------------------------------------------------------------------------------------------------
static void PlotDigitBit(u8* buf, u8 px, u8 py)
{
	u8 cellIdx = (u8)((py >> 3) * 4 + (px >> 3));
	buf[cellIdx * 8 + (py & 7)] |= (u8)(0x80 >> (px & 7));
}

//---------------------------------------------------------------------------------------------------
static void DrawDigitGlyph(u8* buf, u8 x0, u8 y0, u8 digit)
{
	const u8* g = s_FontDigits[digit];
	u8 ry, rx, sy, sx;
	for (ry = 0; ry < 8; ry++)
	{
		u8 byte = g[ry];
		for (rx = 0; rx < 6; rx++)
		{
			if ((byte >> (7 - rx)) & 1)
				for (sy = 0; sy < 2; sy++)
					for (sx = 0; sx < 2; sx++)
						PlotDigitBit(buf, (u8)(x0 + rx * 2 + sx), (u8)(y0 + ry * 2 + sy));
		}
	}
}

//---------------------------------------------------------------------------------------------------
// Whether ANOTHER instance already "owns" this address (manual 5.3.5: a locomotive can only be called
// up on one control panel at a time - entering its address on a second one makes that second display
// blink while the first keeps working normally). True first-come-first-served via claimSeq (see
// Cu6021State's own comment).
static bool IsAddressOwnedElsewhere(u8 addr, u16 mySeq)
{
	u8 i;
	if ((addr < 1) || (addr > 80)) return FALSE;
	for (i = 0; i < CU_INSTANCE_COUNT; i++)
	{
		if (&s_CuState[i] == s_CurCu) continue; // never conflicts with itself
		if ((s_CuState[i].digitCount == 2) && (s_CuState[i].locoAddr == addr) && (s_CuState[i].claimSeq < mySeq))
			return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------------------------------
// A complete entry blinks if it's out of range, not currently controllable (not CONNECT6021-mapped,
// or INFRA-owned - see McCom_IsLokControllable()), OR already claimed by another CU6021/Control80f
// instance (see IsAddressOwnedElsewhere() above, new for this carousel increment).
static bool IsCurrentLocoAddrValid(void)
{
	if (s_CurCu->digitCount != 2) return TRUE;
	return (bool)((s_CurCu->locoAddr >= 1) && (s_CurCu->locoAddr <= 80)
	            && McCom_IsLokControllable(s_CurCu->locoAddr)
	            && !IsAddressOwnedElsewhere(s_CurCu->locoAddr, s_CurCu->claimSeq));
}

//---------------------------------------------------------------------------------------------------
// While the system is Stopp, no loco command may be sent (matches the real CU6021 - track power off
// means nothing is supposed to move) - Stopp/Go itself (HOT_STOP/HOT_GO) stays ungated, only actual
// loco control (F0-F4, speed, direction) is blocked here.
static bool IsLocoControlAllowed(void)
{
	return (bool)((s_CurCu->digitCount == 2) && IsCurrentLocoAddrValid() && !McCom_IsSystemStopped());
}

//---------------------------------------------------------------------------------------------------
static void RenderLocoAddress(void)
{
	u8 buf[64];
	u8 i;
	bool visible = (bool)(IsCurrentLocoAddrValid() || s_AddrBlinkOn);
	for (i = 0; i < 64; i++) buf[i] = 0;
	if (visible)
	{
		if (s_CurCu->digitCount == 1)
		{
			DrawDigitGlyph(buf, 3, 1, s_CurCu->locoAddr);
		}
		else if (s_CurCu->digitCount == 2)
		{
			DrawDigitGlyph(buf, 3, 1, (u8)(s_CurCu->locoAddr / 10));
			DrawDigitGlyph(buf, 15, 1, (u8)(s_CurCu->locoAddr % 10));
		}
	}
	VDP_WriteVRAM(buf,      VDP_GetPatternTable_GM2(0) + DIGIT_CELL_OFF_ROW0, 0, 32);
	VDP_WriteVRAM(buf + 32, VDP_GetPatternTable_GM2(0) + DIGIT_CELL_OFF_ROW1, 0, 32);
	VDP_FillVRAM(COLOR_MERGE(COLOR_MEDIUM_RED, COLOR_BLACK), VDP_GetColorTable_GM2(0) + DIGIT_CELL_OFF_ROW0, 0, 32);
	VDP_FillVRAM(COLOR_MERGE(COLOR_MEDIUM_RED, COLOR_BLACK), VDP_GetColorTable_GM2(0) + DIGIT_CELL_OFF_ROW1, 0, 32);
}

//---------------------------------------------------------------------------------------------------
static void SetLed(u8 ledIdx, bool on)
{
	u8 start = s_LedPatchStart[ledIdx];
	u8 count = s_LedPatchCount[ledIdx];
	VADDR base = VDP_GetPatternTable_GM2(0);
	u8 i;
	for (i = 0; i < count; i++)
	{
		u16 off = s_LedPatchOffset[start + i];
		u8 val = on ? s_LedPatchOnByte[start + i] : 0;
		VDP_Poke(val, base + off, 0);
	}
}

//---------------------------------------------------------------------------------------------------
static void SetDirectionLeds(bool forward)
{
	VADDR base = VDP_GetPatternTable_GM2(0);
	u8 i;
	for (i = 0; i < 9; i++) VDP_Poke(forward ? s_DirUpOnByte[i]   : 0, base + s_DirUpOffset[i],   0);
	for (i = 0; i < 9; i++) VDP_Poke(forward ? 0 : s_DirDownOnByte[i], base + s_DirDownOffset[i], 0);
}

//---------------------------------------------------------------------------------------------------
static void UpdateFunctionLedsVisual(void)
{
	bool allowed = IsLocoControlAllowed();
	SetLed(LED_F1, (bool)(allowed && s_CurCu->f1On));
	SetLed(LED_F2, (bool)(allowed && s_CurCu->f2On));
	SetLed(LED_F3, (bool)(allowed && s_CurCu->f3On));
	SetLed(LED_F4, (bool)(allowed && s_CurCu->f4On));
	SetLed(LED_F0, (bool)(allowed && s_CurCu->f0On));
	if (allowed)
	{
		SetDirectionLeds(s_CurCu->forward);
	}
	else
	{
		VADDR base = VDP_GetPatternTable_GM2(0);
		u8 i;
		for (i = 0; i < 9; i++) VDP_Poke(0, base + s_DirUpOffset[i],   0);
		for (i = 0; i < 9; i++) VDP_Poke(0, base + s_DirDownOffset[i], 0);
	}
}

//---------------------------------------------------------------------------------------------------
static void PositionKnobSprite(void)
{
	VDP_SetSprite(KNOB_SPRITE_INDEX, s_KnobX[s_KnobFrame], s_KnobY[s_KnobFrame], (u8)(KNOB_PATTERN + s_KnobFrame * 4));
}

//---------------------------------------------------------------------------------------------------
// Syncs the GUI's local display fields from the real lok[] DB instead of always starting/staying
// "unconfigured" - mmSpeed(0..15) <-> knob frame(0..26) via a linear round-trip mapping. Also
// repositions the knob sprite immediately - RenderLocoAddress()/UpdateFunctionLedsVisual() (called by
// the caller right after this, on the address-complete call site) cover the address digits and
// F0-F4/direction LEDs already, but not the knob, which would otherwise stay wherever it was until
// the next speed change. Pull only, no CAN send here - this is called on every incoming bus message
// while a valid address is shown, so anything sent from here would go out just as often. See
// McCom_SyncLokState() for the explicit, separate, takeover-only CAN announcement.
static void SyncFromLokState(void)
{
	u8 speed, dir, fx;
	bool f0;
	if (!McCom_IsLokControllable(s_CurCu->locoAddr)) return;
	McCom_GetLokState(s_CurCu->locoAddr, &speed, &dir, &f0, &fx);
	s_Speed = speed;
	s_KnobFrame = (u8)(((u16)s_Speed * (KNOB_FRAME_COUNT - 1) + 7) / 15); // +7 = round to nearest
	s_CurCu->forward = (bool)(dir == DIR_VORWAERTS); // dir==DIR_RUECKWAERTS(2) or 0(unset) both mean "not forward"
	s_CurCu->f0On = f0;
	s_CurCu->f1On = (bool)((fx & 0x01) != 0);
	s_CurCu->f2On = (bool)((fx & 0x02) != 0);
	s_CurCu->f3On = (bool)((fx & 0x04) != 0);
	s_CurCu->f4On = (bool)((fx & 0x08) != 0);

	PositionKnobSprite();
}

//---------------------------------------------------------------------------------------------------
// Dispatches a SPACE press on the currently focused CU6021/Control80f hotspot - digits build the loco
// address one at a time (2 digits max, then the 3rd digit starts a new address), f1-f4/f0/off/
// stop/go call the matching McCom_HandleLoco*()/McCom_HandleStopGo() to actually send CAN, gated by
// IsLocoControlAllowed() except stop/go (track power, always active).
static void HandleCu6021Press(void)
{
	u8 digit = s_DigitForHotspot[s_Hotspot];
	if (digit != 255)
	{
		if (s_CurCu->digitCount >= 2)
		{
			s_CurCu->locoAddr = digit;
			s_CurCu->digitCount = 1;
		}
		else if (s_CurCu->digitCount == 1)
		{
			s_CurCu->locoAddr = (u8)(s_CurCu->locoAddr * 10 + digit);
			s_CurCu->digitCount = 2;
			s_CurCu->claimSeq = s_NextClaimSeq++; // fresh claim - see IsAddressOwnedElsewhere()
			SyncFromLokState(); // address just became complete - pull the loco's real current state
			McCom_SyncLokState(s_CurCu->locoAddr); // ...and announce it as CAN, once, right on takeover
		}
		else
		{
			s_CurCu->locoAddr = digit;
			s_CurCu->digitCount = 1;
		}
		s_AddrBlinkOn = TRUE;
		RenderLocoAddress();
		UpdateFunctionLedsVisual();
		return;
	}
	switch (s_Hotspot)
	{
		case HOT_F1: if (IsLocoControlAllowed()) { s_CurCu->f1On = (bool)!s_CurCu->f1On; McCom_HandleLocoFx(s_CurCu->locoAddr, 1, s_CurCu->f1On); UpdateFunctionLedsVisual(); } break;
		case HOT_F2: if (IsLocoControlAllowed()) { s_CurCu->f2On = (bool)!s_CurCu->f2On; McCom_HandleLocoFx(s_CurCu->locoAddr, 2, s_CurCu->f2On); UpdateFunctionLedsVisual(); } break;
		case HOT_F3: if (IsLocoControlAllowed()) { s_CurCu->f3On = (bool)!s_CurCu->f3On; McCom_HandleLocoFx(s_CurCu->locoAddr, 3, s_CurCu->f3On); UpdateFunctionLedsVisual(); } break;
		case HOT_F4: if (IsLocoControlAllowed()) { s_CurCu->f4On = (bool)!s_CurCu->f4On; McCom_HandleLocoFx(s_CurCu->locoAddr, 4, s_CurCu->f4On); UpdateFunctionLedsVisual(); } break;
		case HOT_F0: if (IsLocoControlAllowed()) { s_CurCu->f0On = TRUE;  McCom_HandleLocoF0(s_CurCu->locoAddr, TRUE);  UpdateFunctionLedsVisual(); } break;
		case HOT_OFF: if (IsLocoControlAllowed()) { s_CurCu->f0On = FALSE; McCom_HandleLocoF0(s_CurCu->locoAddr, FALSE); UpdateFunctionLedsVisual(); } break;
		case HOT_STOP: s_CurCu->stopped = TRUE;  SetLed(LED_GO, FALSE); McCom_HandleStopGo(FALSE); break;
		case HOT_GO:   s_CurCu->stopped = FALSE; SetLed(LED_GO, TRUE);  McCom_HandleStopGo(TRUE);  break;
		default: break; // L(17), F(19), knob(20) - not implemented
	}
}

//---------------------------------------------------------------------------------------------------
static void RefreshCu6021Display(void)
{
	s_AddrBlinkOn = TRUE;
	RenderLocoAddress();
	SetLed(LED_GO, (bool)!s_CurCu->stopped);
	UpdateFunctionLedsVisual();
}

//===================================================================================================
// PANEL SWITCHING / CAROUSEL
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Derives panelType/tableId/slotNumber (and re-aims s_CurCu/s_CurKb) from the linear slot index.
static void RecomputePanelFromSlot(void)
{
	if (s_SlotIndex == 0)
	{
		s_PanelType  = PANEL_CU6021;
		s_TableId    = TBL_CU6021;
		s_SlotNumber = 1;
		s_CuInstance = 0;
	}
	else if (s_SlotIndex < 0)
	{
		s_PanelType  = PANEL_KEYBOARD;
		s_TableId    = TBL_KEYBOARD;
		s_SlotNumber = (u8)(-s_SlotIndex);               // 1..g_KeyboardCount
		s_CurKb = &s_KbColStatus[s_SlotNumber - 1];
	}
	else
	{
		s_PanelType  = PANEL_C80F;
		s_TableId    = TBL_CU6021;
		s_SlotNumber = (u8)s_SlotIndex;                  // 1..g_C80fCount
		s_CuInstance = s_SlotNumber;                     // Control80f #1..#N -> s_CuState[1..N]
	}
	if ((s_PanelType == PANEL_CU6021) || (s_PanelType == PANEL_C80F))
		s_CurCu = &s_CuState[s_CuInstance];
}

//---------------------------------------------------------------------------------------------------
// Patches the baked "01" address-number display with the current slot's own 1-based number - only
// meaningful for Keyboard. CU6021/Control80f use RefreshCu6021Display() instead, which reads each
// instance's own persistent state. Rewrites the COLOR bytes too, not just the pattern bits - the
// digit shape in this table depends on both.
static void UpdateSlotDisplay(void)
{
	if (s_PanelType == PANEL_KEYBOARD)
	{
		VDP_WriteVRAM(&s_DigitPattern[s_SlotNumber - 1][0],  VDP_GetPatternTable_GM2(0) + DIGIT_CELL_OFF_ROW0, 0, 32);
		VDP_WriteVRAM(&s_DigitPattern[s_SlotNumber - 1][32], VDP_GetPatternTable_GM2(0) + DIGIT_CELL_OFF_ROW1, 0, 32);
		VDP_FillVRAM(0xF1, VDP_GetColorTable_GM2(0) + DIGIT_CELL_OFF_ROW0, 0, 32);
		VDP_FillVRAM(0xF1, VDP_GetColorTable_GM2(0) + DIGIT_CELL_OFF_ROW1, 0, 32);
		RefreshKeyboardLeds();
	}
	else if ((s_PanelType == PANEL_CU6021) || (s_PanelType == PANEL_C80F))
	{
		RefreshCu6021Display();
	}
}

//---------------------------------------------------------------------------------------------------
static void PositionFrameSprite(void)
{
	u8 x = s_HotX[s_TableId][s_Hotspot];
	u8 y = s_HotY[s_TableId][s_Hotspot];
	// -8 centers the 16x16 sprite on the hotspot; the extra -1 on Y compensates for the MSX1/TMS9918
	// sprite Y+1 hardware quirk.
	VDP_SetSprite(FRAME_SPRITE_INDEX, (u8)(x - 8), (u8)(y - 9), FRAME_PATTERN);
}

//---------------------------------------------------------------------------------------------------
static void UpdateKnobVisibility(void)
{
	if ((s_PanelType == PANEL_CU6021) || (s_PanelType == PANEL_C80F))
		PositionKnobSprite();
	else
		VDP_HideSprite(KNOB_SPRITE_INDEX);
}

//---------------------------------------------------------------------------------------------------
static void UpdateTriangleVisibility(void)
{
	if (s_SlotIndex > s_SlotLeftMin)
		VDP_SetSprite(TRIL_SPRITE_INDEX, 20 - 8, 88 - 8, TRIL_PATTERN);
	else
		VDP_HideSprite(TRIL_SPRITE_INDEX);

	if (s_SlotIndex < s_SlotRightMax)
		VDP_SetSprite(TRIR_SPRITE_INDEX, 236 - 8, 88 - 8, TRIR_PATTERN);
	else
		VDP_HideSprite(TRIR_SPRITE_INDEX);
}

//---------------------------------------------------------------------------------------------------
// Rebuilds the whole visible state while blanked, then reveals it at once - avoids a visible
// piece-by-piece redraw. Reads s_PanelType, set by RecomputePanelFromSlot() beforehand.
static void SwitchToPanel(void)
{
	VDP_EnableDisplay(FALSE);
	s_Hotspot = 0;

	ShowCurrentPanel();
	if (s_PanelType == PANEL_CU6021)    RenderPanelLabel("unit");
	else if (s_PanelType == PANEL_C80F) RenderPanelLabel("80f");

	// s_Speed/s_KnobFrame are global, not part of Cu6021State - switching to this instance without
	// refreshing them first would leave the knob showing whatever the previously shown panel had.
	// Pull this instance's real state if it has a complete, valid address, else reset to neutral -
	// before UpdateSlotDisplay() below draws the F0-F4 LEDs/digits from these fields.
	if ((s_PanelType == PANEL_CU6021) || (s_PanelType == PANEL_C80F))
	{
		if ((s_CurCu->digitCount == 2) && IsCurrentLocoAddrValid()) SyncFromLokState();
		else { s_Speed = 0; s_KnobFrame = 0; }
	}

	UpdateSlotDisplay();
	PositionFrameSprite();
	UpdateKnobVisibility();
	UpdateTriangleVisibility();
	VDP_EnableDisplay(TRUE);
}

//===================================================================================================
// PUBLIC FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// A standalone SCREEN2 excursion shown before the IO/INT prompt (still SCREEN0), not part of the
// later Gui_Init()/Gui_Run() SCREEN2 session. No fade in/out - just the final color table (see
// panel_splash.h's own comment). Reuses UnpackRLEp()/s_UnpackBuf, the same identity name-table
// pattern, and g_ScreenLayoutLow/High that Gui_Init() also uses below - both are independent,
// self-contained SCREEN2 setups, neither depends on the other having run first.
void Gui_ShowSplash(void)
{
	static u8 nameTable[768];
	u16 i;
	u16 target;

	VDP_SetMode(VDP_MODE_SCREEN2);
	VDP_ClearVRAM();
	// Black backdrop for the splash screen (its own background is black, not the panels' white
	// margin) - main() switches back to its own SCREEN0 setup right after this returns.
	VDP_SetColor(COLOR_BLACK);

	for (i = 0; i < 768; i++) nameTable[i] = (u8)(i & 0xFF);
	VDP_WriteVRAM(nameTable, g_ScreenLayoutLow, g_ScreenLayoutHigh, 768);

	UnpackRLEp(g_PanelSplash_Patterns, sizeof(g_PanelSplash_Patterns), s_UnpackBuf);
	VDP_WriteVRAM(s_UnpackBuf, VDP_GetPatternTable_GM2(0), 0, sizeof(s_UnpackBuf));

	UnpackRLEp(g_PanelSplash_Colors2, sizeof(g_PanelSplash_Colors2), s_UnpackBuf);
	VDP_WriteVRAM(s_UnpackBuf, VDP_GetColorTable_GM2(0), 0, sizeof(s_UnpackBuf));

	VDP_EnableDisplay(TRUE);

	// ~5s at 50/60Hz - g_JIFFY is the BIOS's free-running VBlank counter, same technique as the
	// address-display blink elsewhere in this file.
	target = (u16)(g_JIFFY + 280);
	while (g_JIFFY != target) {}

	VDP_EnableDisplay(FALSE);
}

//---------------------------------------------------------------------------------------------------
void Gui_Init(void)
{
	static u8 nameTable[768];
	u16 i;

	// Derived once, together, from the runtime instance counts (g_KeyboardCount/g_C80fCount, set in
	// main() from the KEYB/C80F env vars before this runs) - both the carousel navigation bound AND
	// the array-index safety of s_KbColStatus[]/s_DigitPattern[]/s_CuState[] depend on the SAME
	// underlying counts staying in sync, so they're computed here, in one place.
	s_SlotLeftMin  = -(i8)g_KeyboardCount;
	s_SlotRightMax = (i8)g_C80fCount;

	// MSXgl's crt0 never calls SDCC's GSINIT - a struct/global with a "plain" initializer (even one
	// that looks like a trivial zero-init) cannot be trusted. Explicit here, not relying on static
	// defaults. Always zeroes the FULL CU_INSTANCE_COUNT/KEYBOARD_COUNT_MAX range (not just
	// g_C80fCount/g_KeyboardCount instances) - trivial cost, and it means any slot beyond the runtime
	// count (unreachable via the carousel, gated by s_SlotLeftMin/s_SlotRightMax above) can never hold
	// stale digitCount==2 state.
	for (i = 0; i < CU_INSTANCE_COUNT; i++)
	{
		s_CuState[i].locoAddr = 0; s_CuState[i].digitCount = 0;
		s_CuState[i].f1On = FALSE; s_CuState[i].f2On = FALSE; s_CuState[i].f3On = FALSE;
		s_CuState[i].f4On = FALSE; s_CuState[i].f0On = FALSE;
		s_CuState[i].stopped = FALSE; s_CuState[i].forward = TRUE;
		s_CuState[i].claimSeq = 0;
	}
	s_CurCu = &s_CuState[0];
	s_CuInstance = 0;
	s_NextClaimSeq = 1;
	for (i = 0; i < KEYBOARD_COUNT_MAX; i++) s_KbColStatus[i] = 0;
	s_CurKb = &s_KbColStatus[0];
	s_KnobFrame = 0;
	s_Speed = 0;
	s_SlotIndex = 0;
	s_Hotspot = 0;

	VDP_SetMode(VDP_MODE_SCREEN2);
	VDP_ClearVRAM();
	VDP_SetColor(COLOR_WHITE);

	// Identity Name Table - matches how MSXimg's --tilesUnique output is addressed: each of the 768
	// GM2 cells maps to its own unique pattern/color 1:1.
	for (i = 0; i < 768; i++) nameTable[i] = (u8)(i & 0xFF);
	VDP_WriteVRAM(nameTable, g_ScreenLayoutLow, g_ScreenLayoutHigh, 768);

	VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16);
	VDP_LoadSpritePattern(g_UiFrame,    FRAME_PATTERN, 1 * 4);
	VDP_LoadSpritePattern(g_KnobSprite, KNOB_PATTERN,  KNOB_FRAME_COUNT * 4);
	VDP_LoadSpritePattern(g_UiTriLeft,  TRIL_PATTERN,  1 * 4);
	VDP_LoadSpritePattern(g_UiTriRight, TRIR_PATTERN,  1 * 4);
	VDP_SetSpriteColorSM1(FRAME_SPRITE_INDEX, COLOR_LIGHT_YELLOW);
	VDP_SetSpriteColorSM1(KNOB_SPRITE_INDEX,  COLOR_MEDIUM_RED);
	VDP_SetSpriteColorSM1(TRIL_SPRITE_INDEX,  COLOR_GRAY);
	VDP_SetSpriteColorSM1(TRIR_SPRITE_INDEX,  COLOR_GRAY);

	RecomputePanelFromSlot();
	SwitchToPanel();
}

//---------------------------------------------------------------------------------------------------
void Gui_Run(void)
{
	static u8 rxMsg[MCAN_MSG_LEN];
	bool prevLeft = FALSE, curLeft, prevRight = FALSE, curRight;
	bool prevUp = FALSE, curUp, prevDown = FALSE, curDown;
	bool prevSpace = FALSE, curSpace;
	bool prevU = FALSE, curU, prevD = FALSE, curD;
	bool prevR = FALSE, curR;
	u8 nxt;
	const u8 *hotL, *hotR, *hotU, *hotD;

	while (!Keyboard_IsKeyPressed(KEY_ESC))
	{
		// Incoming (CAN -> panel) - absorbs McBaseRun()'s CAN pump, plus LED/loco-state sync.
		if (Can_Check(g_CanAddr))
		{
			Can_Rx(g_CanAddr, g_CanRxBuf);
			McanDecode(g_CanRxBuf, rxMsg);
			McBase_Check(rxMsg);
			McCom_Check(rxMsg);
			if (s_PanelType == PANEL_KEYBOARD)
			{
				ApplyIncomingKeyboardMessage(rxMsg);
			}
			else if ((s_PanelType == PANEL_CU6021) || (s_PanelType == PANEL_C80F))
			{
				// Stopp/Go is not address-gated (track power, same as the real CU6021) - sync the
				// GO-LED to the bus-wide state regardless of who sent it (own button OR CS2/ParaCenter).
				bool stopped = McCom_IsSystemStopped();
				if (stopped != s_CurCu->stopped)
				{
					s_CurCu->stopped = stopped;
					SetLed(LED_GO, (bool)!stopped);
				}
				if ((s_CurCu->digitCount == 2) && IsCurrentLocoAddrValid())
				{
					// A LOK_* message elsewhere on the bus may have changed the currently-shown loco's
					// state (another controller, ParaCenter, ...) - re-sync and redraw, same "always
					// show the real bus state" principle as the Keyboard LED. Only the instance CURRENTLY
					// ON SCREEN is refreshed here - matches the real device (a Control80f only shows its
					// own most-recently-selected loco, and only while that panel is actually displayed).
					SyncFromLokState();
					UpdateFunctionLedsVisual();
				}
			}
		}

		// Outgoing (panel -> CAN) - cursor navigation + SPACE, table selection follows the active panel.
		hotL = s_HotL[s_TableId];
		hotR = s_HotR[s_TableId];
		hotU = s_HotU[s_TableId];
		hotD = s_HotD[s_TableId];

		curLeft  = Keyboard_IsKeyPressed(KEY_LEFT);
		curRight = Keyboard_IsKeyPressed(KEY_RIGHT);
		curUp    = Keyboard_IsKeyPressed(KEY_UP);
		curDown  = Keyboard_IsKeyPressed(KEY_DOWN);
		curSpace = Keyboard_IsKeyPressed(KEY_SPACE);

		if (curLeft && !prevLeft)  { nxt = hotL[s_Hotspot]; if (nxt != 255) { s_Hotspot = nxt; PositionFrameSprite(); } }
		if (curRight && !prevRight){ nxt = hotR[s_Hotspot]; if (nxt != 255) { s_Hotspot = nxt; PositionFrameSprite(); } }
		if (curUp && !prevUp)     { nxt = hotU[s_Hotspot]; if (nxt != 255) { s_Hotspot = nxt; PositionFrameSprite(); } }
		if (curDown && !prevDown) { nxt = hotD[s_Hotspot]; if (nxt != 255) { s_Hotspot = nxt; PositionFrameSprite(); } }
		prevLeft = curLeft; prevRight = curRight; prevUp = curUp; prevDown = curDown;

		if (curSpace && !prevSpace)
		{
			// Carousel: SPACE on one of the two fixed triangle hotspots switches panels instead of
			// dispatching a button press.
			if ((s_Hotspot == s_TriL[s_TableId]) && (s_SlotIndex > s_SlotLeftMin))
			{
				s_SlotIndex--;
				RecomputePanelFromSlot();
				s_Hotspot = s_EnterFromRight[s_TableId];
				SwitchToPanel();
			}
			else if ((s_Hotspot == s_TriR[s_TableId]) && (s_SlotIndex < s_SlotRightMax))
			{
				s_SlotIndex++;
				RecomputePanelFromSlot();
				s_Hotspot = s_EnterFromLeft[s_TableId];
				SwitchToPanel();
			}
			else if ((s_TableId == TBL_CU6021) && (s_Hotspot < CU6021_TRI_L))
			{
				HandleCu6021Press();
			}
			else if ((s_TableId == TBL_KEYBOARD) && (s_Hotspot < KEYBOARD_TRI_L))
			{
				HandleKeyboardPress();
			}
		}
		prevSpace = curSpace;

		// CU6021/Control80f speed (<U>/<D> LETTER keys, not the arrow keys - those drive cursor
		// navigation above) and direction (<R>) - gated by IsLocoControlAllowed() like f0-f4. s_Speed
		// (0..15) is authoritative, s_KnobFrame is derived from it purely for the sprite.
		curU = Keyboard_IsKeyPressed(KEY_U);
		curD = Keyboard_IsKeyPressed(KEY_D);
		curR = Keyboard_IsKeyPressed(KEY_R);
		if (((s_PanelType == PANEL_CU6021) || (s_PanelType == PANEL_C80F)) && IsLocoControlAllowed())
		{
			if (curU && !prevU && (s_Speed < 15))
			{
				s_Speed++;
				s_KnobFrame = (u8)(((u16)s_Speed * (KNOB_FRAME_COUNT - 1) + 7) / 15);
				PositionKnobSprite();
				McCom_HandleLocoSpeed(s_CurCu->locoAddr, s_Speed);
			}
			if (curD && !prevD && (s_Speed > 0))
			{
				s_Speed--;
				s_KnobFrame = (u8)(((u16)s_Speed * (KNOB_FRAME_COUNT - 1) + 7) / 15);
				PositionKnobSprite();
				McCom_HandleLocoSpeed(s_CurCu->locoAddr, s_Speed);
			}
			if (curR && !prevR)
			{
				s_CurCu->forward = (bool)!s_CurCu->forward;
				McCom_HandleLocoDir(s_CurCu->locoAddr, s_CurCu->forward ? DIR_VORWAERTS : DIR_RUECKWAERTS);
				SetDirectionLeds(s_CurCu->forward);
			}
		}
		prevU = curU; prevD = curD; prevR = curR;

		// Blink the loco-address display while invalid/uncontrollable (manual 5.3.1/5.3.5 equivalent).
		if ((s_PanelType == PANEL_CU6021) || (s_PanelType == PANEL_C80F))
		{
			if (!IsCurrentLocoAddrValid())
			{
				bool phase = (bool)((g_JIFFY & 0x10) != 0);
				if (phase != s_AddrBlinkOn)
				{
					s_AddrBlinkOn = phase;
					RenderLocoAddress();
				}
			}
			else if (!s_AddrBlinkOn)
			{
				// Became valid again asynchronously (e.g. a CONNECT6021 re-map, not a keypress) while
				// the last-drawn blink frame happened to be the "off"/blank phase - force one redraw
				// so the digits don't stay stuck blank (functions kept working regardless, since they
				// only read s_CurCu->locoAddr, not the display).
				s_AddrBlinkOn = TRUE;
				RenderLocoAddress();
			}
		}
	}
}
