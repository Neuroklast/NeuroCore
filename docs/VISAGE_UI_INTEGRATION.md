# VisageUI Integration Guide

Wie man **visageui** trotz JUCE als Haupt-Framework in NeuroCore integrieren kann.
Dieser Guide beschreibt drei Ansätze von einfach bis komplex.

---

## Hintergrund: Das Problem

JUCE verwendet sein eigenes GUI-System (JUCE-Components, `juce::Graphics`).
visageui ist ein modernes Web-basiertes UI-Framework.
Beide Systeme können **nicht direkt kombiniert** werden – aber es gibt elegante Brücken-Ansätze.

---

## Option A: Embedded WebView (Empfohlen für schrittweise Migration)

### Architektur

```
JUCE PluginEditor
  └─► juce::WebBrowserComponent (als Haupt-Child)
        └─► React/Vue/Svelte App
              └─► visageui Komponenten
                    ↕ JavaScript-Bridge
              └─► Parameter-Sync
                    ↕ ValueTree / Custom Messages
              JUCE AudioProcessorValueTreeState
```

### Vorteile
- Volle CSS/HTML/JS-Freiheit für die UI
- visageui direkt als npm-Package nutzbar
- Hot-Reloading der UI während der Entwicklung (ohne Plugin-Neustart)
- Syntax-Highlighting im DSL-Editor über Monaco ohne eigene Implementierung
- Community-Support für Web-UI-Frameworks ist riesig

### Nachteile
- Höhere Latenz zwischen Audio-Parametern und UI-Updates
- WebView-Rendering kann auf manchen Systemen instabil sein
- Größerer Bundle (Chromium/WebKit embedded)
- Debugging ist komplexer (zwei Debugging-Umgebungen)

### Implementierung

**Schritt 1: WebBrowserComponent einbinden**
```cpp
// PluginEditor.h
#include <juce_gui_extra/juce_gui_extra.h>

class PluginEditor : public juce::AudioProcessorEditor {
    juce::WebBrowserComponent webView;
    // ...
};

// PluginEditor.cpp
PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(p)
{
    addAndMakeVisible(webView);
    webView.goToURL("file:///path/to/ui/index.html");
    // Oder: webView.goToURL(WebBrowserComponent::getResourceProviderURL());
}
```

**Schritt 2: JavaScript-Bridge für Parameter**
```cpp
// Parameter-Änderungen aus JUCE → WebView senden
void PluginEditor::parameterChanged(const juce::String& paramID, float value) {
    juce::String js = "window.onParameterChange('" + paramID + "', " 
                    + juce::String(value) + ");";
    webView.evaluateJavascript(js);
}

// Callbacks aus WebView → JUCE empfangen
bool PluginEditor::pageAboutToLoad(const juce::String& url) {
    // URL-Schema als Kommunikationskanal nutzen:
    // neurocore://setParam?id=paramA&value=0.75
    if (url.startsWith("neurocore://setParam")) {
        auto paramID = /* parse paramID */;
        auto value = /* parse value */;
        processor.apvts.getParameter(paramID)->setValueNotifyingHost(value);
        return false; // Navigation verhindern
    }
    return true;
}
```

**Schritt 3: visageui in der Web-App**
```javascript
// ui/src/App.jsx
import { Knob, Button, Slider } from 'visageui';

function App() {
    const [params, setParams] = useState({ paramA: 0.5 });
    
    // JUCE → React
    window.onParameterChange = (id, value) => {
        setParams(prev => ({ ...prev, [id]: value }));
    };
    
    // React → JUCE
    const setParam = (id, value) => {
        window.location = `neurocore://setParam?id=${id}&value=${value}`;
    };
    
    return (
        <div className="plugin-ui">
            <Knob value={params.paramA} onChange={v => setParam('paramA', v)} />
        </div>
    );
}
```

---

## Option B: Hybrid-Ansatz (Empfohlen für erste Integration)

Behalte performance-kritische JUCE-Komponenten nativ, ersetze den Rest schrittweise durch WebView.

### Was bleibt JUCE-nativ (Performance-kritisch)
- `WaveformDisplayComponent` – Echtzeit-Wellenform-Zeichnen (60fps, kein WebView-Overhead)
- `LoudnessMeterComponent` – Echtzeit-Meter
- `FormulaDisplayComponent` – Vorschau-Kurve
- Alle APVTS-Knobs (direkte Parameter-Bindung ohne Bridge-Overhead)

### Was kommt in den WebView
- `DslTerminalEditor` → **Monaco-Editor** mit visageui-Styling und DSL-Syntax-Highlighting
- `PresetContentComponent` → Web-basierter Preset-Browser mit Suche, Kategorien, Rating
- `FunctionsContentComponent` → Interaktive DSL-Referenz mit Copy-to-Clipboard
- `ValidationContentComponent` → Rich-Text-Validierungsergebnisse

### Implementierung (Schrittweise)

**Phase 1: DSL-Editor migrieren**
```cpp
// DslTerminalEditor durch WebView ersetzen
class DslTerminalEditor : public juce::Component {
    juce::WebBrowserComponent monacoWebView;
    
