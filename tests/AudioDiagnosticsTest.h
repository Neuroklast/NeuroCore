#pragma once

#include <JuceHeader.h>
#include "../src/utils/AudioDiagnostics.h"
#include "../src/core/Config.h"
#include <cmath>
#include <vector>
#include <limits>

class AudioDiagnosticsTest : public juce::UnitTest
{
public:
    AudioDiagnosticsTest() : juce::UnitTest ("AudioDiagnostics") {}

    void runTest() override
    {
        beginTest ("scan detects hard jump");
        {
            std::vector<float> buf (64, 0.1f);
            buf[32] = -0.8f; // jump of 0.9 from 0.1
            float last = 0.1f;
            const float* ptr = buf.data();
            auto r = AudioDiagnostics::scan (&ptr, 1, (int) buf.size(),
                                             Config::kAudioDiagJumpThreshold,
                                             Config::kAudioDiagCrackleJumpMin,
                                             &last, 1);
            expect (r.jumpCount >= 1);
            expect (r.maxJump >= 0.8f);
            expect (r.firstJumpSample == 32);
            expect (r.nanCount == 0);
        }

        beginTest ("scan detects NaN");
        {
            std::vector<float> buf (32, 0.2f);
            buf[10] = std::numeric_limits<float>::quiet_NaN();
            float last = 0.f;
            const float* ptr = buf.data();
            auto r = AudioDiagnostics::scan (&ptr, 1, (int) buf.size(),
                                             Config::kAudioDiagJumpThreshold,
                                             Config::kAudioDiagCrackleJumpMin,
                                             &last, 1);
            expect (r.nanCount == 1);
            expect (r.firstNanSample == 10);
        }

        beginTest ("scan detects crackle cluster");
        {
            std::vector<float> buf (128, 0.0f);
            // alternating spikes → many soft jumps
            for (int i = 0; i < 20; ++i)
                buf[(size_t) (10 + i)] = (i % 2 == 0) ? 0.25f : -0.25f;
            float last = 0.f;
            const float* ptr = buf.data();
            auto r = AudioDiagnostics::scan (&ptr, 1, (int) buf.size(),
                                             Config::kAudioDiagJumpThreshold,
                                             Config::kAudioDiagCrackleJumpMin,
                                             &last, 1);
            expect (r.jumpCount >= 4);
            expect (r.maxJump >= Config::kAudioDiagCrackleJumpMin);
        }

        beginTest ("report pushes event with context");
        {
            AudioDiagnostics diag;
            diag.setEnabled (true);
            diag.setPresetName ("TestPreset");
            diag.setFormulaHead ("stage1: y = softclip(x * a)");
            diag.setLiveParams (0.7f, 0.3f, 0.1f, 0.2f,
                                1.1f, 1.0f, 1.0f,
                                48000.f, 64, 4,
                                false, false, false, true, true);

            AudioDiagnostics::ScanResult r;
            r.nanCount = 2;
            r.firstNanSample = 5;
            r.firstNanCh = 0;
            r.peak = 0.5f;
            diag.report (AudioDiagnostics::Stage::FinalOut, r, 0, 0.4f, 0.5f);

            AudioDiagnostics::Event e;
            expect (diag.popEventForTest (e));
            expect (e.nanCount == 2);
            expect (juce::String (e.ctx.preset) == "TestPreset");
            expect (juce::String (e.ctx.formulaHead).contains ("softclip"));
            expectWithinAbsoluteError (e.ctx.paramA, 0.7f, 1.0e-4f);
            expect (e.stage == AudioDiagnostics::Stage::FinalOut);
            expect (diag.getTotalNanEvents() >= 1);
        }

        beginTest ("clean sine produces no hard jump");
        {
            std::vector<float> buf (256);
            for (int i = 0; i < 256; ++i)
                buf[(size_t) i] = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * (float) i / 64.0f);
            float last = 0.f;
            const float* ptr = buf.data();
            auto r = AudioDiagnostics::scan (&ptr, 1, (int) buf.size(),
                                             Config::kAudioDiagJumpThreshold,
                                             Config::kAudioDiagCrackleJumpMin,
                                             &last, 1);
            expect (r.firstJumpSample < 0);
            expect (r.nanCount == 0);
        }

        beginTest ("dsp-introduced vs input-sourced tagging data");
        {
            AudioDiagnostics diag;
            diag.setEnabled (true);
            diag.setPresetName ("Jumpy");
            diag.setLiveParams (0, 0, 0, 0, 1, 1, 1, 44100, 32, 1,
                                false, false, false, true, true);

            AudioDiagnostics::ScanResult out;
            out.jumpCount = 8;
            out.maxJump = 0.9f;
            out.firstJumpSample = 3;
            out.firstJumpCh = 0;
            out.firstJumpPrev = 0.1f;
            out.firstJumpCurr = -0.8f;
            out.peak = 0.9f;
            diag.report (AudioDiagnostics::Stage::FinalOut, out, /*inputJumps*/ 0, 0.2f, 0.9f);

            AudioDiagnostics::Event e;
            expect (diag.popEventForTest (e));
            expect (e.inputJumpCount == 0);
            expect (e.jumpCount == 8);
            // Writer would tag "dsp-introduced" when inputJumpCount*2 < jumpCount
            expect (e.inputJumpCount * 2 < e.jumpCount);
        }
    }
};
