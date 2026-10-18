#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace svi328 {

struct CassetteState {
    bool inserted = false;
    bool motorOn = false;
    bool inputHigh = true;
    bool atEnd = false;
    std::uint64_t positionTicks = 0;
    bool recording = false;
    bool modified = false;
    bool recordingError = false;
    // Playback: data bytes already passed on tape, excluding block headers. Recording: CAS bytes captured.
    std::size_t bytePosition = 0;
    std::size_t byteCount = 0;
    std::size_t blockIndex = 0;
    std::size_t blockCount = 0;
};

// SVI-318/328 tape: a CAS image is a sequence of blocks, each starting with 16 x 55h and 7Fh.
// Playback signal at 4800 samples per second: a 1 bit is one 2400 Hz period, a 0 bit one
// 1200 Hz period, most significant bit first. Each block is 0.25 s of silence, one 1 bit,
// 199 x 55h, 7Fh, then every data byte preceded by a 0 bit.
// Recording takes the write signal while the motor runs: two short half periods are a 1 bit,
// two long ones a 0 bit; a block starts after 55h 7Fh and ends when the motor stops.
class Cassette final {
public:
    static constexpr std::array<std::uint8_t, 17> BlockHeader = {
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x7F};
    static constexpr std::uint32_t SampleRate = 4800;

    [[nodiscard]] bool insert(const std::vector<std::uint8_t>& image)
    {
        if (recordingActive) return false;
        if (image.size() < BlockHeader.size() ||
            !std::equal(BlockHeader.begin(), BlockHeader.end(), image.begin())) return false;
        std::vector<Block> parsed;
        std::size_t offset = 0;
        while (offset < image.size()) {
            const auto next = std::search(image.begin() + static_cast<std::ptrdiff_t>(offset + BlockHeader.size()),
                                          image.end(), BlockHeader.begin(), BlockHeader.end());
            const auto end = static_cast<std::size_t>(next - image.begin());
            parsed.push_back({offset + BlockHeader.size(), end - offset - BlockHeader.size(), 0, 0});
            offset = end;
        }
        std::vector<std::uint8_t> wave;
        for (auto& block : parsed) {
            block.firstSample = wave.size();
            wave.insert(wave.end(), SilenceSamples, 1);
            writeBit(wave, true);
            for (unsigned i = 0; i < LeaderBytes; ++i) writeByte(wave, 0x55);
            writeByte(wave, 0x7F);
            block.dataSample = wave.size();
            for (std::size_t i = 0; i < block.size; ++i) {
                writeBit(wave, false);
                writeByte(wave, image[block.offset + i]);
            }
        }
        bytes = image;
        blocks = std::move(parsed);
        samples = std::move(wave);
        modified = false;
        recordingFailed = false;
        rewind();
        return true;
    }

    void eject() noexcept
    {
        bytes.clear();
        blocks.clear();
        samples.clear();
        recordingActive = false;
        recordingFailed = false;
        modified = false;
        rewind();
    }

    void rewind() noexcept
    {
        sampleIndex = 0;
        samplePhase = 0;
        positionTicks = 0;
    }

    void startRecording()
    {
        eject();
        recordingActive = true;
        modified = true;
        pulses.clear();
        haveEdge = false;
        elapsed = 0;
    }

    // Ends the recording; on success the recorded image is inserted for playback.
    [[nodiscard]] bool stopRecording()
    {
        if (!recordingActive) return false;
        if (motorOn) finishSegment();
        recordingActive = false;
        if (recordingFailed || bytes.empty()) {
            recordingFailed = true;
            return false;
        }
        const auto recorded = bytes;
        if (!insert(recorded)) return false;
        modified = true;
        return true;
    }

    void markSaved() noexcept { modified = false; }

    void setPins(bool motor, bool output)
    {
        if (recordingActive) {
            if (motor && !motorOn) {
                pulses.clear();
                haveEdge = false;
                elapsed = 0;
            }
            if (!motor && motorOn) finishSegment();
            if (motor && motorOn && output != outputHigh) {
                if (haveEdge) pulses.push_back(elapsed);
                haveEdge = true;
                elapsed = 0;
            }
        }
        motorOn = motor;
        outputHigh = output;
    }

    void tick(std::uint32_t cpuClockHz) noexcept
    {
        if (recordingActive) {
            if (motorOn) {
                ++elapsed;
                ++positionTicks;
            }
            return;
        }
        if (!motorOn || sampleIndex >= samples.size()) return;
        ++positionTicks;
        samplePhase += SampleRate;
        if (samplePhase < cpuClockHz) return;
        samplePhase -= cpuClockHz;
        ++sampleIndex;
    }

    [[nodiscard]] bool inputHigh() const noexcept
    {
        if (recordingActive || !motorOn || sampleIndex >= samples.size()) return true;
        return samples[static_cast<std::size_t>(sampleIndex)] != 0;
    }

    [[nodiscard]] const std::vector<std::uint8_t>& image() const noexcept { return bytes; }

