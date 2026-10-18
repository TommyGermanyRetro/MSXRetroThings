#pragma once

#include "INS8250/INS8250.hpp"
#include "SVI3X8/ExpansionPort.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace svi805 {

// Far end of the RS-232 line as the host provides it, for example a Windows COM port.
class SerialLink {
public:
    virtual ~SerialLink() = default;

    virtual void configure(const ins8250::LineSettings& settings) = 0;
    virtual void setModemOutputs(bool dtr, bool rts) = 0;
    virtual void setBreak(bool active) = 0;
    [[nodiscard]] virtual ins8250::ModemInputs modemInputs() = 0;
    virtual void send(const std::uint8_t* bytes, std::size_t count) = 0;
    // Returns the number of bytes copied; never waits.
    [[nodiscard]] virtual std::size_t receive(std::uint8_t* buffer, std::size_t capacity) = 0;
};

inline constexpr std::uint32_t SVI805CrystalHz = 3072000;

// SVI-805 RS-232 card: INS8250 at 28h-2Fh with a 3.072 MHz crystal (divisor 20 = 9600 baud), its
// interrupt on /INT. TXD, RTS and DTR go to the link; RXD, CTS, DSR and DCD come from it (RI is
// not connected). Every millisecond the card hands sent characters to the link, takes received
// ones and follows the line settings and modem lines; received characters enter the UART at the
// line speed, one frame each.
class SVI805 final : public svi3x8::ExpansionPort {
public:
    explicit SVI805(SerialLink* link = nullptr);

    // The link the card talks to, or nullptr (characters sent go nowhere, inputs stay inactive).
    void setLink(SerialLink* link) noexcept;
    [[nodiscard]] SerialLink* link() const noexcept;
    [[nodiscard]] const ins8250::INS8250& uart() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    void poll();

    ins8250::INS8250 chip;
    SerialLink* remote = nullptr;
    std::deque<std::uint8_t> incoming;
    std::vector<std::uint8_t> outgoing;
    std::uint32_t pollCountdown = 1;
    bool linkFresh = true;
    ins8250::LineSettings appliedSettings;
    ins8250::ModemOutputs appliedOutputs;
    bool appliedBreak = false;
};

// Windows COM port as the far end, for example a serial port connected through a null-modem
// cable to a terminal.
class ComPortLink final : public SerialLink {
public:
    ComPortLink() = default;
    ~ComPortLink() override;
    ComPortLink(const ComPortLink&) = delete;
    ComPortLink& operator=(const ComPortLink&) = delete;

    // name like "COM10"; closes a port opened before.
    [[nodiscard]] bool open(const std::wstring& name);
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] const std::wstring& name() const noexcept;
    // COM port names the system lists, sorted by number.
    [[nodiscard]] static std::vector<std::wstring> availablePorts();

    void configure(const ins8250::LineSettings& settings) override;
    void setModemOutputs(bool dtr, bool rts) override;
    void setBreak(bool active) override;
    [[nodiscard]] ins8250::ModemInputs modemInputs() override;
    void send(const std::uint8_t* bytes, std::size_t count) override;
    [[nodiscard]] std::size_t receive(std::uint8_t* buffer, std::size_t capacity) override;

private:
    void* handle = nullptr;
    std::wstring portName;
};

} // namespace svi805
