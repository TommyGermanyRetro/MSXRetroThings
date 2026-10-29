//***************************************************************************************************
//* SOURCE - mbc89_memory.c - Memory 6043: Fahrstraßen-Aufnahme/-Wiedergabe (diskettenbasiert)        *
//***************************************************************************************************
//* Datei: mbc89_memory.c                                                                             *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
#include "mbc89_memory.h"
#include "mbc89_com.h"
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

#define MEM_RECORD_SIZE  128
#define MEM_TOTAL_ROUTES (MEMORY_COUNT_MAX * MEM_ROUTES_PER_INSTANCE)

#define O_COUNT  0
#define O_FOLLOW 1
#define O_CMDS   2

#define FOLLOW_NONE 0xFF

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
// Siehe mbc89_store.c's DiskReset() - gleiche Begruendung (Warmstart-Stale-Buffer-Risiko).
static void DiskReset(void)
{
__asm
	push	ix
	ld		c, #DOS_FUNC_DSKRST
	call	BDOS
	pop		ix
__endasm;
}

//---------------------------------------------------------------------------------------------------
// Oeffnet MBC89R.CFG fuer eine Folge von Random-Zugriffen. Der Aufrufer schliesst mit EndSession().
// Gibt FALSE zurueck, wenn die Datei nicht existiert (Aufrufer entscheidet, ob das ein Fehler ist).
static bool BeginSession(void)
{
	DiskReset();
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

//===================================================================================================
// PUBLIC FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
void McMem_Init(void)
{
	u8 i;

	for (i = 0; i < sizeof(s_Valid); i++) { s_Valid[i] = 0; s_Active[i] = 0; }
	for (i = 0; i < MEMORY_COUNT_MAX; i++) { s_RecordingLocalRoute[i] = REC_IDLE; s_PendingWholeDelete[i] = FALSE; }

	if (BeginSession())
	{
		u8 r;
		for (r = 0; r < MEM_TOTAL_ROUTES; r++)
			if (ReadRoute(r)) BitSet(s_Valid, r, (bool)(s_MemBuf[O_COUNT] > 0));
		EndSession();
	}
	else
	{
		// Datei existiert noch nicht (Erststart) - anlegen, leer lassen. Jeder spaetere WriteRoute()
		// materialisiert ihren eigenen Satz on demand, kein Vorab-Befuellen aller MEM_TOTAL_ROUTES
		// Saetze noetig.
		DiskReset();
		OpenFile();
		DisableInterrupt();
		DOS_CreateFCB(&s_MemFile);
		DOS_CloseFCB(&s_MemFile);
		EnableInterrupt();
	}
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
	if (localIdx < 5) // ex/off/end/clr - in 8a ohne eigene Dauer-Anzeige
		return FALSE;
	{
		u8 localRoute = (u8)(localIdx - 5);
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
static void PlayRoute(u8 globalRoute)
{
	u8 hop, cur = globalRoute;

	for (hop = 0; hop < 5; hop++)
	{
		u8 count, follow, i;
		bool ok;
		if (!BeginSession()) return;
		ok = ReadRoute(cur);
		EndSession();
		if (!ok) break;
		count  = s_MemBuf[O_COUNT];
		follow = s_MemBuf[O_FOLLOW];
		for (i = 0; i < count; i++)
		{
			u16 addr     = (u16)(((u16)s_MemBuf[O_CMDS + i * 3] << 8) | s_MemBuf[O_CMDS + i * 3 + 1]);
			u8  richtung = s_MemBuf[O_CMDS + i * 3 + 2];
			McCom_SendZubehoer(addr, richtung);
		}
		BitSet(s_Active, cur, TRUE);
		if (follow == FOLLOW_NONE) break;
		cur = follow;
	}
}

//---------------------------------------------------------------------------------------------------
void McMem_HandlePress(u8 instance, u8 localIdx)
{
	if (localIdx == 2) // "in" (input) - Instanz wartet auf Routenwahl
	{
		s_RecordingLocalRoute[instance] = REC_WAITING;
		s_PendingWholeDelete[instance] = FALSE;
		return;
	}
	if (localIdx == 3) // "end"
	{
		FinishRecording(instance); // no-op, wenn weder WAITING noch aktiv aufnehmend
		return;
	}
	if (localIdx == 4) // "clear"
	{
		if (s_RecordingLocalRoute[instance] == REC_WAITING)
			s_PendingWholeDelete[instance] = TRUE;
		else if (s_RecordingLocalRoute[instance] < MEM_ROUTES_PER_INSTANCE)
			RemoveLastCommand(instance);
		return;
	}
	if (localIdx < 5) // ex/off - Automatikbetrieb, erst Increment 8c
		return;

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

		if (McMem_IsValid(globalRoute)) PlayRoute(globalRoute);
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
