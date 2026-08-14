#include "PresetLibrary.h"
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
    return f.existsAsFile() && f.hasFileExtension (".zip");
}

PresetImportResult PresetLibrary::importPaths (const juce::StringArray& paths)
{
    return importPathsInto (paths, userPresetRoot());
}

PresetImportResult PresetLibrary::importPathsInto (const juce::StringArray& paths, const juce::File& destRoot)
{
    PresetImportResult r;
    destRoot.createDirectory();
    for (const auto& p : paths)
    {
        const juce::File f (p);
        if (f.isDirectory())
        {
            importDirectory (f, destRoot, f.getFileName(), r);
        }
        else if (isPackArchive (f))
        {
            auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("neurocore-pack-"
                                          + juce::String (juce::Random::getSystemRandom().nextInt()));
            tmp.deleteRecursively();
            tmp.createDirectory();
            juce::ZipFile zip (f);
            if (zip.uncompressTo (tmp).failed())
            {
                r.errors.add ("Could not read zip " + f.getFileName());
                tmp.deleteRecursively();
                continue;
            }
            importDirectory (tmp, destRoot, f.getFileNameWithoutExtension(), r);
            tmp.deleteRecursively();
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
    if (nrkFiles.empty() || zipDest == juce::File())
        return false;

    const auto folder = sanitizePackName (packName);
    json meta;
    meta["name"] = folder.toStdString();
    if (author.isNotEmpty())
        meta["author"] = author.toStdString();
    meta["format"] = "neurocore-pack";

    auto tmpDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("neurocore-pack-out-"
                                     + juce::String (juce::Random::getSystemRandom().nextInt()));
    tmpDir.createDirectory();
    const auto manifest = tmpDir.getChildFile ("pack.json");
    manifest.replaceWithText (juce::String (meta.dump (2)));

    juce::ZipFile::Builder builder;
    builder.addFile (manifest, 9, folder + "/pack.json");
    for (const auto& f : nrkFiles)
    {
        if (! isNrkFile (f))
            continue;
        builder.addFile (f, 9, folder + "/" + f.getFileName());
    }

    auto dest = zipDest;
    if (! dest.hasFileExtension (".zip"))
        dest = dest.withFileExtension (".zip");
    dest.deleteFile();
    juce::FileOutputStream os (dest);
    if (! os.openedOk())
    {
        tmpDir.deleteRecursively();
        return false;
    }
    const bool ok = builder.writeToStream (os, nullptr);
    os.flush();
    tmpDir.deleteRecursively();
    return ok;
}
