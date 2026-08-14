#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

class NeuroCoreAudioProcessor;

/**
    Preset Explorer: folder list + table + detail (Serum-style browse).
*/
class PresetContentComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               private juce::TextEditor::Listener
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
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    static constexpr int kExplorerSidebarWidth = 208;
    static constexpr int kCategoryRowHeight    = 30;
    static constexpr float kCategoryNameFontPt = 16.5f;
    static constexpr float kCategoryCountFontPt = 13.f;

private:
    class CategoryNav;

    void refreshTable();
    void refreshCategories();
    void restoreBrowserFilters();
    void updateDetail (int row);
    void updateCountLabel();
    void applyScope (int id);
    void applyCategory (const juce::String& cat);
    void textEditorTextChanged (juce::TextEditor&) override;
    void saveCurrentAs();
    void importFromPaths (const juce::StringArray& paths);

    PresetTableComponent table;
    NeuroCoreAudioProcessor& processor;
    juce::LookAndFeel& lookAndFeel;

    juce::TextEditor searchBox;
    juce::Label searchLabel;
    juce::Label countLabel;
    juce::Label folderLabel;
    juce::Label detailTitle, detailMeta, detailBody;
    juce::TextButton scopeAll { "All" };
    juce::TextButton scopeFactory { "Factory" };
    juce::TextButton scopeUser { "User" };
    std::unique_ptr<CategoryNav> folderNav;

    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save As..." };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton newBlankButton { "New Blank" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton importButton { "Import" };
    juce::TextButton closeButton { "Close" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool fileDragActive { false };
};
