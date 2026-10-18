#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace videorec {

// What to record: the picture size in pixels (even numbers), the frame rate as a fraction (for
// example the field rate of the machine: master clock / master ticks per field) and the sound
// format of the frontend.
struct Settings {
    std::filesystem::path path;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frameRateNumerator = 50;
    std::uint32_t frameRateDenominator = 1;
    std::uint32_t sampleRate = 44100;
    std::uint32_t channels = 1;
    std::uint32_t videoBitrate = 6'000'000;
};

// Records the picture and the sound of an emulator into an MP4 file (H.264 video, AAC sound) with
// Windows Media Foundation. The frontend hands over one picture per emulated field and the sound
// samples of that field; the time stamps follow the emulated time, so picture and sound stay in
// step however fast the emulation runs. Encoding runs on a worker thread; when it falls behind,
// addFrame() waits, so no field is lost.
class VideoRecorder {
public:
    VideoRecorder();
    ~VideoRecorder();
    VideoRecorder(const VideoRecorder&) = delete;
    VideoRecorder& operator=(const VideoRecorder&) = delete;

    // Opens the file and starts the encoder; false with a message when Media Foundation refuses.
    [[nodiscard]] bool start(const Settings& settings, std::wstring& error);

    // One picture: width x height pixels 0x00RRGGBB (Windows 32-bit DIB order), stride in pixels.
    void addFrame(const std::uint32_t* pixels, std::size_t stride);
    // Sound samples, 16-bit signed, interleaved when there is more than one channel.
    void addAudio(const std::int16_t* samples, std::size_t count);

    // Finishes the file; false with a message when writing failed at any point.
    [[nodiscard]] bool stop(std::wstring& error);

    [[nodiscard]] bool recording() const noexcept;
    // Recorded time in seconds (emulated time of the frames written so far).
    [[nodiscard]] double seconds() const noexcept;
    [[nodiscard]] const Settings& settings() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace videorec
