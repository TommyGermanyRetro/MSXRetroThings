//***************************************************************************************************
//* HEADER - BridgeTeensy.ino - USB-Slot-Bridge (Teensy 4.1)                                        *
//***************************************************************************************************
//* Erzeugt den MSX-Takt CLOCK (315/88 MHz = 3,579545 MHz) auf Pin 33 aus Audio-PLL4 (SAI2_MCLK)    *
//* und fuehrt Z80-Buszyklen fuer Slot 1 und Slot 2 aus. Pin 33 ist im Taktmodus nicht lesbar;      *
//* CLOCK wird ueber eine Drahtbruecke von Pin 33 auf Pin 3 (CLOCK_IN) zurueckgefuehrt, jede        *
//* Busflanke wird auf eine Flanke von CLOCK_IN ausgerichtet.                                       *
//*                                                                                                 *
//* Zyklen: Speicher lesen (3 T), Opcode-Fetch M1 (4 T + 1 Wartezyklus + Refresh), Speicher         *
//* schreiben (3 T), I/O lesen und schreiben (4 T). Vor jedem Lesezyklus wird der Datenbus ueber    *
//* den 74HCT245 auf FFh vorgeladen und vor /RD freigegeben. /WAIT (Pin 4) verlaengert jeden        *
//* Zyklus, hoechstens um 100 ms. Mit Ueberwachung des Herzschlags gibt die Bridge zwischen den     *
//* Zyklen Leerlauf-Refresh aus und geht ohne Herzschlag nach AUS (wie ein stromloser MSX).         *
//*                                                                                                 *
//* Betrieb ueber das Binaerprotokoll Version 2 (Textkommando b, siehe ANALYSE.md Abschnitt 4.3):   *
//* HELLO, MRD, MWR, IORD, IOWR, IOWRW, MRDN, RESET, RUN, SYNC, CFG, TEST; jede Aenderung von /INT  *
//* meldet die Bridge mit C0h und Pegel.                                                            *
//*                                                                                                 *
//* Textkommandos fuer Tests (eine Zeile, Abschluss LF oder CR, Zahlen hexadezimal):                *
//*   r<slot> <adr>          Speicher lesen           m<slot> <adr>        Opcode-Fetch (M1)        *
//*   w<slot> <adr> <wert>   Speicher schreiben       i <port>             I/O lesen                *
//*   o <port> <wert>        I/O schreiben            <n>*<kommando>       Kommando n-mal           *
//*   u / d                  /RESET freigeben / aktiv c                    CLOCK_IN messen          *
//*   t <n>                  /WAIT n Takte bremsen (Test, 0 = aus)   Bruecke Pin 29 -> Pin 4        *
//*   z <ms>                 Zeitgrenze fuer den Herzschlag (0 = keine Ueberwachung)                *
//*   h / s                  Herzschlag (RUN) / Status       ohne Herzschlag: AUS                   *
//*   p [ms]                 /RESET-Impuls (Vorgabe 10 ms)   q <0|1>  TEST-Leitung LOW/HIGH         *
//*   b                      Binaerprotokoll einschalten (Antwort BIN), Rueckweg mit Byte 00        *
//*   ?                      Hilfe                    slot = 1 oder 2, port = 00..FF                *
//*                                                                                                 *
//* Pins (Teensy 4.1, Arduino-Nummern, siehe ANALYSE.md Abschnitt 6.3):                             *
//*   A0-A15  = 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27   D0-D7 = 10,12,11,13,8,7,36,37     *
//*   /MERQ 6, /RD 9, /WR 32, /IORQ 35, /M1 34, /RFSH 31, /SLTSL1 1, /SLTSL2 0                      *
//*   /CS1 24, /CS2 25, /CS12 28, /RESET 30, /OE_WR 2, /WAIT 4 (Eingang), /INT 5 (Eingang)          *
//*   TEST 29, CLOCK 33 (Ausgang), CLOCK_IN 3 (Eingang, Bruecke von Pin 33)                         *
//*                                                                                                 *
//* Ruhezustand: alle Strobes und Selects HIGH, /RESET LOW, Adressen LOW, Daten Eingang.            *
//***************************************************************************************************
//* Datei: BridgeTeensy.ino                                                                         *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
//***************************************************************************************************
//* VERSION: 23/10/26                                                                               *
//***************************************************************************************************

#include <Arduino.h>

//===================================================================================================
// Pins
//===================================================================================================

static const uint8_t PIN_A[16] = { 19, 18, 14, 15, 40, 41, 17, 16, 22, 23, 20, 21, 38, 39, 26, 27 };
static const uint8_t PIN_D[8]  = { 10, 12, 11, 13, 8, 7, 36, 37 };

static const uint8_t PIN_MERQ     = 6;
static const uint8_t PIN_RD       = 9;
static const uint8_t PIN_WR       = 32;
static const uint8_t PIN_IORQ     = 35;
static const uint8_t PIN_M1       = 34;
static const uint8_t PIN_RFSH     = 31;
static const uint8_t PIN_SLTSL1   = 1;
static const uint8_t PIN_SLTSL2   = 0;
static const uint8_t PIN_CS1      = 24;
static const uint8_t PIN_CS2      = 25;
static const uint8_t PIN_CS12     = 28;
static const uint8_t PIN_RESET    = 30;
static const uint8_t PIN_OE_WR    = 2;
static const uint8_t PIN_WAIT     = 4;
static const uint8_t PIN_INT      = 5;
static const uint8_t PIN_TEST     = 29;
static const uint8_t PIN_CLOCK    = 33;
static const uint8_t PIN_CLOCK_IN = 3;

//===================================================================================================
// Bits in den schnellen GPIO-Registern (Zuordnung Pin -> Port/Bit siehe ANALYSE.md 6.3)
//===================================================================================================

// GPIO6: A0-A15 = Bit 16-31, /SLTSL1 = Bit 2, /SLTSL2 = Bit 3, /CS1 = Bit 12, /CS2 = Bit 13

static const uint32_t G6_SLTSL1 = 1u << 2;
static const uint32_t G6_SLTSL2 = 1u << 3;
static const uint32_t G6_CS1    = 1u << 12;
static const uint32_t G6_CS2    = 1u << 13;

