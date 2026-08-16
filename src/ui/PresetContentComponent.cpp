#include "PresetContentComponent.h"
#include "PluginLookAndFeel.h"
#include "../core/PluginProcessor.h"
#include "../core/Config.h"
#include "../utils/Localiser.h"
#include "../utils/FactoryPresetLibrary.h"
#include "../utils/PresetLibrary.h"
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
                 .getChildFile (Config::kAppDataFolder)
                 .getChildFile ("last_author.txt");
    if (f.existsAsFile())
        return f.loadFileAsString().trim();
    return {};
}

void storeAuthorPreference (const juce::String& author)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("NEUROKLAST")
                   .getChildFile (Config::kAppDataFolder);
    dir.createDirectory();
    dir.getChildFile ("last_author.txt").replaceWithText (author.trim());
}
} // namespace

class PresetContentComponent::CategoryNav : public juce::ListBox,
                                            public juce::ListBoxModel
{
public:
    struct Row { juce::String name; int count { 0 }; };

    CategoryNav()
    {
        setModel (this);
        setRowHeight (PresetContentComponent::kCategoryRowHeight);
        setOutlineThickness (0);
        setColour (juce::ListBox::backgroundColourId, NeuroKoreLookAndFeel::surface());
        setColour (juce::ListBox::outlineColourId, NeuroKoreLookAndFeel::panelBorder());
    }

    void setRows (juce::Array<Row> next, const juce::String& selectedName)
    {
        rows = std::move (next);
        int pick = 0;
        for (int i = 0; i < rows.size(); ++i)
            if (rows.getReference (i).name.equalsIgnoreCase (selectedName))
                pick = i;
        updateContent();
        selectRow (pick);
    }

    juce::String selectedCategory() const
    {
        const int r = getSelectedRow();
        if (! juce::isPositiveAndBelow (r, rows.size()))
            return {};
        return rows.getReference (r).name;
    }

    std::function<void(juce::String)> onPick;

    int getNumRows() override { return rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, rows.size()))
            return;
        const auto& item = rows.getReference (row);
        NeuroKoreLookAndFeel::paintSelectableRow (g, w, h, selected, row % 2);
        g.setColour (NeuroKoreLookAndFeel::ink());
        g.setFont (NeuroKoreLookAndFeel::monoFont (PresetContentComponent::kCategoryNameFontPt));
        const auto label = item.name.isEmpty() ? juce::String ("All") : item.name;
        g.drawText (label, 10, 0, w - 52, h, juce::Justification::centredLeft, true);
        g.setColour (NeuroKoreLookAndFeel::mutedText());
        g.setFont (NeuroKoreLookAndFeel::monoFont (PresetContentComponent::kCategoryCountFontPt));
        g.drawText (juce::String (item.count), 0, 0, w - 10, h,
                    juce::Justification::centredRight, false);
    }

    void selectedRowsChanged (int) override
    {
        if (onPick)
            onPick (selectedCategory());
    }

private:
    juce::Array<Row> rows;
};

