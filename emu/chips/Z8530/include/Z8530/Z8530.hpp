#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace z8530 {

inline constexpr int ChannelA = 0;
inline constexpr int ChannelB = 1;

// One symbol on the serial line of a channel: a character, an SDLC flag, an SDLC abort, or the
// start or end of an asynchronous break. Bit-level coding (start and stop bits, zero insertion,
// NRZ/NRZI/FM) is not modelled: a character carries its data bits (right-justified), its parity
// bit and whether its stop bit was present (framingOk).
struct LineSymbol {
    enum class Kind : std::uint8_t { Data, Flag, Abort, BreakStart, BreakEnd };
    Kind kind = Kind::Data;
    std::uint8_t value = 0;
    bool parityBit = false;
    bool framingOk = true;
};

// Zilog Z8530 (NMOS) Serial Communications Controller, two channels A and B.
//
// CPU side: control and data port per channel (A//B and D//C). A control write with pointer 0 is
// WR0: bits 0-2 select the register of the next control access (plus 8 with the Point High
// command), bits 3-5 are a command, bits 6-7 a CRC reset code; after the next access the pointer
// returns to 0. RR4-RR7 read as RR0-RR3, RR9 as RR13, RR11 as RR15, RR14 as RR10. WR2 (vector) and
// WR9 (master interrupt control, resets) exist once. The data port is WR8/RR8.
//
// Modes: asynchronous (WR4 bits 2-3 not 0) with 5-8 bits (five-or-less coding on transmit),
// parity, 1, 1.5 or 2 stop bits, break, unused receive bits read as 1; SDLC (WR4 bits 4-5 = 10,
// bits 2-3 = 00) with opening and closing flags, abort, address search on WR6 with broadcast FFh
// and the four-bit range (WR3 bit 1), CRC-CCITT preset by WR10 bit 7, sent inverted and checked
// against 0001110100001111b, the two CRC bytes passed into the receive FIFO, end of frame and CRC
// result in RR1, Tx Underrun/EOM latch with CRC, flag or abort (WR10 bit 2) on underrun. Monosync,
// bisync, external sync, SDLC loop mode, DPLL and DMA requests are not modelled.
//
// Timing: tick() is one PCLK cycle. A transmitted character takes its bit count times the bit
// time; the bit time is the clock period times the clock mode (WR4 bits 6-7, x1 in SDLC). The
// clock comes from the RTxC or TRxC pin (setRtxcPeriod, setTrxcPeriod) or from the baud rate
// generator (WR11 = 10b, WR14 bit 0 enable, bit 1 source PCLK, otherwise RTxC) with the period
// 2 x (time constant + 2) source cycles. Received symbols arrive with receive() at the time the
// line delivers them.
//
// Interrupts: six sources in the fixed order Rx A, Tx A, Ext/Status A, Rx B, Tx B, Ext/Status B,
// each with interrupt pending (RR3 in channel A) and under service. /INT needs WR9 bit 3 (MIE);
// interruptAcknowledge() is the /INTACK cycle with the vector of WR2, modified by WR9 bit 0 (VIS)
// in bits 1-3 or, with WR9 bit 4, in bits 6-4. RR2 of channel B is always modified.
class Z8530 final {
public:
    Z8530();

    // Hardware reset (/RD and /WR low together).
    void reset() noexcept;

