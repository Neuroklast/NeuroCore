#pragma once

#include <JuceHeader.h>
#include "../src/bridge/WebAssets.h"
#include "../src/bridge/WebBridge.h"
#include "../src/bridge/WebEditorPolicy.h"

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

        beginTest ("unknown native name is rejected");
        {
            bridge::WebBridge bridge (0);
            expect (! bridge.handleNative ("notAFunction", {}));
            expect (! bridge.allowOutbound());
        }

        beginTest ("wantWebEditor: env overrides compile default");
        {
            const auto prev = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_EDITOR", {});
            setEnv ("NEUROKORE_WEB_EDITOR", "0");
            expect (! bridge::wantWebEditor());
            setEnv ("NEUROKORE_WEB_EDITOR", "1");
            expect (bridge::wantWebEditor());
#if NEUROKORE_NATIVE_EDITOR
            setEnv ("NEUROKORE_WEB_EDITOR", "");
            expect (! bridge::wantWebEditor(), "tests compile with native default");
#else
            setEnv ("NEUROKORE_WEB_EDITOR", "");
            expect (bridge::wantWebEditor(), "plugin compile default is web");
#endif
            if (prev.isNotEmpty())
                setEnv ("NEUROKORE_WEB_EDITOR", prev.toRawUTF8());
            else
                setEnv ("NEUROKORE_WEB_EDITOR", "");
        }

        beginTest ("shouldOpenWebEditor never selects a dead webview");
        {
            const auto prev = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_EDITOR", {});
            setEnv ("NEUROKORE_WEB_EDITOR", "1");
            expect (bridge::wantWebEditor());
            expect (bridge::shouldOpenWebEditor() == bridge::webEditorCanRun(),
                    "env asking for web must not open a WebView2 install screen");
            setEnv ("NEUROKORE_WEB_EDITOR", "0");
            expect (! bridge::shouldOpenWebEditor());
            if (prev.isNotEmpty())
                setEnv ("NEUROKORE_WEB_EDITOR", prev.toRawUTF8());
            else
                setEnv ("NEUROKORE_WEB_EDITOR", "");
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
