#include "WebPluginEditor.h"
#include "../core/Config.h"
#include "../bridge/GraphOps.h"
#include "../bridge/HostKeys.h"
#include "../bridge/HostSnapshot.h"
#include "../bridge/TelemetryPump.h"
#include "../bridge/WebEditorPolicy.h"
#include "../dsl/GraphModel.h"
#include "../utils/PresetLibrary.h"
#include "../utils/UiSettings.h"
#include "StandaloneAudioSettings.h"

#if ! JUCE_WEB_BROWSER
#error WebPluginEditor requires JUCE_WEB_BROWSER=1
#endif

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
#if JUCE_WINDOWS
/** Map a JS KeyboardEvent.key value to a Windows virtual-key code, or 0 if unknown. */
static WORD keyNameToVK (const juce::String& keyName) noexcept
{
    if (keyName.equalsIgnoreCase ("Space") || keyName == " ")  return VK_SPACE;
    if (keyName == "0")  return VK_NUMPAD0;
    if (keyName == "1")  return VK_NUMPAD1;
    if (keyName == "/")  return VK_DIVIDE;
    return 0;
}

bool postKeyToHost (void* hostNative, WORD vk)
{
    auto* hwnd = static_cast<HWND> (hostNative);
    if (hwnd == nullptr || ! IsWindow (hwnd) || vk == 0)
        return false;
    const auto sc = MapVirtualKeyW (vk, MAPVK_VK_TO_VSC);
    const LPARAM down = 1 | (static_cast<LPARAM> (sc) << 16);
    const LPARAM up = down | (1L << 30) | (1L << 31);
    if (PostMessageW (hwnd, WM_KEYDOWN, vk, down) == 0)
        return false;
    PostMessageW (hwnd, WM_KEYUP, vk, up);
    return true;
}

bool forwardKeyToHost (juce::Component& editor, const juce::String& keyName)
{
    const WORD vk = keyNameToVK (keyName);
    if (vk == 0)
        return false;
    auto* peer = editor.getPeer();
    if (peer == nullptr)
        return false;
    auto* plugin = static_cast<HWND> (peer->getNativeHandle());
    if (plugin == nullptr)
        return false;
    HWND root = GetAncestor (plugin, GA_ROOT);
    HWND rootOwner = GetAncestor (plugin, GA_ROOTOWNER);
    HWND owner = GetWindow (root != nullptr ? root : plugin, GW_OWNER);
    auto* dest = static_cast<HWND> (bridge::chooseHostHwnd (plugin, rootOwner, root, owner));
    if (dest == nullptr || dest == plugin)
        return false;
    return postKeyToHost (dest, vk);
}
#else
bool forwardKeyToHost (juce::Component& editor, const juce::String& keyName)
{
    juce::ignoreUnused (editor, keyName);
    return false;
}
#endif
struct QuietCorner : public juce::ResizableCornerComponent
{
    using juce::ResizableCornerComponent::ResizableCornerComponent;
    void paint (juce::Graphics&) override {}
};

juce::File resolveDistRoot()
{
    // Issue 3: cache the filesystem walk so reopening the editor window is fast.
    static const juce::File cached = []() -> juce::File
    {
#ifdef NEUROKORE_WEB_DIST_DIR
        const juce::File fromCMake (NEUROKORE_WEB_DIST_DIR);
        if (fromCMake.getChildFile ("index.html").existsAsFile())
            return fromCMake;
#endif
        auto here = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                        .getParentDirectory();
        for (int i = 0; i < 8; ++i)
        {
            const auto sibling = here.getChildFile ("web").getChildFile ("dist");
            if (sibling.getChildFile ("index.html").existsAsFile())
                return sibling;
            here = here.getParentDirectory();
        }
        return {};
    }();
    return cached;
}

juce::WebBrowserComponent::Resource toResource (const bridge::WebAsset& asset)
{
    return { asset.data, asset.mimeType };
}

