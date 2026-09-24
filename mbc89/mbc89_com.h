//***************************************************************************************************
//* HEADER - mbc89_com.h - CONNECT6021 mapping, system stop/go, loco queries, module config channels *
//***************************************************************************************************
//* The 80-entry loco GUiD/address database and CONNECT6021 handshake (CAN-ID 0x44/0x45), system      *
//* stop/go (CAN-ID 0x00), loco direction/function/speed query answering (CAN-ID 0x08/0x0A/0x0C), the  *
//* 5 "classic" module config channels (SW Major/Minor of a simulated Connect6021 firmware version,    *
//* Zentralentyp-Mapping, Keyboard-Basisadressblock, I2C-Bus-Takt), CU6021/Control80f loco control,     *
//* and CAN-ID 0x16 "Zubehoer schalten" send/receive for the Keyboard panel. Protocol (MM/DCC) is       *
//* hardcoded to MM for now - a per-keyboard protocol-selection config would make this configurable    *
//* but is not implemented.                                                                            *
//***************************************************************************************************
//* Datei: mbc89_com.h                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// Number of module-specific config channels implemented (SW Major/Minor/Mapping/Basis/I2C).
#define MCCOM_ANZKONFIGMODUL 5

// Compile-time MAX number of simulated Keyboard-6040 instances (array sizing only - mbc89_gui.c's
// s_KbColStatus[]/s_DigitPattern[]) - shared here so McCom_DecodeKeyboardEvent()'s address-range
// check and mbc89_gui.c's array sizes can never drift apart. The actual runtime count
// (0..KEYBOARD_COUNT_MAX, read from the KEYB env var at startup) is g_KeyboardCount below.
#define KEYBOARD_COUNT_MAX 16
extern u8 g_KeyboardCount;

// Resets the 80-entry loco address database to its unconnected defaults and the simulated Connect6021
// SW version to its default 1.0. Call once at startup, after McBase_Init().
void McCom_Init(void);

// Evaluates one received, decoded logical Mbc89Can message (see mbc89_can.h) - CONNECT6021 mapping
// (0x44), system stop/go (0x00, DLC=5), loco direction/function/speed queries (0x08/0x0A/0x0C, no-op
// while cuIsPresent/infraIsPresent are both FALSE), and the resync-after-reconnect walk (likewise
// dormant). Call after McBase_Check() for every message.
void McCom_Check(const u8* msg);

// Sets GUiD+HASH on this module's own message templates (_msgStop/_msgGo get HASH only - their GUiD
// stays the universal broadcast 0x00000000; _msgStopResponse/_msgGoResponse/_msgKonfigOk get both)
// and pushes the simulated Connect6021 SW version into the CS2 ping response via McBase_SetPingSw().
// Call from mbc89_base.c's HashInit(), after it has derived guid0..3/hashH/hashL for its own
// templates.
void McCom_HashInit(u8 guid0, u8 guid1, u8 guid2, u8 guid3, u8 hashH, u8 hashL);

// Returns the requested module-specific config-channel row (laufendeNr 0..MCCOM_ANZKONFIGMODUL-1 =
// SW Major/SW Minor/Mapping/Basis/I2C), or NULL if out of range. Call from mbc89_base.c's
// CheckCs2Connection() CAN_ID_KONFIG handler once idNr exceeds the base's own channels.
const u8* McCom_Cs2IndexExtern(u8 laufendeNr);

// Applies a CS2-requested config-channel value (msg[D5]=Kanalnummer 7..11, msg[D6:D7]=new value),
// validates per-channel range, updates RAM state and, for the Mapping channel, live-adjusts every
// loco without LOK_MAPPED_ECHT set. Always confirms via _msgKonfigOk regardless of validity. Call
// from mbc89_base.c's CheckCs2Connection() CAN_ID_SYSTEM_BEFEHLE handler when msg[D4]==SUB_MW and
// msg[DLC]==8 (parameter write, as opposed to the DLC==6 messwert-read case the base handles itself).
void McCom_Cs2IndexParameterExtern(const u8* msg);

// Outgoing (Panel -> CAN): kbIndex=0..g_KeyboardCount-1 (which simulated Keyboard instance, 0-based),
// col=0..15 (that instance's logical column), green=which button was pressed (FALSE=rot/red,
// TRUE=gruen/green). Computes the CS2 accessory address and sends a CAN-ID 0x16 "Zubehoer schalten"
// message with Strom=an. Each simulated instance gets a kbIndex*16 offset, staying within the same
// s_Basis-selected 256-address block (room for 16 keyboards). Call from mbc89_gui.c's
// HandleKeyboardPress().
void McCom_HandleKeyboardEvent(u8 kbIndex, u8 col, bool green);

// Incoming (CAN -> Panel): decodes a received message and, if it is a CAN-ID 0x16 "Zubehoer
// schalten" (DLC=6) addressed to one of this simulated Keyboard family's columns (any of the
// g_KeyboardCount instances, not just the one currently on screen), writes which instance to
// *kbIndex, the column index to *col and the direction to *green, and returns TRUE. Returns FALSE
// (kbIndex/col/green unchanged) for any other message, including 0x16 sub-messages with a different
// DLC/layout. Call from mbc89_gui.c's Gui_Run() after McCom_Check() for every received message - the
// caller updates that instance's own state regardless of which one is currently displayed, only
// touching the screen if it happens to be the visible one.
bool McCom_DecodeKeyboardEvent(const u8* msg, u8* kbIndex, u8* col, bool* green);

// --- CU6021 loco control ---
// addr is the display convention (1..80, 80 maps to array index 0). All loco-control functions
// below are gated by McCom_IsLokControllable() internally (no-op if not mapped/INFRA-owned) EXCEPT
// McCom_HandleStopGo(), which controls the track power (always active).

