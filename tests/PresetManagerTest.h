#ifndef PRESETMANAGERTEST_H
#define PRESETMANAGERTEST_H

#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/core/PluginProcessor.h"
#include "../src/utils/PresetManager.h"
#include "../src/utils/PresetLibrary.h"
#include <JuceHeader.h>
#include <cstring>

class PresetManagerTest : public juce::UnitTest {
public:
  PresetManagerTest() : juce::UnitTest("PresetManagerTest", "Persistence") {}

  using TestProcessor = NeuroKoreAudioProcessor;

  void runTest() override {
    beginTest("Save and load preset");
    TestProcessor proc;
    PresetManager mgr(proc);

    juce::String err;
    const juce::String testScript("stage1: y = x * a");
    expect(proc.setFormula(testScript, err));
    expect(err.isEmpty());

    proc.setVariableName(0, "gain");
    if (auto *p = proc.apvts.getParameter(EffectParameters::paramA))
      p->setValueNotifyingHost(0.5f);

    juce::TemporaryFile tmp(".nrk");
    expect(mgr.savePreset(tmp.getFile(), "test"));

    TestProcessor proc2;
    PresetManager mgr2(proc2);
    expect(mgr2.loadPreset(tmp.getFile()));
    expectEquals(proc2.getScript(), testScript);
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
    expectEquals(scriptFromDscr, testScript);

    beginTest("DSCR chunk has priority over STAT script");
    juce::MemoryBlock presetBytes;
    expect(tmp.getFile().loadFileAsData(presetBytes));
    // In-place patch must keep the same byte length as the original DSCR payload
    const juce::String dscrOverride("stage1: y = x * b"); // same length as testScript
    expectEquals((int64_t) dscrOverride.getNumBytesAsUTF8(), dscrLength);
    if (dscrOffset >= 0 && dscrLength == (int64_t) dscrOverride.getNumBytesAsUTF8() &&
        dscrOffset + dscrLength <= static_cast<int64_t>(presetBytes.getSize()))
    {
      auto* presetData = static_cast<char*>(presetBytes.getData());
      expect(presetData != nullptr);
      std::memcpy(presetData + dscrOffset,
                  dscrOverride.toRawUTF8(),
                  static_cast<size_t>(dscrLength));
      juce::TemporaryFile patched(".nrk");
      expect(patched.getFile().replaceWithData(presetBytes.getData(), presetBytes.getSize()));

      TestProcessor proc3;
      PresetManager mgr3(proc3);
      expect(mgr3.loadPreset(patched.getFile()));
      expectEquals(proc3.getScript(), dscrOverride);
    }

    beginTest ("pack import installs a folder of nrk files");
    {
      TestProcessor proc;
      PresetManager mgr (proc);
      juce::String err;
      expect (proc.setFormula ("stage1: y = x", err));
      auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("nc-pack-src-" + juce::String (juce::Random::getSystemRandom().nextInt()));
      auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("nc-pack-dst-" + juce::String (juce::Random::getSystemRandom().nextInt()));
      work.deleteRecursively();
      dest.deleteRecursively();
      work.createDirectory();
      expect (mgr.savePreset (work.getChildFile ("A.nrk"), "A", "Zardonic", "Bass"));
      expect (mgr.savePreset (work.getChildFile ("B.nrk"), "B", "Zardonic", "Bass"));
      work.getChildFile ("pack.json").replaceWithText ("{ \"name\": \"Zardonic Metal Bass\" }");

      const auto r = PresetLibrary::importPathsInto ({ work.getFullPathName() }, dest);
      expectEquals (r.imported, 2);
      expectEquals (r.packName, juce::String ("Zardonic Metal Bass"));
      expect (dest.getChildFile ("Packs").getChildFile ("Zardonic Metal Bass")
                  .getChildFile ("A.nrk").existsAsFile());
      expect (dest.getChildFile ("Packs").getChildFile ("Zardonic Metal Bass")
                  .getChildFile ("B.nrk").existsAsFile());

      const auto zip = dest.getChildFile ("out.zip");
      std::vector<juce::File> files {
          dest.getChildFile ("Packs").getChildFile ("Zardonic Metal Bass").getChildFile ("A.nrk"),
          dest.getChildFile ("Packs").getChildFile ("Zardonic Metal Bass").getChildFile ("B.nrk")
      };
      expect (PresetLibrary::exportPack (files, zip, "Zardonic Metal Bass", "Zardonic"));
      expect (zip.existsAsFile());

      auto dest2 = dest.getChildFile ("roundtrip");
      dest2.createDirectory();
      const auto r2 = PresetLibrary::importPathsInto ({ zip.getFullPathName() }, dest2);
      expectEquals (r2.imported, 2);
      expect (r2.packName.containsIgnoreCase ("Zardonic"));

      work.deleteRecursively();
      dest.deleteRecursively();
    }

    beginTest ("single nrk import stays in the library root");
    {
      TestProcessor proc;
      PresetManager mgr (proc);
      juce::String err;
      expect (proc.setFormula ("stage1: y = x", err));
      auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("nc-one-" + juce::String (juce::Random::getSystemRandom().nextInt()));
      work.deleteRecursively();
      work.createDirectory();
      const auto src = work.getChildFile ("Solo.nrk");
      expect (mgr.savePreset (src, "Solo"));
      const auto r = PresetLibrary::importPathsInto ({ src.getFullPathName() }, work.getChildFile ("lib"));
      expectEquals (r.imported, 1);
      expect (r.packName.isEmpty());
      expect (work.getChildFile ("lib").getChildFile ("Solo.nrk").existsAsFile());
      work.deleteRecursively();
    }

    beginTest ("sanitize pack names drop path characters");
    {
      expectEquals (PresetLibrary::sanitizePackName ("Zardonic / Metal"),
                    juce::String ("Zardonic Metal"));
      expectEquals (PresetLibrary::sanitizePackName ("   "), juce::String ("Pack"));
    }
  }
};

#endif // PRESETMANAGERTEST_H
