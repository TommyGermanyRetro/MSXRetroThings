//***************************************************************************************************
//* HEADER - BridgeTeensy.ino - USB slot bridge (Teensy 4.1)                                        *
//***************************************************************************************************
//* Generates the MSX clock CLOCK (315/88 MHz = 3.579545 MHz) on pin 33 from audio PLL4 (SAI2_MCLK) *
//* and runs Z80 bus cycles for slot 1 and slot 2. Pin 33 cannot be read in clock mode;             *
//* CLOCK is fed back from pin 33 to pin 3 (CLOCK_IN) via a wire jumper, and every                  *
//* bus edge is aligned to an edge of CLOCK_IN.                                                     *
//*                                                                                                 *
//* Cycles: memory read (3 T), opcode fetch M1 (4 T + 1 wait cycle + refresh), memory               *
//* write (3 T), I/O read and write (4 T). Before every read cycle the data bus is precharged to    *
//* FFh via the 74HCT245 and released before /RD. /WAIT (pin 4) extends every                       *
//* cycle, by at most 100 ms. With heartbeat monitoring the bridge issues idle refresh between      *
//* cycles and goes to OFF without a heartbeat (like an unpowered MSX).                             *
//*                                                                                                 *
//* Operation via binary protocol version 3 (text command b, see ANALYSE.md section 4.3):           *
//* HELLO, MRD, MWR, IORD, IOWR, IOWRW, MRDN, MRDNG, RESET, RUN, SYNC, CFG, TEST; every change      *
//* of /INT is reported by the bridge with C0h and the level. MRDNG reads consecutive addresses     *
//* like MRDN, with a fixed distance in clocks from cycle start to cycle start (SPI window).        *
//*                                                                                                 *
//* Text commands for tests (one line, terminated by LF or CR, numbers in hex):                     *
//*   r<slot> <adr>          memory read              m<slot> <adr>        opcode fetch (M1)        *
//*   w<slot> <adr> <value>  memory write             i <port>             I/O read                 *
//*   o <port> <value>       I/O write                <n>*<command>        command n times          *
//*   u / d                  release / assert /RESET  c                    measure CLOCK_IN         *
//*   t <n>                  slow /WAIT by n clocks (test, 0 = off)   jumper pin 29 -> pin 4        *
//*   z <ms>                 heartbeat timeout (0 = no monitoring)                                  *
//*   h / s                  heartbeat (RUN) / status        without heartbeat: OFF                 *
//*   p [ms]                 /RESET pulse (default 10 ms)    q <0|1>  TEST line LOW/HIGH            *
//*   b                      enable binary protocol (response BIN), back with byte 00               *
//*   ?                      help                     slot = 1 or 2, port = 00..FF                  *
//*                                                                                                 *
//* Pins (Teensy 4.1, Arduino numbers, see ANALYSE.md section 6.3):                                 *
//*   A0-A15  = 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27   D0-D7 = 10,12,11,13,8,7,36,37     *
//*   /MERQ 6, /RD 9, /WR 32, /IORQ 35, /M1 34, /RFSH 31, /SLTSL1 1, /SLTSL2 0                      *
//*   /CS1 24, /CS2 25, /CS12 28, /RESET 30, /OE_WR 2, /WAIT 4 (input), /INT 5 (input)              *
//*   TEST 29, CLOCK 33 (output), CLOCK_IN 3 (input, jumper from pin 33)                            *
//*                                                                                                 *
//* Idle state: all strobes and selects HIGH, /RESET LOW, addresses LOW, data input.                *
//***************************************************************************************************
//* File:  BridgeTeensy.ino                                                                         *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
//***************************************************************************************************
//* VERSION: 28/10/26                                                                               *
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
// Bits in the fast GPIO registers (mapping pin -> port/bit see ANALYSE.md 6.3)
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
// Settings
//===================================================================================================

// PLL4 = 24 MHz * (38 + 2/11) = 916.3636 MHz, SAI2 divider 4 * 64 = 256 -> 3.579545 MHz

