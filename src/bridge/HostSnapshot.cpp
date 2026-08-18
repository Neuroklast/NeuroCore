#include "HostSnapshot.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include "../core/PluginProcessor.h"
#include "../utils/FactoryPresetLibrary.h"
#include "../utils/UiSettings.h"

namespace bridge
{

int footerCpu (float load) noexcept
{
    return Config::cpuDisplayPercent (load);
}

const char* modeWord (bool safe, bool bypass, bool live) noexcept
{
    if (safe) return "SAFE";
    if (bypass) return "BYPASS";
    return live ? "LIVE" : "STUDIO";
}

int osFactorFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, 3, index))
    {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        default: return 8;
    }
}

bool paramGestureFromVar (const juce::var& v, ParamGesture& out, juce::String& error)
{
    if (! v.isObject())
    {
        error = "setParam must be an object";
        return false;
    }
    out.id = v.getProperty ("id", "").toString();
    out.value = (float) v.getProperty ("value", 0.0);
    out.phase = v.getProperty ("gesture", "change").toString().toLowerCase();
    if (out.id.isEmpty())
    {
        error = "setParam needs id";
        return false;
    }
    return true;
}

bool presetCmdFromVar (const juce::var& v, PresetCmd& out, juce::String& error)
{
    if (! v.isObject())
    {
        error = "preset must be an object";
        return false;
    }
    out.action = v.getProperty ("action", "").toString().toLowerCase();
    out.name = v.getProperty ("name", "").toString();
    if (out.action.isEmpty())
    {
        error = "preset needs action";
        return false;
    }
    return true;
}

bool choiceCmdFromVar (const juce::var& v, ChoiceCmd& out, juce::String& error)
{
    if (! v.isObject())
    {
        error = "setChoice must be an object";
        return false;
    }
    out.id = v.getProperty ("id", "").toString().toLowerCase();
    out.index = (int) v.getProperty ("index", 0);
    if (out.id.isEmpty())
    {
        error = "setChoice needs id";
        return false;
    }
    return true;
}

bool applyParamGesture (juce::AudioProcessorValueTreeState& apvts, const ParamGesture& g, juce::String& error)
{
    auto* p = apvts.getParameter (g.id);
    if (p == nullptr)
    {
        error = "Unknown param " + g.id;
        return false;
    }
    if (g.phase == "begin")
        p->beginChangeGesture();
    p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, g.value));
    if (g.phase == "end")
        p->endChangeGesture();
    return true;
}

bool applyChoice (NeuroKoreAudioProcessor& proc, const ChoiceCmd& c, juce::String& error)
{
    if (c.id == "os" || c.id == "oversampling")
    {
        if (auto* p = proc.apvts.getParameter (EffectParameters::oversampling))
        {
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, 3, c.index)));
            return true;
        }
    }
    if (c.id == "polisher")
    {
        if (auto* p = proc.apvts.getParameter (EffectParameters::polisherMode))
        {
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jmax (0, c.index)));
            return true;
        }
    }
    if (c.id == "input")
    {
        bool L = true, R = true;
        EffectParameters::flagsFromMode (
            (EffectParameters::InputChannelMode) juce::jlimit (0, 2, c.index), L, R);
        if (auto* lp = proc.apvts.getParameter (EffectParameters::useInputLeft))
            lp->setValueNotifyingHost (L ? 1.f : 0.f);
        if (auto* rp = proc.apvts.getParameter (EffectParameters::useInputRight))
            rp->setValueNotifyingHost (R ? 1.f : 0.f);
        return true;
    }
    if (c.id == "mode" || c.id == "live")
    {
        proc.setLiveMode (c.index != 0);
        return true;
    }
    error = "Unknown choice " + c.id;
    return false;
}

bool applyPresetCmd (NeuroKoreAudioProcessor& proc, const PresetCmd& c, juce::String& error)
{
    if (c.action == "prev")
    {
        proc.stepPreset (-1);
        return true;
    }
    if (c.action == "next")
    {
        proc.stepPreset (1);
        return true;
    }
    if (c.action == "new")
    {
        juce::String err;
        if (! proc.applyFormula (
                "// New preset\nparam a = Drive [0.5, 4.0]\nstage1: y = softclip(x, a)\n",
                err, true))
        {
            error = err;
            return false;
        }
        proc.setCurrentPresetName ("Untitled");
        return true;
    }
    if (c.action == "load")
    {
        auto& lib = FactoryPresetLibrary::getInstance();
        if (lib.getEntries().empty())
            lib.loadFromEmbedded();
        int idx = -1;
        const auto& entries = lib.getEntries();
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].name == c.name)
            {
                idx = i;
                break;
            }
        if (idx < 0)
        {
            error = "Unknown factory preset";
            return false;
        }
        return lib.applyPreset (proc, idx, error);
    }
    error = "Unknown preset action " + c.action;
    return false;
}

namespace
{
float raw01 (juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    if (auto* p = apvts.getRawParameterValue (id))
        return p->load();
    return 0.f;
}

int choiceIndex (juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
        return p->getIndex();
    return 0;
}
} // namespace

