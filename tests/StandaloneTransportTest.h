#pragma once

#include "../src/core/StandaloneTransport.h"
#include <cmath>

class StandaloneTransportTest : public juce::UnitTest
{
public:
    StandaloneTransportTest() : juce::UnitTest ("Standalone transport and audition", "Transport") {}

    void runTest() override
    {
        beginTest ("default is external input with test tone off and no transport");
        {
            StandaloneTransport transport;
            juce::AudioBuffer<float> b (2, 128);
            b.clear();
            b.applyGain (1.f);
            b.setSample (0, 0, 0.2f);
            auto snap = transport.beginBlock (b, 48000., 120.);
            expect (! snap.playing);
            expectEquals ((int) transport.source(), (int) StandaloneTransport::Source::Input);
            expectWithinAbsoluteError (b.getSample (0, 0), 0.2f, 0.00001f);
            expectWithinAbsoluteError (transport.positionPpq(), 0., 1.e-9);
        }

        beginTest ("sample-clock survives varying block sizes; play, pause and stop");
        {
            StandaloneTransport transport;
            juce::AudioBuffer<float> buf (2, 64);
            transport.play();
            auto snap = transport.beginBlock (buf, 48000., 120.);
            expect (snap.playing);
            expectWithinAbsoluteError (snap.ppq, 0., 1.e-9);
            expectWithinAbsoluteError (transport.positionPpq(), 64. * 120. / 60. / 48000., 1.e-9);
            juce::AudioBuffer<float> big (2, 512);
            transport.beginBlock (big, 48000., 120.);
            const auto expected = 576. * 120. / 60. / 48000.;
            expectWithinAbsoluteError (transport.positionPpq(), expected, 1.e-9);
            transport.pause();
            snap = transport.beginBlock (big, 48000., 220.);
            expect (! snap.playing);
            expectWithinAbsoluteError (transport.positionPpq(), expected, 1.e-9);
            transport.play();
            snap = transport.beginBlock (big, 48000., 220.);
            expect (snap.playing);
            expectWithinAbsoluteError (snap.ppq, expected, 1.e-9);
            transport.stop();
            snap = transport.beginBlock (big, 48000., 120.);
            expect (! snap.playing);
            expectWithinAbsoluteError (snap.ppq, 0., 1.e-9);
            expectWithinAbsoluteError (transport.positionPpq(), 0., 1.e-9);
        }

        beginTest ("explicit test tone is silent when stopped and audible when playing");
        {
            StandaloneTransport transport;
            transport.setSource (StandaloneTransport::Source::TestTone);
            juce::AudioBuffer<float> b (2, 128);
            b.clear();
            b.setSample (0, 0, 1.0f);
            transport.beginBlock (b, 48000., 120.);
            expectWithinAbsoluteError (b.getMagnitude (0, 128), 0.f, 1.e-8f);
            transport.play();
            transport.beginBlock (b, 48000., 120.);
            expect (b.getMagnitude (0, 128) > 0.10f);
            expect (b.getMagnitude (0, 128) <= 0.121f);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 128; ++i)
                    expect (std::isfinite (b.getSample (ch, i)));
            transport.pause();
            transport.beginBlock (b, 48000., 120.);
            expectWithinAbsoluteError (b.getMagnitude (0, 128), 0.f, 1.e-8f);
            transport.setSource (StandaloneTransport::Source::Input);
            b.clear();
            b.setSample (1, 0, 0.4f);
            transport.beginBlock (b, 48000., 120.);
            expectWithinAbsoluteError (b.getSample (1, 0), 0.4f, 1.e-7f);
        }

        beginTest ("audio file load, loop and pause without touching the external input");
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("nk-standalone-" + juce::Uuid().toString() + ".wav");
            juce::WavAudioFormat format;
            juce::AudioBuffer<float> waveform (1, 128);
            for (int i = 0; i < 128; ++i)
                waveform.setSample (0, i, i < 64 ? 0.25f : -0.25f);
            {
                std::unique_ptr<juce::FileOutputStream> out (file.createOutputStream());
                expect (out != nullptr);
                if (out != nullptr)
                {
                    auto* raw = format.createWriterFor (out.get(), 48000.0, 1, 16, {}, 0);
                    expect (raw != nullptr);
                    if (raw != nullptr)
                    {
                        out.release();
                        std::unique_ptr<juce::AudioFormatWriter> writer (raw);
                        expect (writer->writeFromAudioSampleBuffer (waveform, 0, 128));
                    }
                }
            }
            StandaloneTransport transport;
            juce::String error;
            expect (transport.loadFile (file, error), error);
            expectEquals ((int) transport.source(), (int) StandaloneTransport::Source::File);
            expectEquals (transport.loadedFileName(), file.getFileName());
            transport.play();
            juce::AudioBuffer<float> result (2, 256);
            result.clear();
            transport.beginBlock (result, 48000., 120.);
            expect (result.getMagnitude (0, 256) > 0.2f);
            expectWithinAbsoluteError (result.getSample (0, 0), result.getSample (0, 128), 0.0001f);
            expectWithinAbsoluteError (result.getSample (1, 0), result.getSample (0, 0), 0.0001f);
            transport.stop();
            transport.beginBlock (result, 48000., 120.);
            expectWithinAbsoluteError (result.getMagnitude (0, 256), 0.f, 1.e-8f);
            file.deleteFile();
        }
    }
};
