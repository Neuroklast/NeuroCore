#pragma once
#include <JuceHeader.h>
#include "../third_party/nlohmann/json.hpp"
#include <unordered_map>

/** 1–5 star ratings for factory and user presets, keyed by name. */
class PresetRatings
{
public:
    static PresetRatings& getInstance()
    {
        static PresetRatings inst;
        return inst;
    }

    int get (const juce::String& name) const
    {
        if (name.isEmpty())
            return 0;
        const auto it = stars.find (name.toStdString());
        return it == stars.end() ? 0 : juce::jlimit (0, 5, it->second);
    }

    void set (const juce::String& name, int rating)
    {
        if (name.isEmpty())
            return;
        rating = juce::jlimit (0, 5, rating);
        if (rating <= 0)
            stars.erase (name.toStdString());
        else
            stars[name.toStdString()] = rating;
        save();
    }

    void load()
    {
        stars.clear();
        const auto f = ratingsFile();
        if (! f.existsAsFile())
            return;
        auto parsed = nlohmann::json::parse (f.loadFileAsString().toStdString(), nullptr, false);
        if (! parsed.is_object())
            return;
        for (auto it = parsed.begin(); it != parsed.end(); ++it)
            if (it.value().is_number_integer())
                stars[it.key()] = juce::jlimit (0, 5, it.value().get<int>());
    }

private:
    PresetRatings() { load(); }

    static juce::File ratingsFile()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("NEUROKLAST")
                       .getChildFile ("NeuroCore");
        dir.createDirectory();
        return dir.getChildFile ("preset_ratings.json");
    }

    void save() const
    {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& kv : stars)
            j[kv.first] = kv.second;
        ratingsFile().replaceWithText (juce::String (j.dump (2)));
    }

    std::unordered_map<std::string, int> stars;
};