// GPIO7: D0-D3 = Bit 0-3, D4-D7 = Bit 16-19, /MERQ = 10, /RD = 11, /WR = 12, /IORQ = 28, /M1 = 29

static const uint32_t G7_DATA = 0x000F000Fu;
static const uint32_t G7_MERQ = 1u << 10;
static const uint32_t G7_RD   = 1u << 11;
static const uint32_t G7_WR   = 1u << 12;
static const uint32_t G7_IORQ = 1u << 28;
static const uint32_t G7_M1   = 1u << 29;

// GPIO8: /CS12 = Bit 18, /RFSH = Bit 22, /RESET = Bit 23

static const uint32_t G8_CS12  = 1u << 18;
static const uint32_t G8_RFSH  = 1u << 22;
static const uint32_t G8_RESET = 1u << 23;

// GPIO9: /OE_WR = Bit 4, CLOCK_IN = Bit 5, /WAIT = Bit 6, /INT = Bit 8, TEST = Bit 31

static const uint32_t G9_OE_WR    = 1u << 4;
static const uint32_t G9_CLOCK_IN = 1u << 5;
static const uint32_t G9_WAIT     = 1u << 6;
static const uint32_t G9_INT      = 1u << 8;
static const uint32_t G9_TEST     = 1u << 31;

//===================================================================================================
// Einstellungen
//===================================================================================================

// PLL4 = 24 MHz * (38 + 2/11) = 916,3636 MHz, SAI2-Teiler 4 * 64 = 256 -> 3,579545 MHz

static const uint32_t PLL4_DIV   = 38;
static const uint32_t PLL4_NUM   = 2;
static const uint32_t PLL4_DENOM = 11;
static const uint32_t SAI2_PRED  = 4;                   // 1..8
static const uint32_t SAI2_PODF  = 64;                  // 1..64

static const uint32_t MEASURE_MS   = 50;                // Messfenster fuer CLOCK_IN
static const uint32_t EDGE_TIMEOUT = 3000;              // CPU-Takte bis "kein Takt" (5 us)
static const uint32_t MAX_WAIT_T   = 358000;            // hoechste Zahl eingefuegter Wartetakte (100 ms; ein Z80
                                                        // wartet beliebig lange, die Grenze faengt nur eine
                                                        // haengende Karte ab)
static const uint32_t MAX_TEST_WAITS = 64;              // hoechste Zahl Wartetakte im Testmodus (t <n>)
static const size_t   LINE_SIZE    = 48;
static const uint32_t RESET_MS     = 10;                // Vorgabe fuer die Laenge des /RESET-Impulses
static const uint8_t  PROTOCOL_VERSION = 2;             // Version des Binaerprotokolls
static const uint16_t MAX_BLOCK    = 1024;              // hoechste Zahl Lesezyklen je MRDN
static const uint32_t BINARY_SUPERVISE_MS = 200;        // Zeitgrenze fuer RUN nach dem Wechsel ins Binaerprotokoll
static const uint32_t REFRESH_DUE_US = 12;              // spaetestens so oft ein Refresh-Zyklus (128 Zeilen in 2 ms)

//===================================================================================================
// Zustand der Bus-Engine
//===================================================================================================

// Ergebnis eines Buszyklus (vor allen Funktionen definiert, wegen der Arduino-Prototypen)

struct CycleResult
{
    uint8_t  data;
    uint32_t waits;
};

static uint32_t addrBits    = 0;                        // Adresse, wie sie in GPIO6 Bit 16-31 steht
static uint8_t  refreshR    = 0;                        // Refresh-Zaehler R (7 Bit)
static uint8_t  refreshI    = 0;                        // Refresh-Register I
static bool     clockFailed = false;                    // CLOCK_IN kam nicht
static uint32_t testWaits   = 0;                        // Testmodus: /WAIT um so viele Takte verlaengern
static uint32_t superviseMs = 0;                        // Zeitgrenze fuer den Herzschlag, 0 = keine Ueberwachung
static uint32_t lastRun     = 0;                        // Zeit des letzten Herzschlags (millis)
static bool     busOn       = true;                     // LAEUFT (true) oder AUS (false)
static uint32_t refreshCount = 0;                       // Zahl der Leerlauf-Refresh-Zyklen
static bool     intLevel    = true;                     // zuletzt gemeldeter Pegel von /INT
static bool     binaryMode  = false;                    // Binaerprotokoll aktiv (sonst Textkommandos)
static uint32_t lastRefreshCycles = 0;                  // Zyklenzaehler beim letzten Refresh
static uint32_t postedErrors = 0;                       // verworfene Schreibzyklen ohne Antwort

//===================================================================================================
// Ruhezustand
//===================================================================================================

// Pin als Ausgang mit festem Pegel einrichten. Der Pegel geht vor der Richtungsumschaltung direkt in
// das Datenregister, denn digitalWrite() wirkt bei einem Eingang nur auf die Pull-Widerstaende:
// der Pin waere sonst bis zum ersten Zyklus LOW.

static void idleOutput(uint8_t pin, uint8_t level)
{
    if (level)
    {
        *portSetRegister(pin) = digitalPinToBitMask(pin);
    }
    else
    {
        *portClearRegister(pin) = digitalPinToBitMask(pin);
    }
    pinMode(pin, OUTPUT);
}

static void busIdle()
{
    idleOutput(PIN_RESET, LOW);                         // Cartridges im Reset

    const uint8_t highPins[] = { PIN_MERQ, PIN_RD, PIN_WR, PIN_IORQ, PIN_M1, PIN_RFSH, PIN_SLTSL1,
                                 PIN_SLTSL2, PIN_CS1, PIN_CS2, PIN_CS12, PIN_OE_WR };
    for (uint8_t pin : highPins)
    {
        idleOutput(pin, HIGH);                          // inaktiv
    }

    for (uint8_t pin : PIN_A)
    {
        idleOutput(pin, LOW);
    }
    addrBits = 0;                                       // setAddress() schaltet relativ zu diesem Stand

    for (uint8_t pin : PIN_D)
    {
        pinMode(pin, INPUT);
    }

    pinMode(PIN_CLOCK_IN, INPUT);
    pinMode(PIN_WAIT, INPUT_PULLUP);
    pinMode(PIN_INT, INPUT_PULLUP);
    idleOutput(PIN_TEST, HIGH);                         // Testleitung zu /WAIT, HIGH = keine Bremse
}

