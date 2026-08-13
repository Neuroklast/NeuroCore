#ifndef CYBERFXTEST_H
#define CYBERFXTEST_H

#include <JuceHeader.h>
#include "../src/ui/fx/CyberFxDirector.h"
#include "../src/ui/fx/DecodeText.h"
#include "../src/ui/fx/CyberSequence.h"
#include "../src/ui/fx/CyberBackdropCache.h"

class CyberFxTest : public juce::UnitTest
{
public:
    CyberFxTest() : juce::UnitTest ("CyberFxTest", "UI") {}

    void runTest() override
    {
        beginTest ("Director hidden stops glitch");
        {
            CyberFxDirector d;
            juce::Random rng { 1 };
            d.setVisible (false);
            d.tick (1.0f, rng);
            expectEquals (d.getState().glitch, 0.f);
        }

        beginTest ("Director reduced motion never spikes");
        {
            CyberFxDirector d2;
            juce::Random rng { 1 };
            d2.setMotion (CyberMotion::Reduced);
            for (int i = 0; i < 200; ++i)
                d2.tick (0.05f, rng);
            expectEquals (d2.getState().glitch, 0.f);
        }

        beginTest ("Pulse is one-pole and clamped");
        {
            CyberFxDirector d3;
            juce::Random rng { 1 };
            d3.setPeakNorm (2.0f);
            d3.tick (1.0f / 24.f, rng);
            expect (d3.getState().peakPulse <= 1.0f);
            expect (d3.getState().peakPulse > 0.0f);
        }

        beginTest ("Decode preserves spaces and reveals prefix");
        {
            juce::Random r2 { 2 };
            auto s = decodeGlitchText ("AB C", 2, r2);
            expectEquals (s[0], juce::juce_wchar ('A'));
            expectEquals (s[1], juce::juce_wchar ('B'));
            expectEquals (s[2], juce::juce_wchar (' '));
            expect (s[3] != juce::juce_wchar ('C'));
        }

        beginTest ("Enter sequence reaches Shown at 400ms");
        {
            CyberSequence seq;
            seq.playEnter();
            seq.tick (0.40f);
            expect (seq.getPhase() == OverlayPhase::Shown);
            expectWithinAbsoluteError (seq.contentAlpha(), 1.f, 0.001f);
        }

        beginTest ("Exit consumeFinished is one-shot");
        {
            CyberSequence seq;
            seq.playEnter();
            seq.tick (0.40f);
            seq.playExit();
            seq.tick (0.28f);
            expect (seq.consumeFinished());
            expect (! seq.consumeFinished());
        }

        beginTest ("Reduced skipToEnd is instant");
        {
            CyberSequence seq2;
            seq2.playEnter();
            seq2.skipToEnd();
            expect (seq2.getPhase() == OverlayPhase::Shown);
        }

        beginTest ("playExit from Idle closes immediately");
        {
            CyberSequence seq;
            seq.playExit();
            expect (seq.getPhase() == OverlayPhase::Closed);
            expect (seq.consumeFinished());
        }

        beginTest ("triggerGlitch ignored when hidden");
        {
            CyberFxDirector d;
            d.setVisible (false);
            d.triggerGlitch (0.9f, 7);
            expectEquals (d.getState().glitch, 0.f);
        }

        beginTest ("triggerGlitch decays without a new loop");
        {
            CyberFxDirector d;
            juce::Random rng { 3 };
            d.triggerGlitch (0.4f, 1);
            expect (d.getState().glitch >= 0.39f);
            expectEquals (d.getState().glitchSeed, 1);
            for (int i = 0; i < 10; ++i)
                d.tick (0.05f, rng);
            expect (d.getState().glitch < 0.05f);
        }

        beginTest ("Cache rebuilds only on size change");
        {
            CyberBackdropCache cache;
            cache.ensure (1280, 860);
            cache.ensure (1280, 860);
            expectEquals (cache.rebuildCount(), 1);
            cache.ensure (960, 640);
            expectEquals (cache.rebuildCount(), 2);
        }

        beginTest ("shouldPlayBoot respects reduced motion and already-shown");
        {
            expect (shouldPlayBoot (CyberMotion::Full, false));
            expect (! shouldPlayBoot (CyberMotion::Reduced, false));
            expect (! shouldPlayBoot (CyberMotion::Full, true));
        }
    }
};

#endif
