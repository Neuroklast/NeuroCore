#include "PresetPack.h"
#include "PresetIdentity.h"
#include "PresetLibrary.h"
#include "../core/Config.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstring>

using json = nlohmann::json;

namespace
{
juce::String currentPluginVersion()
{
    return juce::String (PLUGIN_VERSION);
}

int versionTuple (const juce::String& v) noexcept
{
    auto s = v.trim();
    const int dash = s.indexOfChar ('-');
    if (dash >= 0)
        s = s.substring (0, dash);
    int major = 0, minor = 0, patch = 0;
    auto a = s.upToFirstOccurrenceOf (".", false, false);
    auto rest = s.fromFirstOccurrenceOf (".", false, false);
    auto b = rest.upToFirstOccurrenceOf (".", false, false);
    auto c = rest.fromFirstOccurrenceOf (".", false, false);
    major = a.getIntValue();
    minor = b.getIntValue();
    patch = c.getIntValue();
    return major * 10000 + minor * 100 + patch;
}

bool underRoot (const juce::File& root, const juce::File& child)
{
    const auto r = root.getFullPathName();
    const auto c = child.getFullPathName();
    if (c == r)
        return true;
    const auto sep = juce::File::getSeparatorString();
    return c.startsWith (r.endsWith (sep) ? r : r + sep);
}

json stringArray (const juce::StringArray& a)
{
    json out = json::array();
    for (const auto& s : a)
        if (s.isNotEmpty())
            out.push_back (s.toStdString());
    return out;
}

juce::StringArray fromArray (const json& j)
{
    juce::StringArray out;
    if (! j.is_array())
        return out;
    for (const auto& t : j)
        if (t.is_string())
            out.addIfNotAlreadyThere (juce::String (t.get<std::string>()).trim());
    return out;
}

bool copyTree (const juce::File& src, const juce::File& dest, juce::String& error)
{
    if (! dest.createDirectory() && ! dest.isDirectory())
    {
        error = "Could not create " + dest.getFullPathName();
        return false;
    }
    for (const auto& child : src.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        const auto target = dest.getChildFile (child.getFileName());
        if (child.isDirectory())
        {
            if (! copyTree (child, target, error))
                return false;
        }
        else if (! child.copyFileTo (target))
        {
            error = "Could not copy " + child.getFileName();
            return false;
        }
    }
    return true;
}

juce::File packDestDir (const juce::File& destRoot, const PackManifest& m)
{
    const auto folder = PresetLibrary::sanitizePackName (m.legacy ? m.name : m.id);
    return destRoot.getChildFile ("Packs").getChildFile (folder);
}

void collectCategories (const PackManifest& m, juce::StringArray& cats)
{
    for (const auto& p : m.presets)
        if (p.category.isNotEmpty())
            cats.addIfNotAlreadyThere (p.category);
}

bool validateExtracted (const juce::File& dir, const PackManifest& m, juce::String& error)
{
    if (m.presets.empty())
    {
        juce::Array<juce::File> nrks;
        dir.findChildFiles (nrks, juce::File::findFiles, true, juce::String ("*") + Config::kPresetFileExtension);
        if (nrks.isEmpty())
        {
            error = "Pack contains no .nrk presets";
            return false;
        }
        return true;
    }
    for (const auto& p : m.presets)
    {
        const auto f = dir.getChildFile (p.file);
        if (! f.existsAsFile())
        {
            error = "Missing preset file " + p.file;
            return false;
        }
        if (p.sha256.isNotEmpty() && PresetIdentity::fileSha256 (f) != p.sha256)
        {
            error = "Checksum mismatch for " + p.file;
            return false;
        }
    }
    return true;
}

PackManifest manifestFromNrks (const juce::File& dir, const juce::String& fallbackName)
{
    PackManifest m;
    m.legacy = true;
    m.formatVersion = 0;
    m.name = fallbackName;
    m.id = PresetIdentity::legacyPackId (fallbackName);
    juce::Array<juce::File> nrks;
    dir.findChildFiles (nrks, juce::File::findFiles, true, juce::String ("*") + Config::kPresetFileExtension);
    for (auto& f : nrks)
    {
        PackPresetRef ref;
        ref.id = f.getFileNameWithoutExtension();
        ref.file = PresetIdentity::relativeKey (f, dir);
        m.presets.push_back (ref);
    }
    return m;
}
} // namespace

