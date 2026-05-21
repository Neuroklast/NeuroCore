#include <JuceHeader.h>
#include "PresetManager.h"
#include "../core/Config.h"
#include "../core/PluginProcessor.h"
#include "Log.h"
#include <cstring>

#ifndef JucePlugin_Name
#define JucePlugin_Name "NeuroCore"
#endif
#ifndef JucePlugin_Manufacturer
#define JucePlugin_Manufacturer "NEUROKLAST"
#endif


using json = nlohmann::json;

namespace {
constexpr char kMagic[4] = {'N','R','K','\0'};
constexpr int kVersion = 2;
constexpr char kMetaId[4] = {'M','E','T','A'};
constexpr char kStateId[4] = {'S','T','A','T'};
constexpr const char* kAttrName       = "Name";
constexpr const char* kAttrFileName   = "FileName";
constexpr const char* kAttrPluginName = "PluginName";
}

PresetManager::PresetManager(NeuroCoreAudioProcessor& proc) : processor(proc)
{
    const juce::String keyString = "NeuroCoreKey";
    std::fill(key.begin(), key.end(), 0);
    auto bytes = keyString.getNumBytesAsUTF8();
    std::memcpy(key.data(), keyString.toRawUTF8(), std::min<size_t>(bytes, key.size()));
}

std::vector<uint8_t> PresetManager::encrypt(const juce::MemoryBlock& data) const
{
    const size_t padded = (data.getSize() + 7u) & ~7u;
    juce::MemoryBlock block(padded);
    block.copyFrom(data.getData(), 0, data.getSize());

    juce::BlowFish bf(key.data(), (int)key.size());

    bf.encrypt(block.getData(), data.getSize(), padded);


    std::vector<uint8_t> result;
    result.resize(4 + padded);
    auto originalSize = static_cast<uint32_t>(data.getSize());
    std::memcpy(result.data(), &originalSize, 4);
    std::memcpy(result.data() + 4, block.getData(), padded);
    return result;
}

bool PresetManager::decrypt(const std::vector<uint8_t>& data, juce::MemoryBlock& dest) const
{
    if (data.size() < 4)
        return false;

    uint32_t originalSize = 0;
    std::memcpy(&originalSize, data.data(), 4);
    const size_t cipherSize = data.size() - 4;

    if ((cipherSize % 8) != 0)
        return false;

    juce::MemoryBlock block(data.data() + 4, cipherSize);
    juce::BlowFish bf(key.data(), (int)key.size());
    bf.decrypt(block.getData(), cipherSize);

    dest.setSize(originalSize);
    dest.copyFrom(block.getData(), 0, originalSize);
    return true;
}

bool PresetManager::savePreset(const juce::File& file, const juce::String& name)
{
    json meta;

    meta[kAttrName] = name.toStdString();
    meta[kAttrFileName] = file.getFileName().toStdString();
    meta[kAttrPluginName] = JucePlugin_Name;

    meta["Author"] = JucePlugin_Manufacturer;

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    auto encrypted = encrypt(state);
    const juce::String script = processor.getScript();
    juce::MemoryBlock scriptBlock(script.toRawUTF8(), static_cast<size_t>(script.getNumBytesAsUTF8()));

    std::string metaStr = meta.dump();
    juce::MemoryBlock metaBlock(metaStr.data(), metaStr.size());

    Header header{};
    std::memcpy(header.magic, kMagic, sizeof(header.magic));
    header.version = kVersion;
    std::memset(header.classID, 0, sizeof(header.classID));
    auto className = juce::String(JucePlugin_Name).toRawUTF8();
    std::memcpy(header.classID, className, std::min<size_t>(strlen(className), sizeof(header.classID)));

    const int64_t metaOffset = sizeof(Header);
    const int64_t stateOffset = metaOffset + (int64_t)metaBlock.getSize();
    const int64_t dscrOffset = stateOffset + (int64_t)encrypted.size();
    const int64_t listOffset = dscrOffset + (int64_t)scriptBlock.getSize();
    header.chunkListOffset = listOffset;

    ChunkEntry entries[3];
    std::memcpy(entries[0].id, kMetaId, 4);
    entries[0].offset = metaOffset;
    entries[0].length = (int64_t)metaBlock.getSize();
    std::memcpy(entries[1].id, kStateId, 4);
    entries[1].offset = stateOffset;
    entries[1].length = (int64_t)encrypted.size();
    std::memcpy(entries[2].id, PresetManager::kDscrId, 4);
    entries[2].offset = dscrOffset;
    entries[2].length = (int64_t)scriptBlock.getSize();

    juce::FileOutputStream out(file);
    if (!out.openedOk())
        return false;
    out.write(&header, sizeof(header));
    out.write(metaBlock.getData(), metaBlock.getSize());
    out.write(encrypted.data(), encrypted.size());
    out.write(scriptBlock.getData(), scriptBlock.getSize());
    out.write(kMetaId, 4); // "List" header
    const int32_t numEntries = 3;
    out.writeIntBigEndian(numEntries); // using big endian for portability
    for (auto& e : entries)
    {
        out.write(e.id, 4);
        out.writeInt64(e.offset);
        out.writeInt64(e.length);
    }
    out.flush();
    return true;
}

