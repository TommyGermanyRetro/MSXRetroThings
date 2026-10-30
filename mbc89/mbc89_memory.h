//***************************************************************************************************
//* HEADER - mbc89_memory.h - Memory 6043: Fahrstraßen-Aufnahme/-Wiedergabe (diskettenbasiert)        *
//***************************************************************************************************
//* Bis zu MEMORY_COUNT_MAX simulierte Memory-6043-Instanzen, je MEM_ROUTES_PER_INSTANCE Fahrstraßen   *
//* (A1-A8/B1-B8/C1-C8) mit bis zu MEM_CMDS_PER_ROUTE Schaltbefehlen. Fahrstraßen-Inhalte liegen in     *
//* MBC89R.CFG (satzweiser FCB-Random-Access, ein 128-Byte-Satz pro Fahrstraße), nicht im RAM - nur     *
//* kompakte Gültig-/Aktiv-Bitfelder und der Aufnahme-Zustand je Instanz sind RAM-resident. Verriegelung *
//* nach Handbuch 6043 Abschnitt 3.3, Automatikbetrieb mit Rückmeldern nach Abschnitt 4.                *
//***************************************************************************************************
//* Datei: mbc89_memory.h                                                                             *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// Compile-time MAX number of simulierter Memory-6043-Instanzen (reale Spezifikation: bis zu 4 je
// Central Unit) und Fahrstraßen je Instanz (A1-A8/B1-B8/C1-C8 = 24). Die tatsächliche Laufzeitanzahl
// (0..MEMORY_COUNT_MAX, aus der MEMB-Umgebungsvariable) ist g_MemoryCount.
#define MEMORY_COUNT_MAX        4
#define MEM_ROUTES_PER_INSTANCE 24
#define MEM_CMDS_PER_ROUTE      20

extern u8 g_MemoryCount;
// Verriegelung der Fahrstraßen (Handbuch 3.3, an der echten Memory Codierschalter 3+4): 1 = an, aus der
// MEMV-Umgebungsvariable, gilt für alle Instanzen.
extern u8 g_MemInterlock;
// Je Memory die s88-Moduladresse (1-93, wie im ParaCenter bei mbc88/mbc90) ihrer Reihe A, 0 = keine
// Rückmelder, aus den Umgebungsvariablen S881-S884; Reihe B und C liegen auf den beiden folgenden Adressen.
// Je Modul Eingang 1-8 = Stell-, 9-16 = Freigabe-Eingang der Fahrstraßen 1-8 der Reihe (Handbuch 4.1).
extern u8 g_S88Module[MEMORY_COUNT_MAX];

// Liest MBC89R.CFG (legt sie bei Erststart an) und füllt die Gültig-Bitfelder durch einmaliges Scannen
// aller MEMORY_COUNT_MAX*MEM_ROUTES_PER_INSTANCE Sätze; stellt gültige Fahrstraßen und Betriebsart
// (Automatik/Hand) des letzten Laufs wieder her. Aufruf einmalig in main(), nach Store_Load().
void McMem_Init(void);

// Status-Abfragen fürs GUI-Panel (globalRoute = instanz*MEM_ROUTES_PER_INSTANCE + lokaleRoute,
// lokaleRoute 0-7=A1-A8, 8-15=B1-B8, 16-23=C1-C8).
bool McMem_IsValid(u8 globalRoute);
bool McMem_IsActive(u8 globalRoute);

// TRUE, ob localIdx (0-28, wie s_Hotspot im GUI: 0-4=ex/off/in/end/clr, 5-28=A1-C8) für diese Instanz
// gerade leuchten soll - kapselt die komplette LED-Logik (Aufnahme-Rückmeldung auf "in" und der gerade
// aufgenommenen Route, sonst nur aktive Routen), damit mbc89_gui.c nur noch die VRAM-Pokes macht.
bool McMem_LedOn(u8 instance, u8 localIdx);

// Dispatcht einen Tastendruck (instance 0-based, localIdx wie s_Hotspot 0-28) - Aufnahme-Start/-Ende/
// -Löschen über ex/off/in/end/clr (off/ex sind in 8a noch ohne Funktion, siehe Plan 8c) sowie
// Fahrstraßen-Wahl/-Wiedergabe über A1-C8. Kann synchron eine CAN-Wiedergabe-Salve auslösen (McCom_
// SendZubehoer()). Aufruf aus mbc89_gui.c's HandleMemoryPress().
void McMem_HandlePress(u8 instance, u8 localIdx);

// Hängt einen soeben vom Keyboard-6040 gesendeten Schaltbefehl an jede Instanz an, die sich gerade im
// aktiven Aufnahme-Zustand befindet (0 bis mehrere gleichzeitig, siehe .c) - gleiche Adresse
// überschreibt den bestehenden Eintrag statt zu duplizieren (Spec: nur einmal pro Fahrstraße), bis
// max. MEM_CMDS_PER_ROUTE Einträge. No-op, wenn keine Instanz gerade aufnimmt. Aufruf aus
// mbc89_com.c's McCom_HandleKeyboardEvent(), direkt nach dessen eigenem SendZubehoerSchalten()-Aufruf.
void McMem_TapKeyboardCommand(u16 addr, bool richtung);

// Ein Magnetartikel wurde geschaltet (eigenes Keyboard oder eingehende 0x16 vom Bus): jede gültige
// Fahrstraße, die ihn in der anderen Richtung enthält, erlischt (Handbuch 3.3.3, gilt auch ohne
// Verriegelung). Prüft zuerst eine RAM-Übersicht, liest MBC89R.CFG nur bei einem Treffer. TRUE, wenn
// sich eine Fahrstraßen-LED geändert hat.
bool McMem_NoteSwitch(u16 addr, bool richtung);

// Zeitbasis fürs Warnblinken kreuzender Fahrstraßen - Aufruf in jedem Durchlauf von Gui_Run(). Liefert
// die Instanz, deren LEDs neu zu zeichnen sind, sonst 0xFF.
u8 McMem_Tick(void);

// S88-Ereignis vom Bus (CAN-ID 0x23, D2/D3 Pinnummer = 1000 x Bus + 16 x (Modul im Bus - 1) + Kontakt, D5
// neuer Zustand): im Automatikbetrieb fordert ein Stell-Eingang die Fahrstraße an, ein Freigabe-Eingang
// gibt sie frei bzw. sperrt ihre Anforderung (Handbuch 4.4). TRUE, wenn sich eine LED geändert haben kann.
bool McMem_S88Event(u16 pin, bool active);