bool PresetPack::zipPathSafe (const juce::String& raw)
{
    auto p = raw.replaceCharacter ('\\', '/').trim();
    if (p.isEmpty() || p.containsChar ('\0'))
        return false;
    if (p.startsWithChar ('/') || p.startsWith ("//"))
        return false;
    if (p.containsChar (':'))
        return false;
    if (p.contains (".."))
        return false;
    return true;
}

bool PresetPack::isPackArchive (const juce::File& f)
{
    return f.existsAsFile()
        && (f.hasFileExtension (".zip") || f.hasFileExtension (kPackExtension));
}

bool PresetPack::pluginVersionMeets (const juce::String& minimum, const juce::String& current)
{
    if (minimum.trim().isEmpty())
        return true;
    const auto now = current.isNotEmpty() ? current : currentPluginVersion();
    return versionTuple (now) >= versionTuple (minimum);
}

bool PresetPack::parseLegacyPackJson (const juce::String& text, PackManifest& out, juce::String& error)
{
    auto j = json::parse (text.toStdString(), nullptr, false);
    if (! j.is_object())
    {
        error = "Invalid pack.json";
        return false;
    }
    out = {};
    out.legacy = true;
    out.formatVersion = 0;
    out.format = "neurokore-pack";
    if (j.contains ("name") && j["name"].is_string())
        out.name = juce::String (j["name"].get<std::string>()).trim();
    if (j.contains ("author") && j["author"].is_string())
        out.author = juce::String (j["author"].get<std::string>()).trim();
    if (j.contains ("description") && j["description"].is_string())
        out.description = juce::String (j["description"].get<std::string>()).trim();
    if (out.name.isEmpty())
    {
        error = "pack.json needs name";
        return false;
    }
    out.id = PresetIdentity::legacyPackId (out.name);
    return true;
}

bool PresetPack::parseManifestText (const juce::String& text, PackManifest& out, juce::String& error)
{
    auto j = json::parse (text.toStdString(), nullptr, false);
    if (! j.is_object())
    {
        error = "Invalid manifest.json";
        return false;
    }
    if (j.contains ("format") && j["format"].is_string()
        && j["format"].get<std::string>() != "neurokore-pack")
    {
        error = "Unknown pack format";
        return false;
    }
    int ver = 1;
    if (j.contains ("formatVersion"))
    {
        if (! j["formatVersion"].is_number_integer())
        {
            error = "formatVersion must be an integer";
            return false;
        }
        ver = j["formatVersion"].get<int>();
    }
    if (ver > kSupportedFormatVersion)
    {
        error = "Pack format version " + juce::String (ver) + " is not supported";
        return false;
    }
    out = {};
    out.formatVersion = ver;
    out.legacy = false;
    auto req = [&] (const char* key, juce::String& dst) -> bool
    {
        if (! j.contains (key) || ! j[key].is_string() || j[key].get<std::string>().empty())
        {
            error = juce::String ("Manifest missing ") + key;
            return false;
        }
        dst = juce::String (j[key].get<std::string>()).trim();
        return true;
    };
    if (! req ("id", out.id) || ! req ("name", out.name))
        return false;
    if (j.contains ("version") && j["version"].is_string())
        out.version = juce::String (j["version"].get<std::string>()).trim();
    if (j.contains ("author") && j["author"].is_string())
        out.author = juce::String (j["author"].get<std::string>()).trim();
    if (j.contains ("description") && j["description"].is_string())
        out.description = juce::String (j["description"].get<std::string>()).trim();
    if (j.contains ("minimumPluginVersion") && j["minimumPluginVersion"].is_string())
        out.minimumPluginVersion = juce::String (j["minimumPluginVersion"].get<std::string>()).trim();
    if (j.contains ("artwork") && j["artwork"].is_string())
        out.artwork = juce::String (j["artwork"].get<std::string>()).trim();
    if (j.contains ("website") && j["website"].is_string())
        out.website = juce::String (j["website"].get<std::string>()).trim();
    if (j.contains ("presets") && j["presets"].is_array())
    {
        for (const auto& p : j["presets"])
        {
            if (! p.is_object())
                continue;
            PackPresetRef ref;
            if (p.contains ("id") && p["id"].is_string())
                ref.id = juce::String (p["id"].get<std::string>()).trim();
            if (p.contains ("file") && p["file"].is_string())
                ref.file = juce::String (p["file"].get<std::string>()).trim();
            if (p.contains ("category") && p["category"].is_string())
                ref.category = juce::String (p["category"].get<std::string>()).trim();
            if (p.contains ("sha256") && p["sha256"].is_string())
                ref.sha256 = juce::String (p["sha256"].get<std::string>()).trim();
            if (p.contains ("tags"))
                ref.tags = fromArray (p["tags"]);
            if (p.contains ("requiredInputs"))
                ref.requiredInputs = fromArray (p["requiredInputs"]);
            if (ref.file.isEmpty())
            {
                error = "Preset entry needs file";
                return false;
            }
            if (! zipPathSafe (ref.file))
            {
                error = "Illegal preset path " + ref.file;
                return false;
            }
            if (ref.id.isEmpty())
                ref.id = juce::File (ref.file).getFileNameWithoutExtension();
            out.presets.push_back (ref);
        }
    }
    if (! pluginVersionMeets (out.minimumPluginVersion))
    {
        error = "Pack needs plugin " + out.minimumPluginVersion + " or newer";
        return false;
    }
    return true;
}

