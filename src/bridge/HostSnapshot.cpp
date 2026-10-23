#include "HostSnapshot.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include "../core/PluginProcessor.h"
#include "../dsl/GraphModel.h"
#include "../licensing/HardwareFingerprint.h"
#include "../utils/FactoryPresetLibrary.h"
#include "../utils/PresetIdentity.h"
#include "../utils/PresetLibrary.h"
#include "../utils/PresetPack.h"
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
    out.id = v.getProperty ("id", "").toString();
    out.packId = v.getProperty ("packId", "").toString();
    out.author = v.getProperty ("author", "").toString();
    out.category = v.getProperty ("category", "").toString();
    out.tags = v.getProperty ("tags", "").toString();
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
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, 1, c.index)));
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

bool knobMetaFromVar (const juce::var& v, KnobMetaCmd& out, juce::String& error)
{
    if (! v.isObject())
    {
        error = "setKnobMeta must be an object";
        return false;
    }
    out = {};
    out.id = v.getProperty ("id", "").toString().trim().toLowerCase();
    if (out.id.length() != 1 || out.id[0] < 'a' || out.id[0] > 'f')
    {
        error = "setKnobMeta needs id a-f";
        return false;
    }
    if (! v.getProperty ("name", juce::var()).isVoid())
    {
        out.hasName = true;
        out.name = v.getProperty ("name", "").toString();
    }
    if (! v.getProperty ("min", juce::var()).isVoid())
    {
        out.hasMin = true;
        out.min = (float) v.getProperty ("min", 0.0);
    }
    if (! v.getProperty ("max", juce::var()).isVoid())
    {
        out.hasMax = true;
        out.max = (float) v.getProperty ("max", 1.0);
    }
    if (! v.getProperty ("isNote", juce::var()).isVoid())
    {
        out.hasNote = true;
        out.isNote = (bool) v.getProperty ("isNote", false);
    }
    return true;
}