static const uint32_t PLL4_DIV   = 38;
static const uint32_t PLL4_NUM   = 2;
static const uint32_t PLL4_DENOM = 11;
static const uint32_t SAI2_PRED  = 4;                   // 1..8
static const uint32_t SAI2_PODF  = 64;                  // 1..64

static const uint32_t MEASURE_MS   = 50;                // measuring window for CLOCK_IN
static const uint32_t EDGE_TIMEOUT = 3000;              // CPU clocks until "no clock" (5 us)
static const uint32_t MAX_WAIT_T   = 358000;            // maximum number of inserted wait clocks (100 ms; a Z80
                                                        // waits any length of time, the limit only catches a
                                                        // hanging card)
static const uint32_t MAX_TEST_WAITS = 64;              // maximum number of wait clocks in test mode (t <n>)
static const size_t   LINE_SIZE    = 48;
static const uint32_t RESET_MS     = 10;                // default length of the /RESET pulse
static const uint8_t  PROTOCOL_VERSION = 3;             // version of the binary protocol
static const uint16_t MAX_BLOCK    = 1024;              // maximum number of read cycles per MRDN
static const uint32_t BINARY_SUPERVISE_MS = 200;        // RUN timeout after switching to the binary protocol
static const uint32_t REFRESH_DUE_US = 12;              // a refresh cycle at least this often (128 rows in 2 ms)

//===================================================================================================
// State of the bus engine
//===================================================================================================

// Result of a bus cycle (defined before all functions because of the Arduino prototypes)

struct CycleResult
{
    uint8_t  data;
    uint32_t waits;
};

static uint32_t addrBits    = 0;                        // address as it is in GPIO6 bits 16-31
static uint8_t  refreshR    = 0;                        // refresh counter R (7 bits)
static uint8_t  refreshI    = 0;                        // refresh register I
static bool     clockFailed = false;                    // CLOCK_IN did not arrive
static uint32_t testWaits   = 0;                        // test mode: extend /WAIT by this many clocks
static uint32_t superviseMs = 0;                        // heartbeat timeout, 0 = no monitoring
static uint32_t lastRun     = 0;                        // time of the last heartbeat (millis)
static bool     busOn       = true;                     // RUNNING (true) or OFF (false)
static uint32_t refreshCount = 0;                       // number of idle refresh cycles
static bool     intLevel    = true;                     // last reported level of /INT
static bool     binaryMode  = false;                    // binary protocol active (otherwise text commands)
static uint32_t lastRefreshCycles = 0;                  // cycle counter at the last refresh
static uint32_t postedErrors = 0;                       // discarded write cycles without a response

//===================================================================================================
// Idle state
//===================================================================================================

// Set up a pin as an output with a fixed level. The level goes directly into the data register
// before the direction is switched, because digitalWrite() only affects the pull resistors on an
// input: the pin would otherwise be LOW until the first cycle.

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
    idleOutput(PIN_RESET, LOW);                         // cartridges in reset

    const uint8_t highPins[] = { PIN_MERQ, PIN_RD, PIN_WR, PIN_IORQ, PIN_M1, PIN_RFSH, PIN_SLTSL1,
                                 PIN_SLTSL2, PIN_CS1, PIN_CS2, PIN_CS12, PIN_OE_WR };
    for (uint8_t pin : highPins)
    {
        idleOutput(pin, HIGH);                          // inactive
    }

    for (uint8_t pin : PIN_A)
    {
        idleOutput(pin, LOW);
    }
    addrBits = 0;                                       // setAddress() switches relative to this state

    for (uint8_t pin : PIN_D)
    {
        pinMode(pin, INPUT);
    }

    pinMode(PIN_CLOCK_IN, INPUT);
    pinMode(PIN_WAIT, INPUT_PULLUP);
    pinMode(PIN_INT, INPUT_PULLUP);
    idleOutput(PIN_TEST, HIGH);                         // test line to /WAIT, HIGH = no slowdown
}

