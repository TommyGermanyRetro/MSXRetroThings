//***************************************************************************************************
//* SOURCE - mbc89_memory.c - Memory 6043: Fahrstraßen-Aufnahme/-Wiedergabe (diskettenbasiert)        *
//***************************************************************************************************
//* Datei: mbc89_memory.c                                                                             *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
#include "mbc89_memory.h"
#include "mbc89_com.h"
#include "mbc89_gui.h"
#include "dos.h"
#include "memory.h"

//===================================================================================================
// FILE FORMAT
//===================================================================================================
// MBC89R.CFG, MEM_TOTAL_ROUTES feste 128-Byte-Sätze (wahlfreier Zugriff über DOS_RandomBlockReadFCB/
// WriteFCB, ein Satz pro Fahrstraße, globalRoute = instanz*MEM_ROUTES_PER_INSTANCE + lokaleRoute):
//   0      Befehlsanzahl (0 = leer/ungueltig, max. MEM_CMDS_PER_ROUTE)
//   1      Folgefahrstrasse: globaler Routenindex 0..(MEM_TOTAL_ROUTES-1), 0xFF = keine
//   2..61  bis zu 20 Befehle x 3 Byte (Adresse-H, Adresse-L, Richtung)
//   62..127 reserviert/Null
// Satz MEM_TOTAL_ROUTES (hinter den Fahrstrassen) haelt den Zustand fuer den naechsten Start:
//   0..11  gueltige Fahrstrassen (1 Bit je Fahrstrasse)
//   12     Automatikbetrieb je Memory (Bit 0-3)
//   13     Kennung STATE_MAGIC (fehlt in Dateien ohne Zustandssatz)

#define MEM_RECORD_SIZE  128
#define MEM_TOTAL_ROUTES (MEMORY_COUNT_MAX * MEM_ROUTES_PER_INSTANCE)

#define O_COUNT  0
#define O_FOLLOW 1
#define O_CMDS   2

#define FOLLOW_NONE 0xFF

#define STATE_RECORD MEM_TOTAL_ROUTES
#define O_ST_ACTIVE  0
#define O_ST_AUTO    12
#define O_ST_MAGIC   13
#define STATE_MAGIC  0xA5

// Sentinel-Werte fuer s_RecordingLocalRoute[] - 0..(MEM_ROUTES_PER_INSTANCE-1) = aktive Aufnahme
// dieser lokalen Route, sonst eines der beiden Sentinels unten.
#define REC_IDLE    0xFF // nicht im Aufnahme-Modus
#define REC_WAITING 0xFE // "input" gedrueckt, wartet auf Routenwahl

// MSX-DOS 1 Disk-I/O darf nicht durch Page 1 transferieren (siehe mbc89_store.c) - fester Page-3-
// Puffer, direkt nach mbc89_store.c's STORE_BUF (endet 0xC71F).
#define MEM_FCB 0xC720 // DOS_FCB struct
#define MEM_BUF 0xC756 // 128-Byte Transferpuffer

// Eigene Namen (nicht s_File/s_Buf wie mbc89_store.c) - SDCC haengt __at()-Variablen offenbar als
// globale Symbole ab, "static" schuetzt bei fester Adresse nicht vor Namenskollision ueber
// Compile-Units hinweg (Linker-Fehler "Multiple definition of _s_Buf/_s_File" bei gleichem Namen).
__at(MEM_FCB) DOS_FCB s_MemFile;
__at(MEM_BUF) u8      s_MemBuf[MEM_RECORD_SIZE];

// Guelig-/Aktiv-Bitfelder, 1 Bit pro Fahrstrasse (MEM_TOTAL_ROUTES=96 Bit = 12 Byte).
static u8 s_Valid[(MEM_TOTAL_ROUTES + 7) / 8];
static u8 s_Active[(MEM_TOTAL_ROUTES + 7) / 8];

// Je Instanz: REC_IDLE/REC_WAITING oder die lokale Route (0..23), die gerade aufgenommen wird.
static u8   s_RecordingLocalRoute[MEMORY_COUNT_MAX];
// Nur gueltig waehrend REC_WAITING: naechster Routendruck loescht die GANZE Route statt Aufnahme zu
// starten (Spec: "input"+"clear"+Routenwahl).
static bool s_PendingWholeDelete[MEMORY_COUNT_MAX];
// Nur ausserhalb einer Aufnahme: "clear" gedrueckt, der naechste Routendruck gibt die Route frei
// (Handbuch 3.3.4), "end" bricht ab.
static bool s_PendingRelease[MEMORY_COUNT_MAX];