//===================================================================================================
// CLOCK
//===================================================================================================

// Audio-PLL4 einstellen, an SAI2 legen und als SAI2_MCLK auf Pin 33 ausgeben

static void clockStart()
{
    CCM_CCGR5 |= CCM_CCGR5_SAI2(CCM_CCGR_ON);

    CCM_ANALOG_PLL_AUDIO = CCM_ANALOG_PLL_AUDIO_BYPASS | CCM_ANALOG_PLL_AUDIO_ENABLE
                         | CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(2)
                         | CCM_ANALOG_PLL_AUDIO_DIV_SELECT(PLL4_DIV);
    CCM_ANALOG_PLL_AUDIO_NUM   = PLL4_NUM & CCM_ANALOG_PLL_AUDIO_NUM_MASK;
    CCM_ANALOG_PLL_AUDIO_DENOM = PLL4_DENOM & CCM_ANALOG_PLL_AUDIO_DENOM_MASK;
    CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_POWERDOWN;

    while (!(CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK))
    {
    }

    CCM_ANALOG_MISC2 &= ~(CCM_ANALOG_MISC2_DIV_MSB | CCM_ANALOG_MISC2_DIV_LSB);
    CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_BYPASS;

    CCM_CSCMR1 = (CCM_CSCMR1 & ~CCM_CSCMR1_SAI2_CLK_SEL_MASK) | CCM_CSCMR1_SAI2_CLK_SEL(2);
    CCM_CS2CDR = (CCM_CS2CDR & ~(CCM_CS2CDR_SAI2_CLK_PRED_MASK | CCM_CS2CDR_SAI2_CLK_PODF_MASK))
               | CCM_CS2CDR_SAI2_CLK_PRED(SAI2_PRED - 1)
               | CCM_CS2CDR_SAI2_CLK_PODF(SAI2_PODF - 1);

    IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~IOMUXC_GPR_GPR1_SAI2_MCLK3_SEL_MASK)
                    | IOMUXC_GPR_GPR1_SAI2_MCLK_DIR
                    | IOMUXC_GPR_GPR1_SAI2_MCLK3_SEL(0);

    CORE_PIN33_CONFIG = 2;                              // ALT2 = SAI2_MCLK
}

// CLOCK-Ausgang sperren (Pin 33 als GPIO auf LOW) und wieder als SAI2_MCLK freigeben. Der Takt selbst
// laeuft intern weiter; beim Wiedereinschalten kann der erste Impuls verkuerzt sein, /RESET ist dann LOW.

static void clockOutputOff()
{
    GPIO9_DR_CLEAR = 1u << 7;
    GPIO9_GDIR |= 1u << 7;
    CORE_PIN33_CONFIG = 5;                              // ALT5 = GPIO
}

static void clockOutputOn()
{
    CORE_PIN33_CONFIG = 2;                              // ALT2 = SAI2_MCLK
}

// Frequenz in mHz aus den Registern: 24 MHz * (DIV + NUM/DENOM) / (PRED * PODF), Nachteiler 1

static uint64_t clockMilliHz()
{
    uint64_t div   = CCM_ANALOG_PLL_AUDIO & 0x3F;
    uint64_t num   = CCM_ANALOG_PLL_AUDIO_NUM;
    uint64_t denom = CCM_ANALOG_PLL_AUDIO_DENOM;
    uint64_t pred  = ((CCM_CS2CDR >> 6) & 0x07) + 1;
    uint64_t podf  = (CCM_CS2CDR & 0x3F) + 1;

    return (24000000000ULL * (div * denom + num)) / (denom * pred * podf);
}

// CLOCK_IN abtasten und gegen den Zyklenzaehler der CPU messen. Die CPU-Zaehlung und die PLL4
// stammen aus demselben 24-MHz-Quarz.

static void clockMeasure(uint32_t &edges, uint32_t &high, uint32_t &total, uint32_t &cycles)
{
    const uint32_t window = (F_CPU_ACTUAL / 1000) * MEASURE_MS;
    uint32_t e = 0;
    uint32_t h = 0;
    uint32_t n = 0;
    uint32_t now;

    noInterrupts();
    uint32_t start = ARM_DWT_CYCCNT;
    bool prev = digitalReadFast(PIN_CLOCK_IN);

    do
    {
        bool cur = digitalReadFast(PIN_CLOCK_IN);
        if (cur && !prev)
        {
            e++;
        }
        if (cur)
        {
            h++;
        }
        n++;
        prev = cur;
        now = ARM_DWT_CYCCNT;
    } while ((now - start) < window);

    interrupts();

    edges  = e;
    high   = h;
    total  = n;
    cycles = now - start;
}

//===================================================================================================
// Warten auf CLOCK_IN
//===================================================================================================

// Warten, bis CLOCK_IN den Pegel hat. Kommt er nicht, wird clockFailed gesetzt.

static inline void waitLevel(bool level)
{
    uint32_t start = ARM_DWT_CYCCNT;

    while (((GPIO9_PSR & G9_CLOCK_IN) != 0) != level)
    {
        if ((ARM_DWT_CYCCNT - start) > EDGE_TIMEOUT)
        {
            clockFailed = true;
            return;
        }
    }
}

static inline void waitRising()
{
    waitLevel(false);
    waitLevel(true);
}

static inline void waitFalling()
{
    waitLevel(true);
    waitLevel(false);
}

//===================================================================================================
// Signale setzen
//===================================================================================================

// Adresse auf den Bus legen; nur die geaenderten Bits werden umgeschaltet

static inline void setAddress(uint16_t addr)
{
    uint32_t bits = (uint32_t)addr << 16;

    GPIO6_DR_TOGGLE = bits ^ addrBits;
    addrBits = bits;
}

// Testmodus: TEST (ueber eine Drahtbruecke mit /WAIT verbunden) zieht /WAIT wie eine langsame
// Cartridge auf LOW, sobald der Zyklus ihre Selects sieht; testWaits = 0 schaltet ihn ab

