//***************************************************************************************************
//* HEADER - mbc89_store.h - File-based persistence (EEPROM replacement) for mbc89_base/mbc89_com     *
//***************************************************************************************************
//* Persists GUiD/CS2-Geraetegruppe/PC-Datenbanknummer/Modulname (mbc89_base.c) and simulated          *
//* Connect6021 SW-Version/Zentralentyp-Mapping/Keyboard-Basisadresse/I2C-Takt (mbc89_com.c) to         *
//* MBC89.CFG on the boot disk, via the FCB-based DOS1-compatible file functions already implemented   *
//* in dos.c (DOS_USE_FCB, not gated behind DOS2 like the newer Handle-based DOS_FOpen/DOS_FRead this   *
//* project's Target="DOS1" cannot use). NOT persisted: the loco database (rebuilt from the             *
//* CONNECT6021 stream on every CS3 reconnect) and Kennung (mbc89 always forces the fixed               *
//* MCAN_KENNER_FIXED_H/L constants, never a PC-proposed value).                                        *
//***************************************************************************************************
//* Datei: mbc89_store.h                                                                              *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// Reads MBC89.CFG (if present) and applies the persisted values via McBase_SetPersist()/
// McCom_SetPersist(), overriding the compile-time defaults McBase_Init()/McCom_Init() just set up.
// If the file does not exist (first start), the compile-time defaults are left untouched and
// Store_Save() is called once to create the file from them. Call once, right after McBase_Init()/
// McCom_Init() and before any CAN traffic is processed.
void Store_Load(void);

// Rewrites MBC89.CFG from the CURRENT values (McBase_GetPersist()/McCom_GetPersist()). Call from
// mbc89_base.c/mbc89_com.c whenever a persisted field actually changes - several call sites, not one
// central flush.
void Store_Save(void);
