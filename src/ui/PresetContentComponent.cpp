#include "PresetContentComponent.h"
#include "PluginLookAndFeel.h"
#include "../core/PluginProcessor.h"
#include "../core/Config.h"
#include "../utils/Localiser.h"
#include "../utils/FactoryPresetLibrary.h"
#include "../core/EffectParameters.h"

namespace {
class ModalCallback : public juce::ModalComponentManager::Callback
{
public:
    explicit ModalCallback (std::function<void(int)> cb) : callback (std::move (cb)) {}
    void modalStateFinished (int result) override { if (callback) callback (result); }
private:
    std::function<void(int)> callback;
};

juce::String lastAuthorPreference()
{
    auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("NEUROKLAST")
                 .getChildFile ("NeuroCore")
                 .getChildFile ("last_author.txt");
    if (f.existsAsFile())
        return f.loadFileAsString().trim();
    return {};
}

void storeAuthorPreference (const juce::String& author)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("NEUROKLAST")
                   .getChildFile ("NeuroCore");
    dir.createDirectory();
    dir.getChildFile ("last_author.txt").replaceWithText (author.trim());
}
} // namespace

PresetContentComponent::PresetContentComponent (NeuroCoreAudioProcessor& proc, juce::LookAndFeel& lf)
    : table (proc), processor (proc), lookAndFeel (lf)
{
    juce::ignoreUnused (lookAndFeel);
    setWantsKeyboardFocus (true);

    searchLabel.setText ("Search", juce::dontSendNotification);
    categoryLabel.setText ("Category", juce::dontSendNotification);
    scopeLabel.setText ("Scope", juce::dontSendNotification);
    for (auto* l : { &searchLabel, &categoryLabel, &scopeLabel })
    {
        l->setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::mutedText());
        l->setFont (NeuroCoreLookAndFeel::brandFont (11.f));
        addAndMakeVisible (*l);
    }

    searchBox.setTextToShowWhenEmpty ("Search name, category, description...",
                                      NeuroCoreLookAndFeel::mutedText());
    searchBox.setColour (juce::TextEditor::backgroundColourId, NeuroCoreLookAndFeel::surfaceHigh());
    searchBox.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour (juce::TextEditor::outlineColourId, NeuroCoreLookAndFeel::panelBorder());
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    categoryBox.addItem ("All categories", 1);
    categoryBox.setSelectedId (1, juce::dontSendNotification);
    categoryBox.addListener (this);
    addAndMakeVisible (categoryBox);

    scopeBox.addItem ("All", 1);
    scopeBox.addItem ("Factory", 2);
    scopeBox.addItem ("User", 3);
    scopeBox.setSelectedId (1, juce::dontSendNotification);
    scopeBox.addListener (this);
    addAndMakeVisible (scopeBox);

    addAndMakeVisible (table);
    table.onSelectionChanged = [this] (int row) { updateDetail (row); };

    detailTitle.setFont (NeuroCoreLookAndFeel::brandFont (16.f, true));
    detailTitle.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    detailTitle.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (detailTitle);

    detailBody.setFont (NeuroCoreLookAndFeel::monoFont (12.5f));
    detailBody.setColour (juce::Label::textColourId, juce::Colour (0xffd0d4dc));
    detailBody.setJustificationType (juce::Justification::topLeft);
    detailBody.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (detailBody);

    auto styleBtn = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, NeuroCoreLookAndFeel::surfaceHigh());
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    };
    for (auto* b : { &loadButton, &saveButton, &deleteButton, &newBlankButton, &closeButton })
    {
        styleBtn (*b);
        addAndMakeVisible (*b);
    }

    auto loadSelected = [this]
    {
        const int row = table.getSelectedRow();
        if (row < 0)
            return;
        if (table.isFactoryRow (row))
        {
            juce::String err;
            const int fi = table.getFactoryIndexForRow (row);
            if (! FactoryPresetLibrary::getInstance().applyPreset (processor, fi, err))
                return;
            processor.setCurrentPresetName (table.getNameForRow (row));
        }
        else
        {
            auto file = table.getFileForRow (row);
            if (! file.existsAsFile() || ! processor.presetManager.loadPreset (file))
                return;
            processor.setCurrentPresetName (table.getNameForRow (row));
        }
        if (auto* p = processor.apvts.getParameter (EffectParameters::outputGain))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (1.0f));
        processor.sendChangeMessage();
        if (onLoaded)
            onLoaded();
    };
    loadButton.onClick = loadSelected;
    table.onRowActivated = [loadSelected] (int) { loadSelected(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    saveButton.onClick = [this] { saveCurrentAs(); };
    newBlankButton.onClick = [this]
    {
        processor.setCurrentPresetName ({});
        juce::String err;
        processor.applyFormula ("// New preset\nparam a = Drive [0.5, 4.0]\nstage1: y = softclip(x, a)\n", err, true);
        processor.setCurrentPresetName ("Untitled");
        if (onSaved) onSaved();
        if (onClose) onClose();
    };
    deleteButton.onClick = [this]
    {
        auto row = table.getSelectedRow();
        if (row < 0 || table.isFactoryRow (row)) return;
        auto file = table.getFileForRow (row);
        if (! file.exists()) return;
        auto* aw = new juce::AlertWindow ("Delete Preset",
                                          "Delete user preset \"" + table.getNameForRow (row) + "\"?",
                                          juce::AlertWindow::WarningIcon);
        aw->addButton ("Delete", 1);
        aw->addButton ("Cancel", 0);
        aw->enterModalState (true, new ModalCallback ([this, file, aw] (int result)
        {
            std::unique_ptr<juce::AlertWindow> cleanup (aw);
            if (result == 1)
            {
                file.deleteFile();
                refreshTable();
            }
        }));
    };

    refreshCategories();
    restoreBrowserFilters();
    refreshTable();
    updateDetail (table.getSelectedRow());
}

PresetContentComponent::~PresetContentComponent()
{
    searchBox.removeListener (this);
    categoryBox.removeListener (this);
    scopeBox.removeListener (this);
}

void PresetContentComponent::refreshTable()
{
    table.refresh();
    refreshCategories();
}

void PresetContentComponent::restoreBrowserFilters()
{
    const auto cat = processor.getLastPresetBrowserCategory();
    int pick = 1;
    if (cat.isNotEmpty())
    {
        for (int i = 0; i < categoryBox.getNumItems(); ++i)
            if (categoryBox.getItemText (i).equalsIgnoreCase (cat))
                pick = categoryBox.getItemId (i);
    }
    categoryBox.setSelectedId (pick, juce::dontSendNotification);
    table.setCategory (pick == 1 ? juce::String() : categoryBox.getText());

    const int scopeId = processor.getLastPresetBrowserScope();
    scopeBox.setSelectedId (scopeId, juce::dontSendNotification);
    table.setScope (scopeId == 2 ? PresetTableComponent::Scope::Factory
                  : scopeId == 3 ? PresetTableComponent::Scope::User
                                 : PresetTableComponent::Scope::All);
}

void PresetContentComponent::refreshCategories()
{
    const auto selected = categoryBox.getText();
    categoryBox.clear (juce::dontSendNotification);
    categoryBox.addItem ("All categories", 1);
    int id = 2;
    for (auto& c : table.getAllCategories())
        categoryBox.addItem (c, id++);
    int pick = 1;
    for (int i = 0; i < categoryBox.getNumItems(); ++i)
        if (categoryBox.getItemText (i) == selected)
            pick = categoryBox.getItemId (i);
    categoryBox.setSelectedId (pick, juce::dontSendNotification);
}

void PresetContentComponent::updateDetail (int row)
{
    if (row < 0)
    {
        detailTitle.setText ("Select a preset", juce::dontSendNotification);
        detailBody.setText ("Double-click or press Load. Save As... stores Name + Author for artist packs.",
                            juce::dontSendNotification);
        return;
    }
    const auto author = table.getAuthorForRow (row);
    detailTitle.setText (table.getNameForRow (row)
                             + "  |  " + table.getCategoryForRow (row)
                             + (table.isFactoryRow (row) ? "  |  Factory" : "  |  User")
                             + (author.isNotEmpty() ? ("  |  " + author) : juce::String()),
                         juce::dontSendNotification);
    auto desc = table.getDescriptionForRow (row);
    if (desc.isEmpty())
        desc = "No description.";
    detailBody.setText (desc, juce::dontSendNotification);
}

void PresetContentComponent::textEditorTextChanged (juce::TextEditor& ed)
{
    if (&ed == &searchBox)
        table.setSearch (searchBox.getText());
}

void PresetContentComponent::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &categoryBox)
    {
        const auto t = categoryBox.getText();
        const auto cat = t.startsWithIgnoreCase ("All") ? juce::String() : t;
        table.setCategory (cat);
        processor.setLastPresetBrowserCategory (cat);
    }
    else if (box == &scopeBox)
    {
        const int id = scopeBox.getSelectedId();
        table.setScope (id == 2 ? PresetTableComponent::Scope::Factory
                      : id == 3 ? PresetTableComponent::Scope::User
                                : PresetTableComponent::Scope::All);
        processor.setLastPresetBrowserScope (id);
    }
}

