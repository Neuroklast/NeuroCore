#include "FactoryPresetLibrary.h"
#include "PresetSearch.h"
#include "../core/PluginProcessor.h"
#include "../core/EffectParameters.h"
#include "../core/Config.h"
#include "../third_party/nlohmann/json.hpp"
#include <BinaryData.h>

using json = nlohmann::json;

namespace
{
constexpr const char* kParamKeys[8] = {
    "paramA", "paramB", "paramC", "paramD",
    "paramE", "paramF", "paramG", "paramH"
};

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

bool parseFactoryPresetsJson(const juce::String& text, std::vector<FactoryPresetEntry>& out)
{
    out.clear();

    const auto parsed = json::parse(text.toStdString(), nullptr, false);
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

        if (item.contains ("tags") && item["tags"].is_array())
        {
            for (const auto& t : item["tags"])
                if (t.is_string())
                    PresetSearch::addTag (e.tags, t.get<std::string>());
        }
        e.tags = PresetSearch::mergeTags (e.tags,
                                          PresetSearch::inferTags (e.script, e.name,
                                                                   e.category, e.description));

        if (e.name.isEmpty() || e.script.isEmpty())
            continue;

        for (int i = 0; i < Config::kNumUserParams; ++i)
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

        out.push_back(std::move(e));
    }

    return ! out.empty();
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

    // currentApplicationFile = the .vst3 module binary when hosted (e.g. Cubase)
    const auto moduleFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    const auto moduleDir  = moduleFile.getParentDirectory();

    // Flat layout: <dir>/NeuroCore.vst3 + <dir>/resources/
    candidates.add(moduleFile.getSiblingFile(Config::kResourceFolder));
    candidates.add(moduleDir.getChildFile(Config::kResourceFolder));

    // Bundle layout: NeuroCore.vst3/Contents/x86_64-win/NeuroCore.vst3
    // resources next to the binary, or under Contents/Resources
    candidates.add(moduleDir.getChildFile(Config::kResourceFolder));
    candidates.add(moduleDir.getParentDirectory().getChildFile("Resources"));
    candidates.add(moduleDir.getParentDirectory().getChildFile(Config::kResourceFolder));
    candidates.add(moduleDir.getParentDirectory().getParentDirectory().getChildFile(Config::kResourceFolder));

    // Common Files / plugin folder copies
    candidates.add(moduleDir.getParentDirectory().getParentDirectory()
                       .getChildFile(Config::kResourceFolder));

    candidates.add(juce::File::getCurrentWorkingDirectory().getChildFile(Config::kResourceFolder));

    // Walk a few parents looking for resources/factory_presets.json
    auto walk = moduleDir;
    for (int i = 0; i < 5; ++i)
    {
        candidates.add(walk.getChildFile(Config::kResourceFolder));
        walk = walk.getParentDirectory();
    }

    for (const auto& dir : candidates)
    {
        if (dir.getChildFile("factory_presets.json").existsAsFile())
            return dir;
    }
    return hint;
}

const FactoryPresetEntry* FactoryPresetLibrary::findByName (const juce::String& name) const noexcept
{
    for (const auto& e : entries)
        if (e.name == name)
            return &e;
    return nullptr;
}

bool FactoryPresetLibrary::loadFromEmbedded()
{
    if (BinaryData::factory_presets_jsonSize <= 0)
        return false;

    const juce::String text = juce::String::fromUTF8(
        BinaryData::factory_presets_json,
        BinaryData::factory_presets_jsonSize);

    return parseFactoryPresetsJson(text, entries);
}

bool FactoryPresetLibrary::loadFromResources(const juce::File& resourcesDir)
{
    entries.clear();

    // Embedded JSON matches this binary. A leftover resources/ folder next to
    // an installed VST3 is often stale (no comments) and must not win.
    if (loadFromEmbedded())
        return true;

    const auto dir  = resolveResourcesDir(resourcesDir);
    const auto file = dir.getChildFile("factory_presets.json");
    if (file.existsAsFile())
    {
        // Always UTF-8 — Windows system codepage would corrupt the JSON
        juce::MemoryBlock mb;
        juce::String text;
        if (file.loadFileAsData (mb) && mb.getSize() > 0)
        {
            auto* raw = static_cast<const char*> (mb.getData());
            int n = (int) mb.getSize();
            int off = 0;
            if (n >= 3 && (uint8_t) raw[0] == 0xEF && (uint8_t) raw[1] == 0xBB && (uint8_t) raw[2] == 0xBF)
                off = 3;
            text = juce::String::fromUTF8 (raw + off, n - off);
        }
        if (text.isNotEmpty() && parseFactoryPresetsJson (text, entries) && ! entries.empty())
            return true;
        entries.clear();
    }

    // Hosts (Cubase etc.) often install only the .vst3 binary without resources/.
    // Fall back to the JSON embedded in the plugin binary.
    return loadFromEmbedded();
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

    // Keep preset name: applyFormula(clear=false) must not wipe the active label
    if (! processor.applyFormula (preset.script, error, false))
        return false;

    processor.setCurrentPresetName (preset.name);

    auto& apvts = processor.apvts;
    for (int i = 0; i < Config::kNumUserParams; ++i)
    {
        if (preset.paramNames[i].isNotEmpty())
            processor.setVariableName(i, preset.paramNames[i]);
        setKnobNormalized(apvts, EffectParameters::userParams[i],
                          preset.paramMin[i], preset.paramMax[i], preset.paramDefault[i]);
    }

    setLinearGainDb(apvts, EffectParameters::inputGain,  preset.inputGainDb);
    // Output gain is no longer a user control — always unity. Loudness is
    // handled by auto-gain + the single Gain (input) knobs partially.
    setLinearGainDb(apvts, EffectParameters::outputGain, 0.0f);

    if (auto* p = apvts.getParameter(EffectParameters::dryWet))
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(
            juce::jlimit(0.0f, 1.0f, preset.mix)));

    // Refresh editor (formula, knob labels, current preset name).
    // Only notify synchronously on the message thread — never post a raw
    // stack/reference capture (processor may be destroyed before delivery).
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
        if (mm->isThisTheMessageThread())
            processor.sendChangeMessage();
    return true;
}