    [[nodiscard]] std::uint8_t readControl(int channel) noexcept;
    void writeControl(int channel, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t readData(int channel) noexcept;
    void writeData(int channel, std::uint8_t value) noexcept;

    // One PCLK cycle.
    void tick() noexcept;

    // /INT output; the interrupt acknowledge cycle returns the vector, or 0xFF (bus not driven)
    // with WR9 bit 1 (NV) set or no interrupt to acknowledge.
    [[nodiscard]] bool interruptRequest() const noexcept;
    [[nodiscard]] std::uint8_t interruptAcknowledge() noexcept;

    // Serial line: symbols on TxD (in order) and symbols arriving on RxD.
    [[nodiscard]] bool hasTransmitted(int channel) const noexcept;
    [[nodiscard]] LineSymbol takeTransmitted(int channel) noexcept;
    void receive(int channel, const LineSymbol& symbol) noexcept;

    // Modem lines: inputs as active states (/CTS low, /DCD low), outputs /DTR and /RTS as active.
    void setCts(int channel, bool active) noexcept;
    void setDcd(int channel, bool active) noexcept;
    [[nodiscard]] bool dtr(int channel) const noexcept;
    [[nodiscard]] bool rts(int channel) const noexcept;

    // Period of the clock on the RTxC or TRxC pin in PCLK cycles (0: no clock).
    void setRtxcPeriod(int channel, std::uint32_t pclkCycles) noexcept;
    void setTrxcPeriod(int channel, std::uint32_t pclkCycles) noexcept;

private:
    static constexpr std::size_t FifoDepth = 3;
    static constexpr int SourceCount = 6;

    struct Received {
        std::uint8_t data = 0;
        std::uint8_t status = 0;
    };

    struct Channel {
        std::array<std::uint8_t, 16> wr{};
        std::uint8_t pointer = 0;
        // receiver
        std::deque<Received> fifo;
        Received pending{};
        bool hasPending = false;
        std::uint8_t lastData = 0;
        std::uint8_t lastStatus = 0;
        std::uint8_t latchedErrors = 0;
        bool locked = false;
        bool firstCharArmed = false;
        bool hunting = true;
        bool inFrame = false;
        bool firstByte = false;
        bool frameAccepted = false;
        std::uint16_t rxCrc = 0;
        bool breakAbort = false;
        // transmitter
        bool bufferFull = false;
        std::uint8_t buffer = 0;
        bool shifting = false;
        std::uint64_t shiftRemaining = 0;
        LineSymbol shiftSymbol{};
        std::deque<LineSymbol> trailer;
        bool trailerIsCrc = false;
        bool frameOpen = false;
        bool underrunLatch = true;
        std::uint16_t txCrc = 0;
        std::deque<LineSymbol> output;
        // clocks
        std::uint32_t rtxcPeriod = 0;
        std::uint32_t trxcPeriod = 0;
        std::uint64_t brgPhase = 0;
        bool zeroCount = false;
        // modem inputs
        bool cts = false;
        bool dcd = false;
        // External/Status latches
        bool extLatched = false;
        std::uint8_t latchedRr0 = 0;
        std::uint8_t lastRr0 = 0;
        // interrupt pending and under service: Rx, Tx, Ext/Status
        bool rxIp = false;
        bool rxSpecial = false;
        bool txIp = false;
        bool extIp = false;
        std::array<bool, 3> ius{};
    };

    void resetChannel(int channel, bool hardware) noexcept;
    void command(int channel, std::uint8_t value) noexcept;
    void writeRegister(int channel, int reg, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t readRegister(int channel, int reg) const noexcept;
    [[nodiscard]] std::uint8_t liveRr0(int channel) const noexcept;
    [[nodiscard]] std::uint8_t rr0(int channel) const noexcept;
    [[nodiscard]] std::uint8_t rr1(int channel) const noexcept;
    [[nodiscard]] std::uint8_t rr3() const noexcept;
    [[nodiscard]] bool sourcePending(int source) const noexcept;
    [[nodiscard]] bool sourceUnderService(int source) const noexcept;
    [[nodiscard]] int highestPending(bool skipServiced) const noexcept;
    [[nodiscard]] std::uint8_t vectorStatus(int source) const noexcept;
    [[nodiscard]] std::uint8_t modifiedVector(int source) const noexcept;
    [[nodiscard]] bool asyncMode(int channel) const noexcept;
    [[nodiscard]] bool sdlcMode(int channel) const noexcept;
    [[nodiscard]] bool transmitterEnabled(int channel) const noexcept;
    [[nodiscard]] bool receiverEnabled(int channel) const noexcept;
    [[nodiscard]] std::uint64_t clockPeriod(int channel) const noexcept;
    [[nodiscard]] std::uint64_t brgPeriod(int channel) const noexcept;
    [[nodiscard]] std::uint64_t symbolTime(int channel, const LineSymbol& symbol, int bits) const noexcept;
    [[nodiscard]] int transmitBits(int channel, std::uint8_t value, std::uint8_t& data) const noexcept;
    [[nodiscard]] bool parityOf(int channel, std::uint8_t data, int bits) const noexcept;
    [[nodiscard]] bool special(int channel, std::uint8_t status) const noexcept;
    void tickBrg(int channel) noexcept;
    void tickTransmitter(int channel) noexcept;
    void startShift(int channel, const LineSymbol& symbol, int bits = 8) noexcept;
    void emit(int channel, const LineSymbol& symbol) noexcept;
    void acceptSymbol(int channel, const LineSymbol& symbol) noexcept;
    void acceptAsync(int channel, const LineSymbol& symbol) noexcept;
    void acceptSdlc(int channel, const LineSymbol& symbol) noexcept;
    void pushReceived(int channel, std::uint8_t data, std::uint8_t status) noexcept;
    void popReceived(int channel) noexcept;
    void updateRx(int channel) noexcept;
    void updateExtStatus(int channel) noexcept;
    [[nodiscard]] static std::uint16_t crcCcitt(std::uint16_t crc, std::uint8_t data) noexcept;

    std::array<Channel, 2> channels{};
    std::uint8_t vector = 0;
    std::uint8_t wr9 = 0;
};

} // namespace z8530