// Uebersicht fuer McMem_NoteSwitch(): je Adress-Low-Byte ein Bit pro Richtung, ob die Adresse in einer
// gueltigen Fahrstrasse vorkommt. Darf zu viel melden (nach Freigaben), nie zu wenig.
static u8 s_UsedDir0[32];
static u8 s_UsedDir1[32];

// Vergleichsbefehle (Format wie im Satz: AdrH, AdrL, Richtung): die Fahrstrasse, die gestellt werden soll,
// oder der eine geschaltete Magnetartikel - s_MemBuf wird beim Lesen der gueltigen Fahrstrassen ueberschrieben.
static u8 s_CandCount;
static u8 s_CandFollow;
static u8 s_Cand[MEM_CMDS_PER_ROUTE * 3];
// Gueltige Fahrstrassen, die s_Cand kreuzen (Ergebnis von ScanActive()).
static u8 s_Cross[(MEM_TOTAL_ROUTES + 7) / 8];

// Warnblinken kreuzender Fahrstrassen (Handbuch 3.3.3): Instanz (0xFF = keins), Routen, Ende, Takt.
#define BLINK_JIFFIES  250 // ~5 s
#define BLINK_PERIOD   15  // ~0,3 s je Phase
static u8   s_BlinkInstance;
static u8   s_Blink[(MEM_TOTAL_ROUTES + 7) / 8];
static u16  s_BlinkUntil;
static u16  s_BlinkNext;
static bool s_BlinkOn;

// Automatikbetrieb (Handbuch 4): je Instanz an/aus; gespeicherte Anforderungen und belegte Freigabe-Eingaenge
// je Fahrstrasse.
static bool s_Auto[MEMORY_COUNT_MAX];
static u8   s_Pending[(MEM_TOTAL_ROUTES + 7) / 8];
static u8   s_ReleaseHeld[(MEM_TOTAL_ROUTES + 7) / 8];

// PlayRoute(): Handbetrieb (blinkt bei Kreuzung), zweiter Druck auf eine blinkende Fahrstrasse, Automatik
// (blinkt nicht, meldet nur, ob gestellt wurde).
#define PLAY_MANUAL 0
#define PLAY_FORCE  1
#define PLAY_AUTO   2

// Zuletzt gespeicherter Zustand - geschrieben wird nur bei einer echten Aenderung.
static u8 s_SavedActive[(MEM_TOTAL_ROUTES + 7) / 8];
static u8 s_SavedAuto;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
static bool BitGet(const u8* arr, u8 idx)
{
	return (bool)((arr[idx >> 3] >> (idx & 7)) & 1);
}

//---------------------------------------------------------------------------------------------------
static void BitSet(u8* arr, u8 idx, bool on)
{
	if (on) arr[idx >> 3] |= (u8)(1 << (idx & 7));
	else    arr[idx >> 3] &= (u8)~(1 << (idx & 7));
}

//---------------------------------------------------------------------------------------------------
static void OpenFile(void)
{
	Mem_Set(0, &s_MemFile, sizeof(DOS_FCB));
	Mem_Copy("MBC89R  CFG", &s_MemFile.Name, 11);
}

