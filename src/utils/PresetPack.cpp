#include "PresetPack.h"
#include "PresetIdentity.h"
#include "PresetLibrary.h"
#include "PresetManager.h"
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

juce::File resolvedFile (juce::File f)
{
    int hops = 0;
    while (f.isSymbolicLink() && hops++ < 8)
    {
        const auto next = f.getLinkedTarget();
        if (next.getFullPathName().isEmpty())
            break;
        f = next;
    }
    return f;
}

juce::String pathKey (const juce::File& f)
{
    auto p = resolvedFile (f).getFullPathName();
#if JUCE_WINDOWS
    p = p.replaceCharacter ('/', '\\').toLowerCase();
#endif
    return p;
}

bool underRoot (const juce::File& root, const juce::File& child)
{
    const auto r = pathKey (root);
    const auto c = pathKey (child);
    if (c == r)
        return true;
    const auto sep = juce::File::getSeparatorString();
    return c.startsWith (r.endsWith (sep) ? r : r + sep);
}

bool isReservedDeviceName (const juce::String& name)
{
    const auto stem = name.upToFirstOccurrenceOf (".", false, false).trim().toUpperCase();
    static const char* const kDev[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    for (auto* d : kDev)
        if (stem == d)
            return true;
    return false;
}

bool isValidPackFolderName (const juce::String& name)
{
    if (name.isEmpty() || name.length() > 100)
        return false;
    if (name == "." || name == ".." || name.contains (".."))
        return false;
    if (name.startsWithChar ('.') || name.endsWithChar ('.') || name.endsWithChar (' '))
        return false;
    if (name.containsChar ('\0') || name.containsChar ('/') || name.containsChar ('\\')
        || name.containsChar (':') || name.containsChar ('*') || name.containsChar ('?')
        || name.containsChar ('"') || name.containsChar ('<') || name.containsChar ('>')
        || name.containsChar ('|'))
        return false;
    return ! isReservedDeviceName (name);
}

bool resolvePackDest (const juce::File& destRoot, const PackManifest& m, juce::File& dest, juce::String& error)
{
    juce::String folder;
    if (m.legacy)
    {
        folder = PresetLibrary::sanitizePackName (m.name);
        if (! isValidPackFolderName (folder))
        {
            error = "Invalid pack folder";
            return false;
        }
    }
    else
    {
        if (! PresetPack::isValidPackId (m.id))
        {
            error = "Invalid pack id";
            return false;
        }
        folder = m.id;
    }

    const auto packs = destRoot.getChildFile ("Packs");
    dest = packs.getChildFile (folder);
    if (dest.getFileName() != folder
        || dest.getParentDirectory().getFullPathName() != packs.getFullPathName())
    {
        error = "Pack folder escaped Packs";
        return false;
    }
    if (dest.exists() && ! underRoot (packs, dest))
    {
        error = "Pack folder escaped Packs";
        return false;
    }
    return true;
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
    if (src.isSymbolicLink())
    {
        error = "Symlinks are not allowed";
        return false;
    }
    if (! dest.createDirectory() && ! dest.isDirectory())
    {
        error = "Could not create " + dest.getFullPathName();
        return false;
    }
    for (const auto& child : src.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        if (child.isSymbolicLink())
        {
            error = "Symlinks are not allowed: " + child.getFileName();
            return false;
        }
        const auto target = dest.getChildFile (child.getFileName());
        if (! underRoot (dest, target))
        {
            error = "Path escaped pack folder: " + child.getFileName();
            return false;
        }
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
    juce::StringArray parts;
    parts.addTokens (p, "/", "");
    for (const auto& part : parts)
        if (part.isNotEmpty() && isReservedDeviceName (part))
            return false;
    return true;
}

bool PresetPack::isValidPackId (const juce::String& id)
{
    if (id.isEmpty() || id.length() > 128 || id.containsChar ('\0'))
        return false;
    const auto first = id[0];
    const bool firstOk = (first >= 'A' && first <= 'Z')
                         || (first >= 'a' && first <= 'z')
                         || (first >= '0' && first <= '9');
    if (! firstOk)
        return false;
    for (int i = 0; i < id.length(); ++i)
    {
        const auto c = id[i];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                        || c == '.' || c == '_' || c == '-';
        if (! ok)
            return false;
    }
    if (id.contains ("..") || id.startsWithChar ('.') || id.endsWithChar ('.'))
        return false;
    return ! isReservedDeviceName (id);
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
    if (! isValidPackId (out.id))
    {
        error = "Invalid pack id";
        return false;
    }
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
    for (const auto& f : destDir.findChildFiles (juce::File::findFilesAndDirectories, true))
    {
        if (f.isSymbolicLink())
        {
            error = "Symlinks are not allowed: " + f.getFileName();
            return false;
        }
        if (! underRoot (destDir, f))
        {
            error = "Path escaped archive root: " + f.getFileName();
            return false;
        }
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
    if (destRoot.exists() && ! destRoot.isDirectory())
    {
        r.error = "Library path is not a folder";
        return r;
    }

    juce::File dest;
    juce::String err;
    if (! resolvePackDest (destRoot, preview.manifest, dest, err))
    {
        r.error = err;
        return r;
    }
    if (dest.exists() && ! dest.isDirectory())
    {
        r.error = "Pack destination is not a folder";
        return r;
    }

    auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("nk-pack-install-" + juce::Uuid().toDashedString());
    work.deleteRecursively();
    auto cleanupWork = [&] { work.deleteRecursively(); };

    juce::File unpacked = work;
    if (preview.source.isDirectory())
    {
        if (! copyTree (preview.source, work, err))
        {
            r.error = err;
            cleanupWork();
            return r;
        }
    }
    else
    {
        if (! extractSafe (preview.source, work, err))
        {
            r.error = err;
            cleanupWork();
            return r;
        }
        auto children = work.findChildFiles (juce::File::findDirectories, false);
        if (children.size() == 1 && work.findChildFiles (juce::File::findFiles, false).isEmpty())
            unpacked = children[0];
    }

    auto manifest = preview.manifest;
    if (manifest.presets.empty())
        manifest = manifestFromNrks (unpacked, manifest.name);
    if (! pluginVersionMeets (manifest.minimumPluginVersion))
    {
        r.error = "Pack needs plugin " + manifest.minimumPluginVersion + " or newer";
        cleanupWork();
        return r;
    }
    if (! validateExtracted (unpacked, manifest, err))
    {
        r.error = err;
        cleanupWork();
        return r;
    }
    if (! resolvePackDest (destRoot, manifest, dest, err))
    {
        r.error = err;
        cleanupWork();
        return r;
    }

    const auto mf = unpacked.getChildFile ("manifest.json");
    if (! mf.replaceWithText (manifestToJson (manifest)))
    {
        r.error = "Could not write manifest";
        cleanupWork();
        return r;
    }
    PackManifest verified;
    juce::String verifyErr;
    if (! parseManifestText (mf.loadFileAsString(), verified, verifyErr)
        || verified.id != manifest.id
        || verified.presets.size() != manifest.presets.size())
    {
        r.error = verifyErr.isNotEmpty() ? verifyErr : juce::String ("Manifest verification failed");
        cleanupWork();
        return r;
    }
    if (! validateExtracted (unpacked, verified, err))
    {
        r.error = err;
        cleanupWork();
        return r;
    }

    const auto packs = dest.getParentDirectory();
    if (! packs.createDirectory() && ! packs.isDirectory())
    {
        r.error = "Could not create Packs folder";
        cleanupWork();
        return r;
    }

    juce::File backup;
    const bool hadDest = dest.isDirectory();
    if (hadDest)
    {
        backup = destRoot.getChildFile (".pack-bak-" + juce::Uuid().toDashedString());
        backup.deleteRecursively();
        if (! dest.moveFileTo (backup))
        {
            r.error = "Could not replace existing pack";
            cleanupWork();
            return r;
        }
    }

    auto restorePrevious = [&]() -> bool
    {
        if (dest.exists())
            dest.deleteRecursively();
        if (! hadDest)
            return ! dest.exists();
        return backup.moveFileTo (dest);
    };

    bool committed = unpacked.moveFileTo (dest);
    if (! committed)
        committed = copyTree (unpacked, dest, err);
    if (! committed)
    {
        if (! restorePrevious())
            r.error = "Could not commit pack install; restore failed";
        else
            r.error = err.isNotEmpty() ? err : juce::String ("Could not commit pack install");
        cleanupWork();
        return r;
    }

    juce::String committedErr;
    PackManifest committedManifest;
    const auto committedMf = dest.getChildFile ("manifest.json");
    const bool ok = dest.isDirectory()
                    && committedMf.existsAsFile()
                    && parseManifestText (committedMf.loadFileAsString(), committedManifest, committedErr)
                    && committedManifest.id == verified.id
                    && validateExtracted (dest, committedManifest, committedErr);
    if (! ok)
    {
        if (! restorePrevious())
            r.error = "Install verification failed; restore failed";
        else
            r.error = committedErr.isNotEmpty() ? committedErr : juce::String ("Install verification failed");
        cleanupWork();
        return r;
    }

    backup.deleteRecursively();
    cleanupWork();
    r.ok = true;
    r.installed = (int) committedManifest.presets.size();
    r.packId = committedManifest.id;
    r.packName = committedManifest.name;
    return r;
}

bool PresetPack::uninstallPack (const juce::File& destRoot, const juce::String& packId, juce::String& error)
{
    if (packId.isEmpty() || PresetIdentity::isFactoryId (packId))
    {
        error = "Cannot remove this library";
        return false;
    }
    if (packId.contains ("..") || packId.containsChar ('/') || packId.containsChar ('\\')
        || packId.containsChar ('\0') || packId.containsChar (':'))
    {
        error = "Invalid pack id";
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
    if (manifest.id.isNotEmpty() && ! isValidPackId (manifest.id))
    {
        error = "Invalid pack id";
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
    juce::StringArray usedFiles;
    for (const auto& f : nrkFiles)
    {
        if (! PresetLibrary::isNrkFile (f))
            continue;
        PresetManager::Info info;
        PresetManager::readInfo (f, info);
        auto stem = PresetIdentity::slug (info.id.isNotEmpty() ? info.id : f.getFileNameWithoutExtension());
        if (stem.isEmpty())
            stem = "preset";
        auto name = stem + Config::kPresetFileExtension;
        int n = 2;
        while (usedFiles.contains (name))
            name = stem + "-" + juce::String (n++) + Config::kPresetFileExtension;
        usedFiles.add (name);

        const auto copied = presetDir.getChildFile (name);
        if (copied.existsAsFile())
        {
            error = "Duplicate archive path " + name;
            tmp.deleteRecursively();
            return false;
        }
        if (! f.copyFileTo (copied))
        {
            error = "Could not stage " + f.getFileName();
            tmp.deleteRecursively();
            return false;
        }
        PackPresetRef ref;
        ref.id = info.id.isNotEmpty() ? info.id : f.getFileNameWithoutExtension();
        ref.file = "presets/" + name;
        ref.category = info.category;
        ref.tags = info.tags;
        ref.requiredInputs = info.requiredInputs;
        ref.sha256 = PresetIdentity::fileSha256 (copied);
        if (! zipPathSafe (ref.file))
        {
            error = "Illegal preset path " + ref.file;
            tmp.deleteRecursively();
            return false;
        }
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