PresetContentComponent::PresetContentComponent (NeuroKoreAudioProcessor& proc, juce::LookAndFeel& lf)
    : table (proc), processor (proc), lookAndFeel (lf)
{
    juce::ignoreUnused (lookAndFeel);
    setWantsKeyboardFocus (true);

    searchLabel.setText ("Search", juce::dontSendNotification);
    folderLabel.setText ("Folders", juce::dontSendNotification);
    countLabel.setText ({}, juce::dontSendNotification);
    countLabel.setJustificationType (juce::Justification::centredRight);
    for (auto* l : { &searchLabel, &folderLabel, &countLabel })
    {
        l->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::mutedText());
        l->setFont (NeuroKoreLookAndFeel::monoFont (11.f));
        addAndMakeVisible (*l);
    }

    searchBox.setFont (NeuroKoreLookAndFeel::monoFont (14.f));
    searchBox.setTextToShowWhenEmpty ("Search name, tags, formula (delay, kick, techno)...",
                                      NeuroKoreLookAndFeel::mutedText());
    searchBox.setColour (juce::TextEditor::backgroundColourId, NeuroKoreLookAndFeel::surfaceHigh());
    searchBox.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour (juce::TextEditor::outlineColourId, NeuroKoreLookAndFeel::panelBorder());
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    auto styleChip = [] (juce::TextButton& b)
    {
        b.setClickingTogglesState (true);
        b.setRadioGroupId (0x50524553);
        b.setColour (juce::TextButton::buttonColourId, NeuroKoreLookAndFeel::surfaceHigh());
        b.setColour (juce::TextButton::buttonOnColourId, NeuroKoreLookAndFeel::accent());
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    };
    styleChip (scopeAll);
    styleChip (scopeFactory);
    styleChip (scopeUser);
    scopeAll.setToggleState (true, juce::dontSendNotification);
    scopeAll.onClick = [this] { applyScope (1); };
    scopeFactory.onClick = [this] { applyScope (2); };
    scopeUser.onClick = [this] { applyScope (3); };
    addAndMakeVisible (scopeAll);
    addAndMakeVisible (scopeFactory);
    addAndMakeVisible (scopeUser);

    folderNav = std::make_unique<CategoryNav>();
    folderNav->onPick = [this] (const juce::String& cat) { applyCategory (cat); };
    addAndMakeVisible (*folderNav);

    addAndMakeVisible (table);
    table.onSelectionChanged = [this] (int row)
    {
        updateDetail (row);
        updateCountLabel();
    };

    detailTitle.setFont (NeuroKoreLookAndFeel::monoFont (18.f));
    detailTitle.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
    detailTitle.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (detailTitle);

    detailMeta.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
    detailMeta.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::mutedText());
    detailMeta.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (detailMeta);

    detailBody.setFont (NeuroKoreLookAndFeel::monoFont (13.f));
    detailBody.setColour (juce::Label::textColourId, juce::Colour (0xffd0d4dc));
    detailBody.setJustificationType (juce::Justification::topLeft);
    detailBody.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (detailBody);

    auto styleBtn = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, NeuroKoreLookAndFeel::surfaceHigh());
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    };
    for (auto* b : { &loadButton, &saveButton, &deleteButton, &newBlankButton,
                     &exportButton, &importButton, &closeButton })
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
    exportButton.onClick = [this]
    {
        const int row = table.getSelectedRow();
        if (row < 0)
            return;
        const auto name = table.getNameForRow (row);
        auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile (name + Config::kPresetFileExtension);
        fileChooser = std::make_unique<juce::FileChooser> (
            "Export preset or pack", start,
            "*" + juce::String (Config::kPresetFileExtension) + ";*.zip");
        constexpr int flags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;
        fileChooser->launchAsync (flags, [this, row] (const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest == juce::File())
                return;
            if (dest.hasFileExtension (".zip"))
            {
                auto files = table.getVisibleUserFiles();
                if (files.empty())
                {
                    detailBody.setText ("A .zip pack needs visible user presets. Filter Scope to User first.",
                                        juce::dontSendNotification);
                    return;
                }
                const auto packName = dest.getFileNameWithoutExtension();
                if (PresetLibrary::exportPack (files, dest, packName, lastAuthorPreference()))
                    detailBody.setText ("Exported pack (" + juce::String ((int) files.size())
                                        + " presets).", juce::dontSendNotification);
                return;
            }
            if (! dest.hasFileExtension (Config::kPresetFileExtension))
                dest = dest.withFileExtension (Config::kPresetFileExtension);
            if (table.isFactoryRow (row))
            {
                processor.presetManager.savePreset (
                    dest, table.getNameForRow (row), table.getAuthorForRow (row),
                    table.getCategoryForRow (row),
                    table.getTagsForRow (row).joinIntoString (","));
            }
            else
            {
                auto src = table.getFileForRow (row);
                if (src.existsAsFile())
                    src.copyFileTo (dest);
            }
        });
    };
    importButton.setTooltip ("Import .nrk, a folder, or a .zip pack");
    importButton.onClick = [this]
    {
        auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        fileChooser = std::make_unique<juce::FileChooser> (
            "Import presets or pack", start,
            "*" + juce::String (Config::kPresetFileExtension) + ";*.zip");
        constexpr int flags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectDirectories
                            | juce::FileBrowserComponent::canSelectMultipleItems;
        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            juce::StringArray paths;
            for (const auto& f : fc.getResults())
                paths.add (f.getFullPathName());
            importFromPaths (paths);
        });
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
    if (folderNav)
        folderNav->onPick = nullptr;
}

