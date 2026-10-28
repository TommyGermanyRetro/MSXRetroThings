//***************************************************************************************************
//* HEADER - TMS9918aPiPico.ino - VDP data stream from the MSX to the TMS9918A emulator (Pi Pico)   *
//***************************************************************************************************
//* Listens to all accesses of the MSX to the VDP (ports 98h/99h) and sends them over USB in the    *
//* format of the TMS9918A emulator (DATA_FORMATS.md): 3XX/4XX read 98h/99h, 5XX/6XX write          *
//* 98h/99h, 7 RESET of the MSX, 8TTTTTT time mark (µs, 24 bits, before an access after >= 64 µs).  *
//* A 7 received from the PC resets the MSX.                                                        *
//*                                                                                                 *
//* Pins:  D0-D7 = GP0-GP7, /CSW = GP8, /CSR = GP9, A0 = GP10, /RES = GP11 (input),                 *
//*        VDP = GP12 (input, HIGH while address 98h/99h is present), /RESMSX = GP20 (output),      *
//*        /WAIT = GP21 (output, active LOW)                                                        *
//*                                                                                                 *
//* Flow: the PIO captures every access (vdpsniff.pio), core 1 adds the time stamp and puts it      *
//* together with the RESET events into a ring buffer, core 0 formats and sends over USB.           *
//* If the ring buffer is full, the PIO holds the access via /WAIT for at most STALL_MAX_US;        *
//* after that accesses are dropped until the buffer is half empty again.                           *
//* /WAIT and /RESMSX drive a 74HCT245 (push-pull, idle level HIGH).                                *
//***************************************************************************************************
//* File: TMS9918aPiPico.ino                                                                        *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
//***************************************************************************************************
//* VERSION: 27/10/26                                                                               *
//***************************************************************************************************

#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "vdpsniff.pio.h"

//===================================================================================================
// Pins
//===================================================================================================

static const uint PIN_D0     = 0;                       // D0-D7, /CSW, /CSR, A0 = GP0-GP10
static const uint PIN_RES    = 11;                      // /RES of the MSX (input)
static const uint PIN_VDP    = 12;                      // address 98h/99h (input, active HIGH)
static const uint PIN_RESMSX = 20;                      // reset the MSX (active LOW)
static const uint PIN_WAIT   = 21;                      // /WAIT (active LOW)

static const uint32_t BIT_CSW = 1u << 8;                // bits in the captured value
static const uint32_t BIT_CSR = 1u << 9;
static const uint32_t BIT_A0  = 1u << 10;

//===================================================================================================
// Settings
//===================================================================================================

static const float    PIO_CLOCK_HZ     = 25000000.0f;   // PIO clock (40 ns per instruction)
static const uint     FIFO_STALL_LEVEL = 6;             // /WAIT from this RX FIFO level on
static const uint32_t STALL_MAX_US     = 100;           // longest hold time via /WAIT
static const uint32_t RES_DEBOUNCE_US  = 50;            // /RES must be LOW this long
static const uint32_t MARK_US          = 64;            // interval of the time marks
static const uint32_t RESET_PULSE_MS   = 1;             // length of the reset pulse
static const uint32_t RESET_CHECK_MS   = 1;             // wait time before checking /RES

//===================================================================================================
// Ring buffer core 1 -> core 0
//===================================================================================================

// Entry: bits 0-10 captured value (D0-D7, /CSW, /CSR, A0), ENTRY_RESET = RESET of the MSX

static const uint32_t RING_SIZE   = 16384;              // number of entries (power of two)
static const uint32_t ENTRY_RESET = 0x80000000u;

static uint32_t ringValue[RING_SIZE];
static uint32_t ringTime[RING_SIZE];
static volatile uint32_t ringHead = 0;                  // written by core 1
static volatile uint32_t ringTail = 0;                  // written by core 0
static volatile bool     ringFlush = false;             // core 0 requests a flush

static uint32_t RING_FILL()
{
    return (ringHead - ringTail) & (RING_SIZE - 1);
}

// Diagnostic counters

static volatile uint32_t countDropped  = 0;             // dropped because the buffer was full
static volatile uint32_t countInvalid  = 0;             // neither /CSW nor /CSR active

