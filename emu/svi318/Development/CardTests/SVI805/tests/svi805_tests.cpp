#include "SVI805/SVI805.hpp"
#include "SVI605B/SVI605B.hpp"
#include "SVI318/SVI318.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// Far end of the RS-232 line for the tests.
class FakeLink final : public svi805::SerialLink {
public:
    ins8250::LineSettings settings;
    bool dtr = false;
    bool rts = false;
    ins8250::ModemInputs inputs;
    std::vector<std::uint8_t> sent;
    std::vector<std::uint8_t> toSend;

    void configure(const ins8250::LineSettings& lineSettings) override { settings = lineSettings; }
    void setModemOutputs(bool dataTerminalReady, bool requestToSend) override
    {
        dtr = dataTerminalReady;
        rts = requestToSend;
    }
    void setBreak(bool) override {}
    ins8250::ModemInputs modemInputs() override { return inputs; }
    void send(const std::uint8_t* bytes, std::size_t count) override { sent.insert(sent.end(), bytes, bytes + count); }
    std::size_t receive(std::uint8_t* buffer, std::size_t capacity) override
    {
        const auto count = std::min(capacity, toSend.size());
        std::copy(toSend.begin(), toSend.begin() + static_cast<std::ptrdiff_t>(count), buffer);
        toSend.erase(toSend.begin(), toSend.begin() + static_cast<std::ptrdiff_t>(count));
        return count;
    }
};

bool serialPass()
{
    FakeLink link;
    svi605b::SVI605B expander;
    auto card = std::make_unique<svi805::SVI805>(&link);
    auto* svi805 = card.get();
    expander.insertCard(2, std::move(card));
    const auto runMs = [&](unsigned ms) {
        for (std::uint32_t t = 0; t < svi318::CpuClockHz / 1000 * ms; ++t) expander.tick();
    };
    expander.writeIo(0x2B, 0x80);
    expander.writeIo(0x28, 20);
    expander.writeIo(0x29, 0);
    expander.writeIo(0x2B, 0x03);
    expander.writeIo(0x2C, 0x03);
    expander.writeIo(0x28, 'H');
    expander.writeIo(0x28, 'I');
    runMs(4);
    if (link.settings.baud != 9600 || link.settings.dataBits != 8 || link.settings.parity != 'N' || !link.dtr || !link.rts) {
        return fail("The link must follow 9600 8N1 and DTR/RTS of the SVI-805.");
    }
    if (link.sent != std::vector<std::uint8_t>{'H', 'I'}) return fail("Sent characters must reach the link.");

    link.inputs = {true, true, true, true};
    link.toSend = {'o', 'k'};
    expander.writeIo(0x29, 0x01);
    std::vector<std::uint8_t> received;
    bool interrupted = false;
    for (unsigned i = 0; i < 400 && received.size() < 2; ++i) {
        runMs(0);
        for (std::uint32_t t = 0; t < 100; ++t) expander.tick();
        if (expander.interruptRequest()) interrupted = true;
        if (expander.readIo(0x2D) & ins8250::LineDataReady) received.push_back(expander.readIo(0x28));
    }
    if (received != std::vector<std::uint8_t>{'o', 'k'} || !interrupted) {
        return fail("Received characters must arrive in RBR at line speed with the receive interrupt.");
    }
    if ((expander.readIo(0x2E) & 0xF0) != 0xB0) return fail("CTS, DSR and DCD must reach MSR, RI must stay off.");
    if (svi805->link() != &link || svi805->uart().lineSettings().baud != 9600) return fail("The card must report its link.");

    svi805::ComPortLink port;
    if (port.open(L"COM250") || port.isOpen()) return fail("Opening a missing COM port must fail.");
    return true;
}

} // namespace

int main()
{
    if (!serialPass()) return EXIT_FAILURE;
    std::cout << "SVI-805 passed.\n";
    return EXIT_SUCCESS;
}
