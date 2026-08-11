#include "FactoryPresetLibrary.h"
#include "../core/PluginProcessor.h"
#include "../core/EffectParameters.h"
#include "../core/Config.h"
#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
constexpr const char* kParamKeys[4] = { "paramA", "paramB", "paramC", "paramD" };
constexpr const char* kParamIds[4]  = { EffectParameters::paramA,
                                        EffectParameters::paramB,
                                        EffectParameters::paramC,
                                        EffectParameters::paramD };

void setLinearGainDb(juce::AudioProcessorValueTreeState& apvts,
                     const char* id,
                     float gainDb)
{
    if (auto* p = apvts.getParameter(id))
    {
        const float linear = juce::Decibels::decibelsToGain(gainDb);
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(linear));
    }
}

/** APVTS knobs a–d are always 0–1; DSL maps them via param ranges. */
void setKnobNormalized(juce::AudioProcessorValueTreeState& apvts,
                       const char* id,
                       float minVal,
                       float maxVal,
                       float defaultVal)
{
    if (auto* p = apvts.getParameter(id))
    {
        const float denom = juce::jmax(1.0e-6f, maxVal - minVal);
        const float norm  = juce::jlimit(0.0f, 1.0f, (defaultVal - minVal) / denom);
        p->setValueNotifyingHost(norm);
    }
}
} // namespace

FactoryPresetLibrary& FactoryPresetLibrary::getInstance()
{
    static FactoryPresetLibrary instance;
    return instance;
}

juce::File FactoryPresetLibrary::resolveResourcesDir(const juce::File& hint)
{
    juce::Array<juce::File> candidates;
    if (hint.isDirectory())
        candidates.add(hint);

    const auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    candidates.add(app.getSiblingFile(Config::kResourceFolder));
    candidates.add(app.getParentDirectory().getChildFile(Config::kResourceFolder));
    candidates.add(app.getParentDirectory().getParentDirectory().getChildFile(Config::kResourceFolder));
    candidates.add(juce::File::getCurrentWorkingDirectory().getChildFile(Config::kResourceFolder));

    for (const auto& dir : candidates)
    {
        if (dir.getChildFile("factory_presets.json").existsAsFile())
            return dir;
    }
    return hint;
}

bool FactoryPresetLibrary::loadFromResources(const juce::File& resourcesDir)
{
    entries.clear();

    const auto dir  = resolveResourcesDir(resourcesDir);
    const auto file = dir.getChildFile("factory_presets.json");
    if (! file.existsAsFile())
        return false;

    const auto parsed = json::parse(file.loadFileAsString().toStdString(), nullptr, false);
    if (! parsed.is_array())
        return false;

    for (const auto& item : parsed)
    {
        if (! item.is_object())
            continue;

        FactoryPresetEntry e;
        e.name        = item.value("name", std::string{});
        e.category    = item.value("category", std::string{"Factory"});
        e.description = item.value("description", std::string{});
        e.script      = item.value("script", std::string{});

        if (e.name.isEmpty() || e.script.isEmpty())
            continue;

        for (int i = 0; i < 4; ++i)
        {
            const auto key = kParamKeys[i];
            if (! item.contains(key) || ! item[key].is_object())
                continue;

            const auto& p = item[key];
            const std::string defaultName(1, static_cast<char>('A' + i));
            e.paramNames[i]   = p.value("name", defaultName);
            e.paramMin[i]     = p.value("min", 0.f);
            e.paramMax[i]     = p.value("max", 1.f);
            e.paramDefault[i] = p.value("default", 0.f);
        }

        e.inputGainDb  = item.value("inputGain", 0.f);
        e.outputGainDb = item.value("outputGain", 0.f);
        e.mix          = item.value("mix", 1.f);

        entries.push_back(std::move(e));
    }

    return ! entries.empty();
}

bool FactoryPresetLibrary::applyPreset(NeuroCoreAudioProcessor& processor,
                                       int index,
                                       juce::String& error) const
{
    if (! juce::isPositiveAndBelow(index, (int) entries.size()))
    {
        error = "Invalid factory preset index";
        return false;
    }

    const auto& preset = entries[(size_t) index];

    if (! processor.applyFormula(preset.script, error))
        return false;

    auto& apvts = processor.apvts;
    for (int i = 0; i < 4; ++i)
    {
        processor.setVariableName(i, preset.paramNames[i]);
        setKnobNormalized(apvts, kParamIds[i],
                          preset.paramMin[i], preset.paramMax[i], preset.paramDefault[i]);
    }

    setLinearGainDb(apvts, EffectParameters::inputGain,  preset.inputGainDb);
    setLinearGainDb(apvts, EffectParameters::outputGain, preset.outputGainDb);

    if (auto* p = apvts.getParameter(EffectParameters::dryWet))
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(
            juce::jlimit(0.0f, 1.0f, preset.mix)));

    processor.sendChangeMessage();
    return true;
}