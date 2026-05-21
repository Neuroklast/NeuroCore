#ifndef PRESETMANAGERTEST_H
#define PRESETMANAGERTEST_H

#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/core/PluginProcessor.h"
#include "../src/utils/PresetManager.h"
#include <JuceHeader.h>
#include <cstring>

class PresetManagerTest : public juce::UnitTest {
public:
  PresetManagerTest() : juce::UnitTest("PresetManagerTest", "Persistence") {}

  using TestProcessor = NeuroCoreAudioProcessor;

  void runTest() override {
    beginTest("Save and load preset");
    TestProcessor proc;
    PresetManager mgr(proc);

    juce::String err;
    proc.setFormula("x * 2", err);

    proc.setVariableName(0, "gain");
    if (auto *p = proc.apvts.getParameter(EffectParameters::paramA))
      p->setValueNotifyingHost(0.5f);

    juce::TemporaryFile tmp(".nrk");
    expect(mgr.savePreset(tmp.getFile(), "test"));

    TestProcessor proc2;
    PresetManager mgr2(proc2);
    expect(mgr2.loadPreset(tmp.getFile()));
    expectEquals(proc2.getScript(), juce::String("x * 2"));
    expectEquals(proc2.getVariableNames()[0], juce::String("gain"));
    if (auto *p2 = proc2.apvts.getParameter(EffectParameters::paramA))
      expectWithinAbsoluteError(p2->getValue(), 0.5f, 0.0001f);

    beginTest("File format");
    juce::FileInputStream in(tmp.getFile());
    char magic[4]{};
    in.read(magic, 4);
    expectEquals(juce::String(magic, 4), juce::String("NRK\0", 4));

    struct Header {
      char magic[4];
      int32_t version;
      char classID[32];
      int64_t chunkListOffset;
    };
    Header header{};
    in.setPosition(0);
    in.read(&header, sizeof(header));
    expectEquals(header.version, 2);

    in.setPosition(header.chunkListOffset);
    char listId[4]{};
    in.read(listId, 4);
    expect(juce::String(listId, 4) == juce::String("META", 4) ||
           juce::String(listId, 4) == juce::String("List", 4));
    auto numEntries = in.readIntBigEndian();
    expectEquals(numEntries, 3);

    bool foundDscr = false;
    juce::String scriptFromDscr;
    int64_t dscrOffset = -1;
    int64_t dscrLength = 0;
    for (int i = 0; i < numEntries; ++i)
    {
      char id[4]{};
      in.read(id, 4);
      const auto offset = in.readInt64();
      const auto length = in.readInt64();
      if (juce::String(id, 4) == juce::String("DSCR", 4))
      {
        foundDscr = true;
        dscrOffset = offset;
        dscrLength = length;
        in.setPosition(offset);
        juce::MemoryBlock scriptBytes;
        in.readIntoMemoryBlock(scriptBytes, static_cast<size_t>(length));
        scriptFromDscr = juce::String::fromUTF8(static_cast<const char*>(scriptBytes.getData()),
                                                static_cast<int>(scriptBytes.getSize()));
      }
    }

    expect(foundDscr);
    expectEquals(scriptFromDscr, juce::String("x * 2"));

    beginTest("DSCR chunk has priority over STAT script");
    juce::MemoryBlock presetBytes;
    expect(tmp.getFile().loadFileAsData(presetBytes));
    const juce::String dscrOverride("x * 3");
    const auto dscrOverrideLength = static_cast<int64_t>(dscrOverride.getNumBytesAsUTF8());
    expectEquals(dscrLength, dscrOverrideLength);
    if (dscrOffset >= 0 && dscrLength == dscrOverrideLength &&
        dscrOffset + dscrLength <= static_cast<int64_t>(presetBytes.getSize()))
    {
      std::memcpy(static_cast<char*>(presetBytes.getData()) + dscrOffset,
                  dscrOverride.toRawUTF8(),
                  static_cast<size_t>(dscrLength));
      juce::TemporaryFile patched(".nrk");
      expect(patched.getFile().replaceWithData(presetBytes.getData(), presetBytes.getSize()));

      TestProcessor proc3;
      PresetManager mgr3(proc3);
      expect(mgr3.loadPreset(patched.getFile()));
      expectEquals(proc3.getScript(), dscrOverride);
    }

  }
};

#endif // PRESETMANAGERTEST_H
