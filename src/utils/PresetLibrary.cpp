#include "PresetLibrary.h"
#include "PresetPack.h"
#include "../core/Config.h"
#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
juce::String readPackNameFromManifest (const juce::File& dir)
{
    juce::Array<juce::File> manifests;
    dir.findChildFiles (manifests, juce::File::findFiles, true, "pack.json");
    if (manifests.isEmpty())
        return {};
    const auto text = manifests[0].loadFileAsString();
    auto j = json::parse (text.toStdString(), nullptr, false);
    if (! j.is_object() || ! j.contains ("name") || ! j["name"].is_string())
        return {};
    return juce::String (j["name"].get<std::string>()).trim();
}

void collectNrk (const juce::File& dir, juce::Array<juce::File>& out)
{
    dir.findChildFiles (out, juce::File::findFiles, true,
                        juce::String ("*") + Config::kPresetFileExtension);
}

void copyOne (const juce::File& src, const juce::File& destDir, PresetImportResult& r)
{
    if (! PresetLibrary::isNrkFile (src))
    {
        ++r.skipped;
        return;
    }
    destDir.createDirectory();
    auto dest = PresetLibrary::uniqueDest (destDir, src.getFileName());
    if (src.copyFileTo (dest))
        ++r.imported;
    else
    {
        ++r.skipped;
        r.errors.add ("Could not copy " + src.getFileName());
    }
}

void importDirectory (const juce::File& dir, const juce::File& destRoot,
                      const juce::String& fallbackName, PresetImportResult& r)
{
    juce::Array<juce::File> nrks;
    collectNrk (dir, nrks);
    if (nrks.isEmpty())
    {
        r.errors.add ("No .nrk files in " + dir.getFileName());
        return;
    }

    auto packName = readPackNameFromManifest (dir);
    if (packName.isEmpty())
        packName = fallbackName;
    packName = PresetLibrary::sanitizePackName (packName);

    const bool asPack = nrks.size() > 1 || dir.getChildFile ("pack.json").existsAsFile()
                        || ! readPackNameFromManifest (dir).isEmpty();
    const auto destDir = asPack ? destRoot.getChildFile ("Packs").getChildFile (packName)
                                : destRoot;
    if (asPack)
        r.packName = packName;
    for (auto& f : nrks)
        copyOne (f, destDir, r);
}
} // namespace

juce::File PresetLibrary::userPresetRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile (Config::kUserPresetFolder);
}

juce::File PresetLibrary::packsRoot()
{
    return userPresetRoot().getChildFile ("Packs");
}

juce::String PresetLibrary::sanitizePackName (juce::String name)
{
    name = name.trim();
    const juce::String illegal ("\\/:*?\"<>|");
    for (int i = 0; i < illegal.length(); ++i)
        name = name.replaceCharacter (illegal[i], ' ');
    while (name.contains ("  "))
        name = name.replace ("  ", " ");
    name = name.trim();
    if (name.endsWithChar ('.'))
        name = name.dropLastCharacters (1).trim();
    return name.isNotEmpty() ? name : juce::String ("Pack");
}

juce::File PresetLibrary::uniqueDest (const juce::File& dir, const juce::String& fileName)
{
    auto dest = dir.getChildFile (fileName);
    if (! dest.hasFileExtension (Config::kPresetFileExtension))
        dest = dest.withFileExtension (Config::kPresetFileExtension);
    int n = 2;
    const auto stem = dest.getFileNameWithoutExtension();
    while (dest.existsAsFile())
    {
        dest = dir.getChildFile (stem + " (" + juce::String (n++) + ")"
                                 + Config::kPresetFileExtension);
    }
    return dest;
}

bool PresetLibrary::isNrkFile (const juce::File& f)
{
    return f.existsAsFile() && f.hasFileExtension (Config::kPresetFileExtension);
}

bool PresetLibrary::isPackArchive (const juce::File& f)
{
    return PresetPack::isPackArchive (f);
}

bool PresetLibrary::looksLikePack (const juce::File& dir)
{
    if (! dir.isDirectory())
        return false;
    if (dir.getChildFile ("manifest.json").existsAsFile()
        || dir.getChildFile ("pack.json").existsAsFile())
        return true;
    juce::Array<juce::File> nrks;
    collectNrk (dir, nrks);
    return nrks.size() > 1;
}

PresetImportResult PresetLibrary::importPaths (const juce::StringArray& paths)
{
    return importPathsInto (paths, userPresetRoot());
}

PresetImportResult PresetLibrary::importPathsInto (const juce::StringArray& paths, const juce::File& destRoot)
{
    PresetImportResult r;
    destRoot.createDirectory();
    auto takeInstall = [&] (const PackInstallResult& inst)
    {
        if (inst.ok)
        {
            r.imported += inst.installed;
            if (inst.packName.isNotEmpty())
                r.packName = inst.packName;
            r.errors.addArray (inst.warnings);
        }
        else
        {
            ++r.skipped;
            r.errors.add (inst.error.isNotEmpty() ? inst.error : juce::String ("Import failed"));
        }
    };
    for (const auto& p : paths)
    {
        const juce::File f (p);
        if (f.isDirectory())
        {
            if (looksLikePack (f))
            {
                auto preview = PresetPack::analyzeDirectory (f);
                takeInstall (PresetPack::installPreview (preview, destRoot));
            }
            else
            {
                importDirectory (f, destRoot, f.getFileName(), r);
            }
        }
        else if (isPackArchive (f))
        {
            auto preview = PresetPack::analyzeArchive (f);
            takeInstall (PresetPack::installPreview (preview, destRoot));
        }
        else if (isNrkFile (f))
        {
            copyOne (f, destRoot, r);
        }
        else
        {
            ++r.skipped;
            r.errors.add ("Ignored " + f.getFileName());
        }
    }
    return r;
}

bool PresetLibrary::exportPack (const std::vector<juce::File>& nrkFiles,
                                const juce::File& zipDest,
                                const juce::String& packName,
                                const juce::String& author)
{
    PackManifest m;
    m.name = sanitizePackName (packName);
    m.author = author;
    juce::String error;
    return PresetPack::exportNrkpack (nrkFiles, zipDest, m, error);
}
