#include "VIDEOREC/VideoRecorder.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

constexpr std::uint32_t Width = 1124;
constexpr std::uint32_t Height = 868;
constexpr std::uint32_t FieldRateNumerator = 21477270;
constexpr std::uint32_t FieldRateDenominator = 428184;
constexpr std::uint32_t SampleRate = 44100;
constexpr int Fields = 100;

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// Left half red, right half blue, a white bar moving down field by field.
void drawField(std::vector<std::uint32_t>& pixels, int field)
{
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            pixels[static_cast<std::size_t>(y) * Width + x] = x < Width / 2 ? 0x00E02020 : 0x002020E0;
        }
    }
    const auto bar = static_cast<std::uint32_t>(field * 8) % (Height - 16);
    for (std::uint32_t y = bar; y < bar + 16; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) pixels[static_cast<std::size_t>(y) * Width + x] = 0x00FFFFFF;
    }
}

bool record(const std::filesystem::path& path)
{
    videorec::VideoRecorder recorder;
    videorec::Settings settings;
    settings.path = path;
    settings.width = Width;
    settings.height = Height;
    settings.frameRateNumerator = FieldRateNumerator;
    settings.frameRateDenominator = FieldRateDenominator;
    settings.sampleRate = SampleRate;
    std::wstring error;
    if (!recorder.start(settings, error)) {
        std::wcerr << error << L'\n';
        return fail("start() failed.");
    }
    if (!recorder.recording()) return fail("recording() must be true after start().");
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(Width) * Height);
    std::vector<std::int16_t> samples;
    double phase = 0.0;
    std::uint64_t sampleTotal = 0;
    for (int field = 0; field < Fields; ++field) {
        drawField(pixels, field);
        recorder.addFrame(pixels.data(), Width);
        const auto target = static_cast<std::uint64_t>(field + 1) * SampleRate * FieldRateDenominator / FieldRateNumerator;
        samples.resize(static_cast<std::size_t>(target - sampleTotal));
        for (auto& sample : samples) {
            sample = static_cast<std::int16_t>(std::sin(phase) * 12000.0);
            phase += 2.0 * 3.14159265358979 * 440.0 / SampleRate;
        }
        sampleTotal = target;
        recorder.addAudio(samples.data(), samples.size());
    }
    if (std::fabs(recorder.seconds() - Fields * static_cast<double>(FieldRateDenominator) / FieldRateNumerator) > 1e-6) {
        return fail("seconds() must follow the emulated time.");
    }
    if (!recorder.stop(error)) {
        std::wcerr << error << L'\n';
        return fail("stop() failed.");
    }
    if (recorder.recording()) return fail("recording() must be false after stop().");
    return true;
}

bool verify(const std::filesystem::path& path)
{
    IMFAttributes* attributes = nullptr;
    IMFSourceReader* reader = nullptr;
    MFCreateAttributes(&attributes, 1);
    attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    HRESULT result = MFCreateSourceReaderFromURL(path.c_str(), attributes, &reader);
    attributes->Release();
    if (FAILED(result)) return fail("The MP4 file cannot be opened.");
    bool ok = true;

    PROPVARIANT duration;
    PropVariantInit(&duration);
    reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &duration);
    const double seconds = static_cast<double>(duration.uhVal.QuadPart) / 1e7;
    PropVariantClear(&duration);
    const double expected = Fields * static_cast<double>(FieldRateDenominator) / FieldRateNumerator;
    if (std::fabs(seconds - expected) > 0.1) ok = fail("The duration does not match the recorded fields.");

    IMFMediaType* native = nullptr;
    GUID subtype{};
    if (FAILED(reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &native))) {
        ok = fail("The file has no video stream.");
    } else {
        UINT32 width = 0;
        UINT32 height = 0;
        native->GetGUID(MF_MT_SUBTYPE, &subtype);
        MFGetAttributeSize(native, MF_MT_FRAME_SIZE, &width, &height);
        if (subtype != MFVideoFormat_H264) ok = fail("The video stream must be H.264.");
        if (width != Width || height != Height) ok = fail("The picture size must stay unchanged.");
        native->Release();
    }
    if (FAILED(reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &native))) {
        ok = fail("The file has no sound stream.");
    } else {
        native->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (subtype != MFAudioFormat_AAC) ok = fail("The sound stream must be AAC.");
        native->Release();
    }

    // First picture decoded to RGB32: left red, right blue.
    IMFMediaType* rgb = nullptr;
    MFCreateMediaType(&rgb);
    rgb->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    rgb->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    result = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, rgb);
    rgb->Release();
    IMFSample* sample = nullptr;
    DWORD flags = 0;
    LONGLONG time = 0;
    if (SUCCEEDED(result)) {
        result = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, &time, &sample);
    }
    if (FAILED(result) || !sample) {
        ok = fail("The first picture cannot be decoded.");
    } else {
        IMFMediaBuffer* buffer = nullptr;
        sample->ConvertToContiguousBuffer(&buffer);
        BYTE* data = nullptr;
        DWORD length = 0;
        buffer->Lock(&data, nullptr, &length);
        // The decoder pads the picture to whole macroblocks; the row length comes from the decoded format.
        UINT32 stride = Width;
        UINT32 rows = Height;
        IMFMediaType* decoded = nullptr;
        if (SUCCEEDED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &decoded))) {
            MFGetAttributeSize(decoded, MF_MT_FRAME_SIZE, &stride, &rows);
            decoded->Release();
        }
        const auto pixel = [&](std::uint32_t x, std::uint32_t y) {
            return reinterpret_cast<const std::uint32_t*>(data)[static_cast<std::size_t>(y) * stride + x] & 0xFFFFFF;
        };
        const auto close = [](std::uint32_t value, std::uint32_t expectedValue) {
            for (int shift = 0; shift < 24; shift += 8) {
                const int a = static_cast<int>((value >> shift) & 0xFF);
                const int b = static_cast<int>((expectedValue >> shift) & 0xFF);
                if (std::abs(a - b) > 24) return false;
            }
            return true;
        };
        if (stride < Width || rows < Height || length < stride * rows * 4) {
            ok = fail("The decoded picture is too small.");
        } else {
            // Rows may come bottom-up from the video processor; both halves are uniform in y here.
            if (!close(pixel(Width / 4, Height / 2), 0xE02020)) ok = fail("The left half must stay red.");
            if (!close(pixel(Width * 3 / 4, Height / 2), 0x2020E0)) ok = fail("The right half must stay blue.");
        }
        buffer->Unlock();
        buffer->Release();
        sample->Release();
    }
    reader->Release();
    return ok;
}

bool refusePass()
{
    videorec::VideoRecorder recorder;
    videorec::Settings settings;
    settings.path = std::filesystem::temp_directory_path() / L"videorec_refuse.mp4";
    settings.width = 101;
    settings.height = 100;
    std::wstring error;
    if (recorder.start(settings, error) || error.empty()) return fail("An odd picture size must be refused.");
    if (!recorder.stop(error)) return fail("stop() without a recording must succeed.");
    return true;
}

} // namespace

int main()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
    const auto path = std::filesystem::temp_directory_path() / L"videorec_test.mp4";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    const bool ok = refusePass() && record(path) && verify(path);
    std::filesystem::remove(path, ignored);
    MFShutdown();
    CoUninitialize();
    if (!ok) return EXIT_FAILURE;
    std::cout << "VIDEOREC passed.\n";
    return EXIT_SUCCESS;
}
