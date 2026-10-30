//***************************************************************************************************
//* SOURCE - mbc89_memory.c - Memory 6043: route recording/playback (disk-based)                    *
//***************************************************************************************************
//* File:  mbc89_memory.c                                                                             *
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
// MBC89R.CFG, MEM_TOTAL_ROUTES fixed 128-byte records (random access via DOS_RandomBlockReadFCB/
// WriteFCB, one record per route, globalRoute = instance*MEM_ROUTES_PER_INSTANCE + localRoute):
//   0      command count (0 = empty/invalid, max. MEM_CMDS_PER_ROUTE)
//   1      follow-on route: global route index 0..(MEM_TOTAL_ROUTES-1), 0xFF = none
//   2..61  up to 20 commands x 3 bytes (address high, address low, direction)
//   62..127 reserved/zero
// Record MEM_TOTAL_ROUTES (after the routes) holds the state for the next start:
//   0..11  valid routes (1 bit per route)
//   12     automatic mode per Memory (bits 0-3)
//   13     marker STATE_MAGIC (missing in files without a state record)

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

// Sentinel values for s_RecordingLocalRoute[] - 0..(MEM_ROUTES_PER_INSTANCE-1) = active recording
// of this local route, otherwise one of the two sentinels below.
#define REC_IDLE    0xFF // not in recording mode
#define REC_WAITING 0xFE // "input" pressed, waiting for route selection

// MSX-DOS 1 disk I/O must not transfer through page 1 (see mbc89_store.c) - fixed page-3
// buffer, directly after mbc89_store.c's STORE_BUF (ends at 0xC71F).
#define MEM_FCB 0xC720 // DOS_FCB struct
#define MEM_BUF 0xC756 // 128-Byte Transferpuffer

// Own names (not s_File/s_Buf like mbc89_store.c) - SDCC apparently emits __at() variables as
// global symbols, "static" does not protect against name collisions across compile units at a
// fixed address (linker error "Multiple definition of _s_Buf/_s_File" with the same name).
__at(MEM_FCB) DOS_FCB s_MemFile;
__at(MEM_BUF) u8      s_MemBuf[MEM_RECORD_SIZE];

// Valid/active bit fields, 1 bit per route (MEM_TOTAL_ROUTES=96 bits = 12 bytes).
static u8 s_Valid[(MEM_TOTAL_ROUTES + 7) / 8];
static u8 s_Active[(MEM_TOTAL_ROUTES + 7) / 8];

// Per instance: REC_IDLE/REC_WAITING or the local route (0..23) currently being recorded.
static u8   s_RecordingLocalRoute[MEMORY_COUNT_MAX];
// Only valid during REC_WAITING: the next route press clears the WHOLE route instead of starting a
// recording (spec: "input"+"clear"+route selection).
static bool s_PendingWholeDelete[MEMORY_COUNT_MAX];
// Only outside a recording: "clear" pressed, the next route press releases the route
// (manual 3.3.4), "end" cancels.
static bool s_PendingRelease[MEMORY_COUNT_MAX];

// Overview for McMem_NoteSwitch(): per address low byte one bit per direction, whether the address
// occurs in a valid route. May report too much (after releases), never too little.
static u8 s_UsedDir0[32];
static u8 s_UsedDir1[32];

// Comparison commands (format as in the record: AdrH, AdrL, direction): the route to be set, or
// the one switched solenoid accessory - s_MemBuf is overwritten while reading the valid routes.
static u8 s_CandCount;
static u8 s_CandFollow;
static u8 s_Cand[MEM_CMDS_PER_ROUTE * 3];
// Valid routes that cross s_Cand (result of ScanActive()).
static u8 s_Cross[(MEM_TOTAL_ROUTES + 7) / 8];

// Warning flash of crossing routes (manual 3.3.3): instance (0xFF = none), routes, end, clock.
#define BLINK_JIFFIES  250 // ~5 s
#define BLINK_PERIOD   15  // ~0.3 s per phase
static u8   s_BlinkInstance;
static u8   s_Blink[(MEM_TOTAL_ROUTES + 7) / 8];
static u16  s_BlinkUntil;
static u16  s_BlinkNext;
static bool s_BlinkOn;

// Automatic mode (manual 4): on/off per instance; stored requests and occupied release inputs
// per route.
static bool s_Auto[MEMORY_COUNT_MAX];
static u8   s_Pending[(MEM_TOTAL_ROUTES + 7) / 8];
static u8   s_ReleaseHeld[(MEM_TOTAL_ROUTES + 7) / 8];

// PlayRoute(): manual mode (flashes on crossing), second press on a flashing route, automatic
// (does not flash, only reports whether the route was set).
#define PLAY_MANUAL 0
#define PLAY_FORCE  1
#define PLAY_AUTO   2

