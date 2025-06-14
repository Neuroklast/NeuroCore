#include "PresetManager.h"
#include "PluginProcessor.h"
#include "ExpressionEvaluator.h"

using json = nlohmann::json;

PresetManager::PresetManager(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{}

std::string PresetManager::encrypt(const std::string& text) const
{
    std::string out = text;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] ^= key[(int) (i % key.size())];
    return out;
}

std::string PresetManager::decrypt(const std::string& text) const
{
    return encrypt(text); // symmetric xor
}

bool PresetManager::savePreset(const juce::File& file, const juce::String& name)
{
    json j;
    j["name"] = name.toStdString();
    j["version"] = PLUGIN_VERSION;
    j["formula"] = processor.getEvaluator().getFormula();

    json vars = json::array();
    for (auto& v : processor.getVariableNames())
        vars.push_back(v.toStdString());
    j["variables"] = vars;

    json params;
    for (auto* p : processor.apvts.getParameters())
        params[p->getName(100).toStdString()] = p->getValue();
    j["parameters"] = params;

    auto data = j.dump();
    auto enc = encrypt(data);
    if (file.existsAsFile()) file.deleteFile();
    return file.replaceWithText(juce::String(enc));
}

bool PresetManager::loadPreset(const juce::File& file)
{
    auto enc = file.loadFileAsString().toStdString();
    if (enc.empty())
        return false;
    auto dec = decrypt(enc);
    json j = json::parse(dec, nullptr, false);
    if (j.is_discarded())
        return false;

    if (j.contains("formula"))
        processor.setFormula(j["formula"].get<std::string>());

    if (j.contains("variables"))
    {
        auto vars = j["variables"];
        for (size_t i = 0; i < std::min(vars.size(), processor.getVariableNames().size()); ++i)
            processor.setVariableName((int)i, vars[i].get<std::string>());
    }

    if (j.contains("parameters"))
    {
        auto params = j["parameters"];
        for (auto* p : processor.apvts.getParameters())
        {
            auto id = p->getName(100).toStdString();
            if (params.contains(id))
                p->setValueNotifyingHost(params[id].get<float>());
        }
    }
    return true;
}

std::vector<juce::File> PresetManager::getAvailablePresets(const juce::File& dir) const
{
    std::vector<juce::File> result;
    if (!dir.exists()) return result;
    juce::DirectoryIterator iter(dir, false, juce::String("*") + Config::kPresetFileExtension, juce::File::TypesOfFileToFind::findFiles);
    while (iter.next())
        result.push_back(iter.getFile());
    return result;
}


