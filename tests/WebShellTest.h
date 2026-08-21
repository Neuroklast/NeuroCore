#pragma once

#include <JuceHeader.h>
#include <cstring>
#include <cstdlib>
#include "../src/bridge/HostKeys.h"
#include "../src/bridge/WebAssets.h"
#include "../src/bridge/WebBridge.h"
#include "../src/bridge/WebEditorPolicy.h"
#include "../src/utils/ExprTapeJit.h"

#ifdef _WIN32
#include <stdlib.h>
#endif

/** WP2 contracts: asset map, UI_READY gate, editor policy. No WebView required. */
class WebShellTest : public juce::UnitTest
{
public:
    WebShellTest() : juce::UnitTest ("WebShell", "Bridge") {}

    void runTest() override
    {
        beginTest ("mimeForPath covers html/js/css/png");
        {
            expectEquals (bridge::mimeForPath ("index.html"), juce::String ("text/html"));
            expectEquals (bridge::mimeForPath ("assets/app.js"), juce::String ("text/javascript"));
            expectEquals (bridge::mimeForPath ("assets/app.css"), juce::String ("text/css"));
            expectEquals (bridge::mimeForPath ("logo.png"), juce::String ("image/png"));
            expectEquals (bridge::mimeForPath ("fonts/apex.otf"), juce::String ("font/otf"));
        }

        beginTest ("loadWebAsset serves / as index.html and rejects traversal");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("nk-web-assets-contract");
            root.deleteRecursively();
            expect (root.createDirectory());
            expect (root.getChildFile ("index.html")
                        .replaceWithText ("<html>nk</html>"));
            expect (root.getChildFile ("assets").createDirectory());
            expect (root.getChildFile ("assets").getChildFile ("app.js")
                        .replaceWithText ("console.log(1)"));

            const auto index = bridge::loadWebAsset (root, "/");
            expect (index.has_value());
            expectEquals (index->mimeType, juce::String ("text/html"));
            const auto html = juce::String::fromUTF8 (reinterpret_cast<const char*> (index->data.data()),
                                                      (int) index->data.size());
            expect (html.contains ("nk"));

            const auto js = bridge::loadWebAsset (root, "/assets/app.js");
            expect (js.has_value());
            expectEquals (js->mimeType, juce::String ("text/javascript"));

            expect (! bridge::loadWebAsset (root, "/missing.css").has_value());
            expect (! bridge::loadWebAsset (root, "/../index.html").has_value());
            expect (! bridge::loadWebAsset (root, "/assets/../../index.html").has_value());

            root.deleteRecursively();
        }

        beginTest ("zip assets serve the editor without a web folder");
        {
            juce::MemoryOutputStream zipOut;
            juce::ZipFile::Builder builder;
            const char* html = "<html><div id=\"root\">nk-app</div></html>";
            builder.addEntry (new juce::MemoryInputStream (html, (size_t) std::strlen (html), false),
                              9, "index.html", juce::Time::getCurrentTime());
            const char* js = "console.log(1)";
            builder.addEntry (new juce::MemoryInputStream (js, (size_t) std::strlen (js), false),
                              9, "assets/app.js", juce::Time::getCurrentTime());
            expect (builder.writeToStream (zipOut, nullptr));

            const auto* data = zipOut.getData();
            const auto n = (size_t) zipOut.getDataSize();
            const juce::File emptyRoot;

            expect (! bridge::loadWebAsset (emptyRoot, "/").has_value(),
                    "testers have no sibling web/ folder");

            const auto index = bridge::loadWebAssetFromZip (data, n, "/");
            expect (index.has_value());
            expectEquals (index->mimeType, juce::String ("text/html"));
            const auto htmlOut = juce::String::fromUTF8 (
                reinterpret_cast<const char*> (index->data.data()), (int) index->data.size());
            expect (htmlOut.contains ("id=\"root\""));
            expect (htmlOut.contains ("nk-app"));

            const auto jsAsset = bridge::loadWebAssetFromZip (data, n, "/assets/app.js");
            expect (jsAsset.has_value());
            expectEquals (jsAsset->mimeType, juce::String ("text/javascript"));

            expect (! bridge::loadWebAssetFromZip (data, n, "/missing.css").has_value());
            expect (! bridge::loadWebAssetFromZip (data, n, "/../index.html").has_value());
            expect (! bridge::loadWebAssetFromZip (data, n, "/assets/../../index.html").has_value());

            const auto resolved = bridge::resolveEditorAsset (emptyRoot, "/");
#if JUCE_WINDOWS
            expect (resolved.has_value(), "RCDATA 41001 must serve index without a web folder");
            const auto embeddedHtml = juce::String::fromUTF8 (
                reinterpret_cast<const char*> (resolved->data.data()), (int) resolved->data.size());
            expect (embeddedHtml.contains ("id=\"root\""));
            expect (! embeddedHtml.contains ("web shell"));
#else
            juce::ignoreUnused (resolved);
#endif
        }

