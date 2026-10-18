#include "SVI602/SVI602.hpp"
#include "SVI328/SVI328.hpp"
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

void selectBanks(svi328::SVI328& machine, std::uint8_t portB)
{
    machine.writeIo(0x88, 7);
    machine.writeIo(0x8C, 0x80);
    machine.writeIo(0x88, 15);
    machine.writeIo(0x8C, portB);
}

// An SVI-807 set to S2 in the SVI-602 adds RAM bank 22 to the 64 KiB of the SVI-328.
bool svi807Pass()
{
    svi328::SVI328 machine;
    if (!machine.loadRom(std::vector<std::uint8_t>(svi328::RomSize, 0xC9))) return fail("A 32 KiB ROM must load.");
    svi602::SVI602 expander;
    (void)expander.insertCard(std::make_unique<svi807::SVI807>(svi807::SVI807Switches{false, true, false, false, false}));
    machine.connectExpansion(&expander);
    machine.reset();
    machine.writeMemory(0x8000, 0x02);
    selectBanks(machine, 0xFB);
    machine.writeMemory(0x8000, 0x22);
    if (machine.readMemory(0x8000) != 0x22) return fail("/BK22 must select the SVI-807 in the SVI-602.");
    selectBanks(machine, 0xFF);
    if (machine.readMemory(0x8000) != 0x02) return fail("Without /BK22 RAM bank 2 must return.");
    return true;
}

} // namespace

int main()
{
    if (!slotPass()) return EXIT_FAILURE;
    if (!svi807Pass()) return EXIT_FAILURE;
    std::cout << "SVI-602 passed.\n";
    return EXIT_SUCCESS;
}