void PresetContentComponent::refreshTable()
{
    table.refresh();
    refreshCategories();
}

void PresetContentComponent::restoreBrowserFilters()
{
    const int scopeId = processor.getLastPresetBrowserScope();
    const int id = (scopeId >= 1 && scopeId <= 3) ? scopeId : 1;
    scopeAll.setToggleState (id == 1, juce::dontSendNotification);
    scopeFactory.setToggleState (id == 2, juce::dontSendNotification);
    scopeUser.setToggleState (id == 3, juce::dontSendNotification);
    table.setScope (id == 2 ? PresetTableComponent::Scope::Factory
                  : id == 3 ? PresetTableComponent::Scope::User
                            : PresetTableComponent::Scope::All);
    table.setCategory (processor.getLastPresetBrowserCategory());
}

void PresetContentComponent::refreshCategories()
{
    if (folderNav == nullptr)
        return;
    juce::Array<CategoryNav::Row> rows;
    rows.add ({ {}, table.countInScope ({}) });
    for (auto& c : table.getAllCategories())
    {
        const int n = table.countInScope (c);
        if (n > 0)
            rows.add ({ c, n });
    }
    const auto keep = processor.getLastPresetBrowserCategory();
    folderNav->onPick = nullptr;
    folderNav->setRows (rows, keep);
    folderNav->onPick = [this] (const juce::String& cat) { applyCategory (cat); };
    updateCountLabel();
}

void PresetContentComponent::applyScope (int id)
{
    table.setScope (id == 2 ? PresetTableComponent::Scope::Factory
                  : id == 3 ? PresetTableComponent::Scope::User
                            : PresetTableComponent::Scope::All);
    processor.setLastPresetBrowserScope (id);
    refreshCategories();
    updateDetail (table.getSelectedRow());
}

void PresetContentComponent::applyCategory (const juce::String& cat)
{
    table.setCategory (cat);
    processor.setLastPresetBrowserCategory (cat);
    updateCountLabel();
    updateDetail (table.getSelectedRow());
}

void PresetContentComponent::updateCountLabel()
{
    countLabel.setText (juce::String (table.getFilteredCount())
                            + " of " + juce::String (table.countInScope ({})),
                        juce::dontSendNotification);
}

void PresetContentComponent::updateDetail (int row)
{
    if (row < 0)
    {
        detailTitle.setText ("Select a preset", juce::dontSendNotification);
        detailMeta.setText ("Folders on the left. Double-click or press Load.",
                            juce::dontSendNotification);
        detailBody.setText ("Import accepts .nrk, a folder, or a .zip pack.",
                            juce::dontSendNotification);
        return;
    }
    detailTitle.setText (table.getNameForRow (row), juce::dontSendNotification);
    const auto author = table.getAuthorForRow (row);
    juce::String meta = table.getCategoryForRow (row);
    meta += table.isFactoryRow (row) ? "   ·   Factory" : "   ·   User";
    if (author.isNotEmpty())
        meta += "   ·   " + author;
    const auto tags = table.getTagsForRow (row);
    if (tags.size() > 0)
        meta += "   ·   " + tags.joinIntoString ("  ");
    detailMeta.setText (meta, juce::dontSendNotification);
    auto desc = table.getDescriptionForRow (row);
    if (desc.isEmpty())
        desc = "No description.";
    detailBody.setText (desc, juce::dontSendNotification);
}