static inline void testAssert()
{
    if (testWaits)
    {
        GPIO9_DR_CLEAR = G9_TEST;
    }
}

// Wartetakte einfuegen, solange /WAIT LOW ist. Aufruf direkt nach einer fallenden Taktflanke,
// an der /WAIT abgetastet wird; liefert die Zahl der eingefuegten Takte. Im Testmodus wird /WAIT
// nach der letzten gewuenschten Abtastung freigegeben.

static inline uint32_t insertWaits()
{
    uint32_t count = 0;

    while (!(GPIO9_PSR & G9_WAIT) && count < MAX_WAIT_T && !clockFailed)
    {
        count++;
        if (testWaits && count >= testWaits)
        {
            GPIO9_DR_SET = G9_TEST;
        }
        waitFalling();
    }

    GPIO9_DR_SET = G9_TEST;

    return count;
}

// Select-Signale fuer Speicherzugriffe; /CSx nur beim Lesen

static inline void selectSlot(uint8_t slot, uint16_t addr, bool read)
{
    uint32_t g6 = (slot == 1) ? G6_SLTSL1 : G6_SLTSL2;

    if (read)
    {
        if (addr >= 0x4000 && addr < 0x8000)
        {
            g6 |= G6_CS1;
            GPIO8_DR_CLEAR = G8_CS12;
        }
        else if (addr >= 0x8000 && addr < 0xC000)
        {
            g6 |= G6_CS2;
            GPIO8_DR_CLEAR = G8_CS12;
        }
    }

    GPIO6_DR_CLEAR = g6;
}

static inline void deselectAll()
{
    GPIO6_DR_SET = G6_SLTSL1 | G6_SLTSL2 | G6_CS1 | G6_CS2;
    GPIO8_DR_SET = G8_CS12;
}

// Datenbus vor einem Lesezyklus auf FFh vorladen: D0-D7 ueber den 74HCT245 treiben und vor /RD wieder
// freigeben. Treibt danach keine Karte, haelt der Bus FFh wie der Bus eines MSX; sonst liefe der zuletzt
// gelesene oder geschriebene Wert zurueck (BIOS-RAM-Suche haelt leere Bereiche dann fuer RAM).

static inline void prechargeStart()
{
    GPIO7_DR_SET = G7_DATA;
    GPIO7_GDIR |= G7_DATA;
    GPIO9_DR_CLEAR = G9_OE_WR;
}

static inline void prechargeEnd()
{
    GPIO9_DR_SET = G9_OE_WR;
    GPIO7_GDIR &= ~G7_DATA;
    delayNanoseconds(30);                               // 74HCT245 abgeschaltet, bevor /RD den 74LVC245 oeffnet
}

//===================================================================================================
// Buszyklen
//===================================================================================================

// Speicher lesen, 3 Takte: T1 steigend Adresse, T1 fallend /MERQ /RD /SLTSL /CSx, T2 fallend /WAIT,
// T3 fallend Daten lesen und alles wieder inaktiv

static CycleResult memRead(uint8_t slot, uint16_t addr)
{
    CycleResult r = { 0xFF, 0 };

    waitRising();                                       // T1
    setAddress(addr);
    prechargeStart();
    waitFalling();
    prechargeEnd();
    GPIO7_DR_CLEAR = G7_MERQ | G7_RD;
    selectSlot(slot, addr, true);
    testAssert();

    waitFalling();                                      // T2 fallend: /WAIT abtasten
    r.waits = insertWaits();

    waitFalling();                                      // T3 fallend
    uint32_t psr = GPIO7_PSR;
    r.data = (uint8_t)((psr & 0x0F) | ((psr >> 12) & 0xF0));

    GPIO7_DR_SET = G7_MERQ | G7_RD;
    deselectAll();

    return r;
}

// Opcode-Fetch, 4 Takte + 1 Wartetakt des MSX: /M1 von T1 bis T3 steigend, Daten bei T3 steigend,
// danach Refresh mit /RFSH LOW und /MERQ von T3 fallend bis T4 fallend

static CycleResult opcodeFetch(uint8_t slot, uint16_t addr)
{
    CycleResult r = { 0xFF, 0 };

    waitRising();                                       // T1
    setAddress(addr);
    GPIO7_DR_CLEAR = G7_M1;
    prechargeStart();
    waitFalling();
    prechargeEnd();
    GPIO7_DR_CLEAR = G7_MERQ | G7_RD;
    selectSlot(slot, addr, true);
    testAssert();

    waitFalling();                                      // T2 fallend
    waitFalling();                                      // Wartetakt des MSX, /WAIT abtasten
    r.waits = insertWaits();

    waitRising();                                       // T3 steigend: Daten lesen
    uint32_t psr = GPIO7_PSR;
    r.data = (uint8_t)((psr & 0x0F) | ((psr >> 12) & 0xF0));

    GPIO7_DR_SET = G7_MERQ | G7_RD | G7_M1;
    deselectAll();
    GPIO8_DR_CLEAR = G8_RFSH;
    setAddress((uint16_t)((refreshI << 8) | (refreshR & 0x7F)));
    refreshR = (uint8_t)((refreshR + 1) & 0x7F);

    waitFalling();                                      // T3 fallend: Refresh-/MERQ
    GPIO7_DR_CLEAR = G7_MERQ;

    waitFalling();                                      // T4 fallend
    GPIO7_DR_SET = G7_MERQ;

    waitRising();                                       // Ende T4
    GPIO8_DR_SET = G8_RFSH;
    lastRefreshCycles = ARM_DWT_CYCCNT;

    return r;
}

// Speicher schreiben, 3 Takte: Daten ab T1 steigend, /WR von T2 fallend bis T3 fallend, Daten bis
// zum Ende von T3 gehalten

