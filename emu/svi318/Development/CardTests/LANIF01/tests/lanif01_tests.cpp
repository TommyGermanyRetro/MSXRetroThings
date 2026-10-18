#include "LANIF01/LANIF01.hpp"
#include "LANIF01/LanCard.hpp"
#include "SVI608M/SVI608M.hpp"
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

// Boot card of the SVI-608M: after reset its EPROM replaces the BASIC ROM, mirrored every 4 KiB in
// 0000h-7FFFh; a write to 6Fh brings the BASIC ROM back until the next reset.
bool bootCardPass()
{
    svi318::SVI318 machine;
    std::vector<std::uint8_t> basic(svi318::RomSize, 0xC9);
    std::vector<std::uint8_t> eprom(lanif01::BootRomSize, 0x00);
    eprom[0] = 0xF3;
    eprom[0xFFF] = 0xE7;
    svi608m::SVI608M expander;
    auto card = std::make_unique<lanif01::LANIF01>();
    if (card->loadRom(std::vector<std::uint8_t>(100)) || !card->loadRom(eprom)) return fail("The boot EPROM must hold 4096 bytes.");
    expander.insertCard(0, std::move(card));
    if (!machine.loadRom(basic)) return fail("A 32 KiB ROM must load.");
    machine.connectExpansion(&expander);
    machine.reset();
    machine.writeMemory(0xC000, 0x12);
    if (machine.readMemory(0x0000) != 0xF3 || machine.readMemory(0x7000) != 0xF3 || machine.readMemory(0x6FFF) != 0xE7 ||
        machine.readMemory(0xC000) != 0x12) {
        return fail("After reset the EPROM must replace the ROM, mirrored, with RAM bank 2 above.");
    }
    machine.writeIo(0x6F, 0x00);
    if (machine.readMemory(0x0000) != 0xC9) return fail("A write to 6Fh must bring the BASIC ROM back.");
    machine.reset();
    if (machine.readMemory(0x0000) != 0xF3) return fail("Reset must switch the EPROM on again.");
    return true;
}


// The printer routines of the CP/M 2.27 BIOS: initialisation (E506h), detection by switching CRB
// bit 2 between DDRB and PRB (E5A7h), printing (E541h) and the interrupt routine (E538h). The PIA
// sits on the LAN card of the SVI-609; the SVI-608M has none.
bool piaPrinterPass()
{
    const auto detect = [](svi3x8::ExpansionPort& port) {
        port.writeIo(0x63, static_cast<std::uint8_t>(port.readIo(0x63) | 0x04));
        const auto first = port.readIo(0x62);
        port.writeIo(0x63, static_cast<std::uint8_t>(port.readIo(0x63) & ~0x04));
        return port.readIo(0x62) != first;
    };
    const auto initialise = [](svi3x8::ExpansionPort& port) {
        port.writeIo(0x61, 0x00);
        port.writeIo(0x63, 0x00);
        port.writeIo(0x62, 0x00);
        port.writeIo(0x60, 0xFF);
        port.writeIo(0x63, 0x34);
        port.writeIo(0x61, 0x34);
    };
    lanif01::LanCard card;
    initialise(card);
    if (!detect(card)) return fail("The BIOS detection must find the printer port of the LAN card.");
    svi608m::SVI608M bootOnly;
    initialise(bootOnly);
    if (detect(bootOnly)) return fail("The SVI-608M has no printer PIA: the detection must find nothing.");

    auto& port = card.printerPort();
    for (const char character : std::string("HII")) {
        if (card.readIo(0x62) & 0x01) return fail("PB0 must report a ready printer.");
        card.writeIo(0x60, static_cast<std::uint8_t>(~character));
        card.writeIo(0x61, 0x3C);
        card.writeIo(0x61, 0x35);
    }
    if (port.printer().output() != std::vector<std::uint8_t>{'H', 'I', 'I'}) {
        return fail("The strobe on CA2 must print the inverted port A data, also a repeated character.");
    }
    if (!card.interruptRequest()) return fail("The /ACK on CA1 must interrupt with CRA bit 0 set.");
    (void)card.readIo(0x60);
    card.writeIo(0x61, static_cast<std::uint8_t>(card.readIo(0x61) & ~0x01));
    if (card.interruptRequest()) return fail("The interrupt routine must clear the request.");
    return true;
}

// The LAN card also carries the boot logic: EPROM in place of the BASIC ROM until 6Fh is written.
bool lanCardBootPass()
{
    lanif01::LanCard card;
    std::vector<std::uint8_t> eprom(lanif01::BootRomSize, 0x00);
    eprom[0] = 0xF3;
    if (card.loadRom(std::vector<std::uint8_t>(10))) return fail("A wrong EPROM size must be refused.");
    if (!card.loadRom(eprom)) return fail("A 4 KiB EPROM must load.");
    card.reset();
    const auto cycle = card.readMemory(0x1000, 0xFF);
    if (!card.romActive() || cycle.data != 0xF3 || !cycle.romDisable) return fail("After /RST the EPROM must replace the BASIC ROM.");
    card.writeIo(0x6F, 0x00);
    if (card.romActive() || card.readMemory(0x0000, 0xFF).romDisable) return fail("A write to 6Fh must switch the EPROM off.");
    return true;
}

} // namespace

int main()
{
    if (!bootCardPass()) return EXIT_FAILURE;
    if (!lanCardBootPass()) return EXIT_FAILURE;
    if (!piaPrinterPass()) return EXIT_FAILURE;
    std::cout << "LAN-IF-01 passed.\n";
    return EXIT_SUCCESS;
}
