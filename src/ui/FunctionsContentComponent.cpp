#include "FunctionsContentComponent.h"
#include "../third_party/nlohmann/json.hpp"
#include "../utils/ExpressionEvaluator.h"
#include "../core/Config.h"
#include "PluginLookAndFeel.h"
#include <BinaryData.h>
#include <cmath>

using json = nlohmann::json;

//==============================================================================
FunctionPlotComponent::FunctionPlotComponent()
{
    setOpaque (false);
    startTimerHz (30);
}

FunctionPlotComponent::~FunctionPlotComponent()
{
    stopTimer();
}

juce::String FunctionPlotComponent::resolvePlotExpression (const juce::String& functionName,
                                                           const juce::String& example)
{
    const auto name = functionName.trim().toLowerCase();

    // Known demos: always pure f(x) with numeric args (no free knobs a/b).
    // This avoids "parse failed" when docs examples use a,b,y,t or multi-line DSL.
    struct Demo { const char* key; const char* expr; };
    static constexpr Demo demos[] = {
        { "sin",        "sin(x * 3.14159)" },
        { "cos",        "cos(x * 3.14159)" },
        { "tan",        "tan(x * 1.2)" },
        { "tanh",       "tanh(x * 2.5)" },
        { "sqrt",       "sqrt(abs(x))" },
        { "abs",        "abs(x)" },
        { "sign",       "sign(x)" },
        { "exp",        "exp(x) - 1" },
        { "log",        "log(abs(x) + 0.05)" },
        { "log10",      "log10(abs(x) + 0.05)" },
        { "log2",       "log2(abs(x) + 0.05)" },
        { "floor",      "floor(x * 4) / 4" },
        { "ceil",       "ceil(x * 4) / 4" },
        { "round",      "round(x * 4) / 4" },
        { "pow",        "pow(abs(x), 2.0) * sign(x)" },
        { "min",        "min(x, 0.4)" },
        { "max",        "max(x, -0.4)" },
        { "fmod",       "fmod(x * 3, 0.6)" },
        { "mod",        "mod(x * 3, 0.6)" },
        { "clamp",      "clamp(x * 1.8, -0.6, 0.6)" },
        { "atan",       "atan(x * 4) / 1.5708" },
        { "asinh",      "asinh(x * 3) / asinh(3)" },
        { "lerp",       "lerp(x, softclip(x, 3), 0.55)" },
        { "map",        "map(x, -1, 1, -0.5, 0.5)" },
        { "step",       "x * step(0.25, abs(x))" },
        { "smoothstep", "smoothstep(-0.6, 0.6, x) * 2 - 1" },
        { "noise",      "x + noise(x * 25) * 0.2" },
        { "softclip",   "softclip(x, 2.5)" },
        { "hardclip",   "hardclip(softclip(x, 1.5), 0.55)" },
        { "tube",       "tube(x, 2.2)" },
        { "diode",      "diode(x, 2.0)" },
        { "fold",       "fold(x * 2.5, -0.7, 0.7)" },
        { "wrap",       "wrap(x * 2.2, -1, 1)" },
        { "bitcrush",   "bitcrush(softclip(x, 1.2), 5)" },
        { "quantize",   "quantize(x, 8)" },
    };

    for (const auto& d : demos)
        if (name == d.key || name.startsWith (juce::String (d.key) + " "))
            return d.expr;

    // Design-pattern entries in the docs
    if (name.contains ("lerp") && name.contains ("blend"))
        return "lerp(x, tube(x, 2), 0.5)";
    if (name.contains ("clip") && name.contains ("lpf"))
        return "hardclip(softclip(x, 2), 0.6)";
    if (name.contains ("ms ") || name.contains ("encode"))
        return "softclip(x, 1.8)";
    if (name.startsWith ("param"))
        return "x * 0.65"; // simple gain knob demo

    // Fallback: first line of example after '=', replace free knobs with numbers
    juce::String ex = example.trim();
    const int nl = ex.indexOfChar ('\n');
    if (nl > 0)
        ex = ex.substring (0, nl).trim();
    // Drop DSL block prefixes
    if (ex.containsChar (':') && ! ex.startsWithIgnoreCase ("y"))
        return "softclip(x, 1.5)"; // non-expression line

    int eq = ex.indexOfChar ('=');
    if (eq >= 0)
        ex = ex.substring (eq + 1).trim();

    // Substitute common free identifiers so the parser gets a closed expression
    auto replaceIdent = [] (juce::String s, const juce::String& id, const juce::String& with) -> juce::String
    {
        juce::String out;
        int i = 0;
        const auto lower = s.toLowerCase();
        const auto key = id.toLowerCase();
        while (i < s.length())
        {
            auto pos = lower.indexOf (i, key);
            if (pos < 0)
            {
                out << s.substring (i);
                break;
            }
            const auto before = pos > 0 ? s[pos - 1] : (juce_wchar) 0;
            const auto after  = pos + key.length() < s.length()
                                  ? s[pos + (int) key.length()] : (juce_wchar) 0;
            const auto isId = [] (juce_wchar c)
            {
                return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
            };
            out << s.substring (i, pos);
            if (! isId (before) && ! isId (after))
                out << with;
            else
                out << s.substring (pos, pos + (int) key.length());
            i = pos + (int) key.length();
        }
        return out;
    };

    ex = replaceIdent (ex, "a", "2.0");
    ex = replaceIdent (ex, "b", "0.5");
    ex = replaceIdent (ex, "c", "0.7");
    ex = replaceIdent (ex, "d", "0.3");
    ex = replaceIdent (ex, "e", "0.6");
    ex = replaceIdent (ex, "f", "1.2");
    ex = replaceIdent (ex, "y", "x"); // treat y as input sample for plot
    ex = replaceIdent (ex, "t", "x");
    ex = replaceIdent (ex, "sr", "44100");

    if (ex.isEmpty())
        return "x";
    return ex;
}

