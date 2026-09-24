//***************************************************************************************************
//* HEADER - mbc89_gui.h - GUI-Prozessor: SCREEN2 Keyboard-6040/CU6021/Control80f-Karussell            *
//***************************************************************************************************
//* Grafische Bedienoberflaeche fuer bis zu 16 Keyboard-Instanzen, 1 CU6021, bis zu 8 Control80f-       *
//* Instanzen (KEYB/C80F-Umgebungsvariablen, mbc89.c), per Karussell (zwei Dreieck-Sprites, SPACE zum   *
//* Wechseln) umschaltbar. Ein Tastendruck loest McCom_HandleKeyboardEvent()/McCom_HandleLoco*() aus    *
//* (CAN-ID 0x16/0x08/0x0A/0x0C); eingehende Nachrichten aktualisieren ueber                            *
//* McCom_DecodeKeyboardEvent()/McCom_GetLokState() live die gerade gezeigte Instanz - beide            *
//* Richtungen laufen in Gui_Run()'s Hauptschleife.                                                     *
//***************************************************************************************************
//* Datei: mbc89_gui.h                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// Compile-time MAX number of simulated Control80f instances (array sizing only - s_CuState[] via
// CU_INSTANCE_COUNT in mbc89_gui.c) - shared here so mbc89.c's env-var range check (GetEnvDecimal
// ("C80F", C80F_COUNT_MAX, ...)) and mbc89_gui.c's array sizes can never drift apart. The actual
// runtime count (0..C80F_COUNT_MAX, read from the C80F env var at startup) is g_C80fCount below.
#define C80F_COUNT_MAX 8
extern u8 g_C80fCount;

// Splash screen, shown for ~5s then blanked again. Standalone SCREEN2 excursion - call once, right
// after the IO/INT/KEYB/C80F env-var gate (still SCREEN0-ready state, no VDP mode set yet), not as
// part of Gui_Init()/Gui_Run(). Own name-table/pattern/color VRAM setup, no dependency on Gui_Init()
// having run yet - and vice versa, Gui_Init() redoes its own SCREEN2 setup afterward regardless, so
// this doesn't need to leave anything behind for it to build on.
void Gui_ShowSplash(void);

// One-time SCREEN2 setup: identity name table, Keyboard panel + cursor-frame sprite load. Call once,
// before the first Gui_Run().
void Gui_Init(void);

// Main interactive+CAN loop - runs Can_Check/McBase_Check/McCom_Check plus the Keyboard panel's
// cursor navigation, SPACE-to-toggle, and both CAN directions (outgoing on button press, incoming LED
// sync). Returns when ESC is pressed.
void Gui_Run(void);
