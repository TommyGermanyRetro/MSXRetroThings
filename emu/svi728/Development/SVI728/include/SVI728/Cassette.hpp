#pragma once

#include "SVI728/CassetteRecorder.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace svi728 {

[[nodiscard]] inline std::wstring suggestedCassetteFileName(
    const std::vector<std::uint8_t>& image)
{
    static constexpr std::array<std::uint8_t, 8> marker =
        {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74};
    if (image.size() < 24 || !std::equal(marker.begin(), marker.end(), image.begin())) {
        return L"Aufnahme.cas";
    }
    const auto type = image[8];
    if ((type != 0xD3 && type != 0xD0 && type != 0xEA) ||
        !std::all_of(image.begin() + 8, image.begin() + 18,
                     [type](std::uint8_t value) { return value == type; })) {
        return L"Aufnahme.cas";
    }
    std::wstring name;
    for (std::size_t index = 18; index < 24 && image[index] != 0; ++index) {
        const auto character = image[index];
        const bool invalid = character < 32 || character >= 127 ||
            std::string("<>:\"/\\|?*").find(static_cast<char>(character)) != std::string::npos;
        name.push_back(invalid ? L'_' : static_cast<wchar_t>(character));
    }
    while (!name.empty() && (name.back() == L' ' || name.back() == L'.')) name.pop_back();
    if (name.empty()) name = L"Aufnahme";
    std::wstring upper = name.substr(0, name.find(L'.'));
    for (auto& character : upper) {
        if (character >= L'a' && character <= L'z') character -= L'a' - L'A';
    }
    if (upper == L"CON" || upper == L"PRN" || upper == L"AUX" || upper == L"NUL" ||
        (upper.size() == 4 && (upper.substr(0, 3) == L"COM" || upper.substr(0, 3) == L"LPT") &&
         upper[3] >= L'1' && upper[3] <= L'9')) name.insert(name.begin(), L'_');
    return name + L".cas";
}

struct CassetteState {
    bool inserted = false;
    bool motorOn = false;
    bool inputHigh = true;
    bool outputHigh = false;
    bool atEnd = false;
    std::uint64_t positionTicks = 0;
    bool recording = false;
    bool modified = false;
    bool recordingError = false;
    // Playback: data bytes already passed on tape, excluding CAS markers. Recording: CAS bytes captured.
    std::size_t bytePosition = 0;
    std::size_t byteCount = 0;
    std::size_t blockIndex = 0;
    std::size_t blockCount = 0;
};

class Cassette final {
public:
    [[nodiscard]] bool insert(const std::vector<std::uint8_t>& image)
    {
        if (recorder.recording()) return false;
        if (image.size() < Marker.size() ||
            !std::equal(Marker.begin(), Marker.end(), image.begin())) return false;
        std::vector<Block> parsed;
        for (std::size_t offset = 0; offset + Marker.size() <= image.size(); offset += 8) {
            if (!std::equal(Marker.begin(), Marker.end(), image.begin() + offset)) continue;
            if (!parsed.empty()) parsed.back().size = offset - parsed.back().offset;
            parsed.push_back({offset + Marker.size(), 0, false});
        }
        parsed.back().size = image.size() - parsed.back().offset;
        for (auto& block : parsed) {
            if (block.size == 0) return false;
            const auto type = image[block.offset];
            block.longHeader = &block == &parsed.front() ||
                (block.size >= 10 && (type == 0xD0 || type == 0xD3 || type == 0xEA) &&
                 std::all_of(image.begin() + block.offset,
                             image.begin() + block.offset + 10,
                             [type](std::uint8_t value) { return value == type; }));
        }
        bytes = image;
        blocks = std::move(parsed);
        recorder = CassetteRecorder{};
        modified = false;
        rewind();
        return true;
    }

    void eject() noexcept
    {
        bytes.clear();
        blocks.clear();
        recorder = CassetteRecorder{};
        modified = false;
        rewind();
    }

    void rewind() noexcept
    {
        if (recorder.recording()) return;
        blockIndex = 0;
        sampleIndex = 0;
        samplePhase = 0;
        positionTicks = 0;
    }

    void startRecording()
    {
        eject();
        recorder.start();
        recorder.setPins(motorOn, outputHigh);
        modified = true;
    }