void FunctionPlotComponent::setFunctionDemo (const juce::String& functionName,
                                             const juce::String& example)
{
    displayName = functionName;
    formula = resolvePlotExpression (functionName, example);
    formulaOk = false;
    rebuildTrace();
    repaint();
}

void FunctionPlotComponent::rebuildTrace()
{
    constexpr int num = 160;
    inSamples.assign ((size_t) num, 0.f);
    outSamples.assign ((size_t) num, 0.f);
    formulaOk = false;

    if (formula.isEmpty())
        return;

    ExpressionEvaluator eval;
    // Try primary expression, then a few safe fallbacks
    auto tryParse = [&] (const juce::String& expr) -> bool
    {
        return eval.parseFormula (expr.toStdString());
    };

    if (! tryParse (formula))
    {
        // Last resort: pass-through or softclip so the panel never stays broken
        if (! tryParse ("softclip(x, 2)"))
            if (! tryParse ("x"))
                return;
        formula = "softclip(x, 2)";
    }

    formulaOk = true;
    const float twoPi = juce::MathConstants<float>::twoPi;
    for (int i = 0; i < num; ++i)
    {
        const float t = phase + twoPi * ((float) i / (float) (num - 1));
        const float xin = std::sin (t);
        inSamples[(size_t) i] = xin;

        // Bind every common free var so leftover identifiers still evaluate
        eval.setVariable ("x", xin);
        eval.setVariable ("a", 2.0f);
        eval.setVariable ("b", 0.5f);
        eval.setVariable ("c", 0.7f);
        eval.setVariable ("d", 0.3f);
        eval.setVariable ("e", 0.6f);
        eval.setVariable ("f", 1.2f);
        eval.setVariable ("y", xin);
        eval.setVariable ("t", t);
        eval.setVariable ("sr", 44100.f);

        float y = eval.evaluate (xin);
        if (! std::isfinite (y))
            y = 0.f;
        outSamples[(size_t) i] = juce::jlimit (-2.f, 2.f, y);
    }
}