void PresetContentComponent::saveCurrentAs()
{
    auto* aw = new juce::AlertWindow ("Save User Preset",
                                      "Save the current formula + knobs as a user preset (for artist packs).",
                                      juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", processor.getCurrentPresetName().isNotEmpty()
                                   ? processor.getCurrentPresetName()
                                   : "My Preset",
                       "Name");
    const auto prevAuthor = lastAuthorPreference();
    aw->addTextEditor ("author", prevAuthor.isNotEmpty() ? prevAuthor : "NEUROKLAST",
                       "Author");
    aw->addTextEditor ("category", "User", "Category");
    aw->addButton ("OK", 1);
    aw->addButton ("Cancel", 0);
    aw->enterModalState (true, new ModalCallback ([this, aw] (int result)
    {
        std::unique_ptr<juce::AlertWindow> cleanup (aw);
        if (result != 1)
            return;
        auto name = aw->getTextEditorContents ("name").trim();
        auto author = aw->getTextEditorContents ("author").trim();
        auto category = aw->getTextEditorContents ("category").trim();
        if (name.isEmpty())
            name = "My Preset";
        if (author.isEmpty())
            author = "NEUROKLAST";
        if (category.isEmpty())
            category = "User";
        storeAuthorPreference (author);
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile (Config::kUserPresetFolder);
        dir.createDirectory();
        auto file = dir.getChildFile (name).withFileExtension (Config::kPresetFileExtension);
        if (processor.presetManager.savePreset (file, name, author, category))
        {
            processor.setCurrentPresetName (name);
            refreshTable();
            if (onSaved)
                onSaved();
        }
    }));
}

