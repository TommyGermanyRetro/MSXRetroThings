#pragma once

#include <cstdint>

namespace ins8250 {

// Frame format set in the line control register and the divisor latch.
struct LineSettings {
    std::uint32_t baud = 0;
    unsigned dataBits = 5;
    char parity = 'N';      // N, O, E, M (mark) or S (space)
    unsigned stopBits = 1;  // 1 or 2 (two means 1.5 with five data bits)
};

struct ModemInputs {
    bool cts = false;
    bool dsr = false;
    bool ri = false;
    bool dcd = false;
};

struct ModemOutputs {
    bool dtr = false;
    bool rts = false;
    bool out1 = false;
    bool out2 = false;
};

// Line status register bits.
inline constexpr std::uint8_t LineDataReady = 0x01;
inline constexpr std::uint8_t LineOverrun = 0x02;
inline constexpr std::uint8_t LineParityError = 0x04;
inline constexpr std::uint8_t LineFramingError = 0x08;
inline constexpr std::uint8_t LineBreak = 0x10;
inline constexpr std::uint8_t LineTransmitterHoldingEmpty = 0x20;
inline constexpr std::uint8_t LineTransmitterEmpty = 0x40;

// National Semiconductor INS8250 UART without FIFO. Registers 0-7: receiver buffer / transmitter
// holding (divisor latch LSB with DLAB), interrupt enable (divisor latch MSB with DLAB), interrupt
// identification, line control, modem control, line status, modem status, scratch. The baud
// clock is crystalHz / 16 / divisor; the host clocks the chip with tick() at hostClockHz and
// moves whole characters on the serial side, each taking one frame time on the line.
class INS8250 final {
public:
    INS8250(std::uint32_t hostClockHz, std::uint32_t crystalHz);

    // Master reset: IER, LCR and MCR cleared, transmitter empty, no interrupt.
    void reset();
    [[nodiscard]] std::uint8_t read(std::uint8_t reg);
    void write(std::uint8_t reg, std::uint8_t value);
    void tick();

    [[nodiscard]] bool interruptRequest() const noexcept;

    // A character that has left the transmitter (not in loopback mode).
    [[nodiscard]] bool takeTransmitted(std::uint8_t& value) noexcept;
    // The receiver can take the next character: the previous one has fully arrived.
    [[nodiscard]] bool readyToReceive() const noexcept;
    // A character starts on the receive line; it reaches the receiver buffer one frame later.
    void receive(std::uint8_t value) noexcept;

    void setModemInputs(const ModemInputs& inputs) noexcept;
    [[nodiscard]] ModemOutputs modemOutputs() const noexcept;
    [[nodiscard]] LineSettings lineSettings() const noexcept;
    [[nodiscard]] bool breakActive() const noexcept;
    [[nodiscard]] bool loopback() const noexcept;

private:
    [[nodiscard]] std::uint64_t frameTicks() const noexcept;
    [[nodiscard]] std::uint8_t interruptIdentification() const noexcept;
    [[nodiscard]] std::uint8_t modemStatusLines() const noexcept;
    void deliver(std::uint8_t value) noexcept;
    void updateModemStatus() noexcept;

    std::uint32_t hostHz;
    std::uint32_t crystal;
    std::uint8_t receiverBuffer = 0;
    std::uint8_t holding = 0;
    std::uint8_t shift = 0;
    std::uint16_t divisor = 0;
    std::uint8_t interruptEnable = 0;
    std::uint8_t lineControl = 0;
    std::uint8_t modemControl = 0;
    std::uint8_t lineStatus = LineTransmitterHoldingEmpty | LineTransmitterEmpty;
    std::uint8_t modemStatus = 0;
    std::uint8_t scratch = 0;
    bool transmitterInterrupt = false;
    bool shifting = false;
    std::uint64_t transmitTicks = 0;
    bool receiving = false;
    std::uint8_t receiveValue = 0;
    std::uint64_t receiveTicks = 0;
    bool outputPending = false;
    std::uint8_t output = 0;
    ModemInputs externalInputs;
};

} // namespace ins8250
