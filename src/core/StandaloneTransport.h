#pragma once

#include <JuceHeader.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

/** Standalone-only transport. Its clock and optional audition source run on the
    audio callback, never a GUI timer. Commands are atomic and allocation-free
    on the audio thread. The decoded clip is immutable after publication. */
class StandaloneTransport
{
public:
    enum class Source : int { Input = 0, File = 1, TestTone = 2 };
    struct Snapshot
    {
        double ppq { 0.0 };
        double bpm { 120.0 };
        bool playing { false };
    };

    void play() noexcept { requestedCommand.store (1, std::memory_order_release); uiPlaying.store (true); }
    void pause() noexcept { requestedCommand.store (2, std::memory_order_release); uiPlaying.store (false); }
    void stop() noexcept { requestedCommand.store (3, std::memory_order_release); uiPlaying.store (false); }
    bool isPlaying() const noexcept { return uiPlaying.load (std::memory_order_relaxed); }
    double positionPpq() const noexcept { return displayedPpq.load (std::memory_order_relaxed); }
    Source source() const noexcept { return (Source) sourceMode.load (std::memory_order_relaxed); }
    void setSource (Source source) noexcept { sourceMode.store ((int) source, std::memory_order_relaxed); }

    // Called from message thread, never from processBlock.
    bool loadFile (const juce::File& file, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (! reader || reader->sampleRate < 8000.0 || reader->sampleRate > 192000.0
            || reader->numChannels < 1 || reader->numChannels > 2 || reader->lengthInSamples <= 0
            || reader->lengthInSamples > 12000000
            || reader->lengthInSamples * (juce::int64) reader->numChannels > 24000000)
        {
            error = "Choose a supported mono/stereo audio file (up to 12 million samples and 96 MB decoded).";
            return false;
        }
        // Reclaim retired buffers on the message thread, only after the audio
        // callback has fully passed the replacement boundary.
        const auto epoch = blocksCompleted.load (std::memory_order_acquire);
        retiredClips.erase (std::remove_if (retiredClips.begin(), retiredClips.end(),
            [epoch] (const RetiredClip& old) { return epoch >= old.retiredAt + 2; }),
            retiredClips.end());
        auto decoded = std::make_shared<AudioClip>();
        decoded->sampleRate = reader->sampleRate;
        decoded->data.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        if (! reader->read (&decoded->data, 0, (int) reader->lengthInSamples, 0, true, true))
        {
            error = "Unable to decode the selected audio file.";
            return false;
        }
        const std::shared_ptr<const AudioClip> published = decoded;
        // Retain the old clip on the message thread until a later safe collection;
        // don't run a potentially expensive buffer destructor in the audio callback.
        if (auto old = std::atomic_load (&clip))
            retiredClips.push_back ({ std::move (old), blocksCompleted.load (std::memory_order_acquire) });
        std::atomic_store (&clip, published);
        fileName = file.getFileName();
        fileGeneration.fetch_add (1, std::memory_order_release);
        sourceMode.store ((int) Source::File, std::memory_order_release);
        return true;
    }

    juce::String loadedFileName() const { return fileName; } // Message thread only.

    /** Processed exactly once per host audio block. Returns musical position at
        the start of that block, even on CPU hold/lock miss. */
    Snapshot beginBlock (juce::AudioBuffer<float>& input, double sampleRate, double bpm) noexcept
    {
        const int command = requestedCommand.exchange (0, std::memory_order_acq_rel);
        if (command == 1) running = true;
        if (command == 2) running = false;
        if (command == 3)
        {
            running = false;
            ppq = 0.0;
            fileCursor = 0.0;
            tonePhase = 0.0;
        }
        const auto generation = fileGeneration.load (std::memory_order_acquire);
        if (generation != activeGeneration)
        {
            activeGeneration = generation;
            fileCursor = 0.0;
        }
        const auto safeBpm = std::isfinite (bpm) && bpm >= 20.0 && bpm <= 400.0 ? bpm : 120.0;
        Snapshot start { ppq, safeBpm, running };
        const int count = input.getNumSamples();
        const auto mode = source();
        if ((mode == Source::File || mode == Source::TestTone) && ! running)
            input.clear(); // Paused internal sources never leak the physical input.
        if (running && count > 0 && sampleRate > 1.0)
        {
            if (mode == Source::File || mode == Source::TestTone)
            {
                std::shared_ptr<const AudioClip> active;
                if (mode == Source::File)
                    active = std::atomic_load (&clip);
                for (int channel = 0; channel < input.getNumChannels(); ++channel)
                    input.clear (channel, 0, count);
                if (mode == Source::File && active && active->data.getNumSamples() > 0)
                {
                    const int total = active->data.getNumSamples();
                    const int channels = active->data.getNumChannels();
                    const double increment = active->sampleRate / sampleRate;
                    const double firstPos = fileCursor;
                    for (int i = 0; i < count; ++i)
                    {
                        double frame = firstPos + (double) i * increment;
                        frame -= std::floor (frame / total) * total;
                        const int a = (int) frame;
                        const int b = a + 1 == total ? 0 : a + 1;
                        const float frac = (float) (frame - a);
                        for (int ch = 0; ch < input.getNumChannels(); ++ch)
                        {
                            const int src = juce::jmin (ch, channels - 1);
                            const float* samples = active->data.getReadPointer (src);
                            input.setSample (ch, i, (1.f - frac) * samples[a] + frac * samples[b]);
                        }
                    }
                    fileCursor = firstPos + (double) count * increment;
                    fileCursor -= std::floor (fileCursor / total) * total;
                }
                else if (mode == Source::TestTone)
                {
                    const double delta = juce::MathConstants<double>::twoPi * 440.0 / sampleRate;
                    for (int i = 0; i < count; ++i)
                    {
                        const float sample = (float) (std::sin (tonePhase) * 0.12);
                        tonePhase += delta;
                        if (tonePhase >= juce::MathConstants<double>::twoPi)
                            tonePhase -= juce::MathConstants<double>::twoPi;
                        for (int ch = 0; ch < input.getNumChannels(); ++ch)
                            input.setSample (ch, i, sample);
                    }
                }
            }
            ppq += (double) count * safeBpm / (60.0 * sampleRate);
        }
        displayedPpq.store (ppq, std::memory_order_release);
        blocksCompleted.fetch_add (1, std::memory_order_release);
        return start;
    }

private:
    struct AudioClip { juce::AudioBuffer<float> data; double sampleRate { 48000.0 }; };
    std::shared_ptr<const AudioClip> clip;
    // Retired clips are never destroyed on the callback. Reclaimed on shutdown.
    struct RetiredClip { std::shared_ptr<const AudioClip> clip; uint64_t retiredAt; };
    std::vector<RetiredClip> retiredClips;
    std::atomic<uint64_t> blocksCompleted { 0 };
    juce::String fileName;
    std::atomic<int> requestedCommand { 0 };
    std::atomic<int> sourceMode { (int) Source::Input };
    std::atomic<bool> uiPlaying { false };
    std::atomic<double> displayedPpq { 0.0 };
    std::atomic<unsigned int> fileGeneration { 0 };
    unsigned int activeGeneration { 0 };
    bool running { false };
    double ppq { 0.0 };
    double fileCursor { 0.0 };
    double tonePhase { 0.0 };
};
