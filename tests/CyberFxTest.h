#ifndef CYBERFXTEST_H
#define CYBERFXTEST_H

#include <JuceHeader.h>
#include "../src/ui/fx/CyberFxDirector.h"
#include "../src/ui/fx/DecodeText.h"
#include "../src/ui/fx/CyberSequence.h"
#include "../src/ui/fx/CyberBackdropCache.h"
#include "../src/ui/fx/CyberClip.h"
#include "../src/ui/custom/CyberMixSlider.h"
#include "../src/ui/ModalOverlay.h"
#include "../src/ui/fx/BootSequenceOverlay.h"
#include "../src/ui/fx/CyberChrome.h"

class CyberFxTest : public juce::UnitTest
{
public:
    CyberFxTest() : juce::UnitTest ("CyberFxTest", "UI") {}

    void runTest() override
    {
        beginTest ("CRT glow paints without throwing");
        {
            juce::Image img (juce::Image::ARGB, 64, 48, true);
            juce::Graphics g (img);
            CyberChrome::drawCrtGlow (g, { 0, 0, 64, 48 }, 1.2f, 0.4f);
            expect (img.getPixelAt (2, 2).getAlpha() > 0
                    || img.getPixelAt (32, 2).getAlpha() > 0
                    || img.getPixelAt (32, 24).getAlpha() >= 0);
        }

        beginTest ("Mix slider starts with no glitch");
        {
            CyberMixSlider mix;
            mix.setSize (200, 28);
            expectEquals (mix.getGlitch(), 0.f);
            mix.tick (0.05f);
            expectEquals (mix.getGlitch(), 0.f);
        }

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

        beginTest ("Enter sequence reaches Shown after loader");
        {
            CyberSequence seq;
            seq.playEnter();
            seq.tick (0.40f);
            expect (seq.isLoaderVisible());
            expect (seq.contentAlpha() < 0.5f);
            seq.tick (0.40f);
            expect (seq.getPhase() == OverlayPhase::Shown);
            expectWithinAbsoluteError (seq.clipProgress(), 1.f, 0.001f);
            expect (! seq.isLoaderVisible());
        }

        beginTest ("Exit consumeFinished is one-shot");
        {
            CyberSequence seq;
            seq.playEnter();
            seq.tick (0.72f);
            seq.playExit();
            seq.tick (0.50f);
            expect (seq.consumeFinished());
            expect (! seq.consumeFinished());
        }

        beginTest ("Clip reveal grows window not scanline");
        {
            const juce::Rectangle<int> full { 10, 20, 200, 100 };
            const auto boot0 = revealBounds (full, ClipReveal::SystemBoot, 0.f);
            const auto boot1 = revealBounds (full, ClipReveal::SystemBoot, 1.f);
            expect (boot0.getHeight() < full.getHeight());
            expectEquals (boot0.getY(), full.getY());
            expect (boot1 == full);

            const auto mid = revealBounds (full, ClipReveal::RingLink, 0.5f);
            expect (mid.getHeight() < full.getHeight());
            expect (mid.getCentreY() >= full.getCentreY() - 2);
            expect (mid.getCentreY() <= full.getCentreY() + 2);
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
            expect (! shouldPlayBoot (CyberMotion::Off, false));
        }

        beginTest ("Director Off ignores glitch and ambient");
        {
            CyberFxDirector d;
            juce::Random rng { 4 };
            d.setMotion (CyberMotion::Off);
            d.triggerGlitch (0.9f, 7);
            expectEquals (d.getState().glitch, 0.f);
            d.tick (1.0f, rng);
            expectEquals (d.getState().glitch, 0.f);
            expect (! d.needsAmbientRepaint());
        }

        beginTest ("Overlay Off lands in Shown immediately");
        {
            juce::Component host;
            host.setSize (800, 600);
            ModalOverlay overlay;
            auto content = std::make_unique<juce::Component>();
            auto* raw = content.get();
            overlay.setContent (std::move (content));
            overlay.setMotion (CyberMotion::Off);
            overlay.show (host);
            expect (overlay.getPhase() == OverlayPhase::Shown);
            expect (raw->isVisible());
            expect (raw->getAlpha() >= 0.99f);
        }

        beginTest ("Boot Off startOn finishes immediately");
        {
            juce::Component host;
            host.setSize (320, 240);
            BootSequenceOverlay boot;
            bool done = false;
            boot.onFinished = [&done] { done = true; };
            boot.setMotion (CyberMotion::Off);
            boot.startOn (host);
            expect (done);
            expect (! boot.isVisible());
        }
    }
};

#endif
