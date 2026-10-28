#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace usbslot {

// Connection to the USB2SLOT board: a Teensy 4.1 that runs real Z80 bus cycles on the cartridges in
// its slots 1 and 2. The serial implementation speaks the binary protocol of the board (version 2);
// tests replace it.
class Transport {
public:
    virtual ~Transport() = default;

    // Memory read cycle in slot 1 or 2 of the board; waits counts the clocks the cartridge added
    // with /WAIT.
    [[nodiscard]] virtual bool memRead(std::uint8_t slot, std::uint16_t address, bool m1, std::uint8_t& data,
                                       std::uint32_t& waits) = 0;
    // Memory write cycle; the board gets it together with the next cycle that has an answer.
    [[nodiscard]] virtual bool memWrite(std::uint8_t slot, std::uint16_t address, std::uint8_t value) = 0;
    // Consecutive memory read cycles from the address on; data holds count bytes afterwards, or nothing
    // when the board refused the block. False only when the transfer failed.
    [[nodiscard]] virtual bool memReadBlock(std::uint8_t slot, std::uint16_t address, std::uint16_t count,
                                            std::vector<std::uint8_t>& data) = 0;
    // The same with gap clocks from the start of one read cycle to the next (protocol 3, for ports that
    // need time per byte). A board without it refuses: data stays empty, the result is true.
    [[nodiscard]] virtual bool memReadBlockSpaced(std::uint8_t slot, std::uint16_t address, std::uint16_t count,
                                                  std::uint8_t gap, std::vector<std::uint8_t>& data)
    {
        (void)slot;
        (void)address;
        (void)count;
        (void)gap;
        data.clear();
        return true;
    }
    [[nodiscard]] virtual bool ioRead(std::uint8_t port, std::uint8_t& data, std::uint32_t& waits) = 0;
    // I/O write cycle, sent at once and answered with the wait clocks.
    [[nodiscard]] virtual bool ioWrite(std::uint8_t port, std::uint8_t value, std::uint32_t& waits) = 0;
    // /RESET pulse of the board.
    [[nodiscard]] virtual bool reset() = 0;
    // Heartbeat; without it the board switches the cartridges off after its time limit.
    [[nodiscard]] virtual bool run() = 0;
    // Waits until all earlier cycles ran and reports whether the board runs (not switched off) and the
    // level of /INT (0 = active).
    [[nodiscard]] virtual bool sync(bool& running, std::uint8_t& intLevel) = 0;
    // Takes the reports that arrived without waiting; false only when the transfer failed.
    [[nodiscard]] virtual bool poll() = 0;
    // Oldest /INT change reported by the board (0 = active), false when none is waiting. The reports
    // arrive with every answer.
    [[nodiscard]] virtual bool popInterrupt(std::uint8_t& level) = 0;
    [[nodiscard]] virtual std::string error() const = 0;
};

// Opens the board on a COM port ("COM16"), switches it to the binary protocol and checks the
// protocol version. Returns nullptr and an error text when that fails.
[[nodiscard]] std::unique_ptr<Transport> openSerial(const std::string& port, std::string& error);

// The board as the machine sees it. After a failed transfer the connection counts as lost: reads
// return FFh and nothing is sent any more. Bus cycles carry a heartbeat along when the last one is
// more than 20 ms old, so that long stretches of board cycles do not let the board switch off.
// When the last heartbeat is more than 100 ms old (the emulation stood still), the next one asks the
// board first whether it switched the cartridges off in the meantime.
class Bridge final {
public:
    explicit Bridge(std::unique_ptr<Transport> transport);

    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

    [[nodiscard]] std::uint8_t readMemory(std::uint8_t slot, std::uint16_t address, bool m1, std::uint32_t& waits);
    void writeMemory(std::uint8_t slot, std::uint16_t address, std::uint8_t value);
    // Reads count bytes from the address on; false (and nothing read) when the board refused the block
    // or the transfer failed.
    [[nodiscard]] bool readBlock(std::uint8_t slot, std::uint16_t address, std::uint16_t count,
                                 std::vector<std::uint8_t>& data);
    // The same with gap clocks between the read cycles; false also when the board cannot do it.
    [[nodiscard]] bool readBlockSpaced(std::uint8_t slot, std::uint16_t address, std::uint16_t count,
                                       std::uint8_t gap, std::vector<std::uint8_t>& data);
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port, std::uint32_t& waits);
    void writeIo(std::uint8_t port, std::uint8_t value, std::uint32_t& waits);

    // Wakes the board (it may have switched off while no heartbeat came), pulses /RESET and takes the
    // level of /INT afterwards.
    void reset();
    // Heartbeat; also takes /INT reports that arrived meanwhile.
    void heartbeat();

    // /INT of the cartridges, one wired-OR line for both slots of the board.
    [[nodiscard]] bool interruptRequest() const noexcept;
    // True once after the board switched the cartridges off while the emulation stood still; they have
    // lost their state then, and the machine should be reset like after switching it on again.
    [[nodiscard]] bool takeSwitchedOff() noexcept;

private:
    void fail();
    void keepAlive();
    void sendRun(std::chrono::steady_clock::time_point now);
    void takeInterrupts();

    std::unique_ptr<Transport> transport;
    bool failed = false;
    std::string error;
    std::chrono::steady_clock::time_point lastRun;
    bool intLow = false;
    bool switchedOff = false;
};

// Finds the first T-state of a bus cycle. The Z80 repeats a T-state with the same request while
// /WAIT is low, and a cartridge on the board must see each cycle only once.
class CycleTracker final {
public:
    // True when a request is active now and was not active at the same address in the T-state before.
    [[nodiscard]] bool begins(bool active, std::uint16_t address) noexcept;

private:
    bool lastActive = false;
    std::uint16_t lastAddress = 0;
};

} // namespace usbslot