void FunctionPlotComponent::timerCallback()
{
    phase += 0.12f;
    if (phase > juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
    rebuildTrace();
    repaint();
}

void FunctionPlotComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (NeuroCoreLookAndFeel::background());
    g.fillRoundedRectangle (area, 8.f);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.45f));
    g.drawRoundedRectangle (area, 8.f, 1.2f);

    auto body = area.reduced (6.f, 4.f);
    auto top = body.removeFromTop (body.getHeight() * 0.48f);
    body.removeFromTop (4.f);
    auto bot = body;

    auto drawWave = [&] (juce::Rectangle<float> r, const std::vector<float>& samples,
                         juce::Colour col, const juce::String& tag)
    {
        g.setColour (NeuroCoreLookAndFeel::surfaceHigh().withAlpha (0.55f));
        g.fillRoundedRectangle (r, 6.f);

        const float midY = r.getCentreY();
        g.setColour (NeuroCoreLookAndFeel::mutedText().withAlpha (0.3f));
        g.drawLine (r.getX() + 2.f, midY, r.getRight() - 2.f, midY, 1.f);

        g.setColour (col.withAlpha (0.95f));
        g.setFont (NeuroCoreLookAndFeel::monoFont (13.f));
        g.drawText (tag, r.reduced (8.f, 4.f), juce::Justification::topLeft, false);

        if (samples.size() < 2)
            return;

        juce::Path p;
        const float h = r.getHeight() * 0.38f;
        const float x0 = r.getX() + 4.f;
        const float w = r.getWidth() - 8.f;
        p.startNewSubPath (x0, midY - samples[0] * h);
        for (size_t i = 1; i < samples.size(); ++i)
        {
            const float x = x0 + ((float) i / (float) (samples.size() - 1)) * w;
            p.lineTo (x, midY - samples[i] * h);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };

    // IN: pure sine  |  OUT: sine after function
    drawWave (top, inSamples, juce::Colour (0xff7aa2ff), "IN  sine");
    drawWave (bot, outSamples,
              formulaOk ? NeuroCoreLookAndFeel::accent() : juce::Colour (0xff666666),
              formulaOk ? ("OUT  " + (displayName.isNotEmpty() ? displayName : juce::String ("f(x)")))
                        : "OUT  (no demo)");
}

//==============================================================================
FunctionsContentComponent::FunctionsContentComponent (NeuroCoreAudioProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (true);
    setOpaque (false);

    addAndMakeVisible (searchField);
    addAndMakeVisible (listBox);
    addAndMakeVisible (insertButton);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (descLabel);
    addAndMakeVisible (soundLabel);
    addAndMakeVisible (useLabel);
    addAndMakeVisible (exampleLabel);
    addAndMakeVisible (extraLabel);
    addAndMakeVisible (plotCaption);
    addAndMakeVisible (plot);

    searchField.setTextToShowWhenEmpty ("Search functions...", NeuroCoreLookAndFeel::mutedText());
    searchField.setFont (NeuroCoreLookAndFeel::monoFont (16.f));
    searchField.setColour (juce::TextEditor::backgroundColourId, NeuroCoreLookAndFeel::surfaceHigh());
    searchField.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe8ecf4));
    searchField.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff2e3545));
    searchField.onTextChange = [this] { filterList(); };

    listBox.setRowHeight (36);
    listBox.setColour (juce::ListBox::backgroundColourId, NeuroCoreLookAndFeel::surface());
    listBox.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

    insertButton.setButtonText ("Insert");
    closeButton.setButtonText ("Close");
    insertButton.onClick = [this]
    {
        if (currentIndex >= 0 && onInsert)
            onInsert (allFunctions[(size_t) filtered[(size_t) currentIndex]].example);
        if (onClose)
            onClose();
    };
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };

    // Readable sizes (Apex brand title + mono body — never tiny 11–12 pt)
    nameLabel.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    nameLabel.setFont (NeuroCoreLookAndFeel::brandFont (22.f, true));

    descLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8ecf4));
    descLabel.setFont (NeuroCoreLookAndFeel::monoFont (16.f));

    soundLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffb08a));
    soundLabel.setFont (NeuroCoreLookAndFeel::monoFont (15.f));

    useLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa8d4a8));
    useLabel.setFont (NeuroCoreLookAndFeel::monoFont (15.f));

    exampleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc8d0e4));
    exampleLabel.setFont (NeuroCoreLookAndFeel::monoFont (16.f));

    extraLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc0c4cc));
    extraLabel.setFont (NeuroCoreLookAndFeel::monoFont (14.f));

    plotCaption.setColour (juce::Label::textColourId, juce::Colour (0xffd0d4dc));
    plotCaption.setFont (NeuroCoreLookAndFeel::monoFont (14.f));
    plotCaption.setText ("Animated: sine IN (top) vs after function OUT (bottom)",
                         juce::dontSendNotification);

    for (auto* lbl : { &descLabel, &soundLabel, &useLabel, &exampleLabel, &extraLabel, &plotCaption })
    {
        lbl->setJustificationType (juce::Justification::topLeft);
        lbl->setMinimumHorizontalScale (1.0f); // do not squash text
    }

    loadFunctions();
    filterList();
}

