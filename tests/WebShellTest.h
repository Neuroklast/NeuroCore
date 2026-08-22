#pragma once

#include <JuceHeader.h>
#include <cstring>
#include <cstdlib>
#include "../src/bridge/HostKeys.h"
#include "../src/bridge/WebAssets.h"
#include "../src/bridge/WebBridge.h"
#include "../src/bridge/WebEditorPolicy.h"
#include "../src/bridge/WebNav.h"
#include "../src/bridge/WebViewHolder.h"
#include "../src/core/PluginProcessor.h"
#include "../src/utils/ExprTapeJit.h"

#ifdef _WIN32
#include <stdlib.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <atomic>
#include <thread>
#include <vector>

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

        beginTest ("external brand URLs stay out of the WebView");
        {
            expect (bridge::isPluginPage ("http://localhost:5173/", {}, "http://localhost:5173"));
            expect (! bridge::isPluginPage ("https://neuroklast.net", "https://juce.local/", {}));
            expect (bridge::isExternalHttp ("https://neuroklast.net"));
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

        beginTest ("processor WebView holder outlives editor; zip index and UI_READY persist");
        {
            // NeuroKoreTests cannot spin a real WebView (no NEUROKORE_HAS_WEB_EDITOR).
            // Contract is the ownership API: holder identity + zip index + latch.
            NeuroKoreAudioProcessor proc;
            auto& holder = proc.getWebView();
            expectEquals ((juce::int64) holder.browserIdentity(),
                          (juce::int64) proc.getWebView().browserIdentity(),
                          "getWebView returns the same holder");
            expect (holder.browserIdentity() != 0);

            const auto first = holder.serve ("/");
            expect (first.has_value(), "resource provider serves /");
            const int builds = holder.zipIndexBuildCount();
            expect (builds >= 1);
            expect (holder.serve ("/").has_value());
            expectEquals (holder.zipIndexBuildCount(), builds,
                          "serve does not rebuild the zip index");

            juce::Component frame1;
            holder.attach (frame1);
            expect (holder.isAttached());
            expect (holder.browserComponent() != nullptr);
            expect (holder.browserComponent()->getParentComponent() == &frame1);

            expect (holder.bridge().handleNative ("UI_READY", {}));
            expect (holder.bridge().allowOutbound());

            holder.detach (frame1);
            expect (! holder.isAttached());
            expect (holder.browserComponent() != nullptr);
            expect (holder.browserComponent()->getParentComponent() != &frame1);
            expect (holder.bridge().allowOutbound(), "UI_READY latch survives editor dtor");
            expectEquals (holder.zipIndexBuildCount(), builds);

            const auto id = holder.browserIdentity();
            {
                juce::Component dying;
                holder.attach (dying);
                expectEquals ((juce::int64) holder.browserIdentity(), (juce::int64) id);
                holder.detach (dying);
            }

            juce::Component frame2;
            holder.attach (frame2);
            expectEquals ((juce::int64) holder.browserIdentity(), (juce::int64) id,
                          "second editor reuses browser identity");
            expectEquals (holder.zipIndexBuildCount(), builds,
                          "second createEditor does not rebuild the zip index");
            expect (holder.bridge().allowOutbound(), "UI_READY is not required again");
            expect (holder.serve ("/").has_value());
            expectEquals (holder.zipIndexBuildCount(), builds);
            holder.detach (frame2);
        }

        beginTest ("WebZipIndex load is serialized across threads");
        {
            bridge::WebZipIndex index;
            expect (index.buildCount() >= 1);
            std::atomic<int> hits { 0 };
            std::vector<std::thread> threads;
            threads.reserve (4);
            for (int t = 0; t < 4; ++t)
            {
                threads.emplace_back ([&index, &hits]
                {
                    for (int i = 0; i < 32; ++i)
                        if (index.load ("/").has_value())
                            hits.fetch_add (1, std::memory_order_relaxed);
                });
            }
            for (auto& th : threads)
                th.join();
            expectEquals (index.buildCount(), 1, "index is not rebuilt under concurrent load");
            if (index.load ("/").has_value())
                expectEquals (hits.load(), 128);
        }

        beginTest ("park HWND is never a child of the IPlugView HWND");
        {
#if JUCE_WINDOWS
            auto* inst = GetModuleHandleW (nullptr);
            HWND owner = CreateWindowExW (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC",
                                          L"nk-park-owner", WS_POPUP, -32000, -32000, 8, 8,
                                          nullptr, nullptr, inst, nullptr);
            HWND host = CreateWindowExW (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC",
                                         L"nk-host", WS_POPUP, -32000, -32000, 400, 300,
                                         nullptr, nullptr, inst, nullptr);
            HWND editor = CreateWindowExW (0, L"STATIC", L"nk-iplugview",
                                           WS_CHILD | WS_VISIBLE, 0, 0, 400, 300,
                                           host, nullptr, inst, nullptr);
            HWND park = CreateWindowExW (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC",
                                         L"nk-park", WS_POPUP, -32000, -32000, 80, 60,
                                         nullptr, nullptr, inst, nullptr);
            expect (owner != nullptr && host != nullptr && editor != nullptr && park != nullptr);

            auto* chosen = static_cast<HWND> (
                bridge::WebViewHolder::parkParentForEditor (editor, owner));
            expect (chosen == host, "VST3 park parent is the host systemWindow");
            expect (chosen != editor, "park parent is never IPlugView");
            expect (bridge::WebViewHolder::parkParentForEditor (editor, owner) != editor);

            auto style = GetWindowLongPtr (park, GWL_STYLE);
            using FlagType = decltype (style);
            style &= ~(FlagType) WS_POPUP;
            style |= (FlagType) WS_CHILD;
            SetWindowLongPtr (park, GWL_STYLE, style);
            SetParent (park, chosen);
            expect (GetParent (park) == host);
            expect (GetParent (park) != editor);
            expect (! IsChild (editor, park), "park is a sibling of IPlugView, not a child");

            const auto id = (juce::int64) (juce::pointer_sized_int) park;
            DestroyWindow (editor);
            expect (IsWindow (park) != 0, "park HWND survives DestroyWindow(IPlugView)");
            expectEquals ((juce::int64) (juce::pointer_sized_int) park, id);

            style = GetWindowLongPtr (park, GWL_STYLE);
            style &= ~(FlagType) WS_CHILD;
            style |= (FlagType) WS_POPUP;
            SetWindowLongPtr (park, GWL_STYLE, style);
            SetParent (park, owner);
            DestroyWindow (host);
            expect (IsWindow (park) != 0, "park HWND survives host destroy after park-to-owner");

            DestroyWindow (park);
            DestroyWindow (owner);
#else
            expect (true);
#endif
        }

        beginTest ("standalone top-level editor HWND is the park parent");
        {
#if JUCE_WINDOWS
            auto* inst = GetModuleHandleW (nullptr);
            HWND owner = CreateWindowExW (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC",
                                          L"nk-park-owner-sa", WS_POPUP, -32000, -32000, 8, 8,
                                          nullptr, nullptr, inst, nullptr);
            HWND standalone = CreateWindowExW (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC",
                                               L"nk-standalone", WS_POPUP, -32000, -32000, 400, 300,
                                               nullptr, nullptr, inst, nullptr);
            HWND park = CreateWindowExW (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC",
                                         L"nk-park-sa", WS_POPUP, -32000, -32000, 80, 60,
                                         nullptr, nullptr, inst, nullptr);
            expect (owner != nullptr && standalone != nullptr && park != nullptr);
            expect (GetParent (standalone) == nullptr, "Standalone peer is a top-level window");

            auto* chosen = static_cast<HWND> (
                bridge::WebViewHolder::parkParentForEditor (standalone, owner));
            expect (chosen == standalone,
                    "Standalone park parent is the app window, not the hidden owner");
            expect (chosen != owner, "hidden owner is only for VST3 close / detach");

            auto style = GetWindowLongPtr (park, GWL_STYLE);
            using FlagType = decltype (style);
            style &= ~(FlagType) WS_POPUP;
            style |= (FlagType) WS_CHILD;
            SetWindowLongPtr (park, GWL_STYLE, style);
            SetParent (park, chosen);
            expect (GetParent (park) == standalone);
            expect (IsChild (standalone, park) != 0);

            style = GetWindowLongPtr (park, GWL_STYLE);
            style &= ~(FlagType) WS_CHILD;
            style |= (FlagType) WS_POPUP;
            SetWindowLongPtr (park, GWL_STYLE, style);
            SetParent (park, owner);
            DestroyWindow (standalone);
            expect (IsWindow (park) != 0, "park HWND survives standalone close after park-to-owner");

            DestroyWindow (park);
            DestroyWindow (owner);
#else
            expect (true);
#endif
        }

        beginTest ("each plugin instance gets its own WebView2 user data folder");
        {
#if JUCE_WINDOWS
            const auto a = bridge::webView2UserDataFolder (0x111);
            const auto b = bridge::webView2UserDataFolder (0x222);
            expect (a != b, "two instances must not share a WebView2 profile");
            expect (a.getParentDirectory() == b.getParentDirectory());
            expect (bridge::webView2UserDataFolder (0).getFileName() == "probe");
            expect (a.getFileName() != "probe");
#else
            expect (true);
#endif
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
