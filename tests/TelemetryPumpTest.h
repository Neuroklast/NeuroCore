#pragma once

#include <JuceHeader.h>
#include "../src/bridge/TelemetryPump.h"
#include "../src/core/PluginProcessor.h"
#include "TestHelpers.h"
#include <atomic>
#include <thread>

class TelemetryPumpTest : public juce::UnitTest
{
public:
    TelemetryPumpTest() : juce::UnitTest ("TelemetryPump", "Bridge") {}

    void runTest() override
    {
        beginTest ("publish then copyLatest has NKTM and peaks");
        {
            bridge::TelemetryPump pump;
            juce::AudioBuffer<float> in (2, 64), out (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                in.setSample (0, i, 0.5f);
                in.setSample (1, i, 0.5f);
                out.setSample (0, i, 0.25f);
                out.setSample (1, i, -0.25f);
            }
            pump.noteInput (in);
            pump.publish (out, 0.42f);

            std::uint8_t buf[bridge::TelemetryPump::kMaxBytes];
            const auto n = pump.copyLatest (buf, sizeof (buf));
            expect (n >= bridge::kTelemetryHeaderBytes);

            bridge::TelemetryDesc d;
            expect (bridge::readTelemetryHeader (buf, n, d));
            expectEquals ((int) d.scopeN, bridge::TelemetryPump::kScopeN);
            expectEquals ((int) d.gonioN, bridge::TelemetryPump::kGonioN);
            expect (d.inPeak > 0.4f);
            expect (d.outPeak > 0.2f);
            expect (std::abs (d.cpu01 - 0.42f) < 1.0e-5f);

            const auto* words = reinterpret_cast<const std::uint32_t*> (buf);
            expectEquals ((int) words[0], (int) bridge::kTelemetryMagic);
        }

        beginTest ("a click in a quiet block is packed as one onset");
        {
            bridge::TelemetryPump pump;
            pump.setSampleRate (48000.0);
            juce::AudioBuffer<float> in (1, 256), out (1, 256);
            in.clear();
            out.clear();
            out.setSample (0, 40, 1.0f);
            pump.noteInput (in);
            pump.publish (out, 0.1f);
            std::uint8_t buf[bridge::TelemetryPump::kMaxBytes];
            const auto n = pump.copyLatest (buf, sizeof (buf));
            expect (n >= bridge::kTelemetryHeaderBytes);
            const auto packed = (std::uint16_t) buf[6] | (std::uint16_t) ((std::uint16_t) buf[7] << 8);
            expect ((packed >> 8) >= 1);
            expect ((packed & 0xff) > 20);
        }

        beginTest ("copyLatest after later blocks still carries earlier onsets");
        {
            bridge::TelemetryPump pump;
            pump.setSampleRate (48000.0);
            juce::AudioBuffer<float> in (1, 64), out (1, 64);
            auto click = [&] (int at)
            {
                in.clear();
                out.clear();
                out.setSample (0, at, 1.0f);
                pump.noteInput (in);
                pump.publish (out, 0.1f);
            };
            auto silence = [&]
            {
                in.clear();
                out.clear();
                pump.noteInput (in);
                pump.publish (out, 0.1f);
            };
            click (8);
            for (int i = 0; i < 48; ++i)
                silence();
            click (8);
            std::uint8_t buf[bridge::TelemetryPump::kMaxBytes];
            const auto n = pump.copyLatest (buf, sizeof (buf));
            bridge::TelemetryDesc d;
            expect (bridge::readTelemetryHeader (buf, n, d));
            expectEquals ((int) d.onsetN, 2, "missed-callback onsets must stay in the ring");
            expect (d.onsets[0].samplePos != d.onsets[1].samplePos);
            expect (d.onsets[0].amp > 0.05f);
            expect (d.onsets[1].amp > 0.05f);
        }

        beginTest ("left then right clicks keep their stereo at the hit");
        {
            bridge::TelemetryPump pump;
            pump.setSampleRate (48000.0);
            juce::AudioBuffer<float> in (2, 64), out (2, 64);
            auto click = [&] (float l, float r)
            {
                in.clear();
                out.clear();
                out.setSample (0, 8, l);
                out.setSample (1, 8, r);
                pump.noteInput (in);
                pump.publish (out, 0.1f);
            };
            auto silence = [&]
            {
                in.clear();
                out.clear();
                pump.noteInput (in);
                pump.publish (out, 0.1f);
            };
            click (1.0f, 0.0f);
            for (int i = 0; i < 48; ++i)
                silence();
            click (0.0f, 1.0f);
            std::uint8_t buf[bridge::TelemetryPump::kMaxBytes];
            const auto n = pump.copyLatest (buf, sizeof (buf));
            bridge::TelemetryDesc d;
            expect (bridge::readTelemetryHeader (buf, n, d));
            expectEquals ((int) d.onsetN, 2);
            expect (d.onsets[0].balance < -0.5f, "first hit was left");
            expect (d.onsets[1].balance > 0.5f, "second hit was right");
        }

        beginTest ("a claimed read slot survives two publishes");
        {
            const int w0 = bridge::TelemetryPump::writeIndex (0, -1);
            expect (w0 != 0, "first write leaves the last published slot");
            const int w1 = bridge::TelemetryPump::writeIndex (w0, 0);
            expect (w1 != 0, "must not overwrite the slot being copied");
            expect (w1 != w0, "must not overwrite the newly published slot");
        }

        beginTest ("copyLatest stays a valid NKTM frame under concurrent publish");
        {
            bridge::TelemetryPump pump;
            {
                juce::AudioBuffer<float> in (2, 64), out (2, 64);
                out.setSample (0, 0, 0.3f);
                pump.noteInput (in);
                pump.publish (out, 0.2f);
            }
            std::atomic<bool> go { true };
            std::thread audio ([&]
            {
                juce::AudioBuffer<float> in (2, 64), out (2, 64);
                float v = 0.1f;
                while (go.load (std::memory_order_relaxed))
                {
                    v = v > 0.8f ? 0.1f : v + 0.05f;
                    for (int i = 0; i < 64; ++i)
                    {
                        in.setSample (0, i, v);
                        in.setSample (1, i, v);
                        out.setSample (0, i, v);
                        out.setSample (1, i, -v);
                    }
                    pump.noteInput (in);
                    pump.publish (out, v);
                }
            });
            int copies = 0, bad = 0;
            std::uint8_t buf[bridge::TelemetryPump::kMaxBytes];
            for (int i = 0; i < 4000; ++i)
            {
                const auto n = pump.copyLatest (buf, sizeof (buf));
                if (n == 0)
                    continue;
                ++copies;
                bridge::TelemetryDesc d;
                if (! bridge::readTelemetryHeader (buf, n, d)
                    || n != bridge::telemetryByteSize (d)
                    || d.seq == 0)
                    ++bad;
            }
            go.store (false, std::memory_order_relaxed);
            audio.join();
            expect (copies > 0, "reader must see published frames");
            expectEquals (bad, 0, "torn NKTM frame under concurrent publish");
        }

        beginTest ("empty pump copies nothing");
        {
            bridge::TelemetryPump pump;
            std::uint8_t buf[64];
            expectEquals ((int) pump.copyLatest (buf, sizeof (buf)), 0);
        }

        beginTest ("setWanted false skips noteInput and publish");
        {
            bridge::TelemetryPump pump;
            pump.setWanted (false);
            juce::AudioBuffer<float> in (2, 32), out (2, 32);
            for (int i = 0; i < 32; ++i)
            {
                in.setSample (0, i, 0.8f);
                out.setSample (0, i, 0.8f);
            }
            pump.noteInput (in);
            pump.publish (out, 0.5f);
            std::uint8_t buf[bridge::TelemetryPump::kMaxBytes];
            expectEquals ((int) pump.copyLatest (buf, sizeof (buf)), 0,
                          "NKTM must not scan when no editor is reading");
        }

        beginTest ("closed editor does not publish NKTM");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 64);
            proc.prepareToPlay (48000.0, 64);
            juce::AudioBuffer<float> buf (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, 0.4f);
                buf.setSample (1, i, 0.4f);
            }
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            std::uint8_t frame[bridge::TelemetryPump::kMaxBytes];
            expectEquals ((int) proc.getTelemetry().copyLatest (frame, sizeof (frame)), 0,
                          "headless insert must not fill telemetry.bin");
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            proc.releaseResources();
        }
    }
};