static CycleResult memWrite(uint8_t slot, uint16_t addr, uint8_t value)
{
    CycleResult r = { value, 0 };
    uint32_t bits = (uint32_t)(value & 0x0F) | ((uint32_t)(value & 0xF0) << 12);

    waitRising();                                       // T1
    setAddress(addr);
    GPIO7_DR_SET = bits;
    GPIO7_DR_CLEAR = ~bits & G7_DATA;
    GPIO7_GDIR |= G7_DATA;
    GPIO9_DR_CLEAR = G9_OE_WR;
    waitFalling();
    GPIO7_DR_CLEAR = G7_MERQ;
    selectSlot(slot, addr, false);
    testAssert();

    waitFalling();                                      // T2 fallend
    GPIO7_DR_CLEAR = G7_WR;
    r.waits = insertWaits();

    waitFalling();                                      // T3 fallend
    GPIO7_DR_SET = G7_WR | G7_MERQ;
    deselectAll();

    waitRising();                                       // Ende T3
    GPIO9_DR_SET = G9_OE_WR;
    GPIO7_GDIR &= ~G7_DATA;

    return r;
}

// I/O lesen, 4 Takte (T1, T2, automatischer Wartetakt, T3): Port auf A0-A7, /IORQ und /RD von
// T2 steigend bis T3 fallend, Daten bei T3 fallend. Kein /MERQ, kein /SLTSL, kein /CSx.

static CycleResult ioRead(uint8_t port)
{
    CycleResult r = { 0xFF, 0 };

    waitRising();                                       // T1
    setAddress(port);
    prechargeStart();
    waitRising();                                       // T2 steigend
    prechargeEnd();
    GPIO7_DR_CLEAR = G7_IORQ | G7_RD;
    testAssert();

    waitFalling();                                      // T2 fallend
    waitFalling();                                      // automatischer Wartetakt, /WAIT abtasten
    r.waits = insertWaits();

    waitFalling();                                      // T3 fallend
    uint32_t psr = GPIO7_PSR;
    r.data = (uint8_t)((psr & 0x0F) | ((psr >> 12) & 0xF0));

    GPIO7_DR_SET = G7_IORQ | G7_RD;

    return r;
}

// I/O schreiben, 4 Takte: Daten ab T1 steigend, /IORQ und /WR von T2 steigend bis T3 fallend,
// Daten bis zum Ende von T3 gehalten

static CycleResult ioWrite(uint8_t port, uint8_t value)
{
    CycleResult r = { value, 0 };
    uint32_t bits = (uint32_t)(value & 0x0F) | ((uint32_t)(value & 0xF0) << 12);

    waitRising();                                       // T1
    setAddress(port);
    GPIO7_DR_SET = bits;
    GPIO7_DR_CLEAR = ~bits & G7_DATA;
    GPIO7_GDIR |= G7_DATA;
    GPIO9_DR_CLEAR = G9_OE_WR;

    waitRising();                                       // T2 steigend
    GPIO7_DR_CLEAR = G7_IORQ | G7_WR;
    testAssert();

    waitFalling();                                      // T2 fallend
    waitFalling();                                      // automatischer Wartetakt, /WAIT abtasten
    r.waits = insertWaits();

    waitFalling();                                      // T3 fallend
    GPIO7_DR_SET = G7_IORQ | G7_WR;

    waitRising();                                       // Ende T3
    GPIO9_DR_SET = G9_OE_WR;
    GPIO7_GDIR &= ~G7_DATA;

    return r;
}

// Leerlauf-Refresh, 4 Takte wie ein NOP des Z80 in HALT: T1 und T2 ohne Strobes, T3 steigend /RFSH
// LOW mit Refresh-Adresse, /MERQ von T3 fallend bis T4 fallend, Ende T4 /RFSH HIGH

static void idleRefresh()
{
    waitRising();                                       // T1
    waitRising();                                       // T2
    waitRising();                                       // T3 steigend
    GPIO8_DR_CLEAR = G8_RFSH;
    setAddress((uint16_t)((refreshI << 8) | (refreshR & 0x7F)));
    refreshR = (uint8_t)((refreshR + 1) & 0x7F);

    waitFalling();                                      // T3 fallend
    GPIO7_DR_CLEAR = G7_MERQ;

    waitFalling();                                      // T4 fallend
    GPIO7_DR_SET = G7_MERQ;

    waitRising();                                       // Ende T4
    GPIO8_DR_SET = G8_RFSH;
    lastRefreshCycles = ARM_DWT_CYCCNT;
}

// /INT beobachten und jede Aenderung melden: "INT 0" = Leitung LOW (aktiv), "INT 1" = frei. Im
// Zustand AUS wird nichts gemeldet.

static void checkInt()
{
    bool level = (GPIO9_PSR & G9_INT) != 0;

    if (level != intLevel)
    {
        intLevel = level;
        if (busOn && binaryMode)
        {
            const uint8_t frame[2] = { 0xC0, (uint8_t)(level ? 1 : 0) };
            Serial.write(frame, sizeof(frame));
            Serial.send_now();
        }
        else if (busOn)
        {
            Serial.println(level ? "INT 1" : "INT 0");
        }
    }
}

// Zustand AUS wie ein stromloser MSX: alle Leitungen inaktiv, /RESET LOW, CLOCK LOW, kein Refresh

static void busGoOff()
{
    noInterrupts();
    GPIO8_DR_CLEAR = G8_RESET;
    clockOutputOff();
    testWaits = 0;
    busIdle();
    busOn = false;
    interrupts();
}

static void busGoOn()
{
    noInterrupts();
    clockOutputOn();
    busOn = true;
    lastRun = millis();
    interrupts();
}

//===================================================================================================
// Kommandos
//===================================================================================================

static void printHelp()
{
    Serial.println("r<slot> <adr>  m<slot> <adr>  w<slot> <adr> <wert>  i <port>  o <port> <wert>");
    Serial.println("<n>*<kommando>  t <n>  z <ms>  h  s  p [ms]  q <0|1>  b  u  d  c  ?");
}

static void printClock()
{
    uint32_t edges, high, total, cycles;
    clockMeasure(edges, high, total, cycles);

    uint64_t setHz  = clockMilliHz();
    uint64_t measHz = ((uint64_t)edges * F_CPU_ACTUAL * 1000ULL) / cycles;
    uint32_t duty   = (uint32_t)(((uint64_t)high * 1000ULL) / total);

    Serial.printf("CLOCK Register %lu.%03lu Hz  gemessen %lu.%03lu Hz  Tastverhaeltnis %lu.%lu %%  "
                  "Flanken %lu\n",
                  (unsigned long)(setHz / 1000ULL), (unsigned long)(setHz % 1000ULL),
                  (unsigned long)(measHz / 1000ULL), (unsigned long)(measHz % 1000ULL),
                  (unsigned long)(duty / 10), (unsigned long)(duty % 10), (unsigned long)edges);
}