    DslTerminalEditor() {
        addAndMakeVisible(monacoWebView);
        monacoWebView.goToURL("file:///ui/monaco-editor/index.html");
    }
};
```

```html
<!-- ui/monaco-editor/index.html -->
<!DOCTYPE html>
<html>
<head>
    <script src="monaco-editor/min/vs/loader.js"></script>
</head>
<body>
    <div id="editor" style="height:100%;width:100%"></div>
    <script>
        require.config({ paths: { vs: 'monaco-editor/min/vs' } });
        require(['vs/editor/editor.main'], function() {
            // NeuroCore DSL Syntax-Definition registrieren
            monaco.languages.register({ id: 'neurocore-dsl' });
            monaco.languages.setMonarchTokensProvider('neurocore-dsl', {
                keywords: ['stage', 'filter', 'comp', 'env', 'osc', 'param'],
                tokenizer: {
                    root: [
                        [/\b(stage|filter|comp|env|osc|param)\b/, 'keyword'],
                        [/\b(sin|cos|tanh|abs|clamp|lerp)\b/, 'builtin'],
                        [/#.*$/, 'comment'],
                        [/[0-9]+\.?[0-9]*/, 'number'],
                    ]
                }
            });
            
            const editor = monaco.editor.create(document.getElementById('editor'), {
                language: 'neurocore-dsl',
                theme: 'vs-dark',
                minimap: { enabled: false }
            });
            
            // Script-Änderungen an JUCE melden
            editor.onDidChangeModelContent(() => {
                window.location = 'neurocore://setScript?value=' 
                    + encodeURIComponent(editor.getValue());
            });
        });
    </script>
</body>
</html>
```

**Phase 2: Preset-Browser migrieren**
```javascript
// ui/src/PresetBrowser.jsx
import { List, SearchInput, Tag } from 'visageui';

function PresetBrowser({ presets, onSelect }) {
    const [search, setSearch] = useState('');
    const filtered = presets.filter(p => 
        p.name.toLowerCase().includes(search.toLowerCase())
    );
    
    return (
        <div>
            <SearchInput value={search} onChange={setSearch} />
            <List items={filtered} onSelect={onSelect} />
        </div>
    );
}
```

---

## Option C: Full WebView (Maximale Flexibilität)

Gesamte UI als Web-App, JUCE nur als Audio-Backend.

### Architektur
```
JUCE PluginEditor (minimaler Host)
  └─► juce::WebBrowserComponent (füllt gesamtes Fenster)
        └─► React/Vue App mit visageui
              ↕ Bidirektionale Kommunikation
        JUCE AudioProcessor (Audio-Backend)
```

### Vorteile
- Maximale Design-Freiheit
- Komplette visageui-Integration ohne Kompromisse
- Modernes Entwicklungssetup (Vite, TypeScript, Hot-Reload)

### Nachteile
- Komplexeste Implementierung
- Höchstes Risiko für Performance-Probleme
- Vollständige IPC-Schicht notwendig

### Kommunikations-Protokoll
```typescript
// Alle Nachrichten via URL-Schema
interface NeuroCoreMessage {
    type: 'setParam' | 'getParam' | 'setScript' | 'loadPreset' | 'savePreset';
    payload: Record<string, unknown>;
}

// Plugin → WebView: WebBrowserComponent::evaluateJavascript()
// WebView → Plugin: window.location = 'neurocore://...'
```

---

## Empfehlung für NeuroCore

**Sofort umsetzbar (Phase 1 in Roadmap):** Option B  

1. Monaco-Editor für den DSL-Terminal (Syntax-Highlighting ist in HTML trivial)
2. Waveforms und Knobs bleiben JUCE-nativ
3. Preset-Browser und Panels können schrittweise zu WebView migriert werden

**Langfristig (Phase 3):** Evaluierung von Option A oder C abhängig von Community-Feedback

---

## Ressourcen

- [juce::WebBrowserComponent Dokumentation](https://docs.juce.com/master/classWebBrowserComponent.html)
- [Monaco Editor npm Package](https://www.npmjs.com/package/monaco-editor)
- [visageui GitHub](https://github.com/visageui/visageui)
- [JUCE + WebView Tutorial](https://forum.juce.com/t/webbrowsercomponent-examples)