//---------------------------------------------------------------------------------------------------
// Oeffnet MBC89R.CFG fuer eine Folge von Random-Zugriffen. Der Aufrufer schliesst mit EndSession().
// Gibt FALSE zurueck, wenn die Datei nicht existiert (Aufrufer entscheidet, ob das ein Fehler ist).
static bool BeginSession(void)
{
	OpenFile();
	DisableInterrupt();
	if (DOS_OpenFCB(&s_MemFile) != DOS_ERR_NONE)
	{
		EnableInterrupt();
		return FALSE;
	}
	// OPEN laesst das Satzgroessen-Feld unberuehrt - Random Block Read/Write (27h/26h) brauchen es.
	s_MemFile.RecordSize = MEM_RECORD_SIZE;
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
static void EndSession(void)
{
	DOS_CloseFCB(&s_MemFile);
	EnableInterrupt();
}

//---------------------------------------------------------------------------------------------------
// Liest einen Satz nach s_MemBuf. TRUE nur bei echtem Treffer (1 Satz gelesen) - FALSE u.a. dann, wenn
// die Datei noch nicht so weit reicht (frische/teilweise befuellte Datei), s_MemBuf bleibt dann
// unveraendert, der Aufrufer muss selbst auf "leer" zurueckfallen.
static bool ReadRoute(u8 globalRoute)
{
	s_MemFile.RandomRecord = globalRoute;
	DOS_SetTransferAddr(s_MemBuf);
	return (bool)(DOS_RandomBlockReadFCB(&s_MemFile, 1) == 1);
}

//---------------------------------------------------------------------------------------------------
static void WriteRoute(u8 globalRoute)
{
	s_MemFile.RandomRecord = globalRoute;
	DOS_SetTransferAddr(s_MemBuf);
	DOS_RandomBlockWriteFCB(&s_MemFile, 1);
}

//---------------------------------------------------------------------------------------------------
// Leerer Satz (Befehlsanzahl 0, keine Folgefahrstrasse) - fuer Neuanlage/komplettes Loeschen.
static void ClearBuf(void)
{
	Mem_Set(0, s_MemBuf, MEM_RECORD_SIZE);
	s_MemBuf[O_FOLLOW] = FOLLOW_NONE;
}

//---------------------------------------------------------------------------------------------------
static void SummaryAdd(const u8* cmds, u8 count)
{
	u8 i;
	for (i = 0; i < count; i++)
		BitSet(cmds[i * 3 + 2] ? s_UsedDir1 : s_UsedDir0, cmds[i * 3 + 1], TRUE);
}

//---------------------------------------------------------------------------------------------------
// TRUE, wenn der Satz in s_MemBuf einen Magnetartikel aus s_Cand in der anderen Richtung schaltet - nur
// das zaehlt als kreuzend (Handbuch 3.3.1).
static bool CrossesCandidate(void)
{
	u8 n = s_MemBuf[O_COUNT];
	u8 i, j;
	if (n > MEM_CMDS_PER_ROUTE) n = MEM_CMDS_PER_ROUTE;
	for (i = 0; i < n; i++)
	{
		const u8* c = &s_MemBuf[O_CMDS + i * 3];
		for (j = 0; j < s_CandCount; j++)
		{
			const u8* k = &s_Cand[j * 3];
			if ((c[0] == k[0]) && (c[1] == k[1]) && ((c[2] != 0) != (k[2] != 0))) return TRUE;
		}
	}
	return FALSE;
}

//---------------------------------------------------------------------------------------------------
// Liest alle gueltigen Fahrstrassen ausser skip: die s_Cand kreuzenden kommen nach s_Cross, alle bilden die
// neue Uebersicht. FALSE, wenn MBC89R.CFG nicht zu oeffnen war.
static bool ScanActive(u8 skip)
{
	u8 r;
	Mem_Set(0, s_Cross, sizeof(s_Cross));
	Mem_Set(0, s_UsedDir0, sizeof(s_UsedDir0));
	Mem_Set(0, s_UsedDir1, sizeof(s_UsedDir1));
	if (!BeginSession()) return FALSE;
	for (r = 0; r < MEM_TOTAL_ROUTES; r++)
	{
		if ((r == skip) || !BitGet(s_Active, r) || !ReadRoute(r)) continue;
		if (CrossesCandidate()) BitSet(s_Cross, r, TRUE);
		SummaryAdd(&s_MemBuf[O_CMDS], (s_MemBuf[O_COUNT] > MEM_CMDS_PER_ROUTE) ? MEM_CMDS_PER_ROUTE : s_MemBuf[O_COUNT]);
	}
	EndSession();
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
// Warnblinken: die kreuzenden gueltigen Fahrstrassen der Instanz und die aufgerufene.
static void StartBlink(u8 instance, u8 route)
{
	u8 r, first = (u8)(instance * MEM_ROUTES_PER_INSTANCE);
	Mem_Set(0, s_Blink, sizeof(s_Blink));
	for (r = first; r < first + MEM_ROUTES_PER_INSTANCE; r++)
		if (BitGet(s_Cross, r)) BitSet(s_Blink, r, TRUE);
	BitSet(s_Blink, route, TRUE);
	s_BlinkInstance = instance;
	s_BlinkUntil = (u16)(g_JIFFY + BLINK_JIFFIES);
	s_BlinkNext = (u16)(g_JIFFY + BLINK_PERIOD);
	s_BlinkOn = TRUE;
}

//---------------------------------------------------------------------------------------------------
static u8 AutoBits(void)
{
	u8 i, bits = 0;
	for (i = 0; i < MEMORY_COUNT_MAX; i++)
		if (s_Auto[i]) bits |= (u8)(1 << i);
	return bits;
}

//---------------------------------------------------------------------------------------------------
// Schreibt gueltige Fahrstrassen und Betriebsart in den Zustandssatz, aber nur wenn sie sich seit dem letzten
// Schreiben geaendert haben. Aufruf am Ende jeder oeffentlichen Funktion, die sie aendern kann.
static void SaveState(void)
{
	u8 auto8 = AutoBits();
	u8 i;
	bool same = (bool)(auto8 == s_SavedAuto);
	for (i = 0; same && (i < sizeof(s_Active)); i++) same = (bool)(s_Active[i] == s_SavedActive[i]);
	if (same) return;
	if (!BeginSession()) return;
	Mem_Set(0, s_MemBuf, MEM_RECORD_SIZE);
	Mem_Copy(s_Active, &s_MemBuf[O_ST_ACTIVE], sizeof(s_Active));
	s_MemBuf[O_ST_AUTO]  = auto8;
	s_MemBuf[O_ST_MAGIC] = STATE_MAGIC;
	WriteRoute(STATE_RECORD);
	EndSession();
	Mem_Copy(s_Active, s_SavedActive, sizeof(s_Active));
	s_SavedAuto = auto8;
}

//===================================================================================================
// PUBLIC FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
void McMem_Init(void)
{
	u8 i;

	for (i = 0; i < sizeof(s_Valid); i++) { s_Valid[i] = 0; s_Active[i] = 0; }
	for (i = 0; i < MEMORY_COUNT_MAX; i++)
	{
		s_RecordingLocalRoute[i] = REC_IDLE;
		s_PendingWholeDelete[i] = FALSE;
		s_PendingRelease[i] = FALSE;
		s_Auto[i] = FALSE;
	}
	for (i = 0; i < sizeof(s_Pending); i++) { s_Pending[i] = 0; s_ReleaseHeld[i] = 0; }
	for (i = 0; i < sizeof(s_UsedDir0); i++) { s_UsedDir0[i] = 0; s_UsedDir1[i] = 0; }
	s_CandCount = 0;
	s_BlinkInstance = 0xFF;

	if (BeginSession())
	{
		u8 r;
		for (r = 0; r < MEM_TOTAL_ROUTES; r++)
			if (ReadRoute(r)) BitSet(s_Valid, r, (bool)(s_MemBuf[O_COUNT] > 0));
		// Zustand des letzten Laufs: nur Fahrstrassen, die es noch gibt; Automatik nur fuer vorhandene Memorys
		if (ReadRoute(STATE_RECORD) && (s_MemBuf[O_ST_MAGIC] == STATE_MAGIC))
		{
			for (i = 0; i < sizeof(s_Active); i++) s_Active[i] = (u8)(s_MemBuf[O_ST_ACTIVE + i] & s_Valid[i]);
			for (i = 0; i < g_MemoryCount; i++) s_Auto[i] = (bool)((s_MemBuf[O_ST_AUTO] >> i) & 1);
		}
		EndSession();
		// Uebersicht fuer McMem_NoteSwitch() aus den gueltigen Fahrstrassen (leere Vergleichsliste: nichts kreuzt)
		(void)ScanActive(0xFF);
	}
	else
	{
		// Datei existiert noch nicht (Erststart) - anlegen, leer lassen. Jeder spaetere WriteRoute()
		// materialisiert ihren eigenen Satz on demand, kein Vorab-Befuellen aller MEM_TOTAL_ROUTES
		// Saetze noetig.
		OpenFile();
		DisableInterrupt();
		DOS_CreateFCB(&s_MemFile);
		DOS_CloseFCB(&s_MemFile);
		EnableInterrupt();
	}
	Mem_Copy(s_Active, s_SavedActive, sizeof(s_Active));
	s_SavedAuto = AutoBits();
}

//---------------------------------------------------------------------------------------------------
bool McMem_IsValid(u8 globalRoute)
{
	return BitGet(s_Valid, globalRoute);
}

//---------------------------------------------------------------------------------------------------
bool McMem_IsActive(u8 globalRoute)
{
	return BitGet(s_Active, globalRoute);
}

//---------------------------------------------------------------------------------------------------
bool McMem_LedOn(u8 instance, u8 localIdx)
{
	if (localIdx == 2) // "in" - leuchtet solange die Instanz im Aufnahme-Modus ist (Wahl oder Aufnahme)
		return (bool)(s_RecordingLocalRoute[instance] != REC_IDLE);
	if (localIdx == 4) // "clr" - wartet auf die Fahrstrasse, die freigegeben werden soll
		return s_PendingRelease[instance];
	if (localIdx == 0) // "ex" - Automatikbetrieb
		return s_Auto[instance];
	if (localIdx == 1) // "off" - Handbetrieb
		return (bool)!s_Auto[instance];
	if (localIdx < 5) // "end" - ohne eigene Dauer-Anzeige
		return FALSE;
	{
		u8 localRoute = (u8)(localIdx - 5);
		u8 globalRoute = (u8)(instance * MEM_ROUTES_PER_INSTANCE + localRoute);
		if ((s_BlinkInstance == instance) && BitGet(s_Blink, globalRoute)) return s_BlinkOn;
		if (s_RecordingLocalRoute[instance] == localRoute) return TRUE; // gerade aufgenommene Route
		return McMem_IsActive((u8)(instance * MEM_ROUTES_PER_INSTANCE + localRoute));
	}
}

//---------------------------------------------------------------------------------------------------
static void FinishRecording(u8 instance)
{
	u8 localRoute = s_RecordingLocalRoute[instance];
	if (localRoute < MEM_ROUTES_PER_INSTANCE)
	{
		u8 globalRoute = (u8)(instance * MEM_ROUTES_PER_INSTANCE + localRoute);
		if (BeginSession())
		{
			if (ReadRoute(globalRoute)) BitSet(s_Valid, globalRoute, (bool)(s_MemBuf[O_COUNT] > 0));
			EndSession();
		}
	}
	s_RecordingLocalRoute[instance] = REC_IDLE;
}

//---------------------------------------------------------------------------------------------------
static void RemoveLastCommand(u8 instance)
{
	u8 localRoute = s_RecordingLocalRoute[instance];
	u8 globalRoute;
	if (localRoute >= MEM_ROUTES_PER_INSTANCE) return;
	globalRoute = (u8)(instance * MEM_ROUTES_PER_INSTANCE + localRoute);
	if (!BeginSession()) return;
	if (ReadRoute(globalRoute) && (s_MemBuf[O_COUNT] > 0))
	{
		s_MemBuf[O_COUNT]--;
		WriteRoute(globalRoute);
	}
	EndSession();
}

//---------------------------------------------------------------------------------------------------
static void DeleteWholeRoute(u8 globalRoute)
{
	if (BeginSession())
	{
		ClearBuf();
		WriteRoute(globalRoute);
		EndSession();
	}
	BitSet(s_Valid, globalRoute, FALSE);
	BitSet(s_Active, globalRoute, FALSE);
}

//---------------------------------------------------------------------------------------------------
// Spielt globalRoute ab, folgt einer evtl. gesetzten Folgefahrstrasse (max. 5 Kettenglieder gegen
// Endlosschleifen). Kein kuenstliches Delay zwischen den Befehlen - entspricht schnellem manuellem
// Tastendruecken derselben Tasten, siehe Plan 8a. Je Kettenglied wird der Satz gelesen und die Datei
// wieder geschlossen, bevor gesendet wird: CAN-Versand nie bei gesperrten Interrupts, die Befehle
// kommen direkt aus s_MemBuf (bleibt nach EndSession() unveraendert).
static bool PlayRoute(u8 globalRoute, u8 mode)
{
	u8 hop, cur = globalRoute;
	bool force = (bool)(mode == PLAY_FORCE);
	bool set = FALSE;
	const u8 instance = (u8)(globalRoute / MEM_ROUTES_PER_INSTANCE);

	for (hop = 0; hop < 5; hop++)
	{
		u8 i, r;
		bool ok;
		if (!BeginSession()) return set;
		ok = ReadRoute(cur);
		EndSession();
		if (!ok) break;
		s_CandCount  = (s_MemBuf[O_COUNT] > MEM_CMDS_PER_ROUTE) ? MEM_CMDS_PER_ROUTE : s_MemBuf[O_COUNT];
		s_CandFollow = s_MemBuf[O_FOLLOW];
		Mem_Copy(&s_MemBuf[O_CMDS], s_Cand, (u16)(s_CandCount * 3));
		if (!ScanActive(cur)) break;
		if (g_MemInterlock && !force)
		{
			// Mit Verriegelung blockiert nur eine kreuzende gueltige Fahrstrasse derselben Memory (3.3.3)
			bool blocked = FALSE;
			for (r = (u8)(instance * MEM_ROUTES_PER_INSTANCE); r < (u8)((instance + 1) * MEM_ROUTES_PER_INSTANCE); r++)
				if (BitGet(s_Cross, r)) blocked = TRUE;
			if (blocked)
			{
				if (BitGet(s_Active, cur)) SummaryAdd(s_Cand, s_CandCount);
				// Folgefahrstrassen werden dann still nicht ausgefuehrt, nur der Aufruf selbst blinkt
				if ((hop == 0) && (mode == PLAY_MANUAL)) StartBlink(instance, cur);
				break;
			}
		}
		for (i = 0; i < s_CandCount; i++)
		{
			u16 addr     = (u16)(((u16)s_Cand[i * 3] << 8) | s_Cand[i * 3 + 1]);
			u8  richtung = s_Cand[i * 3 + 2];
			McCom_SendZubehoer(addr, richtung);
			Gui_MirrorZubehoer(addr, richtung);
		}
		// Die gestellte Fahrstrasse zerstoert jede gueltige, die sie kreuzt
		for (r = 0; r < MEM_TOTAL_ROUTES; r++)
			if (BitGet(s_Cross, r)) BitSet(s_Active, r, FALSE);
		BitSet(s_Active, cur, TRUE);
		SummaryAdd(s_Cand, s_CandCount);
		set = TRUE;
		if (s_CandFollow == FOLLOW_NONE) break;
		cur = s_CandFollow;
		force = FALSE;
	}
	return set;
}

//---------------------------------------------------------------------------------------------------
// Stellt gespeicherte Anforderungen der Automatik, sobald nichts mehr dagegen steht; feste Prioritaet A1 vor
// A2 ... vor C8 (Handbuch 4.4). Aufruf nach jeder Freigabe.
static void TryPending(void)
{
	u8 r;
	for (r = 0; r < MEM_TOTAL_ROUTES; r++)
	{
		if (!BitGet(s_Pending, r)) continue;
		if (!s_Auto[r / MEM_ROUTES_PER_INSTANCE] || BitGet(s_ReleaseHeld, r) || PlayRoute(r, PLAY_AUTO))
			BitSet(s_Pending, r, FALSE);
	}
}

//---------------------------------------------------------------------------------------------------
static void HandlePress(u8 instance, u8 localIdx)
{
	// Jeder Tastendruck dieser Memory beendet ihr Warnblinken - ausser dem Aufruf einer blinkenden Fahrstrasse,
	// der sie trotz Kreuzung stellt (zweiter Druck, Handbuch 3.3.3)
	if (s_BlinkInstance == instance)
	{
		u8 blinkRoute = (u8)(instance * MEM_ROUTES_PER_INSTANCE + localIdx - 5);
		s_BlinkInstance = 0xFF;
		if ((localIdx >= 5) && BitGet(s_Blink, blinkRoute))
		{
			(void)PlayRoute(blinkRoute, PLAY_FORCE);
			return;
		}
	}
	if (localIdx == 2) // "in" (input) - Instanz wartet auf Routenwahl
	{
		s_RecordingLocalRoute[instance] = REC_WAITING;
		s_PendingWholeDelete[instance] = FALSE;
		s_PendingRelease[instance] = FALSE;
		return;
	}
	if (localIdx == 3) // "end"
	{
		s_PendingRelease[instance] = FALSE;
		FinishRecording(instance); // no-op, wenn weder WAITING noch aktiv aufnehmend
		return;
	}
	if (localIdx == 4) // "clear"
	{
		if (s_RecordingLocalRoute[instance] == REC_WAITING)
			s_PendingWholeDelete[instance] = TRUE;
		else if (s_RecordingLocalRoute[instance] < MEM_ROUTES_PER_INSTANCE)
			RemoveLastCommand(instance);
		else
			s_PendingRelease[instance] = TRUE; // Freigeben (3.3.4)
		return;
	}
	if (localIdx == 0) // "extern" - Automatikbetrieb: Rueckmeldekontakte werden ausgewertet (Handbuch 4)
	{
		s_Auto[instance] = TRUE;
		return;
	}
	if (localIdx == 1) // "off" - Handbetrieb, gespeicherte Anforderungen verfallen
	{
		u8 r, first = (u8)(instance * MEM_ROUTES_PER_INSTANCE);
		s_Auto[instance] = FALSE;
		for (r = first; r < first + MEM_ROUTES_PER_INSTANCE; r++) BitSet(s_Pending, r, FALSE);
		return;
	}

	// A1-C8 (localIdx 5..28)
	{
		u8 localRoute  = (u8)(localIdx - 5);
		u8 globalRoute = (u8)(instance * MEM_ROUTES_PER_INSTANCE + localRoute);
		u8 recState    = s_RecordingLocalRoute[instance];

		if (recState == REC_WAITING)
		{
			if (s_PendingWholeDelete[instance])
			{
				DeleteWholeRoute(globalRoute);
				s_RecordingLocalRoute[instance] = REC_IDLE;
				s_PendingWholeDelete[instance] = FALSE;
			}
			else
			{
				s_RecordingLocalRoute[instance] = localRoute; // Aufnahme startet, erster Keyboard-
			}                                                  // Tastendruck legt den Satz ggf. erst an
			return;
		}
		if (recState < MEM_ROUTES_PER_INSTANCE) return; // schon mitten in einer Aufnahme - "end" zuerst

		if (s_PendingRelease[instance])
		{
			// Freigeben: LED aus, Inhalt bleibt; die Uebersicht darf zu viel melden
			s_PendingRelease[instance] = FALSE;
			BitSet(s_Active, globalRoute, FALSE);
			TryPending();
			return;
		}
		if (McMem_IsValid(globalRoute)) (void)PlayRoute(globalRoute, PLAY_MANUAL);
	}
}

//---------------------------------------------------------------------------------------------------
void McMem_TapKeyboardCommand(u16 addr, bool richtung)
{
	u8 instance;
	for (instance = 0; instance < MEMORY_COUNT_MAX; instance++)
	{
		u8 localRoute = s_RecordingLocalRoute[instance];
		u8 globalRoute, count, i;
		bool found;

		if (localRoute >= MEM_ROUTES_PER_INSTANCE) continue;
		globalRoute = (u8)(instance * MEM_ROUTES_PER_INSTANCE + localRoute);

		if (!BeginSession()) continue;
		if (!ReadRoute(globalRoute)) ClearBuf();
		count = s_MemBuf[O_COUNT];
		found = FALSE;
		for (i = 0; i < count; i++)
		{
			u16 existing = (u16)(((u16)s_MemBuf[O_CMDS + i * 3] << 8) | s_MemBuf[O_CMDS + i * 3 + 1]);
			if (existing == addr)
			{
				s_MemBuf[O_CMDS + i * 3 + 2] = richtung ? 1 : 0;
				found = TRUE;
				break;
			}
		}
		if (!found && (count < MEM_CMDS_PER_ROUTE))
		{
			s_MemBuf[O_CMDS + count * 3]     = (u8)(addr >> 8);
			s_MemBuf[O_CMDS + count * 3 + 1] = (u8)(addr & 0xff);
			s_MemBuf[O_CMDS + count * 3 + 2] = richtung ? 1 : 0;
			s_MemBuf[O_COUNT] = (u8)(count + 1);
		}
		WriteRoute(globalRoute);
		EndSession();
	}
}

//---------------------------------------------------------------------------------------------------
static bool NoteSwitch(u16 addr, bool richtung)
{
	u8 r;
	bool changed = FALSE;
	// Kommt der Magnetartikel in einer gueltigen Fahrstrasse in der anderen Richtung vor?
	if (!BitGet(richtung ? s_UsedDir0 : s_UsedDir1, (u8)(addr & 0xFF))) return FALSE;
	s_Cand[0] = (u8)(addr >> 8);
	s_Cand[1] = (u8)(addr & 0xFF);
	s_Cand[2] = richtung ? 1 : 0;
	s_CandCount = 1;
	if (!ScanActive(0xFF)) return FALSE;
	for (r = 0; r < MEM_TOTAL_ROUTES; r++)
	{
		if (BitGet(s_Cross, r))
		{
			BitSet(s_Active, r, FALSE);
			changed = TRUE;
		}
	}
	if (changed) TryPending();
	return changed;
}

//---------------------------------------------------------------------------------------------------
u8 McMem_Tick(void)
{
	u8 instance = s_BlinkInstance;
	if (instance == 0xFF) return 0xFF;
	if ((i16)(g_JIFFY - s_BlinkUntil) >= 0)
	{
		// Kein zweiter Druck: die bisherige Fahrstrasse bleibt gueltig
		s_BlinkInstance = 0xFF;
		return instance;
	}
	if ((i16)(g_JIFFY - s_BlinkNext) >= 0)
	{
		s_BlinkOn = !s_BlinkOn;
		s_BlinkNext = (u16)(g_JIFFY + BLINK_PERIOD);
		return instance;
	}
	return 0xFF;
}

//---------------------------------------------------------------------------------------------------
static bool S88Event(u16 pin, bool active)
{
	u16 bus  = pin / 1000;
	u16 rest = pin % 1000;
	u8  module, contact, idx, instance, route;

	// Umkehrung der Pinnummer von mbc88/mbc90: 1000 x Bus + 16 x (Modul im Bus - 1) + Kontakt 1-16, 31 Module je Bus
	if ((bus < 1) || (bus > 3) || (rest == 0) || (rest > 31 * 16)) return FALSE;
	rest--;
	module  = (u8)(rest / 16 + 1 + 31 * (bus - 1));
	contact = (u8)(rest % 16);
	// Memory, zu deren drei Modulen (Reihe A, B, C) die Moduladresse gehoert
	for (instance = 0; instance < g_MemoryCount; instance++)
		if ((g_S88Module[instance] != 0) && (module >= g_S88Module[instance]) && (module < g_S88Module[instance] + 3)) break;
	if (instance >= g_MemoryCount) return FALSE;
	idx   = (u8)(module - g_S88Module[instance]);
	route = (u8)(instance * MEM_ROUTES_PER_INSTANCE + idx * 8 + (contact & 7));

	if (contact >= 8)
	{
		// Freigabe-Eingang: solange belegt, sperrt er die Anforderung; das Belegen gibt die gueltige Fahrstrasse
		// frei, damit eine kreuzende gestellt werden kann - nur mit Verriegelung (Handbuch 4.4)
		BitSet(s_ReleaseHeld, route, active);
		if (!active || !s_Auto[instance] || !g_MemInterlock || !BitGet(s_Active, route)) return FALSE;
		BitSet(s_Active, route, FALSE);
		TryPending();
		return TRUE;
	}

	// Stell-Eingang: das Belegen fordert die Fahrstrasse an
	if (!active || !s_Auto[instance] || !McMem_IsValid(route)) return FALSE;
	if (g_MemInterlock && BitGet(s_ReleaseHeld, route)) return FALSE; // Anforderung sperren, nicht speichern
	if (!PlayRoute(route, PLAY_AUTO) && g_MemInterlock) BitSet(s_Pending, route, TRUE); // Anforderung speichern
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
void McMem_HandlePress(u8 instance, u8 localIdx)
{
	HandlePress(instance, localIdx);
	SaveState();
}

//---------------------------------------------------------------------------------------------------
bool McMem_NoteSwitch(u16 addr, bool richtung)
{
	bool changed = NoteSwitch(addr, richtung);
	if (changed) SaveState();
	return changed;
}

//---------------------------------------------------------------------------------------------------
bool McMem_S88Event(u16 pin, bool active)
{
	bool changed = S88Event(pin, active);
	if (changed) SaveState();
	return changed;
}