static void runCommand(char *line)
{
    unsigned long count = 1;
    char *star = strchr(line, '*');

    if (star)
    {
        count = strtoul(line, NULL, 10);
        line = star + 1;
    }

    while (*line == ' ')
    {
        line++;
    }

    char op = (char)tolower((unsigned char)*line);

    if (op == '?')
    {
        printHelp();
        return;
    }
    if (op == 'z')
    {
        unsigned ms = 0;

        if (sscanf(line + 1, "%u", &ms) != 1 || ms > 60000)
        {
            Serial.println("Fehler: z <0..60000> (Zeitgrenze in ms, 0 = keine Ueberwachung)");
            return;
        }
        superviseMs = ms;
        if (!busOn && ms == 0)
        {
            busGoOn();
        }
        lastRun = millis();
        if (ms == 0)
        {
            Serial.println("Ueberwachung aus: dauerhaft LAEUFT, kein Leerlauf-Refresh");
        }
        else
        {
            Serial.printf("Ueberwachung %u ms: Herzschlag h noetig, Leerlauf-Refresh aktiv\n", ms);
        }
        return;
    }
    if (op == 'b')
    {
        binaryMode = true;
        superviseMs = BINARY_SUPERVISE_MS;
        lastRun = millis();
        Serial.println("BIN");
        return;
    }
    if (op == 'h')
    {
        lastRun = millis();
        if (!busOn)
        {
            busGoOn();
            Serial.println("Herzschlag: LAEUFT (CLOCK an, /RESET bleibt LOW)");
        }
        else
        {
            Serial.println("Herzschlag: LAEUFT");
        }
        return;
    }
    if (op == 's')
    {
        Serial.printf("%s  Zeitgrenze %lu ms  letzter Herzschlag vor %lu ms  Refresh-Zyklen %lu\n",
                      busOn ? "LAEUFT" : "AUS", (unsigned long)superviseMs,
                      (unsigned long)(millis() - lastRun), (unsigned long)refreshCount);
        return;
    }
    if (!busOn)
    {
        Serial.println("AUS: erst Herzschlag h senden");
        return;
    }
    if (op == 'p')
    {
        unsigned ms = RESET_MS;

        if (sscanf(line + 1, "%u", &ms) == 1 && (ms < 1 || ms > 1000))
        {
            Serial.println("Fehler: p [1..1000] (Laenge des /RESET-Impulses in ms)");
            return;
        }
        GPIO8_DR_CLEAR = G8_RESET;
        delay(ms);
        GPIO8_DR_SET = G8_RESET;
        lastRun = millis();
        Serial.printf("/RESET-Impuls %u ms\n", ms);
        return;
    }
    if (op == 'q')
    {
        unsigned level = 2;

        if (sscanf(line + 1, "%u", &level) != 1 || level > 1)
        {
            Serial.println("Fehler: q <0|1> (Testleitung TEST LOW oder HIGH)");
            return;
        }
        if (level)
        {
            GPIO9_DR_SET = G9_TEST;
        }
        else
        {
            GPIO9_DR_CLEAR = G9_TEST;
        }
        Serial.printf("TEST %u\n", level);
        return;
    }
    if (op == 'u')
    {
        GPIO8_DR_SET = G8_RESET;
        Serial.println("/RESET inaktiv (HIGH)");
        return;
    }
    if (op == 'd')
    {
        GPIO8_DR_CLEAR = G8_RESET;
        Serial.println("/RESET aktiv (LOW)");
        return;
    }
    if (op == 'c')
    {
        printClock();
        return;
    }
    if (op == 't')
    {
        unsigned waits = 0;

        if (sscanf(line + 1, "%u", &waits) != 1 || waits > MAX_TEST_WAITS)
        {
            Serial.println("Fehler: t <0..64> (Anzahl Wartetakte, 0 = aus)");
            return;
        }
        testWaits = waits;
        GPIO9_DR_SET = G9_TEST;
        Serial.printf("Testmodus: %u Wartetakte je Zyklus (Bruecke TEST Pin 29 -> /WAIT Pin 4)\n", waits);
        return;
    }

    if (op == 'i' || op == 'o')
    {
        unsigned port = 0;
        unsigned data = 0;
        int cnt = sscanf(line + 1, "%x %x", &port, &data);

        if (port > 0xFF || data > 0xFF || (op == 'i' && cnt < 1) || (op == 'o' && cnt < 2))
        {
            Serial.println("Fehler: Kommando nicht verstanden (? = Hilfe)");
            return;
        }

        CycleResult io = { 0, 0 };
        clockFailed = false;

        noInterrupts();
        for (unsigned long i = 0; i < count && !clockFailed; i++)
        {
            io = (op == 'i') ? ioRead((uint8_t)port) : ioWrite((uint8_t)port, (uint8_t)data);
        }
        interrupts();

        if (clockFailed)
        {
            Serial.println("Fehler: kein CLOCK an Pin 3 (Bruecke von Pin 33?)");
            return;
        }

        Serial.printf("%c %02X -> %02X  Wartetakte %lu  (%lux)\n", op, port, io.data,
                      (unsigned long)io.waits, count);
        return;
    }

    unsigned slot = 0;
    unsigned addr = 0;
    unsigned value = 0;
    int n = sscanf(line + 1, "%u %x %x", &slot, &addr, &value);

    bool ok = (slot == 1 || slot == 2) && addr <= 0xFFFF && value <= 0xFF &&
              ((op == 'w' && n == 3) || ((op == 'r' || op == 'm') && n >= 2));
    if (!ok)
    {
        Serial.println("Fehler: Kommando nicht verstanden (? = Hilfe)");
        return;
    }

    CycleResult r = { 0, 0 };
    clockFailed = false;

    noInterrupts();
    for (unsigned long i = 0; i < count && !clockFailed; i++)
    {
        if (op == 'r')
        {
            r = memRead((uint8_t)slot, (uint16_t)addr);
        }
        else if (op == 'm')
        {
            r = opcodeFetch((uint8_t)slot, (uint16_t)addr);
        }
        else
        {
            r = memWrite((uint8_t)slot, (uint16_t)addr, (uint8_t)value);
        }
    }
    interrupts();

    if (clockFailed)
    {
        Serial.println("Fehler: kein CLOCK an Pin 3 (Bruecke von Pin 33?)");
        return;
    }

    Serial.printf("%c%u %04X -> %02X  Wartetakte %lu  (%lux)\n", op, slot, addr, r.data,
                  (unsigned long)r.waits, count);
}