void FunctionsContentComponent::loadFunctions()
{
    allFunctions.clear();
    const char* fileName = "functions_en.txt";

    juce::String content;
    juce::Array<juce::File> candidates;
    const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    candidates.add (app.getSiblingFile (Config::kResourceFolder).getChildFile ("locale").getChildFile (fileName));
    candidates.add (app.getParentDirectory().getChildFile (Config::kResourceFolder)
                       .getChildFile ("locale").getChildFile (fileName));
   #ifdef NEUROCORE_RESOURCES_DIR
    candidates.add (juce::File (NEUROCORE_RESOURCES_DIR).getChildFile ("locale").getChildFile (fileName));
   #endif
    auto dir = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 5; ++i)
    {
        candidates.add (dir.getChildFile ("resources/locale").getChildFile (fileName));
        dir = dir.getParentDirectory();
    }

    for (auto& f : candidates)
    {
        if (f.existsAsFile())
        {
            juce::MemoryBlock mb;
            if (f.loadFileAsData (mb) && mb.getSize() > 0)
            {
                auto* raw = static_cast<const char*> (mb.getData());
                int n = (int) mb.getSize(), off = 0;
                if (n >= 3 && (uint8_t) raw[0] == 0xEF && (uint8_t) raw[1] == 0xBB && (uint8_t) raw[2] == 0xBF)
                    off = 3;
                content = juce::String::fromUTF8 (raw + off, n - off);
            }
            break;
        }
    }

    if (content.isEmpty())
        content = juce::String::fromUTF8 (BinaryData::functions_en_txt, BinaryData::functions_en_txtSize);

    auto j = json::parse (content.toStdString(), nullptr, false);
    if (! j.is_object() || ! j.contains ("functions"))
        return;

    for (auto& v : j["functions"])
    {
        FunctionInfo info;
        info.name = v.value ("name", std::string{});
        info.description = v.value ("description", std::string{});
        info.soundCharacter = v.value ("soundCharacter", std::string{});
        info.example = v.value ("example", std::string{});
        if (v.contains ("keywords"))
            for (auto& u : v["keywords"])
                info.keywords.add (u.get<std::string>());
        if (v.contains ("useCases"))
            for (auto& u : v["useCases"])
                info.useCases.add (u.get<std::string>());
        if (v.contains ("dangersAndLimits") && v["dangersAndLimits"].is_object())
        {
            auto& d = v["dangersAndLimits"];
            info.domain = d.value ("domain", std::string{});
            info.aliasing = d.value ("aliasing", std::string{});
            info.performance = d.value ("performance", std::string{});
        }
        if (info.name.isNotEmpty())
            allFunctions.push_back (std::move (info));
    }
}

void FunctionsContentComponent::filterList()
{
    juce::String q = searchField.getText().toLowerCase();
    filtered.clear();
    for (int i = 0; i < (int) allFunctions.size(); ++i)
    {
        const auto& f = allFunctions[(size_t) i];
        auto name = f.name.toLowerCase();
        bool match = q.isEmpty() || name.contains (q)
                     || f.description.toLowerCase().contains (q)
                     || f.soundCharacter.toLowerCase().contains (q);
        if (! match)
            for (auto& k : f.keywords)
                if (k.toLowerCase().contains (q))
                {
                    match = true;
                    break;
                }
        if (! match)
            for (auto& u : f.useCases)
                if (u.toLowerCase().contains (q))
                {
                    match = true;
                    break;
                }
        if (match)
            filtered.push_back (i);
    }
    listBox.updateContent();
    if (! filtered.empty())
    {
        listBox.selectRow (0);
        selectedRowsChanged (0);
    }
}