juce::var failVar (const juce::String& message)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", false);
    juce::Array<juce::var> diags;
    auto* d = new juce::DynamicObject();
    d->setProperty ("line", 1);
    d->setProperty ("column", 1);
    d->setProperty ("message", message);
    diags.add (juce::var (d));
    o->setProperty ("diagnostics", diags);
    return juce::var (o);
}
} // namespace

bool WebPluginEditor::PageBrowser::pageAboutToLoad (const juce::String& newURL)
{
    if (newURL == getResourceProviderRoot())
        return true;
    if (allowedDevUrl.isNotEmpty() && newURL.startsWith (allowedDevUrl))
        return true;
    if (newURL.startsWith ("http://localhost:") || newURL.startsWith ("http://127.0.0.1:"))
        return true;
    return newURL.startsWith (getResourceProviderRoot());
}

juce::WebBrowserComponent::Options WebPluginEditor::makeOptions()
{
#if JUCE_WINDOWS
    juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("NEUROKORE-webview2")
        .createDirectory();
#endif

    const auto devUrl = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_DEV_URL", {});
    std::optional<juce::String> origin;
    if (devUrl.isNotEmpty())
        origin = juce::URL (devUrl).getOrigin();

    auto opts = bridge::webEditorProbeOptions()
                    .withKeepPageLoadedWhenBrowserIsHidden()
                    .withNativeIntegrationEnabled()
                    .withNativeFunction ("UI_READY",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            bridge.handleNative ("UI_READY", args);
                            if (browser != nullptr)
                            {
                                browser->emitEventIfBrowserIsVisible ("hello", bridge.tryEmitHello());
                                auto seeded = session.seed (audioProcessor.getScript());
                                seeded.origin = "host";
                                pushOutcome (seeded);
                                pushHost();
                                startTimerHz (8);
                            }
                            complete (juce::var());
                        })
                    .withNativeFunction ("compile",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String origin = "editor";
                            juce::String script;
                            if (args.size() > 0 && args[0].isObject())
                            {
                                origin = args[0].getProperty ("origin", "editor").toString();
                                script = args[0].getProperty ("script", "").toString();
                            }
                            else if (args.size() > 0)
                            {
                                script = args[0].toString();
                            }

                            const auto out = session.compile (script, origin,
                                [this] (const juce::String& text, juce::String& error)
                                {
                                    return audioProcessor.setFormula (text, error, true);
                                });
                            pushOutcome (out);
                            complete (session.toCompileResultVar (out));
                        })
                    .withNativeFunction ("lint",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String script;
                            if (args.size() > 0 && args[0].isObject())
                                script = args[0].getProperty ("script", "").toString();
                            else if (args.size() > 0)
                                script = args[0].toString();
                            const auto out = session.lint (script);
                            if (browser != nullptr && bridge.allowOutbound())
                                browser->emitEventIfBrowserIsVisible ("compileResult",
                                                                      session.toCompileResultVar (out));
                            complete (session.toCompileResultVar (out));
                        })
                    .withNativeFunction ("graphOp",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String origin = "canvas";
                            juce::var payload;
                            if (args.size() > 0 && args[0].isObject())
                            {
                                origin = args[0].getProperty ("origin", "canvas").toString();
                                payload = args[0];
                            }
                            juce::String error;
                            bridge::GraphOp op;
                            if (! bridge::graphOpFromVar (payload, op, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            dsl::GraphDocument doc;
                            if (! dsl::parse (session.lastValidScript(), doc, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            if (! bridge::applyGraphOp (doc, op, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            const auto script = dsl::emit (doc);
                            const auto out = session.compile (script, origin,
                                [this] (const juce::String& text, juce::String& err)
                                {
                                    return audioProcessor.setFormula (text, err, true);
                                });
                            pushOutcome (out);
                            complete (session.toCompileResultVar (out));
                        })
                    .withNativeFunction ("setParam",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String error;
                            bridge::ParamGesture g;
                            if (! bridge::paramGestureFromVar (args.size() ? args[0] : juce::var(), g, error)
                                || ! bridge::applyParamGesture (audioProcessor.apvts, g, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            if (browser != nullptr && bridge.allowOutbound())
                                browser->emitEventIfBrowserIsVisible ("params",
                                                                      bridge::paramsVar (audioProcessor));
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("setChoice",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String error;
                            bridge::ChoiceCmd c;
                            if (! bridge::choiceCmdFromVar (args.size() ? args[0] : juce::var(), c, error)
                                || ! bridge::applyChoice (audioProcessor, c, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            pushHost();
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("preset",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String error;
                            bridge::PresetCmd c;
                            if (! bridge::presetCmdFromVar (args.size() ? args[0] : juce::var(), c, error)
                                || ! bridge::applyPresetCmd (audioProcessor, c, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            auto seeded = session.seed (audioProcessor.getScript());
                            seeded.origin = "preset";
                            pushOutcome (seeded);
                            pushHost();
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("setUi",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            if (args.size() > 0 && args[0].isObject())
                            {
                                const auto& o = args[0];
                                if (! o.getProperty ("scale", juce::var()).isVoid())
                                    UiSettings::get().setUiScalePercent ((int) o.getProperty ("scale", 100));
                                if (! o.getProperty ("bpmFollow", juce::var()).isVoid())
                                    UiSettings::get().setUseHostTempo ((bool) o.getProperty ("bpmFollow", true));
                                if (! o.getProperty ("bpmUser", juce::var()).isVoid())
                                    UiSettings::get().setUserBpm ((float) o.getProperty ("bpmUser", 120.0));
                                if (! o.getProperty ("language", juce::var()).isVoid())
                                    audioProcessor.loadLanguage (o.getProperty ("language", "en").toString());
                                if (! o.getProperty ("explorerCat", juce::var()).isVoid())
                                    audioProcessor.setLastPresetBrowserCategory (
                                        o.getProperty ("explorerCat", "").toString());
                            }
                            pushHost();
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("midiLearn",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String id = "a";
                            if (args.size() > 0 && args[0].isObject())
                                id = args[0].getProperty ("param", "a").toString();
                            else if (args.size() > 0)
                                id = args[0].toString();
                            audioProcessor.midiLearnManager.startLearning (id);
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("pickFile",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String kind = "ir";
                            juce::String slot = "ir1";
                            if (args.size() > 0 && args[0].isObject())
                            {
                                kind = args[0].getProperty ("kind", "ir").toString();
                                slot = args[0].getProperty ("slot", "ir1").toString();
                            }
                            pickFile (kind, slot);
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("irSlot",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String action = "clear";
                            juce::String slot = "ir1";
                            if (args.size() > 0 && args[0].isObject())
                            {
                                action = args[0].getProperty ("action", "clear").toString();
                                slot = args[0].getProperty ("slot", "ir1").toString();
                            }
                            irSlot (action, slot);
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("overlay",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String name;
                            if (args.size() > 0 && args[0].isObject())
                                name = args[0].getProperty ("name", "").toString();
                            if (name == "audio")
                                tryOpenStandaloneAudioSettings();
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("undo",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String op = "undo";
                            if (args.size() > 0 && args[0].isObject())
                                op = args[0].getProperty ("op", "undo").toString();
                            const bool ok = op == "redo" ? audioProcessor.redo()
                                                         : audioProcessor.undo();
                            if (ok)
                            {
                                auto seeded = session.seed (audioProcessor.getScript());
                                seeded.origin = "undo";
                                pushOutcome (seeded);
                                pushHost();
                            }
                            complete (juce::var (ok));
                        })
                    .withNativeFunction ("hostKey",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String key = "Space";
                            if (args.size() > 0 && args[0].isObject())
                                key = args[0].getProperty ("key", "Space").toString();
                            else if (args.size() > 0)
                                key = args[0].toString();
                            if (bridge::isHostTransportName (key))
                                forwardKeyToHost (*this, key);
                            complete (juce::var (true));
                        })
                    .withNativeFunction ("applyLayout",
                        [this] (const juce::Array<juce::var>& args, auto complete)
                        {
                            juce::String origin = "elk";
                            juce::var positions;
                            if (args.size() > 0 && args[0].isObject())
                            {
                                origin = args[0].getProperty ("origin", "elk").toString();
                                positions = args[0].getProperty ("positions", juce::var());
                            }
                            juce::String error;
                            dsl::GraphDocument doc;
                            if (! dsl::parse (session.lastValidScript(), doc, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            if (! bridge::applyPositions (doc, positions, error))
                            {
                                complete (failVar (error));
                                return;
                            }
                            const auto script = dsl::emit (doc);
                            audioProcessor.storeScriptLayout (script);
                            auto seeded = session.seed (script);
                            seeded.origin = origin;
                            pushOutcome (seeded);
                            complete (session.toCompileResultVar (seeded));
                        })
                    .withResourceProvider ([this] (const juce::String& url)
                                           { return provideResource (url); },
                                           origin);

    return opts;
}

std::optional<juce::WebBrowserComponent::Resource>
WebPluginEditor::provideResource (const juce::String& url)
{
    if (auto fromDisk = bridge::loadWebAsset (distRoot, url))
        return toResource (*fromDisk);

    const auto path = url.upToFirstOccurrenceOf ("?", false, false);
    if (path == "/" || path == "/index.html" || path.isEmpty())
        return toResource (bridge::fallbackIndexAsset());

    auto rel = path.startsWithChar ('/') ? path.substring (1) : path;
    if (rel == "telemetry.bin")
    {
        bridge::WebAsset asset;
        asset.mimeType = "application/octet-stream";
        asset.data.resize (bridge::TelemetryPump::kMaxBytes);
        const auto n = audioProcessor.getTelemetry().copyLatest (asset.data.data(), asset.data.size());
        if (n == 0)
            return std::nullopt;
        asset.data.resize (n);
        return toResource (asset);
    }

    return std::nullopt;
}

void WebPluginEditor::pushOutcome (const bridge::CompileOutcome& out)
{
    if (browser == nullptr || ! bridge.allowOutbound())
        return;
    browser->emitEventIfBrowserIsVisible ("compileResult", session.toCompileResultVar (out));
    browser->emitEventIfBrowserIsVisible ("ast", session.toAstVar (out));
}

void WebPluginEditor::pushHost()
{
    if (browser == nullptr || ! bridge.allowOutbound())
        return;
    browser->emitEventIfBrowserIsVisible ("params", bridge::paramsVar (audioProcessor));
    browser->emitEventIfBrowserIsVisible ("host", bridge::hostVar (audioProcessor));
    browser->emitEventIfBrowserIsVisible ("presetState", bridge::presetStateVar (audioProcessor));
    browser->emitEventIfBrowserIsVisible ("license", bridge::licenseVar (audioProcessor));
    browser->emitEventIfBrowserIsVisible ("ir", bridge::irVar (audioProcessor));
    browser->emitEventIfBrowserIsVisible ("catalog", bridge::catalogVar());
}

void WebPluginEditor::timerCallback()
{
    pushHost();
}

void WebPluginEditor::pickFile (const juce::String& kind, const juce::String& slot)
{
    const bool lic = kind.equalsIgnoreCase ("license");
    const bool preset = kind.equalsIgnoreCase ("preset");
    const char* title = lic ? "Install license" : preset ? "Import preset" : "Load IR";
    const char* filter = lic ? "*.lic" : preset ? "*.nrk;*.zip" : "*.wav;*.aif;*.aiff;*.flac";
    fileChooser = std::make_unique<juce::FileChooser> (title, juce::File(), filter);
    const int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this, lic, preset, slot] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile())
            return;
        if (lic)
            audioProcessor.importProductLicense (f);
        else if (preset)
        {
            PresetLibrary::importPaths (juce::StringArray { f.getFullPathName() });
            if (PresetLibrary::isNrkFile (f) && audioProcessor.presetManager.loadPreset (f))
            {
                PresetManager::Info info;
                audioProcessor.presetManager.readInfo (f, info);
                audioProcessor.setCurrentPresetName (
                    info.name.isNotEmpty() ? info.name : f.getFileNameWithoutExtension());
                if (info.category.isNotEmpty())
                    audioProcessor.setLastPresetBrowserCategory (info.category);
            }
        }
        else
        {
            juce::String error;
            audioProcessor.loadIrFromFile (slot, f, error);
        }
        pushHost();
    });
}

void WebPluginEditor::irSlot (const juce::String& action, const juce::String& slot)
{
    if (action.equalsIgnoreCase ("preview"))
        audioProcessor.startIrPreview (slot);
    else
        audioProcessor.clearIr (slot);
    pushHost();
}

WebPluginEditor::WebPluginEditor (NeuroKoreAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      audioProcessor (p),
      bridge (p.getScript().length()),
      distRoot (resolveDistRoot())
{
    session.seed (p.getScript());

    setOpaque (true);
    const int minW = 800;
    const int minH = juce::jmax (1, (int) std::lround ((double) minW / Config::kUiAspectRatio));
    sizeConstrain.setFixedAspectRatio (Config::kUiAspectRatio);
    sizeConstrain.setSizeLimits (minW, minH, Config::kUiMaxWindowWidth, Config::kUiMaxWindowHeight);
    setConstrainer (&sizeConstrain);
    setResizable (true, false);
    setResizeLimits (minW, minH, Config::kUiMaxWindowWidth, Config::kUiMaxWindowHeight);
    setSize (Config::kUiDesignWidth, Config::kUiDesignHeight);
    cornerGrip = std::make_unique<QuietCorner> (this, &sizeConstrain);
    addAndMakeVisible (*cornerGrip);
    cornerGrip->setAlwaysOnTop (true);
    cornerGrip->setOpaque (false);

    auto options = makeOptions();
    if (! juce::WebBrowserComponent::areOptionsSupported (options))
        return;

    browser = std::make_unique<PageBrowser> (std::move (options));
    const auto devUrl = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_DEV_URL", {});
    browser->allowedDevUrl = devUrl;
    addAndMakeVisible (*browser);
    resized();

    if (devUrl.isNotEmpty())
        browser->goToURL (devUrl.endsWithChar ('/') ? devUrl : devUrl + "/");
    else
        browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    juce::Component::SafePointer<WebPluginEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr)
            safe->resized();
    });
    juce::Timer::callAfterDelay (80, [safe]
    {
        if (safe != nullptr)
            safe->resized();
    });
}

WebPluginEditor::~WebPluginEditor() = default;

void WebPluginEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0e0e12));
    g.setColour (juce::Colour (0x88ff003c));
    g.drawRect (r.reduced (0.5f), 1.0f);
    g.setColour (juce::Colour (0x33ff003c));
    g.drawRect (r.reduced (3.5f), 1.0f);
    const float s = 20.0f;
    const float x = r.getRight();
    const float y = r.getBottom();
    g.setColour (juce::Colour (0x66ff003c));
    for (int i = 0; i < 3; ++i)
    {
        const float o = 5.0f + (float) i * 5.0f;
        g.drawLine (x - o, y - 3.0f, x - 3.0f, y - o, 1.2f);
    }
    juce::ignoreUnused (s);
}

void WebPluginEditor::resized()
{
    auto r = getLocalBounds();
    const int pad = 8;
    const int grip = 22;
    const double ar = Config::kUiAspectRatio;
    int availW = juce::jmax (1, r.getWidth() - pad * 2);
    int availH = juce::jmax (1, r.getHeight() - pad * 2);
    int w = availW;
    int h = (int) std::lround ((double) w / ar);
    if (h > availH)
    {
        h = availH;
        w = (int) std::lround ((double) h * ar);
    }
    if (browser != nullptr)
        browser->setBounds (pad, pad, w, h);
    if (cornerGrip != nullptr)
    {
        cornerGrip->setBounds (r.getWidth() - grip, r.getHeight() - grip, grip, grip);
        cornerGrip->toFront (false);
    }
}

juce::AudioProcessorEditor* createWebEditor (NeuroKoreAudioProcessor& p)
{
    return new WebPluginEditor (p);
}
