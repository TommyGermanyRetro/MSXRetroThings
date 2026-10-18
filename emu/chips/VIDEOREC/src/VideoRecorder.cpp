#include "VIDEOREC/VideoRecorder.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <codecapi.h>
#include <strmif.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

namespace videorec {
namespace {

constexpr std::size_t MaximumQueuedFrames = 6;
constexpr std::size_t AudioChunkSamples = 4096;
constexpr std::uint32_t AacBytesPerSecond = 24000;
// 0-100, used where the encoder offers quality-based variable rate.
constexpr ULONG VideoQuality = 70;

template <typename T>
void release(T*& pointer) noexcept
{
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}

std::wstring hresultText(const wchar_t* what, HRESULT result)
{
    wchar_t buffer[32];
    swprintf_s(buffer, L" (0x%08X)", static_cast<unsigned>(result));
    return std::wstring(what) + buffer;
}

// BT.601, studio range: the picture is converted to NV12 before it reaches the H.264 encoder.
void convertToNv12(const std::uint32_t* pixels, std::size_t stride, std::uint32_t width, std::uint32_t height,
                   std::uint8_t* luma, std::uint8_t* chroma)
{
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto* row = pixels + static_cast<std::size_t>(y) * stride;
        auto* out = luma + static_cast<std::size_t>(y) * width;
        for (std::uint32_t x = 0; x < width; ++x) {
            const int r = (row[x] >> 16) & 0xFF;
            const int g = (row[x] >> 8) & 0xFF;
            const int b = row[x] & 0xFF;
            out[x] = static_cast<std::uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
        }
    }
    for (std::uint32_t y = 0; y < height; y += 2) {
        const auto* top = pixels + static_cast<std::size_t>(y) * stride;
        const auto* bottom = top + stride;
        auto* out = chroma + static_cast<std::size_t>(y / 2) * width;
        for (std::uint32_t x = 0; x < width; x += 2) {
            int r = 0;
            int g = 0;
            int b = 0;
            for (const std::uint32_t pixel : {top[x], top[x + 1], bottom[x], bottom[x + 1]}) {
                r += (pixel >> 16) & 0xFF;
                g += (pixel >> 8) & 0xFF;
                b += pixel & 0xFF;
            }
            r /= 4;
            g /= 4;
            b /= 4;
            out[x] = static_cast<std::uint8_t>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            out[x + 1] = static_cast<std::uint8_t>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
        }
    }
}

} // namespace

struct VideoRecorder::Impl {
    struct Job {
        bool video = false;
        std::vector<std::uint32_t> pixels;
        std::vector<std::int16_t> samples;
    };

    Settings settings;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable space;
    std::deque<Job> jobs;
    std::size_t queuedFrames = 0;
    bool finishing = false;
    std::atomic<bool> active{false};
    std::atomic<std::uint64_t> framesQueued{0};
    std::wstring failure;
    std::vector<std::int16_t> pendingAudio;

    // worker state
    IMFSinkWriter* writer = nullptr;
    DWORD videoStream = 0;
    DWORD audioStream = 0;
    std::uint64_t framesWritten = 0;
    std::uint64_t samplesWritten = 0;

    // count * numerator / denominator in 100 ns units, split so that long recordings do not overflow.
    static LONGLONG scaledTime(std::uint64_t count, std::uint64_t numerator, std::uint64_t denominator)
    {
        const auto ticks = count * numerator;
        return static_cast<LONGLONG>(ticks / denominator * 10'000'000ULL + ticks % denominator * 10'000'000ULL / denominator);
    }

    LONGLONG frameTime(std::uint64_t frame) const
    {
        return scaledTime(frame, settings.frameRateDenominator, settings.frameRateNumerator);
    }

    LONGLONG audioTime(std::uint64_t sample) const
    {
        return scaledTime(sample, 1, settings.sampleRate);
    }

    // Quality-based variable rate where the encoder offers it: a still picture then costs little and
    // a moving one gets the bits it needs. Encoders without it keep the constant rate.
    void useQualityRate()
    {
        ICodecAPI* codec = nullptr;
        if (FAILED(writer->GetServiceForStream(videoStream, GUID_NULL, IID_PPV_ARGS(&codec)))) return;
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_UI4;
        value.ulVal = eAVEncCommonRateControlMode_Quality;
        if (SUCCEEDED(codec->SetValue(&CODECAPI_AVEncCommonRateControlMode, &value))) {
            value.ulVal = VideoQuality;
            (void)codec->SetValue(&CODECAPI_AVEncCommonQuality, &value);
        }
        codec->Release();
    }

