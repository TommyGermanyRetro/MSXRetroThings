//***************************************************************************************************
//* HEADER - mbc89_base.h - PC-/CS2 registration and config-channel state machine                    *
//***************************************************************************************************
//* PC registration ("Neuanmeldeautomat") plus CS2 ping/config-channel response logic. PC_ARRAY/       *
//* PC_ARRAY_DATA (ParaCenter's byte-by-byte systemarray read/write) is bridged against a unified view *
//* of s_Arr[] (this file) and mbc89_sim.c's g_SimArr[] (indices 0-69) plus mbc89_com.c's loco         *
//* database and 5 module-specific config fields (indices 328/329/1205/1206/1207) beyond that - any    *
//* other index is silently ignored. Not implemented: PC_UPGRADE/PC_UPGRADE_DATA/PC_BOOT (firmware-    *
//* over-CAN, needs a boot storage chip this project has none of). Persistence (GUiD/CS2-              *
//* Geraetegruppe/PC-Datenbanknummer/Modulname) is file-based via mbc89_store.c - see                  *
//* McBase_GetPersist()/SetPersist() below. CS2-ping response staggering and the inter-packet          *
//* config-channel delay are not implemented - with a single test unit on the bus and Can_Tx()         *
//* already blocking until the frame is sent, both are safe to omit (see mbc89_base.c for detail).     *
//***************************************************************************************************
//* Datei: mbc89_base.h                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// Prepares the RAM-only test identity (article/SW/HW/name, blank GUID) and clears all protocol
// state. Call once at startup, after mbc89_sim.c's simulated Plug/Voltage/Temperature are set up.
void McBase_Init(void);

// Evaluates one received, decoded logical Mbc89Can message (see mbc89_can.h) and responds via
// Can_Tx() as needed - PC registration (Loc-ID 0x18xx) and CS2 ping/config-channel/messwert
// queries.
void McBase_Check(const u8* msg);

// True once PC registration completed (module has a valid GUID).
bool McBase_IsRegistered(void);

// True once at least one CS2 config-channel walk has been answered.
bool McBase_IsCs2Init(void);

// Overwrites the CS2 ping response's SW-version fields (D4/D5) - used by mbc89_com.c's
// McCom_HashInit() to report a simulated Connect6021 firmware version instead of this MSX port's own.
void McBase_SetPingSw(u8 major, u8 minor);

// Copies the persisted fields out (guid4/db12/name20 must point to 4-/12-/20-byte buffers) - used
// by Store_Save() to build the on-disk record from the current live state. name20 is the module
// name (O_NAME), changeable via ParaCenter and persisted, defaulting to the serial number on an
// unconfigured module (see McBase_Init()'s NameFromSerial()).
void McBase_GetPersist(u8* guid4, u8* cs2Ger, u8* db12, u8* name20);

// Applies persisted fields loaded from disk (guid4/db12/name20 point to 4-/12-/20-byte buffers) -
// used by Store_Load() once at startup, overriding McBase_Init()'s compile-time defaults. No range
// validation needed here (unlike McCom_SetPersist()) - any byte value is a legal GUiD/DB/name byte.
void McBase_SetPersist(const u8* guid4, u8 cs2Ger, const u8* db12, const u8* name20);