bool PresetManager::loadPreset(const juce::File& file)
{
    juce::FileInputStream in(file);
    if (!in.openedOk())
        return false;

    Header header{};
    if (in.read(&header, sizeof(header)) != sizeof(header))
        return false;
    if (std::memcmp(header.magic, kMagic, 4) != 0)
        return false;

    in.setPosition((juce::int64)header.chunkListOffset);
    char listId[4];
    if (in.read(listId, 4) != 4)
        return false;
    if (std::memcmp(listId, kMetaId, 4) != 0 && std::memcmp(listId, "List", 4) != 0)
        return false;
    int32_t numEntries = in.readIntBigEndian();
    std::vector<ChunkEntry> entries(numEntries);
    for (int i = 0; i < numEntries; ++i)
    {
        in.read(entries[i].id, 4);
        entries[i].offset = in.readInt64();
        entries[i].length = in.readInt64();
    }

    std::vector<uint8_t> stateData;
    juce::String dscrScript;
    for (auto& e : entries)
    {
        if (std::memcmp(e.id, kMetaId, 4) == 0)
        {
            in.setPosition(e.offset);

            juce::MemoryBlock metaBytes;
            in.readIntoMemoryBlock(metaBytes, (size_t)e.length);
            auto metaJson = json::parse(metaBytes.toString().toStdString(), nullptr, false);

            // ignore metadata but we could display in UI
        }
        else if (std::memcmp(e.id, kStateId, 4) == 0)
        {
            in.setPosition(e.offset);
            stateData.resize((size_t)e.length);
            in.read(stateData.data(), (int)e.length);
        }
        else if (std::memcmp(e.id, PresetManager::kDscrId, 4) == 0)
        {
            in.setPosition(e.offset);
            juce::MemoryBlock scriptBytes;
            in.readIntoMemoryBlock(scriptBytes, static_cast<size_t>(e.length));
            dscrScript = juce::String::fromUTF8(static_cast<const char*>(scriptBytes.getData()),
                                                static_cast<int>(scriptBytes.getSize()));
        }
    }

    if (stateData.empty())
        return false;

    juce::MemoryBlock plain;
    if (!decrypt(stateData, plain))
        return false;

    processor.setStateInformation(plain.getData(), (int)plain.getSize());

    if (dscrScript.isNotEmpty())
    {
        juce::String err;
        if (!processor.applyFormula(dscrScript, err))
        {
            logError("Failed to apply DSCR script while loading preset: " + err);
            return false;
        }
    }
    return true;
}

std::vector<juce::File> PresetManager::getAvailablePresets(const juce::File& dir) const
{
    std::vector<juce::File> result;
    if (!dir.exists())
        return result;
    juce::DirectoryIterator iter(dir, false, juce::String("*") + Config::kPresetFileExtension,
                                 juce::File::TypesOfFileToFind::findFiles);
    while (iter.next())
        result.push_back(iter.getFile());
    return result;
}
