#include "SVI602/SVI602.hpp"
#include "SVI318/SVI318.hpp"
#include "SVI803/SVI803.hpp"
#include "SVI807/SVI807.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// Test cartridge that records the cycles it sees.
class Probe final : public svi3x8::ExpansionPort {
public:
    unsigned resets = 0;
    unsigned ticks = 0;
    std::uint8_t lastPort = 0;
    bool interrupt = false;
    void reset() override { ++resets; }
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t) const override
    {
        return {static_cast<std::uint8_t>(address >> 8), true, false};
    }
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t, std::uint8_t, std::uint8_t) override { return {}; }
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override { return port; }
    void writeIo(std::uint8_t port, std::uint8_t) override { lastPort = port; }
    void tick() override { ++ticks; }
    [[nodiscard]] bool interruptRequest() const override { return interrupt; }
};

bool slotPass()
{
    svi602::SVI602 expander;
    const auto empty = expander.readMemory(0x1234, 0xFF);
    if (empty.data != 0xFF || empty.romDisable || empty.ramDisable || expander.readIo(0x30) != 0xFF ||
        expander.interruptRequest() || expander.card() != nullptr) {
        return fail("An empty SVI-602 must leave the bus open.");
    }
    auto probe = std::make_unique<Probe>();
    auto* seen = probe.get();
    if (expander.insertCard(std::move(probe)) != nullptr || expander.card() != seen) return fail("The card must sit in the slot.");
    expander.reset();
    expander.tick();
    expander.writeIo(0x28, 0);
    seen->interrupt = true;
    const auto cycle = expander.readMemory(0x1234, 0xFF);
    if (seen->resets != 1 || seen->ticks != 1 || seen->lastPort != 0x28 || expander.readIo(0x2A) != 0x2A ||
        cycle.data != 0x12 || !cycle.romDisable || !expander.interruptRequest()) {
        return fail("The SVI-602 must pass bus cycles, CPUCLK, /RST and /INT to its card.");
    }
    if (expander.removeCard().get() != seen || expander.card() != nullptr) return fail("Removing must empty the slot.");
    return true;
}

// The usual use on the SVI-318: an SVI-803 in the SVI-602 completes RAM bank 2 to 32 KiB.
bool svi803Pass()
{
    svi318::SVI318 machine;
    if (!machine.loadRom(std::vector<std::uint8_t>(svi318::RomSize, 0xC9))) return fail("A 32 KiB ROM must load.");
    svi602::SVI602 expander;
    machine.connectExpansion(&expander);
    machine.reset();
    machine.writeMemory(0x8000, 0x11);
    if (machine.readMemory(0x8000) != 0xFF) return fail("Without a card 8000h-BFFFh must stay empty.");
    (void)expander.insertCard(std::make_unique<svi803::SVI803>());
    machine.reset();
    machine.writeMemory(0x8000, 0x11);
    machine.writeMemory(0xBFFF, 0x22);
    machine.writeMemory(0xC000, 0x33);
    if (machine.readMemory(0x8000) != 0x11 || machine.readMemory(0xBFFF) != 0x22 || machine.readMemory(0xC000) != 0x33) {
        return fail("An SVI-803 in the SVI-602 must give 32 KiB of RAM in 8000h-FFFFh.");
    }
    return true;
}

} // namespace

int main()
{
    if (!slotPass()) return EXIT_FAILURE;
    if (!svi803Pass()) return EXIT_FAILURE;
    std::cout << "SVI-602 passed.\n";
    return EXIT_SUCCESS;
}