//===================================================================================================
// Binaerprotokoll (siehe ANALYSE.md, Abschnitt 4.3)
//===================================================================================================

// PC -> Teensy, alle Zahlen Little Endian. Kennungen kleiner 0x20 ausser LF und CR, damit Textzeilen
// und Binaerrahmen unterscheidbar bleiben. Antwort = Kennung | 0x80, dann Status, dann Nutzdaten.

static const uint8_t OP_TEXT  = 0x00;                   // zurueck zu Textkommandos
static const uint8_t OP_HELLO = 0x01;
static const uint8_t OP_MRD   = 0x02;                   // slot, adr (2), flags (Bit 0 = M1)
static const uint8_t OP_MWR   = 0x03;                   // slot, adr (2), wert; keine Antwort
static const uint8_t OP_IORD  = 0x04;                   // port
static const uint8_t OP_IOWR  = 0x05;                   // port, wert; keine Antwort
static const uint8_t OP_MRDN  = 0x06;                   // slot, adr (2), anzahl (2)
static const uint8_t OP_RESET = 0x07;
static const uint8_t OP_RUN   = 0x08;                   // Herzschlag, keine Antwort
static const uint8_t OP_SYNC  = 0x09;
static const uint8_t OP_CFG   = 0x0B;                   // Zeitgrenze in ms (2)
static const uint8_t OP_TEST  = 0x0C;                   // Testleitung TEST: 0 = LOW, 1 = HIGH
static const uint8_t OP_IOWRW = 0x0D;                   // port, wert; Antwort mit Wartetakten (4)

static const uint8_t ST_OK    = 0;
static const uint8_t ST_OFF   = 1;                      // Zustand AUS
static const uint8_t ST_CLOCK = 2;                      // kein CLOCK_IN
static const uint8_t ST_ARG   = 3;                      // ungueltiger Parameter

static uint8_t blockBuf[MAX_BLOCK + 8];

// Laenge des Rahmens einschliesslich Kennung, 0 = unbekannte Kennung

static size_t frameLength(uint8_t op)
{
    switch (op)
    {
    case OP_TEXT:  return 1;
    case OP_HELLO: return 1;
    case OP_MRD:   return 5;
    case OP_MWR:   return 5;
    case OP_IORD:  return 2;
    case OP_IOWR:  return 3;
    case OP_MRDN:  return 6;
    case OP_RESET: return 1;
    case OP_RUN:   return 1;
    case OP_SYNC:  return 1;
    case OP_CFG:   return 3;
    case OP_TEST:  return 2;
    case OP_IOWRW: return 3;
    default:       return 0;
    }
}

static void sendReply(const uint8_t *data, size_t n)
{
    Serial.write(data, n);
    Serial.send_now();
}

// Leerlauf-Refresh einschieben, wenn der letzte zu lange her ist. Nur im Betrieb mit Ueberwachung;
// Aufruf mit gesperrten Interrupts.

static void refreshIfDue()
{
    const uint32_t due = (F_CPU_ACTUAL / 1000000) * REFRESH_DUE_US;

    if (superviseMs && (ARM_DWT_CYCCNT - lastRefreshCycles) > due)
    {
        idleRefresh();
        refreshCount++;
    }
}

static void replyRead(uint8_t op, uint8_t status, uint8_t data, uint32_t waits)
{
    uint8_t reply[4];

    reply[0] = op | 0x80;
    reply[1] = status;
    reply[2] = data;
    reply[3] = (uint8_t)(waits > 255 ? 255 : waits);
    sendReply(reply, 4);
}

// Antwort auf einen Schreibzyklus mit Antwort: Kennung, Status, Wartetakte (4 Byte, Little Endian)

static void replyWrite(uint8_t op, uint8_t status, uint32_t waits)
{
    uint8_t reply[6];

    reply[0] = op | 0x80;
    reply[1] = status;
    reply[2] = (uint8_t)(waits & 0xFF);
    reply[3] = (uint8_t)((waits >> 8) & 0xFF);
    reply[4] = (uint8_t)((waits >> 16) & 0xFF);
    reply[5] = (uint8_t)((waits >> 24) & 0xFF);
    sendReply(reply, 6);
}