        beginTest ("mac bundle web zip lives under Contents/Resources");
        {
            const auto zip = bridge::expectedEmbeddedWebZip();
#if JUCE_MAC
            expectEquals (zip.getFileName(), juce::String ("neurokore_web_dist.zip"));
            expect (zip.getFullPathName().contains ("Resources")
                    || zip.getParentDirectory() == juce::File::getSpecialLocation (
                           juce::File::currentExecutableFile).getParentDirectory(),
                    zip.getFullPathName());
#else
            expect (zip == juce::File());
#endif
        }

        beginTest ("expr JIT is Windows x64 only; Mac AU/VST3 uses tape");
        {
#if JUCE_WINDOWS && JUCE_64BIT
            expectEquals ((int) NK_HAS_EXPR_JIT, 1);
#else
            expectEquals ((int) NK_HAS_EXPR_JIT, 0);
#endif
        }

        beginTest ("fallback index mentions UI_READY");
        {
            const auto html = bridge::fallbackIndexHtml();
            expect (html.contains ("UI_READY"));
            expect (html.contains ("scriptLength") || html.contains ("NEUROKORE"));
        }

        beginTest ("WebBridge emits hello only after UI_READY");
        {
            bridge::WebBridge bridge (17);
            expect (! bridge.allowOutbound());
            expect (bridge.tryEmitHello() == juce::var());

            expect (bridge.handleNative ("UI_READY", {}));
            expect (bridge.allowOutbound());
            const auto hello = bridge.tryEmitHello();
            expect (hello.isObject());
            expectEquals ((int) hello.getProperty ("scriptLength", -1), 17);
        }

