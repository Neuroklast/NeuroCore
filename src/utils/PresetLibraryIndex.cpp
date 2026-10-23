#include "PresetLibraryIndex.h"
#include "FactoryPresetLibrary.h"
#include "PresetIdentity.h"
#include "PresetLibrary.h"
#include "PresetManager.h"
#include "PresetPack.h"
#include "../core/Config.h"

void PresetLibraryIndex::clear()
{
    items.clear();
}

void PresetLibraryIndex::append (PresetIndexEntry entry)
{
    items.push_back (std::move (entry));
}

const PresetIndexEntry* PresetLibraryIndex::findById (const juce::String& id) const noexcept
{
    if (id.isEmpty())
        return nullptr;
    for (const auto& e : items)
        if (e.presetId == id)
            return &e;
    return nullptr;
}

const PresetIndexEntry* PresetLibraryIndex::findByName (const juce::String& name, const juce::String& packId) const noexcept
{
    const PresetIndexEntry* fallback = nullptr;
    for (const auto& e : items)
    {
        if (e.displayName != name)
            continue;
        if (packId.isNotEmpty() && e.packId == packId)
            return &e;
        if (fallback == nullptr)
            fallback = &e;
    }
    return packId.isEmpty() ? fallback : nullptr;
}

std::vector<PresetIndexEntry> PresetLibraryIndex::matching (const juce::String& query) const
{
    const auto needle = query.trim().toLowerCase();
    if (needle.isEmpty())
        return items;
    std::vector<PresetIndexEntry> out;
    for (const auto& e : items)
    {
        auto hay = (e.displayName + " " + e.author + " " + e.category + " " + e.description
                    + " " + e.tags.joinIntoString (" ") + " " + e.origin).toLowerCase();
        if (hay.contains (needle))
            out.push_back (e);
    }
    return out;
}

void PresetLibraryIndex::rebuild (PresetManager& mgr, FactoryPresetLibrary& factory, const juce::File& userRoot)
{
    items.clear();
    if (factory.getEntries().empty())
        factory.loadFromEmbedded();
    for (const auto& f : factory.getEntries())
    {
        PresetIndexEntry e;
        e.presetId = PresetIdentity::factoryId (f.name);
        e.displayName = f.name;
        e.author = "Neuroklast";
        e.category = f.category;
        e.description = f.description;
        e.tags = f.tags;
        e.requiredInputs = f.requiredInputs;
        e.soundSources = f.soundSources;
        e.processingTypes = f.processingTypes;
        e.style = f.style;
        e.role = f.role;
        e.sourceType = "factory";
        e.origin = "Factory";
        items.push_back (std::move (e));
    }

    auto addNrk = [&] (const juce::File& file, const juce::String& sourceType,
                       const juce::String& packId, const juce::String& origin)
    {
        if (! PresetLibrary::isNrkFile (file))
            return;
        PresetManager::Info info;
        mgr.readInfo (file, info);
        PresetIndexEntry e;
        e.presetId = info.id.isNotEmpty() ? info.id : PresetIdentity::legacyIdFor (file, userRoot);
        e.packId = packId.isNotEmpty() ? packId : info.packId;
        e.displayName = info.name.isNotEmpty() ? info.name : file.getFileNameWithoutExtension();
        e.author = info.author;
        e.category = info.category.isNotEmpty() ? info.category : juce::String ("User");
        e.description = info.description;
        e.tags = info.tags;
        e.requiredInputs = info.requiredInputs;
        e.sourceType = sourceType;
        e.origin = origin;
        e.filePath = file.getFullPathName();
        e.formatVersion = info.formatVersion;
        e.minimumPluginVersion = info.minimumPluginVersion;
        items.push_back (std::move (e));
    };

    if (! userRoot.isDirectory())
        return;

    for (const auto& f : userRoot.findChildFiles (juce::File::findFiles, false,
                                                 juce::String ("*") + Config::kPresetFileExtension))
        addNrk (f, "user", {}, "User");

    const auto packs = userRoot.getChildFile ("Packs");
    for (const auto& dir : packs.findChildFiles (juce::File::findDirectories, false))
    {
        PackManifest m;
        juce::String err;
        juce::String packId;
        juce::String origin = dir.getFileName();
        const auto mf = dir.getChildFile ("manifest.json");
        const auto lf = dir.getChildFile ("pack.json");
        if (mf.existsAsFile() && PresetPack::parseManifestText (mf.loadFileAsString(), m, err))
        {
            packId = m.id;
            origin = m.name.isNotEmpty() ? m.name : origin;
        }
        else if (lf.existsAsFile() && PresetPack::parseLegacyPackJson (lf.loadFileAsString(), m, err))
        {
            packId = m.id;
            origin = m.name.isNotEmpty() ? m.name : origin;
        }
        else
        {
            packId = PresetIdentity::legacyPackId (dir.getFileName());
        }
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, true,
                                                juce::String ("*") + Config::kPresetFileExtension))
            addNrk (f, "pack", packId, origin);
    }
}
