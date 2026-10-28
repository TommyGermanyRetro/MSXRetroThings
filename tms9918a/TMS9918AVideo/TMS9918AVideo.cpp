// C interface of the shared VIDEOREC library for the TMS9918A emulator (Visual Basic .NET).
// One recorder per handle; error texts are copied into a buffer of the caller.

#include <VIDEOREC/VideoRecorder.hpp>

#include <cstdint>
#include <cwchar>
#include <new>
#include <string>

namespace {

void copyError(const std::wstring& error, wchar_t* buffer, int length)
{
    if (buffer == nullptr || length <= 0) return;
    wcsncpy_s(buffer, static_cast<std::size_t>(length), error.c_str(), _TRUNCATE);
}

} // namespace

extern "C" {

__declspec(dllexport) void* __cdecl vr_create()
{
    return new (std::nothrow) videorec::VideoRecorder();
}

__declspec(dllexport) void __cdecl vr_destroy(void* recorder)
{
    delete static_cast<videorec::VideoRecorder*>(recorder);
}

// Opens the MP4 file; 1 on success, 0 with a message in error.
__declspec(dllexport) int __cdecl vr_start(void* recorder, const wchar_t* path, std::uint32_t width, std::uint32_t height,
                                          std::uint32_t frameRateNumerator, std::uint32_t frameRateDenominator,
                                          std::uint32_t sampleRate, wchar_t* error, int errorLength)
{
    videorec::Settings settings;
    settings.path = path;
    settings.width = width;
    settings.height = height;
    settings.frameRateNumerator = frameRateNumerator;
    settings.frameRateDenominator = frameRateDenominator;
    settings.sampleRate = sampleRate;
    settings.channels = 1;
    std::wstring message;
    if (static_cast<videorec::VideoRecorder*>(recorder)->start(settings, message)) return 1;
    copyError(message, error, errorLength);
    return 0;
}

// One picture of width x height pixels 0x00RRGGBB, stride in pixels.
__declspec(dllexport) void __cdecl vr_frame(void* recorder, const std::uint32_t* pixels, std::uint32_t stride)
{
    static_cast<videorec::VideoRecorder*>(recorder)->addFrame(pixels, stride);
}

// Sound samples, 16-bit signed mono.
__declspec(dllexport) void __cdecl vr_audio(void* recorder, const std::int16_t* samples, std::uint32_t count)
{
    static_cast<videorec::VideoRecorder*>(recorder)->addAudio(samples, count);
}

// Finishes the file; 1 on success, 0 with a message in error.
__declspec(dllexport) int __cdecl vr_stop(void* recorder, wchar_t* error, int errorLength)
{
    std::wstring message;
    if (static_cast<videorec::VideoRecorder*>(recorder)->stop(message)) return 1;
    copyError(message, error, errorLength);
    return 0;
}

__declspec(dllexport) int __cdecl vr_recording(void* recorder)
{
    return static_cast<videorec::VideoRecorder*>(recorder)->recording() ? 1 : 0;
}

__declspec(dllexport) double __cdecl vr_seconds(void* recorder)
{
    return static_cast<videorec::VideoRecorder*>(recorder)->seconds();
}

} // extern "C"