bool applyKnobMeta (NeuroKoreAudioProcessor& proc, const KnobMetaCmd& c, juce::String& error)
{
    const int idx = (int) (c.id[0] - 'a');
    auto script = proc.getScript();
    if (c.hasName)
        script = dsl::rewriteParamDisplayName (script, idx, c.name);

    float min = c.min;
    float max = c.max;
    bool isNote = c.isNote;
    if (! c.hasMin || ! c.hasMax || ! c.hasNote)
    {
        dsl::GraphDocument doc;
        juce::String parseErr;
        if (dsl::parse (script, doc, parseErr))
        {
            for (const auto& p : doc.params)
                if (p.alias.equalsIgnoreCase (c.id))
                {
                    if (! c.hasMin) min = p.min;
                    if (! c.hasMax) max = p.max;
                    if (! c.hasNote) isNote = p.isNote;
                    break;
                }
        }
    }
    if (c.hasMin || c.hasMax || c.hasNote || c.hasName)
        script = dsl::rewriteParamRange (script, idx, min, max, isNote);

    if (! proc.setFormula (script, error, true))
        return false;
    return true;
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
                "// New preset\nparam a = Drive [0.5, 4.0]\nstage1: y = hardclip(x, a)\n",
                err, true))
        {
            error = err;
            return false;
        }
        proc.setCurrentPresetName ("Untitled");
        proc.setCurrentPresetId ({});
        return true;
    }
    if (c.action == "save" || c.action == "saveas")
    {
        const auto name = c.name.trim();
        if (name.isEmpty())
        {
            error = "save needs name";
            return false;
        }
        auto& factory = FactoryPresetLibrary::getInstance();
        if (factory.getEntries().empty())
            factory.loadFromEmbedded();
        if (factory.findByName (name) != nullptr)
        {
            error = "Factory presets cannot be overwritten";
            return false;
        }
        auto dir = PresetLibrary::userPresetRoot();
        if (! dir.createDirectory() && ! dir.isDirectory())
        {
            error = "Could not create user preset folder";
            return false;
        }
        const auto stem = PresetLibrary::sanitizePackName (name);
        auto file = dir.getChildFile (stem + Config::kPresetFileExtension);
        if (! proc.presetManager.savePreset (file, name, c.author, c.category, c.tags))
        {
            error = "Could not save preset";
            return false;
        }
        PresetManager::Info info;
        proc.presetManager.readInfo (file, info);
        proc.setCurrentPresetName (name);
        proc.setCurrentPresetId (info.id);
        proc.setLastPresetBrowserCategory (c.category.isNotEmpty() ? c.category : juce::String ("User"));
        proc.invalidatePresetLibrary();
        return true;
    }
    if (c.action == "load")
    {
        auto& lib = FactoryPresetLibrary::getInstance();
        if (lib.getEntries().empty())
            lib.loadFromEmbedded();
        if (proc.presetLibraryDirty)
        {
            proc.presetLibrary.rebuild (proc.presetManager, lib, PresetLibrary::userPresetRoot());
            proc.presetLibraryDirty = false;
        }
        const PresetIndexEntry* hit = nullptr;
        if (c.id.isNotEmpty())
            hit = proc.presetLibrary.findById (c.id);
        if (hit == nullptr && c.name.isNotEmpty())
            hit = proc.presetLibrary.findByName (c.name, c.packId);
        if (hit == nullptr)
        {
            error = "Unknown preset";
            return false;
        }
        if (hit->sourceType == "factory")
        {
            const auto* fac = lib.findByName (PresetIdentity::factoryNameFromId (hit->presetId));
            if (fac == nullptr)
                fac = lib.findByName (hit->displayName);
            if (fac == nullptr)
            {
                error = "Unknown factory preset";
                return false;
            }
            const auto& entries = lib.getEntries();
            for (int i = 0; i < (int) entries.size(); ++i)
                if (&entries[(size_t) i] == fac)
                    return lib.applyPreset (proc, i, error);
            error = "Unknown factory preset";
            return false;
        }
        const juce::File file (hit->filePath);
        if (! proc.presetManager.loadPreset (file))
        {
            error = "Could not load user preset";
            return false;
        }
        proc.setCurrentPresetName (hit->displayName);
        proc.setCurrentPresetId (hit->presetId);
        if (hit->category.isNotEmpty())
            proc.setLastPresetBrowserCategory (hit->category);
        return true;
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
    root->setProperty ("autoGain", raw01 (proc.apvts, EffectParameters::autoGain));
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
    const int lat = proc.getLatencySamples();
    const bool safe = proc.isCpuProtectActive();
    const int osIdx = choiceIndex (proc.apvts, EffectParameters::oversampling);
    const int os = osFactorFromIndex (osIdx);
    auto* root = new juce::DynamicObject();
    root->setProperty ("cpu", footerCpu (proc.getCpuLoad()));
    root->setProperty ("safe", safe);
    root->setProperty ("mode", juce::String (modeWord (safe, false, proc.isLiveMode())));
    root->setProperty ("live", proc.isLiveMode());
    root->setProperty ("lat", lat);
    root->setProperty ("sr", sr);
    root->setProperty ("buf", proc.getHostBlockSize());
    root->setProperty ("bpm", proc.getEffectiveBpm());
    root->setProperty ("tempoSource", proc.isHostTempo() ? "HOST" : "USER");
    root->setProperty ("standalone", proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
    root->setProperty ("os", os);
    root->setProperty ("motion", juce::String (UiSettings::motionKey (UiSettings::get().motion())));
    root->setProperty ("theme", UiSettings::get().themeId());
    root->setProperty ("frameRate", UiSettings::get().frameRate());
    root->setProperty ("discardPrompt", UiSettings::get().discardPrompt());
    root->setProperty ("scopeSource", UiSettings::get().scopeSource());
    root->setProperty ("scopeX", UiSettings::get().scopeX());
    root->setProperty ("scopeY", UiSettings::get().scopeY());
    root->setProperty ("scopeGrid", UiSettings::get().scopeGrid());
    root->setProperty ("scopeInvertY", UiSettings::get().scopeInvertY());
    root->setProperty ("scopeDelta", UiSettings::get().scopeDelta());
    bool sidechainOn = false;
    if (auto* scBus = proc.getBus (true, 1))
        sidechainOn = scBus->isEnabled();
    root->setProperty ("sidechainOn", sidechainOn);
    root->setProperty ("midiEventCount", (juce::int64) proc.getMidiEventCount());
    root->setProperty ("midiGate", proc.getMidiGate());
    root->setProperty ("midiVel", proc.getMidiVel());
    root->setProperty ("midiMod", proc.getMidiMod());
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
    juce::Array<juce::var> clips;
    proc.appendClipPeaks (clips);
    root->setProperty ("clips", clips);
    return juce::var (root);
}

juce::var presetStateVar (NeuroKoreAudioProcessor& proc)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("name", proc.getCurrentPresetName());
    root->setProperty ("id", proc.getCurrentPresetId());
    auto& lib = FactoryPresetLibrary::getInstance();
    if (proc.presetLibraryDirty || ! proc.cachedPresetList.isArray())
    {
        proc.presetLibrary.rebuild (proc.presetManager, lib, PresetLibrary::userPresetRoot());
        proc.presetLibraryDirty = false;
        juce::Array<juce::var> list;
        juce::Array<juce::var> packs;
        juce::StringArray packSeen;
        for (const auto& e : proc.presetLibrary.entries())
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", e.presetId);
            o->setProperty ("packId", e.packId);
            o->setProperty ("name", e.displayName);
            o->setProperty ("category", e.category);
            o->setProperty ("description", e.description);
            o->setProperty ("author", e.author);
            o->setProperty ("factory", e.sourceType == "factory");
            o->setProperty ("sourceType", e.sourceType);
            o->setProperty ("origin", e.origin);
            juce::Array<juce::var> tags;
            for (const auto& t : e.tags)
                tags.add (t);
            o->setProperty ("tags", tags);
            juce::Array<juce::var> sources;
            for (const auto& s : e.soundSources)
                sources.add (s);
            o->setProperty ("soundSources", sources);
            juce::Array<juce::var> types;
            for (const auto& t : e.processingTypes)
                types.add (t);
            o->setProperty ("processingTypes", types);
            o->setProperty ("style", e.style);
            o->setProperty ("role", e.role);
            juce::Array<juce::var> required;
            for (const auto& r : e.requiredInputs)
                required.add (r);
            o->setProperty ("requiredInputs", required);
            list.add (juce::var (o));
            if (e.sourceType == "pack" && e.packId.isNotEmpty() && ! packSeen.contains (e.packId))
            {
                packSeen.add (e.packId);
                auto* p = new juce::DynamicObject();
                p->setProperty ("id", e.packId);
                p->setProperty ("name", e.origin);
                packs.add (juce::var (p));
            }
        }
        proc.cachedPresetList = juce::var (list);
        proc.cachedPacks = juce::var (packs);
    }
    root->setProperty ("list", proc.cachedPresetList);
    root->setProperty ("packs", proc.cachedPacks);
    return juce::var (root);
}

