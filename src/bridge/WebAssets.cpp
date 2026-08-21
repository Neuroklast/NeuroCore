#include "WebAssets.h"

#include <memory>
#include <utility>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace bridge
{
namespace
{

juce::String normalisedPath (const juce::String& url)
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false)
                   .upToFirstOccurrenceOf ("#", false, false);
    if (path.startsWithChar ('/'))
        path = path.substring (1);
    if (path.isEmpty() || path == ".")
        return "index.html";
    return path;
}

bool isSafeRelative (const juce::String& rel)
{
    if (rel.contains ("..") || rel.containsChar (':') || rel.startsWithChar ('/')
        || rel.startsWithChar ('\\'))
        return false;
    return true;
}

} // namespace

juce::String mimeForPath (const juce::String& path)
{
    const auto ext = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "js" || ext == "mjs")   return "text/javascript";
    if (ext == "css")                  return "text/css";
    if (ext == "json" || ext == "map") return "application/json";
    if (ext == "png")                  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "svg")                  return "image/svg+xml";
    if (ext == "woff2")                return "font/woff2";
    if (ext == "woff")                 return "font/woff";
    if (ext == "ttf")                  return "font/ttf";
    if (ext == "otf")                  return "font/otf";
    if (ext == "ico")                  return "image/vnd.microsoft.icon";
    return "application/octet-stream";
}

std::optional<WebAsset> loadWebAsset (const juce::File& root, const juce::String& url)
{
    if (! root.isDirectory())
        return std::nullopt;

    const auto rel = normalisedPath (url);
    if (! isSafeRelative (rel))
        return std::nullopt;

    const auto file = root.getChildFile (rel);
    if (! file.existsAsFile())
        return std::nullopt;
    if (! file.isAChildOf (root) && file != root.getChildFile ("index.html"))
        return std::nullopt;

    juce::MemoryBlock block;
    if (! file.loadFileAsData (block))
        return std::nullopt;

    WebAsset asset;
    asset.mimeType = mimeForPath (rel);
    const auto* p = static_cast<const std::byte*> (block.getData());
    asset.data.assign (p, p + block.getSize());
    return asset;
}

std::optional<WebAsset> loadWebAssetFromZip (const void* zipData, size_t zipSize, const juce::String& url)
{
    if (zipData == nullptr || zipSize == 0)
        return std::nullopt;

    const auto rel = normalisedPath (url);
    if (! isSafeRelative (rel))
        return std::nullopt;

    juce::MemoryInputStream stream (zipData, zipSize, false);
    juce::ZipFile zip (stream);

    const auto wanted = rel.replaceCharacter ('\\', '/');
    int index = -1;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* entry = zip.getEntry (i);
        if (entry == nullptr)
            continue;
        auto name = entry->filename.replaceCharacter ('\\', '/');
        if (name.startsWith ("./"))
            name = name.substring (2);
        if (name == wanted)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
        return std::nullopt;

    std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (index));
    if (in == nullptr)
        return std::nullopt;

    juce::MemoryBlock block;
    in->readIntoMemoryBlock (block);

    WebAsset asset;
    asset.mimeType = mimeForPath (wanted);
    const auto* p = static_cast<const std::byte*> (block.getData());
    asset.data.assign (p, p + block.getSize());
    return asset;
}

static bool lockEmbeddedWebZip (const void*& data, size_t& size)
{
    data = nullptr;
    size = 0;
#if JUCE_WINDOWS
    static const auto blob = []() -> std::pair<const void*, size_t>
    {
        auto* mod = static_cast<HMODULE> (juce::Process::getCurrentModuleInstanceHandle());
        if (mod == nullptr)
            return { nullptr, 0 };
        auto* res = FindResource (mod, MAKEINTRESOURCE (kWebDistResourceId), RT_RCDATA);
        if (res == nullptr)
            return { nullptr, 0 };
        auto* glob = LoadResource (mod, res);
        if (glob == nullptr)
            return { nullptr, 0 };
        return { LockResource (glob), (size_t) SizeofResource (mod, res) };
    }();
    data = blob.first;
    size = blob.second;
    return data != nullptr && size > 0;
#else
    juce::ignoreUnused (data, size);
    return false;
#endif
}

std::optional<WebAsset> loadEmbeddedWebAsset (const juce::String& url)
{
    const void* data = nullptr;
    size_t size = 0;
    if (! lockEmbeddedWebZip (data, size))
        return std::nullopt;
    return loadWebAssetFromZip (data, size, url);
}

bool wantDiskWebAssets()
{
    const auto v = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_DISK", {});
    if (v == "0" || v.equalsIgnoreCase ("off") || v.equalsIgnoreCase ("false")
        || v.equalsIgnoreCase ("embed"))
        return false;
    return true;
}

std::optional<WebAsset> resolveEditorAsset (const juce::File& diskRoot, const juce::String& url)
{
    if (wantDiskWebAssets())
        if (auto fromDisk = loadWebAsset (diskRoot, url))
            return fromDisk;
    return loadEmbeddedWebAsset (url);
}

juce::String fallbackIndexHtml()
{
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>NEUROKORE</title>
  <style>
    html, body { margin: 0; height: 100%; background: #14080a; color: #f2d6d8;
                 font: 14px/1.4 ui-monospace, Consolas, monospace; }
    main { padding: 24px; }
    button { background: #7a1220; color: #f2d6d8; border: 1px solid #c43b4a;
             padding: 10px 16px; min-height: 26px; cursor: pointer; }
    #len { margin-top: 16px; color: #e8a0a6; }
  </style>
</head>
<body>
  <main>
    <h1>NEUROKORE web shell</h1>
    <button id="ready">UI_READY</button>
    <div id="len">scriptLength: —</div>
  </main>
  <script>
    const readyBtn = document.getElementById('ready');
    const lenEl = document.getElementById('len');
    function nativeUiReady() {
      if (window.__JUCE__ && window.__JUCE__.backend) {
        const names = (window.__JUCE__.initialisationData
                       && window.__JUCE__.initialisationData.__juce__functions) || [];
        if (names.indexOf('UI_READY') >= 0) {
          window.__JUCE__.backend.emitEvent('__juce__invoke', {
            name: 'UI_READY', params: [{ build: '0.4.11-alpha', scale: 1 }], resultId: 0
          });
        }
        window.__JUCE__.backend.addEventListener('hello', (payload) => {
          const n = payload && payload.scriptLength;
          lenEl.textContent = 'scriptLength: ' + (n == null ? '?' : n);
        });
      } else {
        lenEl.textContent = 'scriptLength: (no JUCE bridge)';
      }
    }
    readyBtn.addEventListener('click', nativeUiReady);
  </script>
</body>
</html>
)HTML";
}

WebAsset fallbackIndexAsset()
{
    const auto html = fallbackIndexHtml();
    WebAsset asset;
    asset.mimeType = "text/html";
    const auto* p = reinterpret_cast<const std::byte*> (html.toRawUTF8());
    asset.data.assign (p, p + html.getNumBytesAsUTF8());
    return asset;
}

} // namespace bridge