// Last saved state - written only on a real change.
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
// Opens MBC89R.CFG for a sequence of random accesses. The caller closes it with EndSession().
// Returns FALSE if the file does not exist (the caller decides whether that is an error).
static bool BeginSession(void)
{
	OpenFile();
	DisableInterrupt();
	if (DOS_OpenFCB(&s_MemFile) != DOS_ERR_NONE)
	{
		EnableInterrupt();
		return FALSE;
	}
	// OPEN leaves the record size field untouched - Random Block Read/Write (27h/26h) need it.
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
// Reads one record into s_MemBuf. TRUE only on a real hit (1 record read) - FALSE among others
// when the file does not reach that far yet (fresh/partially filled file); s_MemBuf then stays
// unchanged and the caller must fall back to "empty" itself.
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
// Empty record (command count 0, no follow-on route) - for creating/completely clearing.
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
// TRUE if the record in s_MemBuf switches a solenoid accessory from s_Cand in the other direction -
// only that counts as crossing (manual 3.3.1).
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
// Reads all valid routes except skip: those crossing s_Cand go to s_Cross, all of them form the
// new overview. FALSE if MBC89R.CFG could not be opened.
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
// Warning flash: the crossing valid routes of the instance and the called one.
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
// Writes valid routes and operating mode to the state record, but only if they have changed since
// the last write. Call at the end of every public function that can change them.
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
		// State of the last run: only routes that still exist; automatic only for existing Memorys
		if (ReadRoute(STATE_RECORD) && (s_MemBuf[O_ST_MAGIC] == STATE_MAGIC))
		{
			for (i = 0; i < sizeof(s_Active); i++) s_Active[i] = (u8)(s_MemBuf[O_ST_ACTIVE + i] & s_Valid[i]);
			for (i = 0; i < g_MemoryCount; i++) s_Auto[i] = (bool)((s_MemBuf[O_ST_AUTO] >> i) & 1);
		}
		EndSession();
		// Overview for McMem_NoteSwitch() from the valid routes (empty comparison list: nothing crosses)
		(void)ScanActive(0xFF);
	}
	else
	{
		// File does not exist yet (first start) - create it, leave it empty. Every later WriteRoute()
		// materializes its own record on demand, no need to pre-fill all MEM_TOTAL_ROUTES
		// records.
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
	if (localIdx == 2) // "in" - lit while the instance is in recording mode (selection or recording)
		return (bool)(s_RecordingLocalRoute[instance] != REC_IDLE);
	if (localIdx == 4) // "clr" - waiting for the route to be released
		return s_PendingRelease[instance];
	if (localIdx == 0) // "ex" - automatic mode
		return s_Auto[instance];
	if (localIdx == 1) // "off" - Handbetrieb
		return (bool)!s_Auto[instance];
	if (localIdx < 5) // "end" - no steady indication of its own
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
// Plays globalRoute, follows a follow-on route if set (max. 5 chain links against endless loops).
// No artificial delay between the commands - corresponds to quickly pressing the same keys by
// hand, see plan 8a. For each chain link the record is read and the file closed again before
// sending: CAN sending never with interrupts disabled, the commands come straight from s_MemBuf
// (unchanged after EndSession()).
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
			// With interlocking only a crossing valid route of the same Memory blocks (3.3.3)
			bool blocked = FALSE;
			for (r = (u8)(instance * MEM_ROUTES_PER_INSTANCE); r < (u8)((instance + 1) * MEM_ROUTES_PER_INSTANCE); r++)
				if (BitGet(s_Cross, r)) blocked = TRUE;
			if (blocked)
			{
				if (BitGet(s_Active, cur)) SummaryAdd(s_Cand, s_CandCount);
				// Follow-on routes are then silently not executed, only the call itself flashes
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
		// The route being set destroys every valid route it crosses
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
// Sets stored automatic requests as soon as nothing stands against them anymore; fixed priority
// A1 before A2 ... before C8 (manual 4.4). Call after every release.
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
	// Every key press of this Memory ends its warning flash - except calling a flashing route, which
	// sets it despite the crossing (second press, manual 3.3.3)
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
	if (localIdx == 2) // "in" (input) - instance waits for route selection
	{
		s_RecordingLocalRoute[instance] = REC_WAITING;
		s_PendingWholeDelete[instance] = FALSE;
		s_PendingRelease[instance] = FALSE;
		return;
	}
	if (localIdx == 3) // "end"
	{
		s_PendingRelease[instance] = FALSE;
		FinishRecording(instance); // no-op if neither WAITING nor actively recording
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
	if (localIdx == 0) // "extern" - automatic mode: feedback contacts are evaluated (manual 4)
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
				s_RecordingLocalRoute[instance] = localRoute; // recording starts, the first Keyboard
			}                                                  // key press creates the record if needed
			return;
		}
		if (recState < MEM_ROUTES_PER_INSTANCE) return; // already in the middle of a recording - "end" first

		if (s_PendingRelease[instance])
		{
			// Release: LED off, content stays; the overview may report too much
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
	// Does the solenoid accessory occur in a valid route in the other direction?
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
		// No second press: the previous route stays valid
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

	// Inverse of the pin number of mbc88/mbc90: 1000 x bus + 16 x (module in bus - 1) + contact 1-16, 31 modules per bus
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
		// Release input: while occupied, it blocks the request; occupying it releases the valid route
		// so that a crossing one can be set - only with interlocking (manual 4.4)
		BitSet(s_ReleaseHeld, route, active);
		if (!active || !s_Auto[instance] || !g_MemInterlock || !BitGet(s_Active, route)) return FALSE;
		BitSet(s_Active, route, FALSE);
		TryPending();
		return TRUE;
	}

	// Set input: occupying it requests the route
	if (!active || !s_Auto[instance] || !McMem_IsValid(route)) return FALSE;
	if (g_MemInterlock && BitGet(s_ReleaseHeld, route)) return FALSE; // block the request, do not store it
	if (!PlayRoute(route, PLAY_AUTO) && g_MemInterlock) BitSet(s_Pending, route, TRUE); // store the request
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
