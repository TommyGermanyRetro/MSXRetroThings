#include "INS8250/INS8250.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr std::uint32_t HostHz = 3579545;
constexpr std::uint32_t CrystalHz = 3072000;

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// 9600 baud (divisor 20), 8 data bits, no parity, one stop bit.
void setup9600(ins8250::INS8250& uart)
{
    uart.write(3, 0x80);
    uart.write(0, 20);
    uart.write(1, 0);
    uart.write(3, 0x03);
}

void run(ins8250::INS8250& uart, std::uint64_t ticks)
{
    for (std::uint64_t i = 0; i < ticks; ++i) uart.tick();
}

bool registersPass()
{
    ins8250::INS8250 uart(HostHz, CrystalHz);
    if (uart.read(2) != 0x01 || uart.read(5) != 0x60 || uart.interruptRequest()) {
        return fail("After reset no interrupt may be pending and the transmitter must be empty.");
    }
    setup9600(uart);
    uart.write(3, 0x80);
    if (uart.read(0) != 20 || uart.read(1) != 0) return fail("The divisor latch must read back with DLAB.");
    uart.write(3, 0x1F);
    const auto settings = uart.lineSettings();
    if (settings.baud != 9600 || settings.dataBits != 8 || settings.parity != 'E' || settings.stopBits != 2) {
        return fail("LCR 1Fh with divisor 20 must mean 9600 baud, 8E2.");
    }
    uart.write(7, 0xA5);
    if (uart.read(7) != 0xA5 || uart.read(3) != 0x1F) return fail("Scratch and line control must read back.");
    return true;
}

bool transmitPass()
{
    ins8250::INS8250 uart(HostHz, CrystalHz);
    setup9600(uart);
    uart.write(0, 'A');
    uart.write(0, 'B');
    if (uart.read(5) & ins8250::LineTransmitterHoldingEmpty) return fail("A second byte must fill the holding register.");
    // One 8N1 frame at 9600 baud: 10 bits, about 1.04 ms.
    const std::uint64_t frame = static_cast<std::uint64_t>(HostHz) * 16 * 20 / CrystalHz * 10;
    std::uint8_t sent = 0;
    run(uart, frame - 1);
    if (uart.takeTransmitted(sent)) return fail("A character must not leave before its frame time.");
    run(uart, 1);
    if (!uart.takeTransmitted(sent) || sent != 'A' || !(uart.read(5) & ins8250::LineTransmitterHoldingEmpty)) {
        return fail("After one frame the first byte must leave and the holding register empty.");
    }
    run(uart, frame);
    if (!uart.takeTransmitted(sent) || sent != 'B' || !(uart.read(5) & ins8250::LineTransmitterEmpty)) {
        return fail("After two frames both bytes must be out and the transmitter empty.");
    }
    return true;
}

bool receivePass()
{
    ins8250::INS8250 uart(HostHz, CrystalHz);
    setup9600(uart);
    uart.write(1, 0x05);
    uart.receive('x');
    if (uart.readyToReceive() || (uart.read(5) & ins8250::LineDataReady)) return fail("A character needs one frame to arrive.");
    run(uart, 4000);
    if (!uart.readyToReceive() || !uart.interruptRequest() || uart.read(2) != 0x04) {
        return fail("A received character must raise the receive interrupt.");
    }
    if (uart.read(0) != 'x' || uart.interruptRequest()) return fail("Reading RBR must clear data ready.");
    uart.receive('1');
    run(uart, 4000);
    uart.receive('2');
    run(uart, 4000);
    if (uart.read(2) != 0x06 || !(uart.read(5) & ins8250::LineOverrun) || uart.read(0) != '2') {
        return fail("An unread character must cause an overrun with the line status interrupt first.");
    }
    if (uart.read(5) & ins8250::LineOverrun) return fail("Reading LSR must clear the overrun.");
    return true;
}

bool interruptsPass()
{
    ins8250::INS8250 uart(HostHz, CrystalHz);
    setup9600(uart);
    uart.write(1, 0x02);
    if (uart.read(2) != 0x02) return fail("Enabling the transmitter interrupt with an empty THR must raise it.");
    if (uart.read(2) != 0x01) return fail("Reading IIR must clear the transmitter interrupt.");
    uart.write(0, 'Z');
    if (uart.read(2) != 0x02) return fail("The byte moving into the shift register must empty THR again.");
    uart.write(1, 0x08);
    uart.setModemInputs({true, false, false, true});
    if (uart.read(2) != 0x00) return fail("A modem line change must raise the modem status interrupt.");
    const auto msr = uart.read(6);
    if ((msr & 0x99) != 0x99 || uart.interruptRequest()) return fail("MSR must show CTS and DCD with deltas, cleared by reading.");
    return true;
}

bool loopbackPass()
{
    ins8250::INS8250 uart(HostHz, CrystalHz);
    setup9600(uart);
    uart.write(4, 0x13);
    if ((uart.read(6) & 0x30) != 0x30) return fail("In loopback DTR and RTS must appear as DSR and CTS.");
    uart.write(0, 0x5A);
    run(uart, 4000);
    std::uint8_t sent = 0;
    if (uart.takeTransmitted(sent) || uart.read(0) != 0x5A) return fail("In loopback a sent byte must return to RBR only.");
    const auto outputs = uart.modemOutputs();
    if (outputs.dtr || outputs.rts) return fail("In loopback the modem outputs must stay inactive.");
    uart.write(4, 0x03);
    if (!uart.modemOutputs().dtr || !uart.modemOutputs().rts) return fail("DTR and RTS must reach the outputs.");
    return true;
}

} // namespace

int main()
{
    if (!registersPass()) return EXIT_FAILURE;
    if (!transmitPass()) return EXIT_FAILURE;
    if (!receivePass()) return EXIT_FAILURE;
    if (!interruptsPass()) return EXIT_FAILURE;
    if (!loopbackPass()) return EXIT_FAILURE;
    std::cout << "INS8250 passed.\n";
    return EXIT_SUCCESS;
}