//===================================================================================================
// CLOCK
//===================================================================================================

// Set up audio PLL4, connect it to SAI2 and output it as SAI2_MCLK on pin 33

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

// Disable the CLOCK output (pin 33 as GPIO at LOW) and enable it again as SAI2_MCLK. The clock
// itself keeps running internally; when re-enabled the first pulse may be shortened, /RESET is LOW then.

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

// Frequency in mHz from the registers: 24 MHz * (DIV + NUM/DENOM) / (PRED * PODF), post divider 1

static uint64_t clockMilliHz()
{
    uint64_t div   = CCM_ANALOG_PLL_AUDIO & 0x3F;
    uint64_t num   = CCM_ANALOG_PLL_AUDIO_NUM;
    uint64_t denom = CCM_ANALOG_PLL_AUDIO_DENOM;
    uint64_t pred  = ((CCM_CS2CDR >> 6) & 0x07) + 1;
    uint64_t podf  = (CCM_CS2CDR & 0x3F) + 1;

    return (24000000000ULL * (div * denom + num)) / (denom * pred * podf);
}

// Sample CLOCK_IN and measure it against the CPU cycle counter. The CPU counter and PLL4
// come from the same 24 MHz crystal.

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
// Waiting for CLOCK_IN
//===================================================================================================

// Wait until CLOCK_IN has the level. If it does not come, clockFailed is set.

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
// Setting signals
//===================================================================================================

// Put the address on the bus; only the changed bits are switched

static inline void setAddress(uint16_t addr)
{
    uint32_t bits = (uint32_t)addr << 16;

    GPIO6_DR_TOGGLE = bits ^ addrBits;
    addrBits = bits;
}

// Test mode: TEST (connected to /WAIT via a wire jumper) pulls /WAIT LOW like a slow
// cartridge as soon as the cycle sees its selects; testWaits = 0 turns it off

static inline void testAssert()
{
    if (testWaits)
    {
        GPIO9_DR_CLEAR = G9_TEST;
    }
}

// Insert wait clocks as long as /WAIT is LOW. Call directly after a falling clock edge
// at which /WAIT is sampled; returns the number of inserted clocks. In test mode /WAIT is
// released after the last requested sample.

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

// Select signals for memory accesses; /CSx only when reading

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

// Precharge the data bus to FFh before a read cycle: drive D0-D7 via the 74HCT245 and release it
// again before /RD. If no card drives afterwards, the bus holds FFh like an MSX bus; otherwise the
// last value read or written would come back (the BIOS RAM search then takes empty areas for RAM).

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
    delayNanoseconds(30);                               // 74HCT245 switched off before /RD opens the 74LVC245
}

//===================================================================================================
// Bus cycles
//===================================================================================================

// Memory read, 3 clocks: T1 rising address, T1 falling /MERQ /RD /SLTSL /CSx, T2 falling /WAIT,
// T3 falling read data and everything inactive again

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

    waitFalling();                                      // T2 falling: sample /WAIT
    r.waits = insertWaits();

    waitFalling();                                      // T3 falling
    uint32_t psr = GPIO7_PSR;
    r.data = (uint8_t)((psr & 0x0F) | ((psr >> 12) & 0xF0));

    GPIO7_DR_SET = G7_MERQ | G7_RD;
    deselectAll();

    return r;
}

