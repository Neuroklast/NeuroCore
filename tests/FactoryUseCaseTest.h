#pragma once

#include <JuceHeader.h>
#include "../src/core/Config.h"
#include "../src/dsl/SignalChain.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "TestHelpers.h"
#include <cmath>

#ifndef NEUROKORE_RESOURCES_DIR
#define NEUROKORE_RESOURCES_DIR "resources"
#endif

class FactoryUseCaseTest : public juce::UnitTest
{
public:
    FactoryUseCaseTest() : juce::UnitTest ("FactoryUseCaseTest", "Presets") {}

    void runTest() override
    {
        beginTest ("Sidechain Duck Bass is tagged sidechain, not vocoder");
        auto& lib = FactoryPresetLibrary::getInstance();
        if (lib.getEntries().empty())
            expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
        const auto* duck = find (lib, "Sidechain Duck Bass");
        expect (duck != nullptr, "missing Sidechain Duck Bass");
        if (duck != nullptr)
        {
            expect (duck->tags.contains ("sidechain", true));
            expect (! duck->tags.contains ("vocoder", true));
        }

        beginTest ("native factory facet labels keep the explorer's canonical case");
        {
            const auto* acid = find (lib, "Acid Line");
            const auto* drum = find (lib, "909 Newstyle");
            const auto* guitar = find (lib, "Tight Lead Stack");
            expect (acid != nullptr);
            expect (drum != nullptr);
            expect (guitar != nullptr);
            if (acid != nullptr)
            {
                expect (acid->soundSources.contains ("Synth", false));
                expect (acid->processingTypes.contains ("Saturation", false));
                expect (acid->processingTypes.contains ("Modulation", false));
                expect (! acid->soundSources.contains ("synth", false));
            }
            if (drum != nullptr)
            {
                expect (drum->soundSources.contains ("Drums", false));
                expect (drum->processingTypes.contains ("Filter", false));
            }
            if (guitar != nullptr)
            {
                expect (guitar->soundSources.contains ("Guitar", false));
                expect (guitar->processingTypes.contains ("Distortion", false));
            }

            int sourced = 0, typed = 0;
            for (const auto& entry : lib.getEntries())
            {
                if (! entry.soundSources.isEmpty())
                    ++sourced;
                if (! entry.processingTypes.isEmpty())
                    ++typed;
            }
            // Mesa High Gain is intentionally untouched by the metadata migration.
            expectEquals (sourced, (int) lib.getEntries().size() - 1);
            expectEquals (typed, (int) lib.getEntries().size() - 1);
        }

        beginTest ("Sidechain Duck Bass ducks when the sidechain is loud");
        {
            if (duck == nullptr)
                return;
            const float open = runDuck (*duck, false);
            const float ducked = runDuck (*duck, true);
            expect (ducked < open * 0.9f,
                    "duck=" + juce::String (ducked, 4) + " open=" + juce::String (open, 4));
        }

        beginTest ("Reese Mid Growl keeps an 80 Hz sub centred");
        {
            const auto* e = find (lib, "Reese Mid Growl");
            expect (e != nullptr);
            if (e == nullptr)
                return;
            float peak = 0.f, diff = 0.f;
            runTone (*e, 80.f, 0.4f, peak, diff);
            expect (peak > 0.05f);
            expect (diff < peak * 0.2f, "L-R=" + juce::String (diff, 4) + " peak=" + juce::String (peak, 4));
        }

        beginTest ("2026-10-23 templates parse and pass a mid tone");
        {
            const char* names[] = {
                "Poly Synth Motion", "Velocity Driven Drive", "Hybrid Dynamic Wah",
                "Subsafe Groove Bass", "Resonant Drum Body", "Dynamic Hat Tamer",
                "Triggered Room Send", "Patterned Delay Throws", "Vocal Presence Duck",
                "Metered Drive Template"
            };
            for (const char* name : names)
            {
                const auto* e = find (lib, name);
                expect (e != nullptr, name);
                if (e == nullptr)
                    continue;
                dsl::SignalChain chain;
                expect (load (chain, *e), juce::String (name) + " parse");
                if (juce::String (name) == "Hybrid Dynamic Wah")
                    continue;
                float peak = 0.f, diff = 0.f;
                runTone (*e, 440.f, 0.4f, peak, diff);
                expect (peak > 0.02f, juce::String (name) + " peak=" + juce::String (peak, 4));
                juce::ignoreUnused (diff);
            }
        }

        beginTest ("Vocal Presence Duck ducks when the sidechain is loud");
        {
            const auto* e = find (lib, "Vocal Presence Duck");
            expect (e != nullptr);
            if (e != nullptr)
            {
                const float open = runDuck (*e, false, 2000.f);
                const float ducked = runDuck (*e, true, 2000.f);
                expect (open > 0.02f, "open=" + juce::String (open, 4));
                expect (ducked < open * 0.95f,
                        "duck=" + juce::String (ducked, 4) + " open=" + juce::String (open, 4));
            }
        }

        beginTest ("Subsafe Groove Bass keeps an 80 Hz sub centred");
        {
            const auto* e = find (lib, "Subsafe Groove Bass");
            expect (e != nullptr);
            if (e != nullptr)
            {
                float peak = 0.f, diff = 0.f;
                runTone (*e, 80.f, 0.4f, peak, diff);
                expect (peak > 0.05f);
                expect (diff < peak * 0.2f, "L-R=" + juce::String (diff, 4) + " peak=" + juce::String (peak, 4));
            }
        }

        beginTest ("Loudness Clip Master holds a hot tone at or under unity");
        {
            const auto* e = find (lib, "Loudness Clip Master");
            expect (e != nullptr);
            if (e == nullptr)
                return;
            float peak = 0.f, diff = 0.f;
            runTone (*e, 440.f, 0.98f, peak, diff);
            expect (peak > 0.2f);
            expect (peak <= 1.05f, "peak=" + juce::String (peak, 4));
            juce::ignoreUnused (diff);
        }
    }

private:
    static const FactoryPresetEntry* find (const FactoryPresetLibrary& lib, const juce::String& name)
    {
        for (const auto& e : lib.getEntries())
            if (e.name == name)
                return &e;
        return nullptr;
    }