juce::String PresetPack::manifestToJson (const PackManifest& m)
{
    json j;
    j["format"] = "neurokore-pack";
    j["formatVersion"] = m.formatVersion <= 0 ? 1 : m.formatVersion;
    j["id"] = m.id.toStdString();
    j["name"] = m.name.toStdString();
    j["version"] = m.version.toStdString();
    if (m.author.isNotEmpty())
        j["author"] = m.author.toStdString();
    if (m.description.isNotEmpty())
        j["description"] = m.description.toStdString();
    if (m.minimumPluginVersion.isNotEmpty())
        j["minimumPluginVersion"] = m.minimumPluginVersion.toStdString();
    if (m.artwork.isNotEmpty())
        j["artwork"] = m.artwork.toStdString();
    if (m.website.isNotEmpty())
        j["website"] = m.website.toStdString();
    json presets = json::array();
    for (const auto& p : m.presets)
    {
        json e;
        e["id"] = p.id.toStdString();
        e["file"] = p.file.toStdString();
        if (p.category.isNotEmpty())
            e["category"] = p.category.toStdString();
        if (p.tags.size() > 0)
            e["tags"] = stringArray (p.tags);
        if (p.requiredInputs.size() > 0)
            e["requiredInputs"] = stringArray (p.requiredInputs);
        e["sha256"] = p.sha256.toStdString();
        presets.push_back (e);
    }
    j["presets"] = presets;
    return juce::String (j.dump (2));
}

bool PresetPack::extractSafe (const juce::File& archive, const juce::File& destDir, juce::String& error)
{
    if (! archive.existsAsFile())
    {
        error = "Archive not found";
        return false;
    }
    if (archive.getSize() > kMaxArchiveBytes)
    {
        error = "Archive exceeds 256 MB";
        return false;
    }
    juce::ZipFile zip (archive);
    const int n = zip.getNumEntries();
    if (n <= 0)
    {
        error = "Empty archive";
        return false;
    }
    if (n > kMaxEntries)
    {
        error = "Archive has too many entries";
        return false;
    }
    juce::int64 totalUncompressed = 0;
    for (int i = 0; i < n; ++i)
    {
        const auto* e = zip.getEntry (i);
        if (e == nullptr)
        {
            error = "Corrupt zip entry";
            return false;
        }
        if (! zipPathSafe (e->filename))
        {
            error = "Illegal path in archive: " + e->filename;
            return false;
        }
        if (e->isSymbolicLink)
        {
            error = "Symlinks are not allowed: " + e->filename;
            return false;
        }
        if (e->uncompressedSize < 0 || e->uncompressedSize > kMaxEntryBytes)
        {
            error = "Entry too large: " + e->filename;
            return false;
        }
        totalUncompressed += e->uncompressedSize;
        if (totalUncompressed > kMaxUncompressedTotal)
        {
            error = "Uncompressed size exceeds 512 MB";
            return false;
        }
        const auto out = destDir.getChildFile (e->filename.replaceCharacter ('\\', '/'));
        if (! underRoot (destDir, out))
        {
            error = "Path escaped archive root: " + e->filename;
            return false;
        }
    }
    destDir.createDirectory();
    const auto result = zip.uncompressTo (destDir);
    if (result.failed())
    {
        error = result.getErrorMessage();
        return false;
    }
    return true;
}

