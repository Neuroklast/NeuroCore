#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

class NeuroCoreAudioProcessor;

/**
    Preset Explorer:
    - Search + category + Factory/User scope
    - Table with description panel
    - Load / Save as (with author) / Delete / New blank
*/
class PresetContentComponent : public juce::Component,
                               private juce::TextEditor::Listener,
                               private juce::ComboBox::Listener
{
public:
    PresetContentComponent (NeuroCoreAudioProcessor& processor, juce::LookAndFeel& lf);
    ~PresetContentComponent() override;

    /** Called after a preset was loaded into the processor. */
    std::function<void()> onLoaded;
    std::function<void()> onClose;
    std::function<void()> onSaved;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void refreshTable();
    void refreshCategories();
    void updateDetail (int row);
    void textEditorTextChanged (juce::TextEditor&) override;
    void comboBoxChanged (juce::ComboBox*) override;
    void saveCurrentAs();

    PresetTableComponent table;
    NeuroCoreAudioProcessor& processor;
    juce::LookAndFeel& lookAndFeel;

    juce::TextEditor searchBox;
    juce::ComboBox categoryBox;
    juce::ComboBox scopeBox;
    juce::Label searchLabel, categoryLabel, scopeLabel;
    juce::Label detailTitle, detailBody;

    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save As..." };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton newBlankButton { "New Blank" };
    juce::TextButton closeButton { "Close" };
};