void PresetContentComponent::textEditorTextChanged (juce::TextEditor& ed)
{
    if (&ed == &searchBox)
    {
        table.setSearch (searchBox.getText());
        updateCountLabel();
        updateDetail (table.getSelectedRow());
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
    aw->addTextEditor ("tags", {}, "Tags (comma: tape, vocal, send)");
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
        auto tags = aw->getTextEditorContents ("tags").trim();
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
        if (processor.presetManager.savePreset (file, name, author, category, tags))
        {
            processor.setCurrentPresetName (name);
            refreshTable();
            if (onSaved)
                onSaved();
        }
    }));
}

void PresetContentComponent::importFromPaths (const juce::StringArray& paths)
{
    if (paths.isEmpty())
        return;
    const auto r = PresetLibrary::importPaths (paths);
    refreshTable();
    juce::String msg = "Imported " + juce::String (r.imported);
    if (r.packName.isNotEmpty())
        msg += " into pack \"" + r.packName + "\"";
    msg += ".";
    if (r.skipped > 0)
        msg += " Skipped " + juce::String (r.skipped) + ".";
    if (r.errors.size() > 0)
        msg += " " + r.errors[0];
    detailBody.setText (msg, juce::dontSendNotification);
    if (r.packName.isNotEmpty())
        table.setCategory ({});
}

bool PresetContentComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& p : files)
    {
        const juce::File f (p);
        if (f.isDirectory() || PresetLibrary::isNrkFile (f) || PresetLibrary::isPackArchive (f))
            return true;
    }
    return false;
}

void PresetContentComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    fileDragActive = true;
    repaint();
}

void PresetContentComponent::fileDragExit (const juce::StringArray&)
{
    fileDragActive = false;
    repaint();
}

void PresetContentComponent::filesDropped (const juce::StringArray& files, int, int)
{
    fileDragActive = false;
    repaint();
    importFromPaths (files);
}

void PresetContentComponent::paint (juce::Graphics& g)
{
    if (! fileDragActive)
        return;
    g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.18f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.f), 6.f);
    g.setColour (NeuroKoreLookAndFeel::accent());
    g.setFont (NeuroKoreLookAndFeel::brandFont (16.f, true));
    g.drawFittedText ("Drop .nrk, a folder, or a .zip pack",
                      getLocalBounds(), juce::Justification::centred, 1);
}

void PresetContentComponent::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (50);
    auto lab = top.removeFromTop (14);
    searchLabel.setBounds (lab.removeFromLeft (lab.getWidth() / 2));
    countLabel.setBounds (lab);

    auto tools = top;
    auto chips = tools.removeFromRight (juce::jmin (280, tools.getWidth() / 3));
    const int cw = chips.getWidth() / 3;
    scopeAll.setBounds (chips.removeFromLeft (cw).reduced (2, 2));
    scopeFactory.setBounds (chips.removeFromLeft (cw).reduced (2, 2));
    scopeUser.setBounds (chips.reduced (2, 2));
    searchBox.setBounds (tools.reduced (0, 2));

    auto bottom = r.removeFromBottom (40);
    const int bw = bottom.getWidth() / 7;
    loadButton.setBounds (bottom.removeFromLeft (bw).reduced (2));
    saveButton.setBounds (bottom.removeFromLeft (bw).reduced (2));
    newBlankButton.setBounds (bottom.removeFromLeft (bw).reduced (2));
    deleteButton.setBounds (bottom.removeFromLeft (bw).reduced (2));
    exportButton.setBounds (bottom.removeFromLeft (bw).reduced (2));
    importButton.setBounds (bottom.removeFromLeft (bw).reduced (2));
    closeButton.setBounds (bottom.reduced (2));

    r.removeFromBottom (6);
    auto detail = r.removeFromBottom (112);
    detailTitle.setBounds (detail.removeFromTop (22));
    detailMeta.setBounds (detail.removeFromTop (18));
    detailBody.setBounds (detail);
    r.removeFromBottom (6);

    auto side = r.removeFromLeft (kExplorerSidebarWidth);
    folderLabel.setBounds (side.removeFromTop (16));
    if (folderNav)
        folderNav->setBounds (side);
    r.removeFromLeft (8);
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