// Opcode fetch, 4 clocks + 1 MSX wait clock: /M1 from T1 to T3 rising, data at T3 rising,
// then refresh with /RFSH LOW and /MERQ from T3 falling to T4 falling

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

    waitFalling();                                      // T2 falling
    waitFalling();                                      // MSX wait clock, sample /WAIT
    r.waits = insertWaits();

    waitRising();                                       // T3 rising: read data
    uint32_t psr = GPIO7_PSR;
    r.data = (uint8_t)((psr & 0x0F) | ((psr >> 12) & 0xF0));

    GPIO7_DR_SET = G7_MERQ | G7_RD | G7_M1;
    deselectAll();
    GPIO8_DR_CLEAR = G8_RFSH;
    setAddress((uint16_t)((refreshI << 8) | (refreshR & 0x7F)));
    refreshR = (uint8_t)((refreshR + 1) & 0x7F);

    waitFalling();                                      // T3 falling: refresh /MERQ
    GPIO7_DR_CLEAR = G7_MERQ;

    waitFalling();                                      // T4 falling
    GPIO7_DR_SET = G7_MERQ;

    waitRising();                                       // end of T4
    GPIO8_DR_SET = G8_RFSH;
    lastRefreshCycles = ARM_DWT_CYCCNT;

    return r;
}

// Memory write, 3 clocks: data from T1 rising, /WR from T2 falling to T3 falling, data held
// until the end of T3

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

    waitFalling();                                      // T2 falling
    GPIO7_DR_CLEAR = G7_WR;
    r.waits = insertWaits();

    waitFalling();                                      // T3 falling
    GPIO7_DR_SET = G7_WR | G7_MERQ;
    deselectAll();

    waitRising();                                       // end of T3
    GPIO9_DR_SET = G9_OE_WR;
    GPIO7_GDIR &= ~G7_DATA;

    return r;
}

// I/O read, 4 clocks (T1, T2, automatic wait clock, T3): port on A0-A7, /IORQ and /RD from
// T2 rising to T3 falling, data at T3 falling. No /MERQ, no /SLTSL, no /CSx.

static CycleResult ioRead(uint8_t port)
{
    CycleResult r = { 0xFF, 0 };

    waitRising();                                       // T1
    setAddress(port);
    prechargeStart();
    waitRising();                                       // T2 rising
    prechargeEnd();
    GPIO7_DR_CLEAR = G7_IORQ | G7_RD;
    testAssert();

    waitFalling();                                      // T2 falling
    waitFalling();                                      // automatic wait clock, sample /WAIT
    r.waits = insertWaits();

    waitFalling();                                      // T3 falling
    uint32_t psr = GPIO7_PSR;
    r.data = (uint8_t)((psr & 0x0F) | ((psr >> 12) & 0xF0));

    GPIO7_DR_SET = G7_IORQ | G7_RD;

    return r;
}

// I/O write, 4 clocks: data from T1 rising, /IORQ and /WR from T2 rising to T3 falling,
// data held until the end of T3

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

    waitRising();                                       // T2 rising
    GPIO7_DR_CLEAR = G7_IORQ | G7_WR;
    testAssert();

    waitFalling();                                      // T2 falling
    waitFalling();                                      // automatic wait clock, sample /WAIT
    r.waits = insertWaits();

    waitFalling();                                      // T3 falling
    GPIO7_DR_SET = G7_IORQ | G7_WR;

    waitRising();                                       // end of T3
    GPIO9_DR_SET = G9_OE_WR;
    GPIO7_GDIR &= ~G7_DATA;

    return r;
}

// Idle refresh, 4 clocks like a Z80 NOP in HALT: T1 and T2 without strobes, T3 rising /RFSH
// LOW with refresh address, /MERQ from T3 falling to T4 falling, end of T4 /RFSH HIGH

static void idleRefresh()
{
    waitRising();                                       // T1
    waitRising();                                       // T2
    waitRising();                                       // T3 rising
    GPIO8_DR_CLEAR = G8_RFSH;
    setAddress((uint16_t)((refreshI << 8) | (refreshR & 0x7F)));
    refreshR = (uint8_t)((refreshR + 1) & 0x7F);

    waitFalling();                                      // T3 falling
    GPIO7_DR_CLEAR = G7_MERQ;

    waitFalling();                                      // T4 falling
    GPIO7_DR_SET = G7_MERQ;

    waitRising();                                       // end of T4
    GPIO8_DR_SET = G8_RFSH;
    lastRefreshCycles = ARM_DWT_CYCCNT;
}