// Loco DB dimensions, exposed here (not just mbc89_com.c-local) so mbc89_base.c's PC_ARRAY_DATA
// handler (ParaCenter's generic systemarray byte read/write, used by its "Lokeigenschaften" dialog)
// can compute the absolute systemarray range this database occupies - see McCom_LokAt() below.
#define LOKS_ANZAHL 80
#define LOKS_ITEMS  10

// Byte offset of the MAPPED field within one loco's 10-byte record - exposed so mbc89_base.c's
// PC_ARRAY_DATA bridge can normalize it for ParaCenter: the internal byte is
// 0x01|LOK_MAPPED_ECHT=0x03 for a CONNECT6021-mapped loco, but ParaCenter checks "=1" exactly.
#define LOK_MAPPED 8

// Direct pointer into one loco's 10-byte record (index 0..79, addr 1..80 with 80->index 0 - this
// function takes the raw array index directly, not the display address). Field layout: LOK_SPEED=0,
// LOK_DIR=1, LOK_F0=2, LOK_FX=3, LOK_ADDR4..1=4..7, LOK_MAPPED=8, LOK_INFRA=9. Exposed for
// mbc89_base.c's PC_ARRAY_DATA handler to bridge ParaCenter's "Lokeigenschaften" dialog directly onto
// this module's live loco state.
u8* McCom_LokAt(u8 index);

// Read-only: TRUE if addr is CONNECT6021-mapped (lok[LOK_MAPPED]) and not INFRA-owned (lok[LOK_INFRA])
// - i.e. whether loco control for this address is currently meaningful. Call from mbc89_gui.c to gate
// the CU6021 panel's f1-f4/f0/direction/speed controls.
bool McCom_IsLokControllable(u8 addr);

// Read-only: current speed(0..15)/direction(0/1)/F0/F1-F4-bitmask for addr, straight from lok[] -
// call once a complete address is entered so the GUI can sync its display to the loco's real state
// instead of always starting "unconfigured".
void McCom_GetLokState(u8 addr, u8* speed, u8* dir, bool* f0, u8* fx);

// Direction values - NOT 0/1: a bare 0 is not a valid direction value on the real bus, it is merely
// lok[LOK_DIR]'s uninitialized-at-reset default.
#define DIR_VORWAERTS   1
#define DIR_RUECKWAERTS 2

void McCom_HandleLocoSpeed(u8 addr, u8 mmSpeed); // 0..15
void McCom_HandleLocoDir(u8 addr, u8 dir);       // DIR_VORWAERTS or 0
void McCom_HandleLocoF0(u8 addr, bool on);
void McCom_HandleLocoFx(u8 addr, u8 fxIndex, bool on); // fxIndex 1..4

// Force-sends the CURRENT speed/dir/F0/F1-F4 state for addr as CAN, unlike McCom_HandleLoco*() above
// (which only send on an actual value change) - for an explicit "announce current state"
// confirmation. Call right after addr gets freshly CONNECT6021-mapped, and right after a CU6021/
// Control80f panel takes it over (mbc89_gui.c's SyncFromLokState()).
void McCom_SyncLokState(u8 addr);

// Sends the CU6021's own Stopp/Go broadcast - NOT gated by McCom_IsLokControllable(), track power
// always works regardless of any loco's mapping state.
void McCom_HandleStopGo(bool go);

// Read-only: TRUE if the system is currently stopped, kept in sync from BOTH directions (own button
// press AND incoming CAN-ID 0x00 from elsewhere on the bus) - call from mbc89_gui.c to keep the
// CU6021 panel's GO-LED in sync with the real bus state, not just this GUI's own last press.
bool McCom_IsSystemStopped(void);

// --- Persistence (mbc89_store.c) ---
// Copies the persisted fields out (sw2 must point to a 2-byte buffer) - used by Store_Save() to
// build the on-disk record from the current live state.
void McCom_GetPersist(u8* sw2, u8* mapping, u8* basis, u8* i2c);

// Applies persisted fields loaded from disk (sw2 points to a 2-byte buffer), each range-checked
// exactly like McCom_Cs2IndexParameterExtern() already does - a corrupted/stale file must never
// silently produce an out-of-range runtime value. An out-of-range field is left at McCom_Init()'s
// compile-time default. Used by Store_Load() once at startup.
void McCom_SetPersist(const u8* sw2, u8 mapping, u8 basis, u8 i2c);

// --- ParaCenter "Stammdaten" dialog bridge (mbc89_base.c's PC_ARRAY_DATA handler, absolute
// systemarray offsets MBC_89_S_SW/MAPPING/BASIS/I2C) ---
// Raw single-byte access, deliberately WITHOUT the CS2 path's live loco-mapping cascade
// (McCom_Cs2IndexParameterExtern()'s Mapping write updates every non-ECHT-mapped loco immediately) -
// this path only persists, never cascades. Each setter DOES range-check (same ranges as
// McCom_SetPersist()) and returns TRUE only when the value was both valid and actually different
// from the current one - an out-of-range byte is rejected outright, and a valid-but-unchanged byte
// is a no-op, so mbc89_base.c's PC_ARRAY_DATA handler only needs to persist when a setter reports a
// real change.
u8   McCom_GetSw(u8 index);	// index 0=Major, 1=Minor
bool McCom_SetSw(u8 index, u8 value);
u8   McCom_GetMapping(void);
bool McCom_SetMapping(u8 value);
u8   McCom_GetBasis(void);
bool McCom_SetBasis(u8 value);
u8   McCom_GetI2c(void);
bool McCom_SetI2c(u8 value);