void PresetContentComponent::paint (juce::Graphics&)
{
}

void PresetContentComponent::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (52);
    auto labH = 14;
    auto row1 = top.removeFromTop (labH);
    searchLabel.setBounds (row1.removeFromLeft (top.getWidth() / 2));
    categoryLabel.setBounds (row1.removeFromLeft (top.getWidth() / 4));
    scopeLabel.setBounds (row1);
    auto row2 = top;
    searchBox.setBounds (row2.removeFromLeft (getWidth() / 2 - 20).reduced (0, 2));
    categoryBox.setBounds (row2.removeFromLeft (getWidth() / 4 - 10).reduced (4, 2));
    scopeBox.setBounds (row2.reduced (4, 2));

    auto bottom = r.removeFromBottom (44);
    const int bw = bottom.getWidth() / 5;
    loadButton.setBounds (bottom.removeFromLeft (bw).reduced (3));
    saveButton.setBounds (bottom.removeFromLeft (bw).reduced (3));
    newBlankButton.setBounds (bottom.removeFromLeft (bw).reduced (3));
    deleteButton.setBounds (bottom.removeFromLeft (bw).reduced (3));
    closeButton.setBounds (bottom.reduced (3));

    r.removeFromBottom (6);
    auto detail = r.removeFromBottom (100);
    detailTitle.setBounds (detail.removeFromTop (24));
    detailBody.setBounds (detail);
    r.removeFromBottom (6);
    table.setBounds (r);
}

bool PresetContentComponent::keyPressed (const juce::KeyPress& kp)
{
    if (kp == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    if (kp == juce::KeyPress::returnKey)
    {
        loadButton.triggerClick();
        return true;
    }
    return false;
}