        beginTest ("Space is host transport, never a plugin command");
        {
            expect (bridge::isHostTransportKey (juce::KeyPress (juce::KeyPress::spaceKey)));
            expect (! bridge::isHostTransportKey (juce::KeyPress (juce::KeyPress::spaceKey,
                                                                 juce::ModifierKeys::ctrlModifier, 0)));
            expect (! bridge::isHostTransportKey (juce::KeyPress ('a')));
            expect (bridge::isHostTransportName ("Space"));
            expect (bridge::isHostTransportName (" "));
            expect (! bridge::isHostTransportName ("Enter"));

            expect (bridge::hostKeyNameToVk ("ArrowDown") != 0, "ArrowDown must map");
            expect (bridge::hostKeyNameToVk ("ArrowUp", "ArrowUp") != 0);
            expect (bridge::hostKeyNameToVk ("Enter") != 0, "Enter must map");
            expect (bridge::hostKeyNameToVk ("a") != 0, "letter must map");
            expect (bridge::hostKeyNameToVk ("A", "KeyA") != 0);
            expect (bridge::hostKeyNameToVk ("0", "Numpad0") != 0);
            expect (bridge::hostKeyNameToVk ("0", "Digit0") != 0);
            expect (bridge::canForwardHostKey ("ArrowDown"));
            expect (bridge::canForwardHostKey ("Enter"));
            expect (bridge::canForwardHostKey ("a"));
            expect (! bridge::canForwardHostKey ({}));
            expectEquals (bridge::hostKeyNameToVk ("ArrowDown"), 0x28);
            expectEquals (bridge::hostKeyNameToVk ("Enter"), 0x0D);
            expectEquals (bridge::hostKeyNameToVk ("a"), 0x41);
            expectEquals (bridge::hostKeyNameToVk ("0", "Numpad0"), 0x60);
            expectEquals (bridge::hostKeyNameToVk ("0", "Digit0"), 0x30);
            expectEquals (bridge::hostKeyNameToVk ("/", "Slash"), 0xBF);
            expect (bridge::canForwardHostKey ("/", "Slash"));
            expectEquals (bridge::hostKeyNameToCgKeyCode ("ArrowDown"), 0x7D);
            expectEquals (bridge::hostKeyNameToCgKeyCode (" "), 0x31);
            expectEquals (bridge::hostKeyNameToCgKeyCode ("/", "Slash"), 0x2C);

            void* plugin = reinterpret_cast<void*> ((juce::pointer_sized_int) 0x100);
            void* host = reinterpret_cast<void*> ((juce::pointer_sized_int) 0x200);
            expect (bridge::chooseHostHwnd (plugin, host, plugin, host) == host);
            expect (bridge::chooseHostHwnd (plugin, host, host, nullptr) == host);
            expect (bridge::chooseHostHwnd (plugin, plugin, plugin, nullptr) == nullptr);
            expect (bridge::chooseHostHwnd (nullptr, host, host, host) == nullptr);
        }

        beginTest ("unknown native name is rejected");
        {
            bridge::WebBridge bridge (0);
            expect (! bridge.handleNative ("notAFunction", {}));
            expect (! bridge.allowOutbound());
        }

        beginTest ("NEUROKORE_WEB_DISK=0 skips the sibling web folder");
        {
            const auto prev = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_DISK", {});
            setEnv ("NEUROKORE_WEB_DISK", "0");
            expect (! bridge::wantDiskWebAssets(), "0 is tester mode: embed only");
            setEnv ("NEUROKORE_WEB_DISK", "embed");
            expect (! bridge::wantDiskWebAssets());
            setEnv ("NEUROKORE_WEB_DISK", "1");
            expect (bridge::wantDiskWebAssets());
            setEnv ("NEUROKORE_WEB_DISK", "");
            expect (bridge::wantDiskWebAssets(), "unset keeps disk for local builds");
            if (prev.isNotEmpty())
                setEnv ("NEUROKORE_WEB_DISK", prev.toRawUTF8());
            else
                setEnv ("NEUROKORE_WEB_DISK", "");
        }

        beginTest ("wantWebEditor is always web — native chrome is retired");
        {
            const auto prev = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_EDITOR", {});
            setEnv ("NEUROKORE_WEB_EDITOR", "0");
            expect (bridge::wantWebEditor(), "env 0 no longer opens native");
            setEnv ("NEUROKORE_WEB_EDITOR", "1");
            expect (bridge::wantWebEditor());
            setEnv ("NEUROKORE_WEB_EDITOR", "");
            expect (bridge::wantWebEditor());
            if (prev.isNotEmpty())
                setEnv ("NEUROKORE_WEB_EDITOR", prev.toRawUTF8());
            else
                setEnv ("NEUROKORE_WEB_EDITOR", "");
        }

        beginTest ("shouldOpenWebEditor never selects a dead webview");
        {
            expect (bridge::wantWebEditor());
            expect (bridge::shouldOpenWebEditor() == bridge::webEditorCanRun(),
                    "web must not open a WebView2 install screen");
        }
    }

private:
    static void setEnv (const char* key, const char* value)
    {
#ifdef _WIN32
        _putenv_s (key, value);
#else
        setenv (key, value, 1);
#endif
    }
};
