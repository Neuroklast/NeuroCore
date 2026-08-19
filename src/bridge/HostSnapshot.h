#pragma once

#include <JuceHeader.h>

class NeuroKoreAudioProcessor;

namespace bridge
{

struct ParamGesture
{
    juce::String id;
    float value { 0.f };
    juce::String phase { "change" }; // begin | change | end
};

struct PresetCmd
{
    juce::String action; // load | save | prev | next | new
    juce::String name;
    juce::String author;
    juce::String category;
    juce::String tags;
};

struct ChoiceCmd
{
    juce::String id; // os | polisher | input
    int index { 0 };
};

int footerCpu (float load) noexcept;
const char* modeWord (bool safe, bool bypass, bool live) noexcept;
int osFactorFromIndex (int index) noexcept;

bool paramGestureFromVar (const juce::var& v, ParamGesture& out, juce::String& error);
bool presetCmdFromVar (const juce::var& v, PresetCmd& out, juce::String& error);
bool choiceCmdFromVar (const juce::var& v, ChoiceCmd& out, juce::String& error);

bool applyParamGesture (juce::AudioProcessorValueTreeState& apvts, const ParamGesture& g, juce::String& error);
bool applyChoice (NeuroKoreAudioProcessor& proc, const ChoiceCmd& c, juce::String& error);
bool applyPresetCmd (NeuroKoreAudioProcessor& proc, const PresetCmd& c, juce::String& error);

juce::var paramsVar (NeuroKoreAudioProcessor& proc);
juce::var hostVar (NeuroKoreAudioProcessor& proc);
juce::var presetStateVar (NeuroKoreAudioProcessor& proc);
juce::var licenseVar (NeuroKoreAudioProcessor& proc);
juce::var irVar (NeuroKoreAudioProcessor& proc);
juce::var catalogVar();

} // namespace bridge