namespace
{
juce::var previewVar (const PackPreview& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", p.error.isEmpty());
    o->setProperty ("error", p.error);
    o->setProperty ("name", p.manifest.name);
    o->setProperty ("id", p.manifest.id);
    o->setProperty ("author", p.manifest.author);
    o->setProperty ("description", p.manifest.description);
    o->setProperty ("version", p.manifest.version);
    o->setProperty ("presetCount", p.presetCount);
    o->setProperty ("existing", p.existing);
    o->setProperty ("source", p.source.getFullPathName());
    juce::Array<juce::var> cats;
    for (const auto& c : p.categories)
        cats.add (c);
    o->setProperty ("categories", cats);
    juce::Array<juce::var> warns;
    for (const auto& w : p.warnings)
        warns.add (w);
    o->setProperty ("warnings", warns);
    return juce::var (o);
}

juce::StringArray varPaths (const juce::var& v)
{
    juce::StringArray out;
    if (v.isArray())
    {
        for (int i = 0; i < v.size(); ++i)
            out.add (v[i].toString());
    }
    else if (v.toString().isNotEmpty())
        out.add (v.toString());
    return out;
}
} // namespace

juce::var applyLibraryCmd (NeuroKoreAudioProcessor& proc, const juce::var& cmd, juce::String& error)
{
    auto* ok = new juce::DynamicObject();
    ok->setProperty ("ok", true);
    if (! cmd.isObject())
    {
        error = "presetLibrary needs an object";
        return {};
    }
    const auto action = cmd.getProperty ("action", "").toString().toLowerCase();
    const auto root = PresetLibrary::userPresetRoot();
    if (action == "list" || action == "scan" || action == "rescan")
    {
        if (action != "list")
            proc.invalidatePresetLibrary();
        return presetStateVar (proc);
    }
    if (action == "analyze")
    {
        const auto paths = varPaths (cmd.getProperty ("paths", juce::var()));
        if (paths.isEmpty())
        {
            error = "analyze needs paths";
            return {};
        }
        const juce::File f (paths[0]);
        auto preview = PresetPack::isPackArchive (f) ? PresetPack::analyzeArchive (f)
                      : f.isDirectory() ? PresetPack::analyzeDirectory (f)
                      : PackPreview{};
        if (! PresetPack::isPackArchive (f) && ! f.isDirectory())
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("ok", PresetLibrary::isNrkFile (f));
            o->setProperty ("name", f.getFileNameWithoutExtension());
            o->setProperty ("presetCount", PresetLibrary::isNrkFile (f) ? 1 : 0);
            o->setProperty ("source", f.getFullPathName());
            if (! PresetLibrary::isNrkFile (f))
                o->setProperty ("error", "Not a preset or pack");
            return juce::var (o);
        }
        return previewVar (preview);
    }
    if (action == "import" || action == "install")
    {
        const auto paths = varPaths (cmd.getProperty ("paths", juce::var()));
        if (paths.isEmpty())
        {
            error = "import needs paths";
            return {};
        }
        const auto r = PresetLibrary::importPathsInto (paths, root);
        proc.invalidatePresetLibrary();
        ok->setProperty ("imported", r.imported);
        ok->setProperty ("skipped", r.skipped);
        ok->setProperty ("packName", r.packName);
        juce::Array<juce::var> errs;
        for (const auto& e : r.errors)
            errs.add (e);
        ok->setProperty ("errors", errs);
        if (r.imported <= 0)
        {
            error = r.errors.isEmpty() ? juce::String ("Import failed") : r.errors[0];
            return {};
        }
        return juce::var (ok);
    }
    if (action == "exportpack")
    {
        const auto ids = varPaths (cmd.getProperty ("presetIds", juce::var()));
        const auto dest = juce::File (cmd.getProperty ("dest", "").toString());
        if (dest == juce::File() || ids.isEmpty())
        {
            error = "exportPack needs dest and presetIds";
            return {};
        }
        if (proc.presetLibraryDirty)
        {
            proc.presetLibrary.rebuild (proc.presetManager, FactoryPresetLibrary::getInstance(), root);
            proc.presetLibraryDirty = false;
        }
        std::vector<juce::File> files;
        for (const auto& id : ids)
            if (auto* e = proc.presetLibrary.findById (id))
                if (e->filePath.isNotEmpty())
                    files.push_back (juce::File (e->filePath));
        PackManifest m;
        m.name = cmd.getProperty ("name", "Pack").toString();
        m.author = cmd.getProperty ("author", "").toString();
        m.description = cmd.getProperty ("description", "").toString();
        m.version = cmd.getProperty ("version", "1.0.0").toString();
        if (! PresetPack::exportNrkpack (files, dest, m, error))
            return {};
        return juce::var (ok);
    }
    if (action == "exportpreset")
    {
        const auto dest = juce::File (cmd.getProperty ("dest", "").toString());
        const auto name = cmd.getProperty ("name", proc.getCurrentPresetName()).toString();
        if (dest == juce::File())
        {
            error = "exportPreset needs dest";
            return {};
        }
        if (! proc.presetManager.savePreset (dest, name.isNotEmpty() ? name : juce::String ("Preset"),
                                             cmd.getProperty ("author", "").toString(),
                                             cmd.getProperty ("category", "").toString(),
                                             cmd.getProperty ("tags", "").toString()))
        {
            error = "Could not export preset";
            return {};
        }
        return juce::var (ok);
    }
    if (action == "removepack")
    {
        const auto packId = cmd.getProperty ("packId", "").toString();
        if (! PresetPack::uninstallPack (root, packId, error))
            return {};
        proc.invalidatePresetLibrary();
        return juce::var (ok);
    }
    if (action == "deletepreset")
    {
        const auto id = cmd.getProperty ("id", "").toString();
        if (PresetIdentity::isFactoryId (id))
        {
            error = "Factory presets cannot be deleted";
            return {};
        }
        if (proc.presetLibraryDirty)
        {
            proc.presetLibrary.rebuild (proc.presetManager, FactoryPresetLibrary::getInstance(), root);
            proc.presetLibraryDirty = false;
        }
        auto* e = proc.presetLibrary.findById (id);
        if (e == nullptr || e->filePath.isEmpty())
        {
            error = "Preset not found";
            return {};
        }
        if (e->sourceType == "pack")
        {
            error = "Uninstall the pack to remove pack presets";
            return {};
        }
        if (! juce::File (e->filePath).deleteFile())
        {
            error = "Could not delete preset";
            return {};
        }
        proc.invalidatePresetLibrary();
        return juce::var (ok);
    }
    error = "Unknown library action " + action;
    return {};
}

juce::var licenseVar (NeuroKoreAudioProcessor& proc)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("licensed", proc.isProductLicensed());
    root->setProperty ("demoLocked", proc.isDemoMixLocked());
    root->setProperty ("demoRemainSec", proc.demoSecondsRemaining());
    root->setProperty ("demoEndsMs", (juce::int64) (juce::Time::currentTimeMillis()
        + (juce::int64) proc.demoSecondsRemaining() * 1000));
    root->setProperty ("error", proc.licenseError());
    root->setProperty ("email", proc.licensedEmail());
    root->setProperty ("systemId", HardwareFingerprint::generate());
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
        "ott", "widen", "ir", "osc", "env", "xover", "octaver", "vocoder",
        "phaser", "flanger", "chorus", "deesser", "transient", "utility", "pitch"
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
