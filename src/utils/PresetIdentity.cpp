#include "PresetIdentity.h"
#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

namespace PresetIdentity
{

juce::String factoryId (const juce::String& name)
{
    return juce::String (kFactoryPrefix) + name;
}

bool isFactoryId (const juce::String& id) noexcept
{
    return id.startsWith (kFactoryPrefix);
}

juce::String factoryNameFromId (const juce::String& id)
{
    return isFactoryId (id) ? id.fromFirstOccurrenceOf (kFactoryPrefix, false, false) : juce::String();
}

juce::String newUuid()
{
    return juce::Uuid().toDashedString();
}

juce::String fileSha256 (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};
    return juce::SHA256 (file).toHexString();
}

juce::String relativeKey (const juce::File& file, const juce::File& root)
{
    auto rel = file.getRelativePathFrom (root);
    if (rel.isEmpty() || rel == file.getFullPathName())
        rel = file.getFileName();
    return rel.replaceCharacter ('\\', '/');
}

juce::String slug (const juce::String& text)
{
    auto s = text.trim().toLowerCase();
    juce::String out;
    bool dash = false;
    for (int i = 0; i < s.length(); ++i)
    {
        const auto c = s[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (ok)
        {
            out += c;
            dash = false;
        }
        else if (out.isNotEmpty() && ! dash)
        {
            out += '-';
            dash = true;
        }
    }
    if (out.endsWithChar ('-'))
        out = out.dropLastCharacters (1);
    return out.isNotEmpty() ? out : juce::String ("pack");
}

juce::String legacyPackId (const juce::String& packName)
{
    return juce::String (kLegacyPackPrefix) + slug (packName);
}

namespace
{
json loadCatalog (const juce::File& file)
{
    if (! file.existsAsFile())
        return json::object();
    auto parsed = json::parse (file.loadFileAsString().toStdString(), nullptr, false);
    return parsed.is_object() ? parsed : json::object();
}

void saveCatalog (const juce::File& file, const json& j)
{
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::String (j.dump (2)));
}
} // namespace

juce::String legacyIdFor (const juce::File& file, const juce::File& catalogRoot)
{
    const auto catalogFile = catalogRoot.getChildFile (kCatalogFile);
    const auto key = relativeKey (file, catalogRoot).toStdString();
    auto cat = loadCatalog (catalogFile);
    if (! cat.contains ("ids") || ! cat["ids"].is_object())
        cat["ids"] = json::object();
    auto& ids = cat["ids"];
    if (ids.contains (key) && ids[key].is_object() && ids[key].contains ("id") && ids[key]["id"].is_string())
    {
        const auto id = juce::String (ids[key]["id"].get<std::string>());
        if (id.isNotEmpty())
            return id;
    }

    const auto id = newUuid();
    ids[key] = { { "id", id.toStdString() }, { "hash", fileSha256 (file).toStdString() } };
    cat["version"] = 1;
    saveCatalog (catalogFile, cat);
    return id;
}

} // namespace PresetIdentity
