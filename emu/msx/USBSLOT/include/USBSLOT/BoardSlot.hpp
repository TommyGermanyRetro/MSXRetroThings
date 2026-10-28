#pragma once

#include "USBSLOT/UsbSlot.hpp"

#include <array>
#include <bitset>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace usbslot {

inline constexpr unsigned PageSize = 0x4000;
inline constexpr unsigned WindowSize = 0x2000;
inline constexpr unsigned LineSize = 256;
inline constexpr unsigned LinesPerWindow = WindowSize / LineSize;

// How a cartridge switches its memory, for the read cache: one set for a plain cartridge, one per
// subslot behind its own slot expander.
struct Rules {
    // Lines of 256 bytes that are always read on the board (registers, status, ports).
    std::bitset<0x10000 / LineSize> uncached;
    // Lines that are read on the board only while bit 0 of the last write to switchAddress is set (SD
    // Mapper: 6001h switches 4000h-47FFh and 4800h from the Nextor ROM to the SPI port); -1: no switch.
    std::bitset<0x10000 / LineSize> switchedUncached;
    int switchAddress = -1;
    // Reads through a port that needs time per byte, done by an unrolled chain of LDI in the ROM of the
    // cartridge (SD Mapper: LD HL,4000h and 16 or 512 LDI). When the LDI fetched from this slot reads
    // spiBlockStart with the switch on, the LDI that follow it in the code give the number of bytes still
    // to come: they are read in one block with spiBlockGap clocks per byte (at most up to spiBlockEnd), and
    // the reads in order come from it; -1: none.
    int spiBlockStart = -1;
    std::uint16_t spiBlockEnd = 0;
    std::uint8_t spiBlockGap = 0;
    // A write to bankAddress selects the ROM bank of bankPage (-1: no ROM bank).
    int bankPage = -1;
    std::uint16_t bankAddress = 0;
    std::uint8_t bankMask = 0xFF;
    // Memory Mapper RAM: ports mapperPort to mapperPort+3 select the segments of the four pages.
    bool mapper = false;
    std::uint8_t mapperPort = 0xFC;
    std::uint8_t mapperMask = 0xFF;
    // MegaRAM: four 8 KiB bank registers chosen by A14-A13; OUT on megaramPort/+1 switches to bank
    // switching, IN to RAM writes.
    bool megaram = false;
    std::uint8_t megaramPort = 0x8E;

    void noCache(std::uint16_t base, std::uint32_t size);
    void noCacheWhenSwitched(std::uint16_t base, std::uint32_t size);
};

struct SlotConfig {
    bool expander = false;
    std::array<Rules, 4> rules{};
    // I/O ports the machine forwards to the board while this slot is in use.
    std::vector<std::uint8_t> ioPorts;
    std::string description;
};

// What a probe of the board found: an empty slot has no configuration. The probe writes FFFFh of
// both slots (expander test, afterwards 00h or the old value) and port 40h (switched I/O ID C0h,
// afterwards 00h) and pulses /RESET first.
struct Detection {
    std::array<bool, 2> occupied{};
    std::array<SlotConfig, 2> slots{};
    bool switchedIo = false;
};
[[nodiscard]] Detection detectBoard(Bridge& bridge);

// Slot 1 or 2 of the board as a module slot of the machine, with a read cache of 8 KiB windows: the
// first read of a 256-byte line fetches the whole line, further reads cost no USB transfer. A write
// marks only its byte as stale (RAM of a mapper or MegaRAM takes the value at once); a stale byte is
// read on the board again. Every mapper segment, MegaRAM bank and ROM bank has its own window image.
// Cached reads never reach the board, so a cartridge only sees /M1 in areas without cache.
class BoardSlot final {
public:
    BoardSlot(Bridge& bridge, std::uint8_t number, SlotConfig config);

    [[nodiscard]] Bridge& bridge() noexcept;
    [[nodiscard]] const Bridge& bridge() const noexcept;
    [[nodiscard]] std::uint8_t number() const noexcept;
    [[nodiscard]] const SlotConfig& config() const noexcept;

    // Memory read and write cycle of the CPU.
    [[nodiscard]] std::uint8_t read(std::uint16_t address, bool m1, std::uint32_t& waits);
    void write(std::uint16_t address, std::uint8_t value);
    // Cached byte without a bus cycle, FFh when the cache does not hold it.
    [[nodiscard]] std::uint8_t peek(std::uint16_t address) const noexcept;

    // True when the machine forwards this port to the board while the slot is in use.
    [[nodiscard]] bool claimsPort(std::uint8_t port) const noexcept;
    // I/O cycles of the machine on the board: the slot follows mapper and MegaRAM switching.
    void ioRead(std::uint8_t port);
    void ioWritten(std::uint8_t port, std::uint8_t value);

    // Clears the cache and the switching state, as after power-on of the cartridge.
    void reset();

    // Sector reads served by one spaced block, and those broken off by an unexpected access.
    [[nodiscard]] unsigned long spiBlocks() const noexcept;
    [[nodiscard]] unsigned long spiBlockErrors() const noexcept;

private:
    struct WindowCache {
        std::array<std::uint8_t, WindowSize> data{};
        std::bitset<LinesPerWindow> valid;
        std::bitset<WindowSize> stale;
        std::array<std::uint16_t, LinesPerWindow> staleCount{};
    };

    [[nodiscard]] unsigned subslotOf(unsigned page) const noexcept;
    [[nodiscard]] std::uint32_t imageKey(unsigned window) const noexcept;
    [[nodiscard]] const WindowCache* findImage(unsigned window) const noexcept;
    WindowCache& image(unsigned window);
    [[nodiscard]] bool isUncached(std::uint16_t address) const noexcept;
    bool fetchLine(WindowCache& cache, std::uint16_t address);
    std::uint8_t readCycle(std::uint16_t address, bool m1, std::uint32_t& waits);
    void dropSpiBlock();
    // Byte of a cacheable area through the cache (fetching its line), -1 when it cannot be read that way.
    int cachedByte(std::uint16_t address);
    // Number of bytes the LDI chain around the last instruction fetch will read from address on.
    unsigned ldiChainLength(std::uint16_t address, unsigned limit);

    Bridge& board;
    std::uint8_t slotNumber;
    SlotConfig settings;
    std::map<std::uint32_t, std::unique_ptr<WindowCache>> images;
    std::uint8_t expanderRegister = 0;
    std::array<std::uint8_t, 4> bank{};
    std::array<std::uint8_t, 4> segment{0, 1, 2, 3};
    std::array<std::uint8_t, 4> megaramBank{0, 1, 2, 3};
    bool megaramWrite = false;
    std::array<bool, 4> switchedOn{};
    std::uint16_t lastFetch = 0;
    bool fetched = false;
    std::vector<std::uint8_t> spiBuffer;
    std::size_t spiPosition = 0;
    std::uint16_t spiNext = 0;
    unsigned long spiBlockCount = 0;
    unsigned long spiErrorCount = 0;
};

} // namespace usbslot