static void handleFrame(const uint8_t *f)
{
    uint8_t reply[8];
    uint8_t op = f[0];
    uint8_t status = ST_OK;

    switch (op)
    {
    case OP_TEXT:
        binaryMode = false;
        return;

    case OP_HELLO:
        reply[0] = op | 0x80;
        reply[1] = ST_OK;
        reply[2] = PROTOCOL_VERSION;
        reply[3] = (uint8_t)(MAX_BLOCK & 0xFF);
        reply[4] = (uint8_t)(MAX_BLOCK >> 8);
        sendReply(reply, 5);
        return;

    case OP_RUN:
        lastRun = millis();
        if (!busOn)
        {
            busGoOn();
        }
        return;

    case OP_SYNC:
        reply[0] = op | 0x80;
        reply[1] = ST_OK;
        reply[2] = busOn ? 1 : 0;
        reply[3] = intLevel ? 1 : 0;
        reply[4] = (uint8_t)(postedErrors > 255 ? 255 : postedErrors);
        postedErrors = 0;
        sendReply(reply, 5);
        return;

    case OP_CFG:
    {
        uint16_t ms = (uint16_t)(f[1] | (f[2] << 8));

        status = (ms > 60000) ? ST_ARG : ST_OK;
        if (status == ST_OK)
        {
            superviseMs = ms;
            lastRun = millis();
            if (!busOn && ms == 0)
            {
                busGoOn();
            }
        }
        reply[0] = op | 0x80;
        reply[1] = status;
        sendReply(reply, 2);
        return;
    }

    case OP_TEST:
        if (f[1])
        {
            GPIO9_DR_SET = G9_TEST;
        }
        else
        {
            GPIO9_DR_CLEAR = G9_TEST;
        }
        reply[0] = op | 0x80;
        reply[1] = ST_OK;
        sendReply(reply, 2);
        return;

    case OP_RESET:
        reply[0] = op | 0x80;
        reply[1] = busOn ? ST_OK : ST_OFF;
        if (busOn)
        {
            GPIO8_DR_CLEAR = G8_RESET;
            delay(RESET_MS);
            GPIO8_DR_SET = G8_RESET;
            lastRun = millis();
        }
        sendReply(reply, 2);
        return;

    default:
        break;
    }

    // Buszyklen; ein Wechsel von /INT, den der Zyklus ausloest, wird noch vor seiner Antwort gemeldet
    uint8_t slot = f[1];
    uint16_t addr = (uint16_t)(f[2] | (f[3] << 8));
    bool memory = (op == OP_MRD || op == OP_MWR || op == OP_MRDN);

    if (!busOn)
    {
        status = ST_OFF;
    }
    else if (memory && slot != 1 && slot != 2)
    {
        status = ST_ARG;
    }

    if (status != ST_OK)
    {
        if (op == OP_MWR || op == OP_IOWR)
        {
            postedErrors++;
        }
        else if (op == OP_MRDN)
        {
            uint16_t n = (uint16_t)(f[4] | (f[5] << 8));

            if (n == 0 || n > MAX_BLOCK || status == ST_ARG)
            {
                reply[0] = op | 0x80;
                reply[1] = ST_ARG;
                sendReply(reply, 2);                    // Argumentfehler: nur der Kopf
                return;
            }
            blockBuf[0] = op | 0x80;
            blockBuf[1] = status;
            memset(blockBuf + 2, 0xFF, n);
            blockBuf[2 + n] = 0;
            blockBuf[3 + n] = 0;
            sendReply(blockBuf, 4 + n);
        }
        else if (op == OP_IOWRW)
        {
            replyWrite(op, status, 0);
        }
        else
        {
            replyRead(op, status, 0xFF, 0);
        }
        return;
    }

    clockFailed = false;

    if (op == OP_MRD)
    {
        CycleResult r;

        noInterrupts();
        refreshIfDue();
        r = (f[4] & 1) ? opcodeFetch(slot, addr) : memRead(slot, addr);
        interrupts();
        checkInt();
        replyRead(op, clockFailed ? ST_CLOCK : ST_OK, r.data, r.waits);
    }
    else if (op == OP_MWR)
    {
        noInterrupts();
        refreshIfDue();
        memWrite(slot, addr, f[4]);
        interrupts();
        checkInt();
        if (clockFailed)
        {
            postedErrors++;
        }
    }
    else if (op == OP_IORD)
    {
        CycleResult r;

        noInterrupts();
        refreshIfDue();
        r = ioRead(f[1]);
        interrupts();
        checkInt();
        replyRead(op, clockFailed ? ST_CLOCK : ST_OK, r.data, r.waits);
    }
    else if (op == OP_IOWR)
    {
        noInterrupts();
        refreshIfDue();
        ioWrite(f[1], f[2]);
        interrupts();
        checkInt();
        if (clockFailed)
        {
            postedErrors++;
        }
    }
    else if (op == OP_IOWRW)
    {
        CycleResult r;

        noInterrupts();
        refreshIfDue();
        r = ioWrite(f[1], f[2]);
        interrupts();
        checkInt();
        replyWrite(op, clockFailed ? ST_CLOCK : ST_OK, r.waits);
    }
    else if (op == OP_MRDN)
    {
        uint16_t n = (uint16_t)(f[4] | (f[5] << 8));
        uint32_t waits = 0;

        if (n == 0 || n > MAX_BLOCK)
        {
            reply[0] = op | 0x80;
            reply[1] = ST_ARG;
            sendReply(reply, 2);
            return;
        }
        for (uint16_t i = 0; i < n && !clockFailed; i++)
        {
            noInterrupts();
            refreshIfDue();
            CycleResult r = memRead(slot, (uint16_t)(addr + i));
            interrupts();
            blockBuf[2 + i] = r.data;
            waits += r.waits;
        }
        checkInt();
        blockBuf[0] = op | 0x80;
        blockBuf[1] = clockFailed ? ST_CLOCK : ST_OK;
        if (waits > 0xFFFF)
        {
            waits = 0xFFFF;
        }
        blockBuf[2 + n] = (uint8_t)(waits & 0xFF);
        blockBuf[3 + n] = (uint8_t)((waits >> 8) & 0xFF);
        sendReply(blockBuf, 4 + n);
    }
}

// Alle vollstaendig angekommenen Rahmen abarbeiten; ein unbekanntes Byte wird verworfen

static void binaryService()
{
    while (binaryMode && Serial.available())
    {
        uint8_t op = (uint8_t)Serial.peek();
        size_t need = frameLength(op);

        if (need == 0)
        {
            Serial.read();
            continue;
        }
        if ((size_t)Serial.available() < need)
        {
            return;
        }

        uint8_t frame[8];
        Serial.readBytes((char *)frame, need);
        handleFrame(frame);
    }
}

//===================================================================================================
// Setup / Hauptschleife
//===================================================================================================

void setup()
{
    busIdle();
    clockStart();
    Serial.begin(115200);
}

void loop()
{
    static char line[LINE_SIZE];
    static size_t length = 0;

    if (binaryMode)
    {
        binaryService();
    }

    while (!binaryMode && Serial.available())
    {
        char c = (char)Serial.read();

        if (c == '\n' || c == '\r')
        {
            if (length > 0)
            {
                line[length] = 0;
                runCommand(line);
                length = 0;
                if (binaryMode)
                {
                    break;
                }
            }
        }
        else if (length < LINE_SIZE - 1)
        {
            line[length++] = c;
        }
    }

    checkInt();

    if (superviseMs && busOn)
    {
        if ((millis() - lastRun) > superviseMs)
        {
            busGoOff();
            return;
        }

        clockFailed = false;
        noInterrupts();
        idleRefresh();
        interrupts();
        refreshCount++;
    }
}