    [[nodiscard]] CassetteState state() const noexcept
    {
        CassetteState result;
        result.inserted = !blocks.empty() || recordingActive;
        result.motorOn = motorOn;
        result.inputHigh = inputHigh();
        result.positionTicks = positionTicks;
        result.recording = recordingActive;
        result.modified = modified;
        result.recordingError = recordingFailed;
        if (recordingActive) {
            result.bytePosition = bytes.size();
            return result;
        }
        result.atEnd = !blocks.empty() && sampleIndex >= samples.size();
        result.blockCount = blocks.size();
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            const auto& block = blocks[index];
            result.byteCount += block.size;
            if (sampleIndex >= block.firstSample) result.blockIndex = index;
            if (sampleIndex >= block.dataSample) {
                // A data byte takes 9 bits of 2 or 4 samples, on average about 27 samples.
                result.bytePosition += std::min<std::size_t>(
                    block.size, static_cast<std::size_t>((sampleIndex - block.dataSample) / 27));
            }
        }
        return result;
    }

private:
    struct Block {
        std::size_t offset;
        std::size_t size;
        std::size_t firstSample;
        std::size_t dataSample;
    };

    static constexpr std::size_t SilenceSamples = 1200;
    static constexpr unsigned LeaderBytes = 199;
    // Half periods of the recorded signal: about 650 T-states for a 1 bit, 1300 or more for a 0 bit.
    static constexpr std::uint64_t LongHalfPeriodTicks = 975;
    static constexpr std::size_t MaxBytes = 16 * 1024 * 1024;

    static void writeBit(std::vector<std::uint8_t>& wave, bool bit)
    {
        const std::size_t count = bit ? 1 : 2;
        wave.insert(wave.end(), count, 1);
        wave.insert(wave.end(), count, 0);
    }

    static void writeByte(std::vector<std::uint8_t>& wave, std::uint8_t value)
    {
        for (int bit = 7; bit >= 0; --bit) writeBit(wave, ((value >> bit) & 1) != 0);
    }

    // Decodes the half periods of one motor-on segment into one CAS block.
    void finishSegment()
    {
        std::vector<bool> bits;
        for (std::size_t index = 0; index + 1 < pulses.size();) {
            const bool firstLong = pulses[index] >= LongHalfPeriodTicks;
            const bool secondLong = pulses[index + 1] >= LongHalfPeriodTicks;
            if (firstLong == secondLong) {
                bits.push_back(!firstLong);
                index += 2;
            } else {
                ++index;
            }
        }
        pulses.clear();
        haveEdge = false;
        elapsed = 0;
        if (bits.empty()) return;

        std::uint16_t window = 0;
        std::size_t position = 0;
        bool synced = false;
        for (; position < bits.size(); ++position) {
            window = static_cast<std::uint16_t>((window << 1) | (bits[position] ? 1 : 0));
            if (position >= 15 && window == 0x557F) {
                synced = true;
                ++position;
                break;
            }
        }
        if (!synced) {
            recordingFailed = true;
            return;
        }
        std::vector<std::uint8_t> data;
        while (position + 9 <= bits.size()) {
            if (bits[position]) break;
            std::uint8_t value = 0;
            for (unsigned bit = 1; bit <= 8; ++bit) {
                value = static_cast<std::uint8_t>((value << 1) | (bits[position + bit] ? 1 : 0));
            }
            data.push_back(value);
            position += 9;
        }
        if (bytes.size() + BlockHeader.size() + data.size() > MaxBytes) {
            recordingFailed = true;
            return;
        }
        bytes.insert(bytes.end(), BlockHeader.begin(), BlockHeader.end());
        bytes.insert(bytes.end(), data.begin(), data.end());
    }

    std::vector<std::uint8_t> bytes;
    std::vector<Block> blocks;
    std::vector<std::uint8_t> samples;
    std::uint64_t sampleIndex = 0;
    std::uint32_t samplePhase = 0;
    std::uint64_t positionTicks = 0;
    bool motorOn = false;
    bool outputHigh = false;
    bool recordingActive = false;
    bool recordingFailed = false;
    bool modified = false;
    std::vector<std::uint64_t> pulses;
    std::uint64_t elapsed = 0;
    bool haveEdge = false;
};

// File name for a recording, taken from the name in its first block (10 type bytes, 6 name characters).
[[nodiscard]] inline std::wstring suggestedCassetteFileName(const std::vector<std::uint8_t>& image)
{
    constexpr std::size_t header = Cassette::BlockHeader.size();
    if (image.size() < header + 16) return L"Aufnahme.cas";
    const auto type = image[header];
    if ((type != 0xD3 && type != 0xD0 && type != 0xEA) ||
        !std::all_of(image.begin() + header, image.begin() + header + 10,
                     [type](std::uint8_t value) { return value == type; })) {
        return L"Aufnahme.cas";
    }
    std::wstring name;
    for (std::size_t index = header + 10; index < header + 16 && image[index] != 0; ++index) {
        const auto character = image[index];
        const bool invalid = character < 32 || character >= 127 ||
            std::string("<>:\"/\\|?*").find(static_cast<char>(character)) != std::string::npos;
        name.push_back(invalid ? L'_' : static_cast<wchar_t>(character));
    }
    while (!name.empty() && (name.back() == L' ' || name.back() == L'.')) name.pop_back();
    if (name.empty()) name = L"Aufnahme";
    return name + L".cas";
}

} // namespace svi328