namespace
{
PackPreview previewFromDir (const juce::File& dir, const juce::File& source)
{
    PackPreview p;
    p.source = source;
    const auto manifestFile = dir.getChildFile ("manifest.json");
    const auto legacyFile = dir.getChildFile ("pack.json");
    juce::String err;
    if (manifestFile.existsAsFile())
    {
        if (! PresetPack::parseManifestText (manifestFile.loadFileAsString(), p.manifest, err))
        {
            p.error = err;
            return p;
        }
    }
    else if (legacyFile.existsAsFile())
    {
        if (! PresetPack::parseLegacyPackJson (legacyFile.loadFileAsString(), p.manifest, err))
        {
            p.error = err;
            return p;
        }
        p.warnings.add ("Legacy pack.json; export will use manifest.json");
    }
    else
    {
        p.manifest = manifestFromNrks (dir, source.getFileNameWithoutExtension());
        p.warnings.add ("Archive has no manifest; treating as a loose preset pack");
    }
    if (p.manifest.presets.empty())
        p.manifest = manifestFromNrks (dir, p.manifest.name.isNotEmpty() ? p.manifest.name
                                                                        : source.getFileNameWithoutExtension());
    if (! validateExtracted (dir, p.manifest, err))
    {
        p.error = err;
        return p;
    }
    p.presetCount = (int) p.manifest.presets.size();
    collectCategories (p.manifest, p.categories);
    return p;
}
} // namespace

PackPreview PresetPack::analyzeDirectory (const juce::File& dir)
{
    if (! dir.isDirectory())
    {
        PackPreview p;
        p.source = dir;
        p.error = "Not a folder";
        return p;
    }
    return previewFromDir (dir, dir);
}

PackPreview PresetPack::analyzeArchive (const juce::File& archive)
{
    PackPreview p;
    p.source = archive;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("nk-pack-preview-" + juce::String (juce::Random::getSystemRandom().nextInt()));
    tmp.deleteRecursively();
    juce::String err;
    if (! extractSafe (archive, tmp, err))
    {
        p.error = err;
        tmp.deleteRecursively();
        return p;
    }
    auto unpacked = tmp;
    auto children = tmp.findChildFiles (juce::File::findDirectories, false);
    if (children.size() == 1 && tmp.findChildFiles (juce::File::findFiles, false).isEmpty())
        unpacked = children[0];
    p = previewFromDir (unpacked, archive);
    tmp.deleteRecursively();
    return p;
}

PackInstallResult PresetPack::installPreview (const PackPreview& preview, const juce::File& destRoot)
{
    PackInstallResult r;
    r.packId = preview.manifest.id;
    r.packName = preview.manifest.name;
    r.warnings = preview.warnings;
    if (preview.error.isNotEmpty())
    {
        r.error = preview.error;
        return r;
    }
    if (preview.manifest.id.isEmpty() || preview.manifest.name.isEmpty())
    {
        r.error = "Pack is missing id or name";
        return r;
    }

    auto staging = destRoot.getChildFile (".staging-" + juce::String (juce::Random::getSystemRandom().nextInt()));
    staging.deleteRecursively();
    juce::String err;
    juce::File unpacked = staging;
    if (preview.source.isDirectory())
    {
        if (! copyTree (preview.source, staging, err))
        {
            r.error = err;
            staging.deleteRecursively();
            return r;
        }
    }
    else
    {
        if (! extractSafe (preview.source, staging, err))
        {
            r.error = err;
            staging.deleteRecursively();
            return r;
        }
        auto children = staging.findChildFiles (juce::File::findDirectories, false);
        if (children.size() == 1 && staging.findChildFiles (juce::File::findFiles, false).isEmpty())
            unpacked = children[0];
    }

    auto manifest = preview.manifest;
    if (manifest.presets.empty())
        manifest = manifestFromNrks (unpacked, manifest.name);
    if (! validateExtracted (unpacked, manifest, err))
    {
        r.error = err;
        staging.deleteRecursively();
        return r;
    }

    const auto dest = packDestDir (destRoot, manifest);
    dest.getParentDirectory().createDirectory();
    auto backup = dest.getSiblingFile (dest.getFileName() + ".bak-"
                                       + juce::String (juce::Random::getSystemRandom().nextInt()));
    const bool hadDest = dest.isDirectory();
    if (hadDest && ! dest.moveFileTo (backup))
    {
        r.error = "Could not replace existing pack";
        staging.deleteRecursively();
        return r;
    }
    if (! unpacked.moveFileTo (dest))
    {
        if (hadDest)
            backup.moveFileTo (dest);
        r.error = "Could not commit pack install";
        staging.deleteRecursively();
        return r;
    }
    dest.getChildFile ("manifest.json").replaceWithText (manifestToJson (manifest));
    backup.deleteRecursively();
    staging.deleteRecursively();
    r.ok = true;
    r.installed = (int) manifest.presets.size();
    r.packId = manifest.id;
    r.packName = manifest.name;
    return r;
}

