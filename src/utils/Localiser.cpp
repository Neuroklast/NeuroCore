#include "Localiser.h"

Localiser& Localiser::getInstance()
{
    static Localiser instance;
    return instance;
}

void Localiser::loadFile(const juce::File& file,
                         std::unordered_map<juce::String, juce::String>& dest)
{
    dest.clear();
    if (auto in = file.createInputStream())
    {
        juce::StringArray lines;
        lines.addLines(in->readEntireStreamAsString());
        for (auto& line : lines)
        {
            auto trimmed = line.trim();
            if (trimmed.isEmpty() || trimmed.startsWithChar('#'))
                continue;
            auto pos = trimmed.indexOfChar('=');
            if (pos >= 0)
            {
                auto key = trimmed.substring(0, pos).trim();
                auto value = trimmed.substring(pos + 1).trim();
                dest[key] = value;
            }
        }
    }
}

void Localiser::loadMemory(const void* data, int size,
                           std::unordered_map<juce::String, juce::String>& dest)
{
    dest.clear();
    juce::StringArray lines;
    lines.addLines (juce::String::fromUTF8 (static_cast<const char*>(data), size));
    for (auto& line : lines)
    {
        auto trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWithChar('#'))
            continue;
        auto pos = trimmed.indexOfChar('=');
        if (pos >= 0)
        {
            auto key = trimmed.substring(0, pos).trim();
            auto value = trimmed.substring(pos + 1).trim();
            dest[key] = value;
        }
    }
}

void Localiser::loadLanguage(const juce::File& localeDir, const juce::String& lang)
{
    auto fallbackFile = localeDir.getChildFile("en.txt");
    if (fallbackFile.existsAsFile())
        loadFile(fallbackFile, english);
    else
        loadMemory(BinaryData::en_txt, BinaryData::en_txtSize, english);

    juce::File langFile = localeDir.getChildFile(lang + ".txt");
    if (!langFile.existsAsFile() && lang.startsWithIgnoreCase("de"))
        langFile = localeDir.getChildFile("de.txt");

    if (langFile.existsAsFile())
        loadFile(langFile, current);
    else if (lang.startsWithIgnoreCase("de"))
        loadMemory(BinaryData::de_txt, BinaryData::de_txtSize, current);
    else
        current = english;

    currentLanguage = langFile.existsAsFile() ? langFile.getFileNameWithoutExtension()
                                             : (lang.startsWithIgnoreCase("de") ? "de" : "en");
    listeners.call(&Listener::languageChanged);
}

juce::String Localiser::translate(const juce::String& key) const
{
    if (auto it = current.find(key); it != current.end())
        return it->second;
    if (auto it = english.find(key); it != english.end())
        return it->second;
    return key;
}

void Localiser::addListener(Listener* l)
{
    listeners.add(l);
}

void Localiser::removeListener(Listener* l)
{
    listeners.remove(l);
}

