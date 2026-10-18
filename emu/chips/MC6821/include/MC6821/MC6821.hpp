#pragma once

#include <cstdint>

namespace mc6821 {

// Motorola MC6821 (Hitachi HD46821) peripheral interface adapter. Register select RS1/RS0:
// 0 = PRA or DDRA, 1 = CRA, 2 = PRB or DDRB, 3 = CRB; bit 2 of CRA/CRB chooses the peripheral
// register (1) or the data direction register (0).
//
// Control register bits: 0 C1 interrupt enable, 1 C1 active edge (0 falling, 1 rising), 2 PR/DDR
// select, 3-5 C2 control, 6 IRQ2 flag and 7 IRQ1 flag (read only, cleared by reading the
// peripheral register). C2 as input (bit 5 = 0): bit 4 active edge, bit 3 interrupt enable.
// C2 as output (bit 5 = 1): bit 4 = 1 sets C2 to bit 3; bit 4 = 0 is handshake mode: CA2 goes low
// on a read of PRA (CB2 on a write of PRB) and returns high on the next active C1 edge (bit 3 = 0)
// or right after the access (bit 3 = 1).
class MC6821 final {
public:
    MC6821();

    void reset() noexcept;
    [[nodiscard]] std::uint8_t read(std::uint8_t reg) noexcept;
    void write(std::uint8_t reg, std::uint8_t value) noexcept;

    // Levels on the port pins driven from outside (used for the input bits).
    void setPortAInputs(std::uint8_t value) noexcept;
    void setPortBInputs(std::uint8_t value) noexcept;
    void setCA1(bool level) noexcept;
    void setCB1(bool level) noexcept;
    void setCA2(bool level) noexcept;
    void setCB2(bool level) noexcept;

    // Levels on the port pins: output bits from the peripheral register, input bits from outside.
    [[nodiscard]] std::uint8_t portA() const noexcept;
    [[nodiscard]] std::uint8_t portB() const noexcept;
    // C2 pin levels in output mode (high while C2 is an input).
    [[nodiscard]] bool ca2() const noexcept;
    [[nodiscard]] bool cb2() const noexcept;
    [[nodiscard]] bool irqA() const noexcept;
    [[nodiscard]] bool irqB() const noexcept;

private:
    struct Side {
        std::uint8_t output = 0;
        std::uint8_t direction = 0;
        std::uint8_t control = 0;
        std::uint8_t inputs = 0xFF;
        bool c1 = true;
        bool c2In = true;
        bool c2Out = true;
    };

    static void edgeC1(Side& side, bool level) noexcept;
    static void edgeC2(Side& side, bool level) noexcept;
    static void updateC2Output(Side& side) noexcept;
    [[nodiscard]] static std::uint8_t pins(const Side& side) noexcept;
    [[nodiscard]] static bool irq(const Side& side) noexcept;

    Side a;
    Side b;
};

} // namespace mc6821