//===================================================================================================
// Core 1: PIO, time stamps, /RES
//===================================================================================================

static PIO  pio = pio0;
static uint sm  = 0;

static bool     resArmed    = true;                     // next LOW phase reports a RESET
static uint32_t resLowStart = 0;
static bool     resPending  = false;
static uint32_t resTime     = 0;

static bool     stalling    = false;
static uint32_t stallStart  = 0;
static bool     overflow    = false;

static volatile bool core1Ready = false;                // PIO and /WAIT are set up

static void RING_PUT(uint32_t value, uint32_t time)
{
    uint32_t head = ringHead;
    ringValue[head] = value;
    ringTime[head]  = time;
    __dmb();
    ringHead = (head + 1) & (RING_SIZE - 1);
}

void setup1()
{
    // Inputs D0-D7, /CSW, /CSR, A0 and VDP without pull resistors

    for (uint pin = PIN_D0; pin <= PIN_VDP; pin++)
    {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
    }

    // /WAIT: output with idle level HIGH, then handed over to the PIO

    gpio_init(PIN_WAIT);
    gpio_put(PIN_WAIT, 1);
    gpio_set_dir(PIN_WAIT, GPIO_OUT);
    gpio_disable_pulls(PIN_WAIT);

    uint offset = pio_add_program(pio, &vdpsniff_program);
    pio_sm_config c = vdpsniff_program_get_default_config(offset);

    sm_config_set_in_pins(&c, PIN_D0);
    sm_config_set_jmp_pin(&c, PIN_VDP);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_set_pins(&c, PIN_WAIT, 1);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    sm_config_set_mov_status(&c, STATUS_RX_LESSTHAN, FIFO_STALL_LEVEL);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / PIO_CLOCK_HZ);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_pins_with_mask(pio, sm, 1u << PIN_WAIT, 1u << PIN_WAIT);
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_WAIT, 1, true);
    pio_gpio_init(pio, PIN_WAIT);
    pio_sm_set_enabled(pio, sm, true);

    // /RES of the MSX

    gpio_init(PIN_RES);
    gpio_set_dir(PIN_RES, GPIO_IN);

    core1Ready = true;
}

void loop1()
{
    uint32_t now = time_us_32();

    // Flush the buffer on request of core 0 (only core 1 writes ringHead)

    if (ringFlush)
    {
        ringHead = ringTail;
        resPending = false;
        overflow = false;
        __dmb();
        ringFlush = false;
    }

    // /RES: a LOW phase of at least RES_DEBOUNCE_US reports a RESET

    if (gpio_get(PIN_RES))
    {
        resArmed = true;
        resLowStart = 0;
    }
    else if (resArmed)
    {
        if (resLowStart == 0)
        {
            resLowStart = now | 1;
        }
        else if (now - resLowStart >= RES_DEBOUNCE_US)
        {
            resArmed = false;
            resPending = true;
            resTime = now;
        }
    }

    if (resPending && RING_FILL() < RING_SIZE - 1)
    {
        RING_PUT(ENTRY_RESET, resTime);
        resPending = false;
    }

    // Captured accesses with time stamp into the ring buffer

    while (!pio_sm_is_rx_fifo_empty(pio, sm))
    {
        if (overflow && RING_FILL() < RING_SIZE / 2)
        {
            overflow = false;
        }

        if (!overflow && RING_FILL() < RING_SIZE - 1)
        {
            RING_PUT(pio_sm_get(pio, sm), time_us_32());
            stalling = false;
            continue;
        }

        // Buffer full: the PIO holds the access via /WAIT, for at most STALL_MAX_US

        if (!overflow)
        {
            if (!stalling)
            {
                stalling = true;
                stallStart = time_us_32();
                break;
            }

            if (time_us_32() - stallStart < STALL_MAX_US)
            {
                break;
            }

            overflow = true;
            stalling = false;
        }

        pio_sm_get(pio, sm);
        countDropped = countDropped + 1;
    }
}

//===================================================================================================
// Core 0: formatting, USB, reset of the MSX
//===================================================================================================