    [[nodiscard]] bool stopRecording()
    {
        recorder.stop();
        if (recorder.error() || recorder.image().empty()) return false;
        const auto recorded = recorder.image();
        if (!insert(recorded)) return false;
        modified = true;
        return true;
    }

    [[nodiscard]] const std::vector<std::uint8_t>& image() const noexcept
    {
        return recorder.recording() || recorder.error() ? recorder.image() : bytes;
    }

    void markSaved() noexcept { modified = false; }

    void setPins(bool motor, bool output)
    {
        recorder.setPins(motor, output);
        motorOn = motor;
        outputHigh = output;
    }

    void tick(std::uint32_t cpuClockHz) noexcept
    {
        recorder.tick(cpuClockHz);
        if (recorder.recording()) {
            if (motorOn) ++positionTicks;
            return;
        }
        if (!motorOn || blockIndex >= blocks.size()) return;
        ++positionTicks;
        samplePhase += SampleRate;
        if (samplePhase < cpuClockHz) return;
        samplePhase -= cpuClockHz;
        if (++sampleIndex >= blockSamples(blocks[blockIndex])) {
            sampleIndex = 0;
            ++blockIndex;
        }
    }

    [[nodiscard]] CassetteState state() const noexcept
    {
        CassetteState result{!blocks.empty() || recorder.recording() || recorder.error(),
            motorOn, inputHigh(), outputHigh,
            !blocks.empty() && blockIndex == blocks.size(), positionTicks,
            recorder.recording(), modified, recorder.error()};
        if (recorder.recording() || recorder.error()) {
            result.bytePosition = recorder.image().size();
            return result;
        }
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            result.byteCount += blocks[index].size;
            if (index < blockIndex) result.bytePosition += blocks[index].size;
        }
        if (blockIndex < blocks.size()) {
            const auto& block = blocks[blockIndex];
            const auto signalStart = silenceSamples(block) + leaderSamples(block);
            if (sampleIndex > signalStart) {
                result.bytePosition += std::min<std::size_t>(
                    block.size, static_cast<std::size_t>((sampleIndex - signalStart) / 44));
            }
        }
        result.blockIndex = blockIndex;
        result.blockCount = blocks.size();
        return result;
    }

    [[nodiscard]] bool inputHigh() const noexcept
    {
        if (recorder.recording() || !motorOn || blockIndex >= blocks.size()) return true;
        const auto& block = blocks[blockIndex];
        const auto silence = silenceSamples(block);
        if (sampleIndex < silence) return true;
        const auto signalSample = sampleIndex - silence;
        const auto leader = leaderSamples(block);
        if (signalSample < leader) return (signalSample & 1) == 0;
        const auto dataSample = signalSample - leader;
        const auto byteIndex = static_cast<std::size_t>(dataSample / 44);
        const auto bitIndex = (dataSample % 44) / 4;
        const auto quarter = dataSample % 4;
        const bool bit = bitIndex >= 9 ||
            (bitIndex > 0 && (bytes[block.offset + byteIndex] & (1u << (bitIndex - 1))) != 0);
        return bit ? (quarter & 1) == 0 : quarter < 2;
    }

private:
    struct Block {
        std::size_t offset;
        std::size_t size;
        bool longHeader;
    };

    static constexpr std::array<std::uint8_t, 8> Marker =
        {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74};
    static constexpr std::uint32_t SampleRate = 4800;

    [[nodiscard]] static std::uint64_t silenceSamples(const Block& block) noexcept
    {
        return block.longHeader ? 9600 : 4800;
    }

    [[nodiscard]] static std::uint64_t leaderSamples(const Block& block) noexcept
    {
        return block.longHeader ? 32000 : 8000;
    }

    [[nodiscard]] static std::uint64_t blockSamples(const Block& block) noexcept
    {
        return silenceSamples(block) + leaderSamples(block) + block.size * 44ull;
    }

    std::vector<std::uint8_t> bytes;
    std::vector<Block> blocks;
    std::size_t blockIndex = 0;
    std::uint64_t sampleIndex = 0;
    std::uint32_t samplePhase = 0;
    std::uint64_t positionTicks = 0;
    bool motorOn = false;
    bool outputHigh = false;
    CassetteRecorder recorder;
    bool modified = false;
};

} // namespace svi728