// Watch /INT and report every change: "INT 0" = line LOW (active), "INT 1" = released. In
// state OFF nothing is reported.

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

// State OFF like an unpowered MSX: all lines inactive, /RESET LOW, CLOCK LOW, no refresh

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
// Commands
//===================================================================================================

static void printHelp()
{
    Serial.println("r<slot> <adr>  m<slot> <adr>  w<slot> <adr> <value>  i <port>  o <port> <value>");
    Serial.println("<n>*<command>  t <n>  z <ms>  h  s  p [ms]  q <0|1>  b  u  d  c  ?");
}

static void printClock()
{
    uint32_t edges, high, total, cycles;
    clockMeasure(edges, high, total, cycles);

    uint64_t setHz  = clockMilliHz();
    uint64_t measHz = ((uint64_t)edges * F_CPU_ACTUAL * 1000ULL) / cycles;
    uint32_t duty   = (uint32_t)(((uint64_t)high * 1000ULL) / total);

    Serial.printf("CLOCK register %lu.%03lu Hz  measured %lu.%03lu Hz  duty cycle %lu.%lu %%  "
                  "edges %lu\n",
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
            Serial.println("Error: z <0..60000> (timeout in ms, 0 = no monitoring)");
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
            Serial.println("monitoring off: permanently RUNNING, no idle refresh");
        }
        else
        {
            Serial.printf("monitoring %u ms: heartbeat h required, idle refresh active\n", ms);
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
            Serial.println("heartbeat: RUNNING (CLOCK on, /RESET stays LOW)");
        }
        else
        {
            Serial.println("heartbeat: RUNNING");
        }
        return;
    }
    if (op == 's')
    {
        Serial.printf("%s  timeout %lu ms  last heartbeat %lu ms ago  refresh cycles %lu\n",
                      busOn ? "RUNNING" : "OFF", (unsigned long)superviseMs,
                      (unsigned long)(millis() - lastRun), (unsigned long)refreshCount);
        return;
    }
    if (!busOn)
    {
        Serial.println("OFF: send heartbeat h first");
        return;
    }
    if (op == 'p')
    {
        unsigned ms = RESET_MS;

        if (sscanf(line + 1, "%u", &ms) == 1 && (ms < 1 || ms > 1000))
        {
            Serial.println("Error: p [1..1000] (length of the /RESET pulse in ms)");
            return;
        }
        GPIO8_DR_CLEAR = G8_RESET;
        delay(ms);
        GPIO8_DR_SET = G8_RESET;
        lastRun = millis();
        Serial.printf("/RESET pulse %u ms\n", ms);
        return;
    }
    if (op == 'q')
    {
        unsigned level = 2;

        if (sscanf(line + 1, "%u", &level) != 1 || level > 1)
        {
            Serial.println("Error: q <0|1> (test line TEST LOW or HIGH)");
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
        Serial.println("/RESET inactive (HIGH)");
        return;
    }
    if (op == 'd')
    {
        GPIO8_DR_CLEAR = G8_RESET;
        Serial.println("/RESET active (LOW)");
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
            Serial.println("Error: t <0..64> (number of wait clocks, 0 = off)");
            return;
        }
        testWaits = waits;
        GPIO9_DR_SET = G9_TEST;
        Serial.printf("test mode: %u wait clocks per cycle (jumper TEST pin 29 -> /WAIT pin 4)\n", waits);
        return;
    }

    if (op == 'i' || op == 'o')
    {
        unsigned port = 0;
        unsigned data = 0;
        int cnt = sscanf(line + 1, "%x %x", &port, &data);

        if (port > 0xFF || data > 0xFF || (op == 'i' && cnt < 1) || (op == 'o' && cnt < 2))
        {
            Serial.println("Error: command not understood (? = help)");
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
            Serial.println("Error: no CLOCK on pin 3 (jumper from pin 33?)");
            return;
        }

        Serial.printf("%c %02X -> %02X  wait clocks %lu  (%lux)\n", op, port, io.data,
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
        Serial.println("Error: command not understood (? = help)");
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
        Serial.println("Error: no CLOCK on pin 3 (jumper from pin 33?)");
        return;
    }

    Serial.printf("%c%u %04X -> %02X  wait clocks %lu  (%lux)\n", op, slot, addr, r.data,
                  (unsigned long)r.waits, count);
}