static const uint32_t TX_SIZE  = 2048;
static const uint32_t TX_FLUSH = 512;

static uint8_t  tx[TX_SIZE];
static uint32_t txLen = 0;

static bool     markValid = false;
static uint32_t markTime  = 0;

static char     rxLine[8];
static uint32_t rxLen = 0;

static const char HEX_DIGIT[] = "0123456789ABCDEF";

static void TX_HEX(uint32_t value, int digits)
{
    for (int i = digits - 1; i >= 0; i--)
    {
        tx[txLen++] = HEX_DIGIT[(value >> (4 * i)) & 0x0F];
    }
}

// Entry as message(s) into the send buffer, time mark before an access after >= MARK_US

static void FORMAT_ENTRY(uint32_t value, uint32_t time)
{
    if (value == ENTRY_RESET)
    {
        tx[txLen++] = '7';
        tx[txLen++] = '\n';
        return;
    }

    bool write = (value & BIT_CSW) == 0;
    bool read  = (value & BIT_CSR) == 0;

    if (write == read)
    {
        countInvalid = countInvalid + 1;
        return;
    }

    if (!markValid || time - markTime >= MARK_US)
    {
        markValid = true;
        markTime = time;
        tx[txLen++] = '8';
        TX_HEX(time & 0xFFFFFF, 6);
        tx[txLen++] = '\n';
    }

    bool port99 = (value & BIT_A0) != 0;

    tx[txLen++] = read ? (port99 ? '4' : '3') : (port99 ? '6' : '5');
    TX_HEX(value & 0xFF, 2);
    tx[txLen++] = '\n';
}

// Reset of the MSX: flush the buffers, pulse /RESMSX, then wait RESET_CHECK_MS. Core 1 detects
// the LOW phase of /RES and thereby reports the RESET (7) to the PC.

static void RESET_MSX()
{
    ringFlush = true;
    while (ringFlush)
    {
    }

    ringTail = ringHead;
    txLen = 0;
    markValid = false;

    gpio_put(PIN_RESMSX, 0);
    delay(RESET_PULSE_MS);
    gpio_put(PIN_RESMSX, 1);
    delay(RESET_CHECK_MS);
}

void setup()
{
    // Keep the MSX in RESET until core 1 has set up the PIO and /WAIT; the release produces the
    // RESET message 7 and thereby synchronises the emulator

    gpio_init(PIN_RESMSX);
    gpio_put(PIN_RESMSX, 0);
    gpio_set_dir(PIN_RESMSX, GPIO_OUT);
    gpio_disable_pulls(PIN_RESMSX);

    // The emulator opens the port without DTR, so send without flow control

    Serial.ignoreFlowControl(true);
    Serial.begin(1000000);

    while (!core1Ready)
    {
    }

    delay(RESET_PULSE_MS);
    gpio_put(PIN_RESMSX, 1);
}

void loop()
{
    // Messages from the PC: 7 = reset the MSX

    while (Serial.available() > 0)
    {
        int ch = Serial.read();

        if (ch == '\n')
        {
            if (rxLen == 1 && rxLine[0] == '7')
            {
                RESET_MSX();
            }
            rxLen = 0;
        }
        else if (rxLen < sizeof(rxLine))
        {
            rxLine[rxLen++] = (char)ch;
        }
        else
        {
            rxLen = sizeof(rxLine) + 1;
        }
    }

    // Format entries as long as the send buffer has room for a time mark and a message

    while (ringTail != ringHead && txLen <= TX_SIZE - 16)
    {
        __dmb();
        uint32_t tail = ringTail;
        FORMAT_ENTRY(ringValue[tail], ringTime[tail]);
        ringTail = (tail + 1) & (RING_SIZE - 1);
    }

    // Send as soon as enough is collected or the ring buffer is empty; without a reading PC
    // write returns immediately, the rest stays in the send buffer

    if (txLen > 0 && (txLen >= TX_FLUSH || ringTail == ringHead))
    {
        size_t sent = Serial.write(tx, txLen);

        if (sent > 0)
        {
            memmove(tx, tx + sent, txLen - sent);
            txLen -= sent;
        }
    }
}
