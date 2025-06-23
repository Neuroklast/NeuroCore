#pragma once

#include <JuceHeader.h>
#include <unordered_map>

class Localiser
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void languageChanged() = 0;
    };

    static Localiser& getInstance();

    void loadLanguage(const juce::File& localeDir, const juce::String& lang);
    juce::String getCurrentLanguage() const noexcept { return currentLanguage; }

    juce::String translate(const juce::String& key) const;

    void addListener(Listener* l);
    void removeListener(Listener* l);

private:
    Localiser() = default;
    ~Localiser() = default;
    Localiser(const Localiser&) = delete;
    Localiser& operator=(const Localiser&) = delete;

    void loadFile(const juce::File& file,
                  std::unordered_map<juce::String, juce::String>& dest);
    void loadMemory(const void* data, int size,
                    std::unordered_map<juce::String, juce::String>& dest);

    juce::String currentLanguage { "en" };
    std::unordered_map<juce::String, juce::String> english;
    std::unordered_map<juce::String, juce::String> current;
    juce::ListenerList<Listener> listeners;
};

#ifdef TRANS
#undef TRANS
#endif
#define TRANS(x) Localiser::getInstance().translate(x)