//===================================================================================================
// Binary protocol (see ANALYSE.md, section 4.3)
//===================================================================================================

// PC -> Teensy, all numbers little endian. IDs below 0x20 except LF and CR, so that text lines
// and binary frames stay distinguishable. Response = ID | 0x80, then status, then payload.

static const uint8_t OP_TEXT  = 0x00;                   // back to text commands
static const uint8_t OP_HELLO = 0x01;
static const uint8_t OP_MRD   = 0x02;                   // slot, adr (2), flags (bit 0 = M1)
static const uint8_t OP_MWR   = 0x03;                   // slot, adr (2), value; no response
static const uint8_t OP_IORD  = 0x04;                   // port
static const uint8_t OP_IOWR  = 0x05;                   // port, value; no response
static const uint8_t OP_MRDN  = 0x06;                   // slot, adr (2), count (2)
static const uint8_t OP_RESET = 0x07;
static const uint8_t OP_RUN   = 0x08;                   // heartbeat, no response
static const uint8_t OP_SYNC  = 0x09;
static const uint8_t OP_CFG   = 0x0B;                   // timeout in ms (2)
static const uint8_t OP_TEST  = 0x0C;                   // test line TEST: 0 = LOW, 1 = HIGH
static const uint8_t OP_IOWRW = 0x0D;                   // port, value; response with wait clocks (4)
static const uint8_t OP_MRDNG = 0x0E;                   // slot, adr (2), count (2), distance in clocks

static const uint8_t ST_OK    = 0;
static const uint8_t ST_OFF   = 1;                      // state OFF
static const uint8_t ST_CLOCK = 2;                      // no CLOCK_IN
static const uint8_t ST_ARG   = 3;                      // invalid parameter

static uint8_t blockBuf[MAX_BLOCK + 8];

// frame length including the ID, 0 = unknown ID

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
    case OP_MRDNG: return 7;
    default:       return 0;
    }
}

static void sendReply(const uint8_t *data, size_t n)
{
    Serial.write(data, n);
    Serial.send_now();
}

// Insert an idle refresh if the last one is too long ago. Only in operation with monitoring;
// call with interrupts disabled.

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

// Response to a write cycle with response: ID, status, wait clocks (4 bytes, little endian)

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

    // Bus cycles; a change of /INT triggered by the cycle is reported before its response
    uint8_t slot = f[1];
    uint16_t addr = (uint16_t)(f[2] | (f[3] << 8));
    bool memory = (op == OP_MRD || op == OP_MWR || op == OP_MRDN || op == OP_MRDNG);

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
        else if (op == OP_MRDN || op == OP_MRDNG)
        {
            uint16_t n = (uint16_t)(f[4] | (f[5] << 8));

            if (n == 0 || n > MAX_BLOCK || status == ST_ARG)
            {
                reply[0] = op | 0x80;
                reply[1] = ST_ARG;
                sendReply(reply, 2);                    // argument error: header only
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
    else if (op == OP_MRDN || op == OP_MRDNG)
    {
        uint16_t n = (uint16_t)(f[4] | (f[5] << 8));
        uint8_t gap = (op == OP_MRDNG) ? f[6] : 0;      // clocks from cycle start to cycle start
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
            // read cycle = 3 clocks + wait clocks; wait out the rest of the distance in idle
            for (uint32_t used = 3 + r.waits; used < gap && i + 1 < n && !clockFailed; used++)
            {
                waitRising();
            }
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

// Process all completely received frames; an unknown byte is discarded

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
// Setup / main loop
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
