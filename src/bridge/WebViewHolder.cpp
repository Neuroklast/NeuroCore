#include "WebViewHolder.h"
#include "../core/Config.h"
#include "../core/PluginProcessor.h"
#include "../utils/PresetLibrary.h"
#include "../utils/UiSettings.h"
#include "CompileSession.h"
#include "GraphOps.h"
#include "HostKeys.h"
#include "HostSnapshot.h"
#include "TelemetryPump.h"
#include "WebEditorPolicy.h"
#include "../dsl/GraphModel.h"
#include "../ui/StandaloneAudioSettings.h"

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif JUCE_MAC && defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace bridge
{
namespace
{
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
#if JUCE_WINDOWS
bool postKeyToHost (void* hostNative, WORD vk)
{
    auto* hwnd = static_cast<HWND> (hostNative);
    if (hwnd == nullptr || ! IsWindow (hwnd) || vk == 0)
        return false;
    const auto sc = MapVirtualKeyW (vk, MAPVK_VK_TO_VSC);
    LPARAM down = 1 | (static_cast<LPARAM> (sc) << 16);
    if (bridge::isExtendedVk ((int) vk))
        down |= (1L << 24);
    const LPARAM up = down | (1L << 30) | (1L << 31);
    if (PostMessageW (hwnd, WM_KEYDOWN, vk, down) == 0)
        return false;
    PostMessageW (hwnd, WM_KEYUP, vk, up);
    return true;
}

bool forwardKeyToHost (juce::Component& editor,
                       const juce::String& keyName,
                       const juce::String& code)
{
    const auto vk = (WORD) bridge::hostKeyNameToVk (keyName, code);
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
#elif JUCE_MAC
bool forwardKeyToHost (juce::Component& editor,
                       const juce::String& keyName,
                       const juce::String& code)
{
    juce::ignoreUnused (editor);
   #if JucePlugin_Build_Standalone
    juce::ignoreUnused (keyName, code);
    return false;
   #else
    const int cg = bridge::hostKeyNameToCgKeyCode (keyName, code);
    if (cg == 0xFFFF)
        return false;
    const auto kc = static_cast<CGKeyCode> (cg);
    CGEventRef down = CGEventCreateKeyboardEvent (nullptr, kc, true);
    CGEventRef up   = CGEventCreateKeyboardEvent (nullptr, kc, false);
    if (down == nullptr || up == nullptr)
    {
        if (down != nullptr) CFRelease (down);
        if (up != nullptr) CFRelease (up);
        return false;
    }
    CGEventPost (kCGSessionEventTap, down);
    CGEventPost (kCGSessionEventTap, up);
    CFRelease (down);
    CFRelease (up);
    return true;
   #endif
}
#else
bool forwardKeyToHost (juce::Component& editor,
                       const juce::String& keyName,
                       const juce::String& code)
{
    juce::ignoreUnused (editor, keyName, code);
    return false;
}
#endif
#endif

juce::File resolveDistRoot()
{
    static const juce::File cached = []() -> juce::File
    {
        if (! bridge::wantDiskWebAssets())
            return {};
#ifdef NEUROKORE_WEB_DIST_DIR
        const juce::File fromCMake (NEUROKORE_WEB_DIST_DIR);
        if (fromCMake.getChildFile ("index.html").existsAsFile())
            return fromCMake;
#endif
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto bundled = exe.getParentDirectory().getParentDirectory()
                                 .getChildFile ("Resources").getChildFile ("web");
        if (bundled.getChildFile ("index.html").existsAsFile())
            return bundled;
        auto here = exe.getParentDirectory();
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
} // namespace

#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
juce::WebBrowserComponent::Resource toResource (const WebAsset& asset)
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

struct PageBrowser : juce::WebBrowserComponent
{
    using juce::WebBrowserComponent::WebBrowserComponent;
    bool pageAboutToLoad (const juce::String& newURL) override
    {
        if (newURL == getResourceProviderRoot())
            return true;
        if (allowedDevUrl.isNotEmpty() && newURL.startsWith (allowedDevUrl))
            return true;
        if (newURL.startsWith ("http://localhost:") || newURL.startsWith ("http://127.0.0.1:"))
            return true;
        return newURL.startsWith (getResourceProviderRoot());
    }
    juce::String allowedDevUrl;
};

#if JUCE_WINDOWS
class ParkWindow : public juce::Component
{
public:
    ParkWindow()
    {
        setOpaque (true);
        setSize (8, 8);
        addToDesktop (juce::ComponentPeer::windowIsTemporary
                      | juce::ComponentPeer::windowIgnoresMouseClicks);
        setTopLeftPosition (-4000, -4000);
        setVisible (true);
    }

    ~ParkWindow() override
    {
        if (isOnDesktop())
            removeFromDesktop();
    }
};
#endif
#endif

struct WebViewHolder::Impl : private juce::Timer,
                             private juce::ComponentListener
{
    explicit Impl (NeuroKoreAudioProcessor& p)
        : proc (p),
          bridge (p.getScript().length()),
          distRoot (resolveDistRoot())
    {
        session.seed (p.getScript());
        constructSurface();
    }

    ~Impl() override
    {
        stopTimer();
        if (attachedTo != nullptr)
        {
            attachedTo->removeComponentListener (this);
            if (surface != nullptr && surface->getParentComponent() == attachedTo)
                attachedTo->removeChildComponent (surface.get());
            attachedTo = nullptr;
        }
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER && JUCE_WINDOWS
        parkNative();
        surface.reset();
        browser = nullptr;
        park.reset();
        if (ownerHwnd != nullptr)
        {
            DestroyWindow (ownerHwnd);
            ownerHwnd = nullptr;
        }
#endif
    }

    std::uint64_t identity() const noexcept
    {
        return (std::uint64_t) (juce::pointer_sized_int) surface.get();
    }

    std::optional<WebAsset> serve (const juce::String& url)
    {
        if (wantDiskWebAssets())
            if (auto fromDisk = loadWebAsset (distRoot, url))
                return fromDisk;
        if (auto fromZip = zip.load (url))
            return fromZip;

        const auto path = url.upToFirstOccurrenceOf ("?", false, false);
        if (path == "/" || path == "/index.html" || path.isEmpty())
            return fallbackIndexAsset();
        return std::nullopt;
    }

    void attach (juce::Component& editor)
    {
        if (attachedTo == &editor)
        {
            syncNative (editor);
            return;
        }
        if (attachedTo != nullptr)
            detach (*attachedTo);

        attachedTo = &editor;
        editor.addComponentListener (this);

#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER && JUCE_WINDOWS
        syncNative (editor);
#else
        if (surface != nullptr)
        {
            editor.addAndMakeVisible (*surface);
            surface->setVisible (true);
        }
#endif
        onEditorShown();
    }

    void detach (juce::Component& editor)
    {
        if (attachedTo != &editor)
            return;
        stopTimer();
        editor.removeComponentListener (this);

#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER && JUCE_WINDOWS
        parkNative();
#endif
        if (surface != nullptr && surface->getParentComponent() == &editor)
            editor.removeChildComponent (surface.get());

        if (surface != nullptr)
            surface->setVisible (false);
        attachedTo = nullptr;
    }

    void layout (juce::Rectangle<int> inner)
    {
        lastInner = inner;
        if (surface == nullptr)
            return;
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER && JUCE_WINDOWS
        if (park != nullptr)
        {
            park->setSize (juce::jmax (1, inner.getWidth()), juce::jmax (1, inner.getHeight()));
            surface->setBounds (0, 0, inner.getWidth(), inner.getHeight());
            positionPark (inner);
        }
#else
        surface->setBounds (inner);
#endif
    }

    void constructSurface()
    {
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
#if JUCE_WINDOWS
        juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("NEUROKORE-webview2")
            .createDirectory();
#endif
        auto options = makeOptions();
        if (! juce::WebBrowserComponent::areOptionsSupported (options))
        {
            surface = std::make_unique<juce::Component>();
            return;
        }

        auto page = std::make_unique<PageBrowser> (std::move (options));
        const auto devUrl = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_DEV_URL", {});
        page->allowedDevUrl = devUrl;
        browser = page.get();
#if JUCE_WINDOWS
        park = std::make_unique<ParkWindow>();
        park->addAndMakeVisible (*browser);
        ensureOwnerHwnd();
        parkNative(); // HWND lives under ownerHwnd from birth — never under IPlugView
#else
        browser->setVisible (false);
#endif
        surface = std::move (page);

        if (devUrl.isNotEmpty())
            browser->goToURL (devUrl.endsWithChar ('/') ? devUrl : devUrl + "/");
        else
            browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        didNavigate = true;
#else
        surface = std::make_unique<juce::Component>();
#endif
    }

#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
    juce::WebBrowserComponent::Options makeOptions()
    {
        const auto devUrl = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_DEV_URL", {});
        std::optional<juce::String> origin;
        if (devUrl.isNotEmpty())
            origin = juce::URL (devUrl).getOrigin();

        auto opts = webEditorProbeOptions()
                        .withKeepPageLoadedWhenBrowserIsHidden()
                        .withNativeIntegrationEnabled()
                        .withNativeFunction ("UI_READY",
                            [this] (const juce::Array<juce::var>& args, auto complete)
                            {
                                bridge.handleNative ("UI_READY", args);
                                if (browser != nullptr)
                                {
                                    browser->emitEventIfBrowserIsVisible ("hello", bridge.tryEmitHello());
                                    auto seeded = session.seed (proc.getScript());
                                    seeded.origin = "host";
                                    pushOutcome (seeded);
                                    pushHost();
                                    if (attachedTo != nullptr)
                                        startTimerHz (8);
                                }
                                complete (juce::var());
                            })
                        .withNativeFunction ("compile",
                            [this] (const juce::Array<juce::var>& args, auto complete)
                            {
                                juce::String originName = "editor";
                                juce::String script;
                                if (args.size() > 0 && args[0].isObject())
                                {
                                    originName = args[0].getProperty ("origin", "editor").toString();
                                    script = args[0].getProperty ("script", "").toString();
                                }
                                else if (args.size() > 0)
                                {
                                    script = args[0].toString();
                                }

                                const auto out = session.compile (script, originName,
                                    [this] (const juce::String& text, juce::String& error)
                                    {
                                        return proc.setFormula (text, error, true);
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
                                juce::String originName = "canvas";
                                juce::var payload;
                                if (args.size() > 0 && args[0].isObject())
                                {
                                    originName = args[0].getProperty ("origin", "canvas").toString();
                                    payload = args[0];
                                }
                                juce::String error;
                                GraphOp op;
                                if (! graphOpFromVar (payload, op, error))
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
                                if (! applyGraphOp (doc, op, error))
                                {
                                    complete (failVar (error));
                                    return;
                                }
                                const auto script = dsl::emit (doc);
                                const auto out = session.compile (script, originName,
                                    [this] (const juce::String& text, juce::String& err)
                                    {
                                        return proc.setFormula (text, err, true);
                                    });
                                pushOutcome (out);
                                complete (session.toCompileResultVar (out));
                            })
                        .withNativeFunction ("setParam",
                            [this] (const juce::Array<juce::var>& args, auto complete)
                            {
                                juce::String error;
                                ParamGesture g;
                                if (! paramGestureFromVar (args.size() ? args[0] : juce::var(), g, error)
                                    || ! applyParamGesture (proc.apvts, g, error))
                                {
                                    complete (failVar (error));
                                    return;
                                }
                                if (browser != nullptr && bridge.allowOutbound())
                                    browser->emitEventIfBrowserIsVisible ("params",
                                                                          paramsVar (proc));
                                complete (juce::var (true));
                            })
                        .withNativeFunction ("setChoice",
                            [this] (const juce::Array<juce::var>& args, auto complete)
                            {
                                juce::String error;
                                ChoiceCmd c;
                                if (! choiceCmdFromVar (args.size() ? args[0] : juce::var(), c, error)
                                    || ! applyChoice (proc, c, error))
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
                                PresetCmd c;
                                if (! presetCmdFromVar (args.size() ? args[0] : juce::var(), c, error)
                                    || ! applyPresetCmd (proc, c, error))
                                {
                                    complete (failVar (error));
                                    return;
                                }
                                auto seeded = session.seed (proc.getScript());
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
                                        proc.loadLanguage (o.getProperty ("language", "en").toString());
                                    if (! o.getProperty ("explorerCat", juce::var()).isVoid())
                                        proc.setLastPresetBrowserCategory (
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
                                proc.midiLearnManager.startLearning (id);
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
                                juce::ignoreUnused (args);
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
                                const bool ok = op == "redo" ? proc.redo()
                                                             : proc.undo();
                                if (ok)
                                {
                                    auto seeded = session.seed (proc.getScript());
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
                                juce::String code;
                                if (args.size() > 0 && args[0].isObject())
                                {
                                    key  = args[0].getProperty ("key", "Space").toString();
                                    code = args[0].getProperty ("code", "").toString();
                                }
                                else if (args.size() > 0)
                                {
                                    key = args[0].toString();
                                }
                                if (canForwardHostKey (key, code) && attachedTo != nullptr)
                                    forwardKeyToHost (*attachedTo, key, code);
                                complete (juce::var (true));
                            })
                        .withNativeFunction ("applyLayout",
                            [this] (const juce::Array<juce::var>& args, auto complete)
                            {
                                juce::String originName = "elk";
                                juce::var positions;
                                if (args.size() > 0 && args[0].isObject())
                                {
                                    originName = args[0].getProperty ("origin", "elk").toString();
                                    positions = args[0].getProperty ("positions", juce::var());
                                }
                                juce::String error;
                                dsl::GraphDocument doc;
                                if (! dsl::parse (session.lastValidScript(), doc, error))
                                {
                                    complete (failVar (error));
                                    return;
                                }
                                if (! applyPositions (doc, positions, error))
                                {
                                    complete (failVar (error));
                                    return;
                                }
                                const auto script = dsl::emit (doc);
                                proc.storeScriptLayout (script);
                                auto seeded = session.seed (script);
                                seeded.origin = originName;
                                pushOutcome (seeded);
                                complete (session.toCompileResultVar (seeded));
                            })
                        .withResourceProvider ([this] (const juce::String& url)
                                               { return provideResource (url); },
                                               origin);
        return opts;
    }

    std::optional<juce::WebBrowserComponent::Resource> provideResource (const juce::String& url)
    {
        const auto path = url.upToFirstOccurrenceOf ("?", false, false);
        auto rel = path.startsWithChar ('/') ? path.substring (1) : path;
        if (rel == "telemetry.bin")
        {
            WebAsset asset;
            asset.mimeType = "application/octet-stream";
            asset.data.resize (TelemetryPump::kMaxBytes);
            const auto n = proc.getTelemetry().copyLatest (asset.data.data(), asset.data.size());
            if (n == 0)
                return std::nullopt;
            asset.data.resize (n);
            return toResource (asset);
        }

        if (auto asset = serve (url))
            return toResource (*asset);
        return std::nullopt;
    }

    void pushOutcome (const CompileOutcome& out)
    {
        if (browser == nullptr || ! bridge.allowOutbound())
            return;
        browser->emitEventIfBrowserIsVisible ("compileResult", session.toCompileResultVar (out));
        browser->emitEventIfBrowserIsVisible ("ast", session.toAstVar (out));
    }

    void pushHost()
    {
        if (browser == nullptr || ! bridge.allowOutbound())
            return;
        browser->emitEventIfBrowserIsVisible ("params", paramsVar (proc));
        browser->emitEventIfBrowserIsVisible ("host", hostVar (proc));
        browser->emitEventIfBrowserIsVisible ("presetState", presetStateVar (proc));
        browser->emitEventIfBrowserIsVisible ("license", licenseVar (proc));
        browser->emitEventIfBrowserIsVisible ("ir", irVar (proc));
        browser->emitEventIfBrowserIsVisible ("catalog", catalogVar());
    }

    void pickFile (const juce::String& kind, const juce::String& slot)
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
                proc.importProductLicense (f);
            else if (preset)
            {
                PresetLibrary::importPaths (juce::StringArray { f.getFullPathName() });
                if (PresetLibrary::isNrkFile (f) && proc.presetManager.loadPreset (f))
                {
                    PresetManager::Info info;
                    proc.presetManager.readInfo (f, info);
                    proc.setCurrentPresetName (
                        info.name.isNotEmpty() ? info.name : f.getFileNameWithoutExtension());
                    if (info.category.isNotEmpty())
                        proc.setLastPresetBrowserCategory (info.category);
                }
            }
            else
            {
                juce::String error;
                proc.loadIrFromFile (slot, f, error);
            }
            pushHost();
        });
    }

    void irSlot (const juce::String& action, const juce::String& slot)
    {
        if (action.equalsIgnoreCase ("preview"))
            proc.startIrPreview (slot);
        else
            proc.clearIr (slot);
        pushHost();
    }
#endif

    void onEditorShown()
    {
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
        if (bridge.allowOutbound() && attachedTo != nullptr)
        {
            pushHost();
            startTimerHz (8);
        }
#endif
    }

    void timerCallback() override
    {
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
        if (attachedTo == nullptr)
        {
            stopTimer();
            return;
        }
        pushHost();
#endif
    }

    void componentParentHierarchyChanged (juce::Component& c) override
    {
        if (&c == attachedTo)
            syncNative (c);
    }

    void componentVisibilityChanged (juce::Component& c) override
    {
        if (&c == attachedTo)
            syncNative (c);
    }

    void syncNative (juce::Component& editor)
    {
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER && JUCE_WINDOWS
        if (attachedTo != &editor)
            return;
        if (! editor.isShowing() || editor.getPeer() == nullptr)
            parkNative();
        else
            tryEmbed();
#else
        juce::ignoreUnused (editor);
#endif
    }

    void tryEmbed()
    {
#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER && JUCE_WINDOWS
        if (attachedTo == nullptr || park == nullptr)
            return;
        auto* edPeer = attachedTo->getPeer();
        auto* parkPeer = park->getPeer();
        if (edPeer == nullptr || parkPeer == nullptr)
            return;
        auto* child = static_cast<HWND> (parkPeer->getNativeHandle());
        auto* editorHwnd = static_cast<HWND> (edPeer->getNativeHandle());
        if (child == nullptr || editorHwnd == nullptr || ! IsWindow (child) || ! IsWindow (editorHwnd))
            return;
        ensureOwnerHwnd();
        auto* parent = static_cast<HWND> (WebViewHolder::parkParentForEditor (editorHwnd, ownerHwnd));
        // Never WS_CHILD of IPlugView — DestroyWindow(plugin) would take the WebView with it.
        if (parent == nullptr || parent == editorHwnd)
            parent = ownerHwnd;
        if (parent == nullptr || parent == editorHwnd)
            return;

        auto style = GetWindowLongPtr (child, GWL_STYLE);
        using FlagType = decltype (style);
        if (parent == ownerHwnd)
        {
            style &= ~(FlagType) WS_CHILD;
            style |= (FlagType) WS_POPUP;
        }
        else
        {
            style &= ~(FlagType) WS_POPUP;
            style |= (FlagType) WS_CHILD;
        }
        SetWindowLongPtr (child, GWL_STYLE, style);
        SetParent (child, parent);
        if (IsChild (editorHwnd, child) || GetParent (child) == editorHwnd)
        {
            parkNative();
            return;
        }
        park->setVisible (true);
        if (surface != nullptr)
            surface->setVisible (true);
        ShowWindow (child, SW_SHOWNA);
        positionPark (lastInner.isEmpty() && attachedTo != nullptr
                          ? attachedTo->getLocalBounds().reduced (8)
                          : lastInner);
#endif
        juce::ignoreUnused (this);
    }

#if defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER && JUCE_WINDOWS
    void ensureOwnerHwnd()
    {
        if (ownerHwnd != nullptr)
            return;
        ownerHwnd = CreateWindowExW (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                     L"STATIC", L"NEUROKORE-webview-park",
                                     WS_POPUP, -32000, -32000, 8, 8,
                                     nullptr, nullptr, GetModuleHandleW (nullptr), nullptr);
        if (ownerHwnd != nullptr)
            ShowWindow (ownerHwnd, SW_HIDE);
    }

    void parkNative()
    {
        if (park == nullptr)
            return;
        auto* parkPeer = park->getPeer();
        if (parkPeer == nullptr)
            return;
        auto* child = static_cast<HWND> (parkPeer->getNativeHandle());
        if (child == nullptr || ! IsWindow (child))
            return;
        ensureOwnerHwnd();
        auto style = GetWindowLongPtr (child, GWL_STYLE);
        using FlagType = decltype (style);
        style &= ~(FlagType) WS_CHILD;
        style |= (FlagType) WS_POPUP;
        SetWindowLongPtr (child, GWL_STYLE, style);
        ShowWindow (child, SW_HIDE);
        if (ownerHwnd != nullptr)
            SetParent (child, ownerHwnd);
        park->setVisible (false);
    }

    void positionPark (juce::Rectangle<int> inner)
    {
        if (park == nullptr || attachedTo == nullptr || inner.isEmpty())
            return;
        auto* parkPeer = park->getPeer();
        auto* edPeer = attachedTo->getPeer();
        if (parkPeer == nullptr || edPeer == nullptr)
            return;
        auto* child = static_cast<HWND> (parkPeer->getNativeHandle());
        auto* editorHwnd = static_cast<HWND> (edPeer->getNativeHandle());
        if (child == nullptr || editorHwnd == nullptr || ! IsWindow (child) || ! IsWindow (editorHwnd))
            return;
        POINT tl { inner.getX(), inner.getY() };
        POINT br { inner.getRight(), inner.getBottom() };
        ClientToScreen (editorHwnd, &tl);
        ClientToScreen (editorHwnd, &br);
        auto* parent = GetParent (child);
        if (parent != nullptr && (GetWindowLongPtr (child, GWL_STYLE) & WS_CHILD) != 0)
        {
            ScreenToClient (parent, &tl);
            ScreenToClient (parent, &br);
        }
        SetWindowPos (child, HWND_TOP, tl.x, tl.y,
                      juce::jmax (1, (int) (br.x - tl.x)),
                      juce::jmax (1, (int) (br.y - tl.y)),
                      SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    }

    HWND ownerHwnd { nullptr };
    std::unique_ptr<ParkWindow> park;
    PageBrowser* browser { nullptr };
    std::unique_ptr<juce::FileChooser> fileChooser;
#elif defined(NEUROKORE_HAS_WEB_EDITOR) && JUCE_WEB_BROWSER
    PageBrowser* browser { nullptr };
    std::unique_ptr<juce::FileChooser> fileChooser;
#endif

    NeuroKoreAudioProcessor& proc;
    WebZipIndex zip;
    WebBridge bridge;
    CompileSession session;
    juce::File distRoot;
    std::unique_ptr<juce::Component> surface;
    juce::Component* attachedTo { nullptr };
    juce::Rectangle<int> lastInner;
    bool didNavigate { false };
};

WebViewHolder::WebViewHolder (NeuroKoreAudioProcessor& p)
    : impl (std::make_unique<Impl> (p))
{
}

WebViewHolder::~WebViewHolder() = default;

std::uint64_t WebViewHolder::browserIdentity() const noexcept
{
    return impl->identity();
}

int WebViewHolder::zipIndexBuildCount() const noexcept
{
    return impl->zip.buildCount();
}

std::optional<WebAsset> WebViewHolder::serve (const juce::String& url)
{
    return impl->serve (url);
}

WebBridge& WebViewHolder::bridge() noexcept
{
    return impl->bridge;
}

const WebBridge& WebViewHolder::bridge() const noexcept
{
    return impl->bridge;
}

void WebViewHolder::attach (juce::Component& editor)
{
    impl->attach (editor);
}

void WebViewHolder::detach (juce::Component& editor)
{
    impl->detach (editor);
}

bool WebViewHolder::isAttached() const noexcept
{
    return impl->attachedTo != nullptr;
}

juce::Component* WebViewHolder::browserComponent() noexcept
{
    return impl->surface.get();
}

void WebViewHolder::layout (juce::Rectangle<int> inner)
{
    impl->layout (inner);
}

void WebViewHolder::syncNativeAttachment (juce::Component& editor)
{
    impl->syncNative (editor);
}

#if JUCE_WINDOWS
void* WebViewHolder::parkParentForEditor (void* editorHwnd, void* ownerHwnd) noexcept
{
    auto* owner = static_cast<HWND> (ownerHwnd);
    auto* editor = static_cast<HWND> (editorHwnd);
    if (editor == nullptr || ! IsWindow (editor))
        return owner;
    HWND parent = GetParent (editor);
    // VST3 IPlugView HWND is destroyed in removeFromDesktop *before* ~Editor.
    // Parent the park as a sibling (host systemWindow) or under ownerHwnd — never
    // as a child of the editor HWND.
    if (parent == nullptr || parent == editor)
        return owner;
    return parent;
}
#endif

} // namespace bridge