bool PresetPack::uninstallPack (const juce::File& destRoot, const juce::String& packId, juce::String& error)
{
    if (packId.isEmpty() || PresetIdentity::isFactoryId (packId))
    {
        error = "Cannot remove this library";
        return false;
    }
    const auto packs = destRoot.getChildFile ("Packs");
    for (const auto& dir : packs.findChildFiles (juce::File::findDirectories, false))
    {
        PackManifest m;
        juce::String err;
        const auto mf = dir.getChildFile ("manifest.json");
        const auto lf = dir.getChildFile ("pack.json");
        bool match = false;
        if (mf.existsAsFile() && parseManifestText (mf.loadFileAsString(), m, err))
            match = m.id == packId;
        else if (lf.existsAsFile() && parseLegacyPackJson (lf.loadFileAsString(), m, err))
            match = m.id == packId;
        else
            match = PresetIdentity::legacyPackId (dir.getFileName()) == packId
                    || dir.getFileName() == packId;
        if (! match)
            continue;
        if (! dir.deleteRecursively())
        {
            error = "Could not delete pack folder";
            return false;
        }
        return true;
    }
    error = "Pack not found";
    return false;
}

bool PresetPack::exportNrkpack (const std::vector<juce::File>& nrkFiles,
                               const juce::File& dest,
                               PackManifest manifest,
                               juce::String& error)
{
    if (nrkFiles.empty())
    {
        error = "No presets to export";
        return false;
    }
    if (manifest.name.isEmpty())
        manifest.name = "Pack";
    if (manifest.id.isEmpty())
        manifest.id = "net.neuroklast." + PresetIdentity::slug (manifest.name) + "-"
                      + PresetIdentity::newUuid().substring (0, 8);
    if (manifest.version.isEmpty())
        manifest.version = "1.0.0";
    if (manifest.minimumPluginVersion.isEmpty())
        manifest.minimumPluginVersion = currentPluginVersion();
    manifest.format = "neurokore-pack";
    manifest.formatVersion = 1;
    manifest.legacy = false;
    manifest.presets.clear();

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("nk-pack-out-" + juce::String (juce::Random::getSystemRandom().nextInt()));
    tmp.deleteRecursively();
    const auto presetDir = tmp.getChildFile ("presets");
    presetDir.createDirectory();

    juce::ZipFile::Builder builder;
    for (const auto& f : nrkFiles)
    {
        if (! PresetLibrary::isNrkFile (f))
            continue;
        const auto name = f.getFileName();
        const auto copied = presetDir.getChildFile (name);
        if (! f.copyFileTo (copied))
        {
            error = "Could not stage " + name;
            tmp.deleteRecursively();
            return false;
        }
        PackPresetRef ref;
        ref.id = f.getFileNameWithoutExtension();
        ref.file = "presets/" + name;
        ref.sha256 = PresetIdentity::fileSha256 (copied);
        manifest.presets.push_back (ref);
        builder.addFile (copied, 9, ref.file);
    }
    if (manifest.presets.empty())
    {
        error = "No .nrk files to export";
        tmp.deleteRecursively();
        return false;
    }
    const auto mf = tmp.getChildFile ("manifest.json");
    mf.replaceWithText (manifestToJson (manifest));
    builder.addFile (mf, 9, "manifest.json");

    auto outFile = dest;
    if (! outFile.hasFileExtension (kPackExtension) && ! outFile.hasFileExtension (".zip"))
        outFile = outFile.withFileExtension (kPackExtension);
    juce::TemporaryFile staging (outFile);
    {
        juce::FileOutputStream os (staging.getFile());
        if (! os.openedOk() || ! builder.writeToStream (os, nullptr))
        {
            error = "Could not write pack";
            tmp.deleteRecursively();
            return false;
        }
        os.flush();
    }
    tmp.deleteRecursively();
    if (! staging.overwriteTargetFileWithTemporary())
    {
        error = "Could not replace destination";
        return false;
    }
    return true;
}
