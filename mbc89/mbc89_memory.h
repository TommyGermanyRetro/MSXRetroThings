//***************************************************************************************************
//* HEADER - mbc89_memory.h - Memory 6043: route recording/playback (disk-based)                    *
//***************************************************************************************************
//* Up to MEMORY_COUNT_MAX simulated Memory-6043 instances, each with MEM_ROUTES_PER_INSTANCE       *
//* routes (A1-A8/B1-B8/C1-C8) of up to MEM_CMDS_PER_ROUTE switch commands. Route contents live in  *
//* MBC89R.CFG (record-wise FCB random access, one 128-byte record per route), not in RAM - only    *
//* compact valid/active bit fields and the recording state per instance are RAM-resident.          *
//* Interlocking per manual 6043 section 3.3, automatic mode with feedback modules per section 4.   *
//***************************************************************************************************
//* File:  mbc89_memory.h                                                                             *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// Compile-time MAX number of simulated Memory-6043 instances (real spec: up to 4 per Central Unit)
// and of routes per instance (A1-A8/B1-B8/C1-C8 = 24). The actual runtime count
// (0..MEMORY_COUNT_MAX, from the MEMB environment variable) is g_MemoryCount.
#define MEMORY_COUNT_MAX        4
#define MEM_ROUTES_PER_INSTANCE 24
#define MEM_CMDS_PER_ROUTE      20

extern u8 g_MemoryCount;
// Route interlocking (manual 3.3, DIP switches 3+4 on the real Memory): 1 = on, from the MEMV
// environment variable, applies to all instances.
extern u8 g_MemInterlock;
// Per Memory the s88 module address (1-93, as in ParaCenter for mbc88/mbc90) of its row A, 0 = no
// feedback, from the environment variables S881-S884; rows B and C use the two following addresses.
// Per module, inputs 1-8 = set inputs, 9-16 = release inputs of routes 1-8 of the row (manual 4.1).
extern u8 g_S88Module[MEMORY_COUNT_MAX];

// Reads MBC89R.CFG (creates it on first start) and fills the valid bit fields by scanning all
// MEMORY_COUNT_MAX*MEM_ROUTES_PER_INSTANCE records once; restores the valid routes and the operating
// mode (automatic/manual) of the last run. Call once in main(), after Store_Load().
void McMem_Init(void);

// Status queries for the GUI panel (globalRoute = instance*MEM_ROUTES_PER_INSTANCE + localRoute,
// localRoute 0-7=A1-A8, 8-15=B1-B8, 16-23=C1-C8).
bool McMem_IsValid(u8 globalRoute);
bool McMem_IsActive(u8 globalRoute);

// TRUE if localIdx (0-28, like s_Hotspot in the GUI: 0-4=ex/off/in/end/clr, 5-28=A1-C8) should
// currently be lit for this instance - encapsulates the complete LED logic (recording feedback on
// "in" and on the route being recorded, otherwise only active routes), so that mbc89_gui.c only
// does the VRAM pokes.
bool McMem_LedOn(u8 instance, u8 localIdx);

// Dispatches a key press (instance 0-based, localIdx like s_Hotspot 0-28) - recording start/end/
// clear via ex/off/in/end/clr (off/ex still have no function in 8a, see plan 8c) and route
// selection/playback via A1-C8. May synchronously trigger a CAN playback burst
// (McCom_SendZubehoer()). Called from mbc89_gui.c's HandleMemoryPress().
void McMem_HandlePress(u8 instance, u8 localIdx);

// Appends a switch command just sent by the Keyboard-6040 to every instance currently in the active
// recording state (0 to several at once, see .c) - the same address overwrites the existing entry
// instead of duplicating it (spec: only once per route), up to MEM_CMDS_PER_ROUTE entries. No-op if
// no instance is recording. Called from mbc89_com.c's McCom_HandleKeyboardEvent(), right after its
// own SendZubehoerSchalten() call.
void McMem_TapKeyboardCommand(u16 addr, bool richtung);

// A solenoid accessory was switched (own Keyboard or incoming 0x16 from the bus): every valid route
// that contains it in the other direction goes out (manual 3.3.3, also applies without
// interlocking). Checks a RAM overview first and reads MBC89R.CFG only on a hit. TRUE if a route
// LED has changed.
bool McMem_NoteSwitch(u16 addr, bool richtung);

// Time base for the warning flash of crossing routes - call in every pass of Gui_Run(). Returns the
// instance whose LEDs need to be redrawn, otherwise 0xFF.
u8 McMem_Tick(void);

// S88 event from the bus (CAN-ID 0x23, D2/D3 pin number = 1000 x bus + 16 x (module in bus - 1) +
// contact, D5 new state): in automatic mode a set input requests the route, a release input
// releases it or cancels its request (manual 4.4). TRUE if an LED may have changed.
bool McMem_S88Event(u16 pin, bool active);