juce::var paramsVar (NeuroKoreAudioProcessor& proc)
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> knobs;
    const auto names = proc.getVariableNames();
    const auto& info = proc.getParamInfo();
    for (int i = 0; i < Config::kNumUserParams; ++i)
    {
        auto* k = new juce::DynamicObject();
        k->setProperty ("id", juce::String (EffectParameters::userParams[i]));
        k->setProperty ("name", names[(size_t) i]);
        k->setProperty ("value", raw01 (proc.apvts, EffectParameters::userParams[i]));
        k->setProperty ("active", proc.isParameterActive (i));
        float mn = 0.f, mx = 1.f;
        bool note = false;
        for (const auto& p : info)
            if (p.alias == EffectParameters::userParams[i])
            {
                mn = p.min;
                mx = p.max;
                note = p.isNote;
            }
        k->setProperty ("min", mn);
        k->setProperty ("max", mx);
        k->setProperty ("isNote", note);
        knobs.add (juce::var (k));
    }
    root->setProperty ("knobs", knobs);
    root->setProperty ("mix", raw01 (proc.apvts, EffectParameters::dryWet));
    root->setProperty ("os", choiceIndex (proc.apvts, EffectParameters::oversampling));
    root->setProperty ("polisher", choiceIndex (proc.apvts, EffectParameters::polisherMode));
    const bool L = raw01 (proc.apvts, EffectParameters::useInputLeft) > 0.5f;
    const bool R = raw01 (proc.apvts, EffectParameters::useInputRight) > 0.5f;
    root->setProperty ("input", (int) EffectParameters::modeFromFlags (L, R));
    root->setProperty ("bypass", raw01 (proc.apvts, EffectParameters::dryWet) <= 1.0e-5f);
    return juce::var (root);
}

juce::var hostVar (NeuroKoreAudioProcessor& proc)
{
    const auto spec = proc.getCurrentSpec();
    const int sr = (int) spec.sampleRate;
    const int lat = proc.getOversamplingLatencySamples();
    const bool safe = proc.isCpuProtectActive();
    const int osIdx = choiceIndex (proc.apvts, EffectParameters::oversampling);
    const int os = osFactorFromIndex (osIdx);
    auto* root = new juce::DynamicObject();
    root->setProperty ("cpu", footerCpu (proc.getCpuLoad()));
    root->setProperty ("safe", safe);
    root->setProperty ("mode", juce::String (modeWord (safe, false, proc.isLiveMode())));
    root->setProperty ("lat", lat);
    root->setProperty ("sr", sr);
    root->setProperty ("buf", proc.getHostBlockSize());
    root->setProperty ("bpm", proc.getEffectiveBpm());
    root->setProperty ("tempoSource", proc.isHostTempo() ? "HOST" : "USER");
    root->setProperty ("os", os);
    root->setProperty ("scale", UiSettings::get().uiScalePercent());
    bool sidechainOn = false;
    if (auto* scBus = proc.getBus (true, 1))
        sidechainOn = scBus->isEnabled();
    root->setProperty ("sidechainOn", sidechainOn);
    juce::Array<juce::var> mods;
    for (const auto& id : proc.getModNames())
    {
        float wave[8] {};
        if (! proc.copyCircuitTap (id, wave, 8))
            continue;
        float peak = 0.f;
        for (int i = 0; i < 8; ++i)
            peak = juce::jmax (peak, std::abs (wave[i]));
        auto* m = new juce::DynamicObject();
        m->setProperty ("id", id);
        m->setProperty ("value", peak);
        mods.add (juce::var (m));
    }
    root->setProperty ("mods", mods);
    return juce::var (root);
}

juce::var presetStateVar (NeuroKoreAudioProcessor& proc)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("name", proc.getCurrentPresetName());
    juce::Array<juce::var> list;
    auto& lib = FactoryPresetLibrary::getInstance();
    if (lib.getEntries().empty())
        lib.loadFromEmbedded();
    for (const auto& e : lib.getEntries())
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", e.name);
        o->setProperty ("category", e.category);
        o->setProperty ("description", e.description);
        o->setProperty ("author", "Neuroklast");
        o->setProperty ("factory", true);
        juce::Array<juce::var> tags;
        for (const auto& t : e.tags)
            tags.add (t);
        o->setProperty ("tags", tags);
        list.add (juce::var (o));
    }
    root->setProperty ("list", list);
    return juce::var (root);
}

juce::var licenseVar (NeuroKoreAudioProcessor& proc)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("licensed", proc.isProductLicensed());
    root->setProperty ("demoLocked", proc.isDemoMixLocked());
    root->setProperty ("demoRemainSec", proc.demoSecondsRemaining());
    root->setProperty ("error", proc.licenseError());
    return juce::var (root);
}

juce::var irVar (NeuroKoreAudioProcessor& proc)
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> slots;
    for (const auto& name : proc.getIrSlotNames())
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("slot", name);
        o->setProperty ("name", proc.getIrName (name));
        o->setProperty ("loaded", proc.getIrNumSamples (name) > 0);
        slots.add (juce::var (o));
    }
    root->setProperty ("slots", slots);
    return juce::var (root);
}

juce::var catalogVar()
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> blocks;
    const char* types[] = {
        "stage", "filter", "eq", "comp", "gate", "limit", "delay", "reverb",
        "ott", "widen", "ir", "osc", "env", "xover", "octaver", "vocoder"
    };
    for (auto* t : types)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", juce::String (t));
        blocks.add (juce::var (o));
    }
    root->setProperty ("blocks", blocks);
    return juce::var (root);
}

} // namespace bridge
