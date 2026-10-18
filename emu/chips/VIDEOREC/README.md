# VIDEOREC

VIDEOREC is a C++17 static library that records the picture and the sound of an emulator into an MP4 file (H.264 video, AAC sound) with Windows Media Foundation. It is not a chip: the frontends of all four emulators use it for the menu item System > Record video.

## Functionality

- **Container and codecs:** MP4 with H.264 (High profile, progressive, square pixels) and AAC (16 bit, 24 000 bytes per second). Media Foundation may use a hardware encoder.
- **Data rate:** quality-based variable rate (quality 70 of 100) where the encoder offers it, so a still picture costs a few kilobytes per second and a moving one gets what it needs; otherwise the constant rate of `Settings::videoBitrate` (6 Mbit/s by default).
- **Picture:** one picture per emulated field, 32-bit pixels `0x00RRGGBB` as in a Windows DIB, any even width and height. The picture is converted to NV12 (BT.601, studio range, chroma averaged over 2 x 2 pixels) before it reaches the encoder.
- **Sound:** 16-bit signed samples, mono or interleaved channels, at the sample rate of the frontend; collected into blocks of 4096 samples.
- **Time stamps:** taken from the emulated time, not from the wall clock: picture n starts at n x denominator / numerator seconds, sound sample n at n / sample rate. Picture and sound stay in step however fast the emulation runs, and the video plays at the field rate of the machine (for example 21 477 270 / 428 184 = 50.159 Hz on PAL).
- **Worker thread:** conversion and encoding run on a worker thread. At most six pictures wait; when the encoder falls behind, `addFrame()` waits, so no field is lost and the emulation slows down instead.
- **Errors:** a failure of Media Foundation at the start is returned by `start()`; a failure while writing ends the recording (`recording()` turns false) and is returned by `stop()`.

## Interface and Integration

[VideoRecorder.hpp](include/VIDEOREC/VideoRecorder.hpp) exposes `videorec::Settings` (path, width, height, frame rate as numerator and denominator, sample rate, channels, video bit rate) and `videorec::VideoRecorder` with `start()`, `addFrame()`, `addAudio()`, `stop()`, `recording()`, `seconds()` and `settings()`. The library links `mfplat`, `mfreadwrite`, `mfuuid` and `ole32` itself.

```cpp
#include <VIDEOREC/VideoRecorder.hpp>

videorec::VideoRecorder recorder;
videorec::Settings settings;
settings.path = L"capture.mp4";
settings.width = 1124;
settings.height = 868;
settings.frameRateNumerator = 21477270;     // VDP master clock
settings.frameRateDenominator = 428184;     // master clock ticks per PAL field
std::wstring error;
if (recorder.start(settings, error)) {
    recorder.addFrame(pixels.data(), 1124);         // once per field
    recorder.addAudio(samples.data(), samples.size());
    (void)recorder.stop(error);
}
```

## Build and Test

The project is shared by the solutions of all four emulators: [SVI728.sln](../../svi728/Development/SVI728.sln), [VG8235.sln](../../vg8235/Development/VG8235.sln), [SVI318.sln](../../svi318/Development/SVI318.sln) and [SVI328.sln](../../svi328/Development/SVI328.sln). Shared compiler settings are in [Common.props](../Common.props). Each solution builds the project into its own `Build` folder, and its `run_tests.ps1` runs the tests.

| Project | Type | Output |
| --- | --- | --- |
| `videorec` | Static library | `videorec.lib` |
| `videorec_tests` | Console application | `videorec_tests.exe` |

## Tests

[videorec_tests.cpp](tests/videorec_tests.cpp) refuses an odd picture size, then records 100 fields of 1124 x 868 pixels (left half red, right half blue, a moving white bar) at 21 477 270 / 428 184 fields per second with a 440 Hz tone, and reads the file back with the Media Foundation source reader: an H.264 and an AAC stream, the picture size unchanged, the duration of the fields recorded and the colours of the first picture.
