/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "WaveformCapture.h"

void WaveformCapture::prepare(int numChannels, int displaySamples)
{
    inputWaveBuffer.setSize(numChannels, displaySamples, false, true, true);
    inputWaveBuffer.clear();
    inputWritePos.store(0);

    outputWaveBuffer.setSize(numChannels, displaySamples, false, true, true);
    outputWaveBuffer.clear();
    outputWritePos.store(0);
}

void WaveformCapture::reset()
{
    inputWaveBuffer.clear();
    outputWaveBuffer.clear();
    inputWritePos.store(0);
    outputWritePos.store(0);
}

void WaveformCapture::pushInput(const juce::AudioBuffer<float>& src) noexcept
{
    pushToRingBuffer(src, inputWaveBuffer, inputWritePos);
}

void WaveformCapture::pushOutput(const juce::AudioBuffer<float>& src) noexcept
{
    pushToRingBuffer(src, outputWaveBuffer, outputWritePos);
}

void WaveformCapture::getInputWaveform(juce::AudioBuffer<float>& dest) const
{
    readFromRingBuffer(dest, inputWaveBuffer, const_cast<std::atomic<int>&>(inputWritePos));
}

void WaveformCapture::getOutputWaveform(juce::AudioBuffer<float>& dest) const
{
    readFromRingBuffer(dest, outputWaveBuffer, const_cast<std::atomic<int>&>(outputWritePos));
}

void WaveformCapture::pushToRingBuffer(const juce::AudioBuffer<float>& src,
                                       juce::AudioBuffer<float>& dst,
                                       std::atomic<int>& pos) noexcept
{
    const int total = dst.getNumSamples();
    if (total == 0)
        return;
    const int num = juce::jmin(src.getNumSamples(), total);
    int w = pos.load(std::memory_order_relaxed);
    for (int ch = 0; ch < juce::jmin(dst.getNumChannels(), src.getNumChannels()); ++ch)
    {
        auto* d = dst.getWritePointer(ch);
        auto* s = src.getReadPointer(ch);
        const int first = juce::jmin(num, total - w);
        juce::FloatVectorOperations::copy(d + w, s, first);
        if (num > first)
            juce::FloatVectorOperations::copy(d, s + first, num - first);
    }
    pos.store((w + num) % total, std::memory_order_release);
}

void WaveformCapture::readFromRingBuffer(juce::AudioBuffer<float>& dest,
                                         const juce::AudioBuffer<float>& src,
                                         std::atomic<int>& writePos) const
{
    const int num = dest.getNumSamples();
    const int total = src.getNumSamples();
    if (total == 0 || num == 0)
        return;
    int w = writePos.load(std::memory_order_acquire);
    int start = w - num;
    if (start < 0)
        start += total;
    for (int ch = 0; ch < juce::jmin(dest.getNumChannels(), src.getNumChannels()); ++ch)
    {
        auto* srcPtr = src.getReadPointer(ch);
        auto* dstPtr = dest.getWritePointer(ch);
        const int first = juce::jmin(num, total - start);
        juce::FloatVectorOperations::copy(dstPtr, srcPtr + start, first);
        if (num > first)
            juce::FloatVectorOperations::copy(dstPtr + first, srcPtr, num - first);
    }
}