int FunctionsContentComponent::getNumRows() { return (int) filtered.size(); }

void FunctionsContentComponent::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) filtered.size()))
        return;

    if (selected)
        g.fillAll (NeuroCoreLookAndFeel::accent().withAlpha (0.22f));
    else if (row % 2)
        g.fillAll (NeuroCoreLookAndFeel::surfaceHigh().withAlpha (0.45f));

    g.setColour (juce::Colour (0xffe8ecf4));
    g.setFont (NeuroCoreLookAndFeel::monoFont (16.f));
    g.drawText (allFunctions[(size_t) filtered[(size_t) row]].name,
                12, 0, width - 16, height, juce::Justification::centredLeft, true);
}

void FunctionsContentComponent::selectedRowsChanged (int row)
{
    currentIndex = row;
    if (juce::isPositiveAndBelow (row, (int) filtered.size()))
        updateDetails (filtered[(size_t) row]);
}

void FunctionsContentComponent::updateDetails (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) allFunctions.size()))
        return;
    auto& f = allFunctions[(size_t) index];
    nameLabel.setText (f.name, juce::dontSendNotification);
    descLabel.setText (f.description, juce::dontSendNotification);
    soundLabel.setText (f.soundCharacter.isNotEmpty()
                            ? ("Sound: " + f.soundCharacter)
                            : juce::String ("Sound: (see description)"),
                        juce::dontSendNotification);

    juce::String uses;
    if (f.useCases.size() > 0)
    {
        uses = "Use: ";
        for (int i = 0; i < f.useCases.size(); ++i)
        {
            if (i) uses << " | ";
            uses << f.useCases[i];
        }
    }
    if (f.keywords.size() > 0)
    {
        if (uses.isNotEmpty()) uses << "\n";
        uses << "Tags: " << f.keywords.joinIntoString (", ");
    }
    useLabel.setText (uses, juce::dontSendNotification);

    exampleLabel.setText ("Example: " + f.example, juce::dontSendNotification);
    extraLabel.setText ("Domain: " + f.domain
                            + "\nAliasing: " + f.aliasing
                            + "\nPerf: " + f.performance,
                        juce::dontSendNotification);

    // Plot uses a closed f(x) demo (not raw docs example with a/b/DSL)
    plot.setFunctionDemo (f.name, f.example);
}

void FunctionsContentComponent::paint (juce::Graphics&)
{
}

void FunctionsContentComponent::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto left = area.removeFromLeft (juce::jmax (200, area.getWidth() * 30 / 100));
    searchField.setBounds (left.removeFromTop (36).reduced (0, 2));
    left.removeFromTop (8);
    listBox.setBounds (left);

    area.removeFromLeft (12);
    // More height for larger body text
    nameLabel.setBounds (area.removeFromTop (30));
    area.removeFromTop (4);
    descLabel.setBounds (area.removeFromTop (52));
    area.removeFromTop (4);
    soundLabel.setBounds (area.removeFromTop (48));
    area.removeFromTop (4);
    useLabel.setBounds (area.removeFromTop (52));
    area.removeFromTop (6);
    plotCaption.setBounds (area.removeFromTop (20));
    plot.setBounds (area.removeFromTop (juce::jmax (150, area.getHeight() * 38 / 100)));
    area.removeFromTop (6);
    exampleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (4);
    extraLabel.setBounds (area.removeFromTop (64));

    auto buttons = area.removeFromBottom (40);
    insertButton.setBounds (buttons.removeFromLeft (130).reduced (2));
    closeButton.setBounds (buttons.removeFromLeft (130).reduced (2));
}

bool FunctionsContentComponent::keyPressed (const juce::KeyPress& kp)
{
    if (kp == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    if (kp == juce::KeyPress::returnKey)
    {
        insertButton.triggerClick();
        return true;
    }
    return false;
}