    static std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobPtrs (
        std::array<juce::SmoothedValue<float>, Config::kNumUserParams>& knobs)
    {
        std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> ptrs {};
        for (int i = 0; i < Config::kNumUserParams; ++i)
        {
            knobs[(size_t) i].reset (48000.0, 0.01);
            knobs[(size_t) i].setCurrentAndTargetValue (0.65f);
            ptrs[(size_t) i] = &knobs[(size_t) i];
        }
        return ptrs;
    }

    static bool load (dsl::SignalChain& chain, const FactoryPresetEntry& e)
    {
        juce::String err;
        if (! chain.loadScript (e.script, err))
            return false;
        chain.prepare ({ 48000.0, 512, 2 });
        return true;
    }

    static float runDuck (const FactoryPresetEntry& e, bool kick, float hz = 55.f)
    {
        dsl::SignalChain chain;
        if (! load (chain, e))
            return 0.f;
        std::array<juce::SmoothedValue<float>, Config::kNumUserParams> knobs {};
        auto ptrs = knobPtrs (knobs);
        juce::AudioBuffer<float> main (2, 512);
        juce::AudioBuffer<float> sc (2, 512);
        double acc = 0.0;
        int n = 0;
        for (int b = 0; b < 32; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = (float) (b * 512 + i) / 48000.f;
                const float bass = 0.35f * std::sin (2.f * juce::MathConstants<float>::pi * hz * t);
                main.setSample (0, i, bass);
                main.setSample (1, i, bass);
                const float k = kick && std::fmod (t, 0.5f) < 0.05f ? 0.95f : 0.f;
                sc.setSample (0, i, k);
                sc.setSample (1, i, k);
            }
            chain.setExternalSidechain (sc.getReadPointer (0), sc.getReadPointer (1), 512);
            chain.processBlockSmoothed (main, ptrs);
            if (b < 12)
                continue;
            for (int i = 0; i < 512; ++i)
            {
                const float v = main.getSample (0, i);
                acc += (double) v * (double) v;
                ++n;
            }
        }
        return n > 0 ? (float) std::sqrt (acc / (double) n) : 0.f;
    }

    static void runTone (const FactoryPresetEntry& e, float hz, float amp, float& peak, float& diff)
    {
        peak = 0.f;
        diff = 0.f;
        dsl::SignalChain chain;
        if (! load (chain, e))
            return;
        std::array<juce::SmoothedValue<float>, Config::kNumUserParams> knobs {};
        auto ptrs = knobPtrs (knobs);
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 24; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = (float) (b * 512 + i) / 48000.f;
                const float x = amp * std::sin (2.f * juce::MathConstants<float>::pi * hz * t);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
            }
            chain.processBlockSmoothed (buf, ptrs);
            if (b < 8)
                continue;
            for (int i = 0; i < 512; ++i)
            {
                const float l = buf.getSample (0, i);
                const float r = buf.getSample (1, i);
                peak = juce::jmax (peak, juce::jmax (std::abs (l), std::abs (r)));
                diff = juce::jmax (diff, std::abs (l - r));
            }
        }
    }
};
