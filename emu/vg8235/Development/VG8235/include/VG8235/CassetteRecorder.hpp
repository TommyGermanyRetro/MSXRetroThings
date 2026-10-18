#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace vg8235 {

class CassetteRecorder final {
public:
    void start()
    {
        *this = CassetteRecorder{};
        active = true;
    }

    void tick(std::uint32_t clockHz) noexcept
    {
        clock = clockHz;
        if (active && motorOn) ++elapsed;
    }

    void setPins(bool motor, bool output)
    {
        if (active) {
            if (motorOn && !motor) finishBurst();
            if (!motorOn && motor) {
                pulses.clear();
                elapsed = 0;
                haveEdge = false;
            }
            if (motor && motorOn && output != outputHigh) {
                if (haveEdge && pulses.size() < MaxPulses) pulses.push_back(elapsed);
                else if (pulses.size() >= MaxPulses) failed = true;
                haveEdge = true;
                elapsed = 0;
            }
        }
        motorOn = motor;
        outputHigh = output;
    }

    void stop()
    {
        if (active && motorOn) finishBurst();
        if (active && bytes.empty()) failed = true;
        active = false;
    }

    [[nodiscard]] bool recording() const noexcept { return active; }
    [[nodiscard]] bool error() const noexcept { return failed; }
    [[nodiscard]] const std::vector<std::uint8_t>& image() const noexcept { return bytes; }

private:
    static constexpr std::array<std::uint8_t, 8> Marker =
        {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74};
    static constexpr std::size_t MaxPulses = 8 * 1024 * 1024;
    static constexpr std::size_t MaxBytes = 16 * 1024 * 1024;

    [[nodiscard]] static bool near(std::uint64_t duration, std::uint64_t expected) noexcept
    {
        return duration * 4 >= expected * 3 && duration * 4 <= expected * 5;
    }

    [[nodiscard]] bool findLeader(std::size_t& index, std::uint64_t& shortPulse) const
    {
        while (index + 64 <= pulses.size()) {
            const auto candidate = pulses[index];
            if (candidate != 0 && candidate * 1000 <= clock && candidate * 20000 >= clock) {
                std::uint64_t sum = 0;
                std::size_t count = 0;
                while (count < 64 && near(pulses[index + count], candidate)) {
                    sum += pulses[index + count];
                    ++count;
                }
                if (count == 64) {
                    shortPulse = sum / 64;
                    index += 64;
                    return true;
                }
            }
            ++index;
        }
        return false;
    }

    [[nodiscard]] bool readBit(std::size_t& index, std::uint64_t shortPulse, bool& bit) const
    {
        if (index >= pulses.size()) return false;
        bit = near(pulses[index], shortPulse);
        const auto expected = bit ? shortPulse : shortPulse * 2;
        const std::size_t count = bit ? 4 : 2;
        if (index + count > pulses.size()) return false;
        for (std::size_t pulse = 0; pulse < count; ++pulse) {
            if (!near(pulses[index + pulse], expected)) return false;
        }
        index += count;
        return true;
    }

    void appendByte(std::uint8_t value, bool newBlock)
    {
        if (bytes.size() + 16 >= MaxBytes) {
            failed = true;
            return;
        }
        if (newBlock) {
            while (bytes.size() % 8 != 0) bytes.push_back(0);
            bytes.insert(bytes.end(), Marker.begin(), Marker.end());
        }
        bytes.push_back(value);
        if (bytes.size() >= 16 && bytes.size() % 8 == 0 &&
            std::equal(Marker.begin(), Marker.end(), bytes.end() - 8)) failed = true;
    }

    void finishBurst()
    {
        if (failed) {
            pulses.clear();
            return;
        }
        if (haveEdge && elapsed > 0 && !pulses.empty()) {
            if (elapsed < pulses.back()) failed = true;
            pulses.push_back(std::min(elapsed, pulses.back()));
        }
        std::size_t index = 0;
        std::uint64_t shortPulse = 0;
        bool foundLeader = false;
        while (findLeader(index, shortPulse)) {
            foundLeader = true;
            bool newBlock = true;
            while (index < pulses.size()) {
                std::size_t ones = 0;
                while (index < pulses.size() && near(pulses[index], shortPulse)) {
                    ++index;
                    ++ones;
                }
                if (ones >= 64) newBlock = true;
                if (index == pulses.size()) break;
                if (!near(pulses[index], shortPulse * 2)) {
                    failed = true;
                    break;
                }
                bool bit = false;
                if (!readBit(index, shortPulse, bit) || bit) {
                    failed = true;
                    break;
                }
                std::uint8_t value = 0;
                bool complete = true;
                for (unsigned bitIndex = 0; bitIndex < 10; ++bitIndex) {
                    if (!readBit(index, shortPulse, bit) || (bitIndex >= 8 && !bit)) {
                        complete = false;
                        break;
                    }
                    if (bitIndex < 8 && bit) value |= static_cast<std::uint8_t>(1u << bitIndex);
                }
                if (!complete) {
                    failed = true;
                    break;
                }
                appendByte(value, newBlock);
                newBlock = false;
            }
            if (failed) break;
        }
        if (!foundLeader && pulses.size() >= 8) failed = true;
        pulses.clear();
        elapsed = 0;
        haveEdge = false;
    }

    std::vector<std::uint64_t> pulses;
    std::vector<std::uint8_t> bytes;
    std::uint64_t elapsed = 0;
    std::uint32_t clock = 3579545;
    bool active = false;
    bool failed = false;
    bool motorOn = false;
    bool outputHigh = false;
    bool haveEdge = false;
};

} // namespace vg8235