    HRESULT open()
    {
        IMFAttributes* attributes = nullptr;
        IMFMediaType* type = nullptr;
        HRESULT result = MFCreateAttributes(&attributes, 3);
        if (SUCCEEDED(result)) result = attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        if (SUCCEEDED(result)) result = attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
        if (SUCCEEDED(result)) result = attributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
        if (SUCCEEDED(result)) result = MFCreateSinkWriterFromURL(settings.path.c_str(), nullptr, attributes, &writer);
        release(attributes);

        // video: H.264 out, NV12 in
        if (SUCCEEDED(result)) result = MFCreateMediaType(&type);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AVG_BITRATE, settings.videoBitrate);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
        if (SUCCEEDED(result)) result = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, settings.width, settings.height);
        if (SUCCEEDED(result)) result = MFSetAttributeRatio(type, MF_MT_FRAME_RATE, settings.frameRateNumerator, settings.frameRateDenominator);
        if (SUCCEEDED(result)) result = MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (SUCCEEDED(result)) result = writer->AddStream(type, &videoStream);
        release(type);
        if (SUCCEEDED(result)) result = MFCreateMediaType(&type);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_DEFAULT_STRIDE, settings.width);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT601);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);
        if (SUCCEEDED(result)) result = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, settings.width, settings.height);
        if (SUCCEEDED(result)) result = MFSetAttributeRatio(type, MF_MT_FRAME_RATE, settings.frameRateNumerator, settings.frameRateDenominator);
        if (SUCCEEDED(result)) result = MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (SUCCEEDED(result)) result = writer->SetInputMediaType(videoStream, type, nullptr);
        release(type);

        // sound: AAC out, 16-bit PCM in
        if (SUCCEEDED(result)) result = MFCreateMediaType(&type);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, settings.sampleRate);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, settings.channels);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, AacBytesPerSecond);
        if (SUCCEEDED(result)) result = writer->AddStream(type, &audioStream);
        release(type);
        if (SUCCEEDED(result)) result = MFCreateMediaType(&type);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (SUCCEEDED(result)) result = type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, settings.sampleRate);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, settings.channels);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * settings.channels);
        if (SUCCEEDED(result)) result = type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, settings.sampleRate * 2 * settings.channels);
        if (SUCCEEDED(result)) result = writer->SetInputMediaType(audioStream, type, nullptr);
        release(type);

        if (SUCCEEDED(result)) useQualityRate();
        if (SUCCEEDED(result)) result = writer->BeginWriting();
        return result;
    }

    HRESULT writeBuffer(DWORD stream, const void* data, DWORD bytes, LONGLONG time, LONGLONG duration,
                        bool nv12, const std::uint32_t* pixels = nullptr)
    {
        IMFMediaBuffer* buffer = nullptr;
        IMFSample* sample = nullptr;
        BYTE* target = nullptr;
        HRESULT result = MFCreateMemoryBuffer(bytes, &buffer);
        if (SUCCEEDED(result)) result = buffer->Lock(&target, nullptr, nullptr);
        if (SUCCEEDED(result)) {
            if (nv12) {
                convertToNv12(pixels, settings.width, settings.width, settings.height, target,
                              target + static_cast<std::size_t>(settings.width) * settings.height);
            } else {
                std::memcpy(target, data, bytes);
            }
            buffer->Unlock();
            result = buffer->SetCurrentLength(bytes);
        }
        if (SUCCEEDED(result)) result = MFCreateSample(&sample);
        if (SUCCEEDED(result)) result = sample->AddBuffer(buffer);
        if (SUCCEEDED(result)) result = sample->SetSampleTime(time);
        if (SUCCEEDED(result)) result = sample->SetSampleDuration(duration);
        if (SUCCEEDED(result)) result = writer->WriteSample(stream, sample);
        release(sample);
        release(buffer);
        return result;
    }

    HRESULT writeJob(const Job& job)
    {
        if (job.video) {
            const auto bytes = static_cast<DWORD>(static_cast<std::size_t>(settings.width) * settings.height * 3 / 2);
            const auto time = frameTime(framesWritten);
            const auto duration = frameTime(framesWritten + 1) - time;
            ++framesWritten;
            return writeBuffer(videoStream, nullptr, bytes, time, duration, true, job.pixels.data());
        }
        const auto frames = job.samples.size() / settings.channels;
        const auto time = audioTime(samplesWritten);
        const auto duration = audioTime(samplesWritten + frames) - time;
        samplesWritten += frames;
        return writeBuffer(audioStream, job.samples.data(), static_cast<DWORD>(job.samples.size() * sizeof(std::int16_t)),
                           time, duration, false);
    }

    void run(std::promise<std::wstring>* opened)
    {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_LITE);
        if (SUCCEEDED(result)) result = open();
        if (FAILED(result)) {
            release(writer);
            opened->set_value(hresultText(L"Media Foundation could not start the MP4 encoder", result));
            if (SUCCEEDED(com)) CoUninitialize();
            return;
        }
        opened->set_value(L"");
        bool failed = false;
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this] { return !jobs.empty() || finishing; });
                if (jobs.empty()) break;
                job = std::move(jobs.front());
                jobs.pop_front();
                if (job.video) --queuedFrames;
            }
            space.notify_all();
            if (failed) continue;
            result = writeJob(job);
            if (FAILED(result)) {
                failed = true;
                std::lock_guard<std::mutex> lock(mutex);
                failure = hresultText(L"Writing the video failed", result);
                active = false;
            }
        }
        result = writer->Finalize();
        if (FAILED(result) && !failed) failure = hresultText(L"Finishing the video file failed", result);
        release(writer);
        MFShutdown();
        if (SUCCEEDED(com)) CoUninitialize();
    }

    void push(Job&& job)
    {
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (job.video) {
                space.wait(lock, [this] { return queuedFrames < MaximumQueuedFrames || !active; });
                ++queuedFrames;
            }
            jobs.push_back(std::move(job));
        }
        wake.notify_one();
    }

    void flushAudio(bool all)
    {
        const std::size_t chunk = AudioChunkSamples * settings.channels;
        while (pendingAudio.size() >= chunk || (all && !pendingAudio.empty())) {
            const auto count = std::min(chunk, pendingAudio.size());
            Job job;
            job.samples.assign(pendingAudio.begin(), pendingAudio.begin() + static_cast<std::ptrdiff_t>(count));
            pendingAudio.erase(pendingAudio.begin(), pendingAudio.begin() + static_cast<std::ptrdiff_t>(count));
            push(std::move(job));
        }
    }
};

