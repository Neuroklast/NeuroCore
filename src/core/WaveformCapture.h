#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file WaveformCapture.h
    @brief Ring-buffer based waveform capture for real-time display.

    Captures input and output waveforms from the audio thread into
    lock-free ring buffers so that the UI thread can read them safely.
*/

#include <JuceHeader.h>
#include <atomic>

class WaveformCapture
{
public:
    WaveformCapture() = default;

    /** Allocate ring buffers.
        @param numChannels  Number of audio channels to capture.
        @param displaySamples  Number of samples per ring buffer (e.g. Config::kWaveformDisplaySamples). */
    void prepare(int numChannels, int displaySamples);

    /** Push the current audio block into the input ring buffer.
        Called from the audio thread. */
    void pushInput(const juce::AudioBuffer<float>& src) noexcept;

    /** Push the current audio block into the output ring buffer.
        Called from the audio thread. */
    void pushOutput(const juce::AudioBuffer<float>& src) noexcept;

    /** Read the most recent `dest.getNumSamples()` samples from the input ring buffer.
        Can be called from the UI thread. */
    void getInputWaveform(juce::AudioBuffer<float>& dest) const;

    /** Read the most recent `dest.getNumSamples()` samples from the output ring buffer.
        Can be called from the UI thread. */
    void getOutputWaveform(juce::AudioBuffer<float>& dest) const;

    /** Clear all buffers and reset write positions. */
    void reset();

private:
    juce::AudioBuffer<float> inputWaveBuffer;
    juce::AudioBuffer<float> outputWaveBuffer;
    std::atomic<int> inputWritePos  { 0 };
    std::atomic<int> outputWritePos { 0 };

    void pushToRingBuffer(const juce::AudioBuffer<float>& src,
                          juce::AudioBuffer<float>& dst,
                          std::atomic<int>& pos) noexcept;

    void readFromRingBuffer(juce::AudioBuffer<float>& dest,
                            const juce::AudioBuffer<float>& src,
                            const std::atomic<int>& writePos) const;
};
