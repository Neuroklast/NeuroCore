#ifndef PRESETMANAGERTEST_H
#define PRESETMANAGERTEST_H

#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/core/PluginProcessor.h"
#include "../src/utils/PresetManager.h"
#include <JuceHeader.h>

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
    auto content = tmp.getFile().loadFileAsString();
    expect(!content.contains("x * 2"));
  }
};

#endif // PRESETMANAGERTEST_H