VideoRecorder::VideoRecorder()
    : impl(std::make_unique<Impl>())
{
}

VideoRecorder::~VideoRecorder()
{
    std::wstring ignored;
    (void)stop(ignored);
}

bool VideoRecorder::start(const Settings& settings, std::wstring& error)
{
    if (impl->active || impl->worker.joinable()) {
        error = L"A recording is already running.";
        return false;
    }
    if (settings.width == 0 || settings.height == 0 || settings.width % 2 != 0 || settings.height % 2 != 0 ||
        settings.frameRateNumerator == 0 || settings.frameRateDenominator == 0 || settings.channels == 0) {
        error = L"The picture size must be even and the frame rate positive.";
        return false;
    }
    impl->settings = settings;
    impl->jobs.clear();
    impl->queuedFrames = 0;
    impl->finishing = false;
    impl->failure.clear();
    impl->pendingAudio.clear();
    impl->framesQueued = 0;
    impl->framesWritten = 0;
    impl->samplesWritten = 0;
    std::promise<std::wstring> opened;
    auto result = opened.get_future();
    impl->worker = std::thread([this, &opened] { impl->run(&opened); });
    error = result.get();
    if (!error.empty()) {
        impl->worker.join();
        return false;
    }
    impl->active = true;
    return true;
}

void VideoRecorder::addFrame(const std::uint32_t* pixels, std::size_t stride)
{
    if (!impl->active) return;
    Impl::Job job;
    job.video = true;
    job.pixels.resize(static_cast<std::size_t>(impl->settings.width) * impl->settings.height);
    for (std::uint32_t y = 0; y < impl->settings.height; ++y) {
        std::memcpy(job.pixels.data() + static_cast<std::size_t>(y) * impl->settings.width,
                    pixels + static_cast<std::size_t>(y) * stride, impl->settings.width * sizeof(std::uint32_t));
    }
    impl->push(std::move(job));
    ++impl->framesQueued;
}

void VideoRecorder::addAudio(const std::int16_t* samples, std::size_t count)
{
    if (!impl->active) return;
    impl->pendingAudio.insert(impl->pendingAudio.end(), samples, samples + count);
    impl->flushAudio(false);
}

bool VideoRecorder::stop(std::wstring& error)
{
    if (!impl->worker.joinable()) return true;
    if (impl->active) impl->flushAudio(true);
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->finishing = true;
        impl->active = false;
    }
    impl->wake.notify_all();
    impl->space.notify_all();
    impl->worker.join();
    error = impl->failure;
    return error.empty();
}

bool VideoRecorder::recording() const noexcept
{
    return impl->active;
}

double VideoRecorder::seconds() const noexcept
{
    return static_cast<double>(impl->framesQueued) * impl->settings.frameRateDenominator /
           (impl->settings.frameRateNumerator ? impl->settings.frameRateNumerator : 1);
}

const Settings& VideoRecorder::settings() const noexcept
{
    return impl->settings;
}

} // namespace videorec
