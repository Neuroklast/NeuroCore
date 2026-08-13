#include <JuceHeader.h>
#include "FormulaDisplayComponent.h"
#include "PluginLookAndFeel.h"
#include "../utils/ExpressionEvaluator.h"
#include "../core/Config.h"

namespace
{
bool isIdentStart (juce_wchar c) noexcept
{
    return juce::CharacterFunctions::isLetter (c) || c == '_';
}

bool isIdentChar (juce_wchar c) noexcept
{
    return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
}

/** True if expr references audio/time sample vars that make a single live value meaningless. */
bool usesSampleVars (const juce::String& expr)
{
    auto lower = expr.toLowerCase();
    // whole-word-ish checks for x, y, t, ch and their variants
    static const char* blocked[] = {
        "x", "y", "t", "ch", "x_prev", "y_prev", "sample", "in", "out"
    };
    for (auto* w : blocked)
    {
        int start = 0;
        const juce::String needle (w);
        while (start < lower.length())
        {
            auto pos = lower.indexOfIgnoreCase (start, needle);
            if (pos < 0)
                break;
            const auto before = pos > 0 ? lower[pos - 1] : (juce_wchar) 0;
            const auto after  = pos + needle.length() < lower.length()
                                  ? lower[pos + needle.length()] : (juce_wchar) 0;
            const bool whole = ! isIdentChar (before) && ! isIdentChar (after);
            if (whole)
                return true;
            start = pos + needle.length();
        }
    }
    return false;
}

} // namespace

class FormulaDisplayComponent::Body : public juce::Component
{
public:
    explicit Body (FormulaDisplayComponent& o) : owner (o) { setOpaque (true); }
    void paint (juce::Graphics& g) override { owner.paintBody (g); }
private:
    FormulaDisplayComponent& owner;
};

FormulaDisplayComponent::FormulaDisplayComponent()
{
    for (int i = 0; i < Config::kNumUserParams; ++i)
    {
        varNames[(size_t) i]   = Config::kDefaultVariableNames[i];
        varColours[(size_t) i] = knobColour (i);
    }
    setOpaque (true);
    body = std::make_unique<Body> (*this);
    viewport.setViewedComponent (body.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);
}

FormulaDisplayComponent::~FormulaDisplayComponent()
{
    viewport.setViewedComponent (nullptr, false);
}

void FormulaDisplayComponent::setVariableColours (const std::array<juce::String, Config::kNumUserParams>& names,
                                                  const std::array<juce::Colour, Config::kNumUserParams>& colours)
{
    varNames   = names;
    varColours = colours;
    layoutDirty = true;
    refreshBodySize();
    repaint();
}

void FormulaDisplayComponent::setFormula (const juce::String& text)
{
    if (formula == text)
        return;
    formula = text;
    parseParamLines();
    layoutDirty = true;
    refreshBodySize();
    repaint();
}

void FormulaDisplayComponent::setKnobValues (const std::array<float, Config::kNumUserParams>& values)
{
    bool changed = false;
    for (int i = 0; i < Config::kNumUserParams; ++i)
    {
        if (std::abs (knobValues[(size_t) i] - values[(size_t) i]) > 1.0e-5f)
        {
            changed = true;
            break;
        }
    }
    if (! changed)
        return;
    knobValues = values;
    layoutDirty = true;
    refreshBodySize();
    repaint();
}

void FormulaDisplayComponent::setError (const juce::String& err)
{
    error = err;
    layoutDirty = true;
    refreshBodySize();
    repaint();
}

void FormulaDisplayComponent::parseParamLines()
{
    paramRanges.clear();
    juce::StringArray lines;
    lines.addLines (formula);
    for (auto line : lines)
    {
        line = line.trim();
        if (! line.startsWithIgnoreCase ("param"))
            continue;

        auto eqPos = line.indexOfChar ('=');
        if (eqPos < 0)
            continue;

        auto sym = line.substring (5, eqPos).trim().toLowerCase();
        auto rest = line.substring (eqPos + 1).trim();
        // Accept all host knobs a..f (Config::kNumUserParams)
        if (sym.length() != 1 || sym[0] < 'a'
            || sym[0] >= 'a' + Config::kNumUserParams)
            continue;

        ParamRange pr;
        pr.alias = sym;
        pr.min = 0.f;
        pr.max = 1.f;

        const int rangeOpen = rest.indexOfChar ('[');
        if (rangeOpen >= 0)
        {
            pr.name = rest.substring (0, rangeOpen).trim();
            const int rangeClose = rest.lastIndexOfChar (']');
            if (rangeClose > rangeOpen)
            {
                auto inner = rest.substring (rangeOpen + 1, rangeClose);
                auto comma = inner.indexOfChar (',');
                if (comma > 0)
                {
                    pr.min = inner.substring (0, comma).trim().getFloatValue();
                    pr.max = inner.substring (comma + 1).trim().getFloatValue();
                    if (pr.max < pr.min)
                        std::swap (pr.min, pr.max);
                }
            }
        }
        else
        {
            pr.name = rest.trim();
        }
        paramRanges.push_back (pr);
    }
}

int FormulaDisplayComponent::knobIndexForToken (const juce::String& token) const
{
    const auto t = token.toLowerCase();
    if (t.length() == 1 && t[0] >= 'a' && t[0] < 'a' + Config::kNumUserParams)
        return t[0] - 'a';

    for (int i = 0; i < Config::kNumUserParams; ++i)
        if (varNames[(size_t) i].isNotEmpty() && varNames[(size_t) i].equalsIgnoreCase (token))
            return i;

    for (const auto& p : paramRanges)
    {
        if (p.alias.equalsIgnoreCase (token) || p.name.equalsIgnoreCase (token))
        {
            if (p.alias.length() == 1 && p.alias[0] >= 'a'
                && p.alias[0] < 'a' + Config::kNumUserParams)
                return p.alias[0] - 'a';
        }
    }
    return -1;
}

float FormulaDisplayComponent::mappedKnobValue (int knobIndex) const noexcept
{
    if (knobIndex < 0 || knobIndex >= Config::kNumUserParams)
        return 0.f;
    const float norm = juce::jlimit (0.f, 1.f, knobValues[(size_t) knobIndex]);
    const char letter = static_cast<char> ('a' + knobIndex);
    for (const auto& p : paramRanges)
    {
        if (p.alias.length() == 1 && p.alias[0] == letter)
            return p.min + norm * (p.max - p.min);
    }
    return norm;
}

juce::Colour FormulaDisplayComponent::colourForToken (const juce::String& token) const
{
    const int idx = knobIndexForToken (token);
    if (idx >= 0)
        return varColours[(size_t) idx];
    return juce::Colour (0xfffff0f0);
}

juce::String FormulaDisplayComponent::formatValue (float v) const
{
    if (! std::isfinite (v))
        return "nan";
    const float av = std::abs (v);
    if (av >= 1000.f)
        return juce::String (v, 0);
    if (av >= 100.f)
        return juce::String (v, 1);
    if (av >= 10.f)
        return juce::String (v, 2);
    return juce::String (v, 3);
}

bool FormulaDisplayComponent::tryEvalPureExpr (const juce::String& expr, float& out, int& dominantKnob) const
{
    auto e = expr.trim();
    if (e.isEmpty())
        return false;
    if (usesSampleVars (e))
        return false;

    // Must involve at least one knob so we don't annotate pure constants
    bool hasKnob = false;
    dominantKnob = -1;
    {
        // scan tokens for knobs
        int i = 0;
        while (i < e.length())
        {
            if (! isIdentStart (e[i]))
            {
                ++i;
                continue;
            }
            int j = i + 1;
            while (j < e.length() && isIdentChar (e[j]))
                ++j;
            auto tok = e.substring (i, j);
            const int ki = knobIndexForToken (tok);
            if (ki >= 0)
            {
                hasKnob = true;
                if (dominantKnob < 0)
                    dominantKnob = ki;
                else if (dominantKnob != ki)
                    dominantKnob = -2; // multi-knob
            }
            i = j;
        }
    }
    if (! hasKnob)
        return false;

    // Build expression with aliases → a/b/c/d and inject map() for ranged params
    juce::String rewritten = e;
    for (const auto& p : paramRanges)
    {
        if (std::abs (p.max - p.min) < 1.0e-6f)
            continue;
        if (p.min == 0.f && p.max == 1.f)
            continue;

        const auto mapped = "map(" + p.alias + ",0,1,"
                          + juce::String (p.min) + "," + juce::String (p.max) + ")";

        // Replace whole-word alias and name with map(...)
        auto replaceWhole = [&] (const juce::String& word)
        {
            if (word.isEmpty())
                return;
            juce::String result;
            int start = 0;
            auto src = rewritten;
            auto lower = src.toLowerCase();
            auto w = word.toLowerCase();
            while (start < src.length())
            {
                auto pos = lower.indexOf (start, w);
                if (pos < 0)
                {
                    result += src.substring (start);
                    break;
                }
                const auto before = pos > 0 ? src[pos - 1] : (juce_wchar) 0;
                const auto after  = pos + w.length() < src.length()
                                      ? src[pos + w.length()] : (juce_wchar) 0;
                const bool whole = ! isIdentChar (before) && ! isIdentChar (after);
                // skip if already inside map(
                bool insideMap = false;
                if (whole && pos >= 4)
                {
                    auto pre = lower.substring (juce::jmax (0, pos - 4), pos);
                    if (pre == "map(")
                        insideMap = true;
                }
                result += src.substring (start, pos);
                result += (whole && ! insideMap) ? mapped : src.substring (pos, pos + (int) w.length());
                start = pos + (int) w.length();
            }
            rewritten = result;
        };
        replaceWhole (p.alias);
        if (p.name.isNotEmpty() && ! p.name.equalsIgnoreCase (p.alias))
            replaceWhole (p.name);
    }

    ExpressionEvaluator eval;
    if (! eval.parseFormula (rewritten.toStdString()))
        return false;

    // Reject if parser pulled in sample vars
    for (const char* blocked : { "x", "y", "t", "ch", "x_prev", "y_prev" })
        if (eval.getVariableIndex (blocked) != ExpressionEvaluator::invalidIndex)
            return false;

    // Inject 0–1 knob positions (map() expands ranges)
    for (int i = 0; i < Config::kNumUserParams; ++i)
    {
        const char letter = static_cast<char> ('a' + i);
        eval.setVariable (std::string (1, letter), knobValues[(size_t) i]);
        if (varNames[(size_t) i].isNotEmpty())
            eval.setVariable (varNames[(size_t) i].toStdString(), knobValues[(size_t) i]);
    }
    for (const auto& p : paramRanges)
    {
        const int idx = (p.alias.length() == 1 && p.alias[0] >= 'a'
                         && p.alias[0] < 'a' + Config::kNumUserParams)
                          ? (p.alias[0] - 'a') : -1;
        if (idx >= 0)
        {
            eval.setVariable (p.alias.toStdString(), knobValues[(size_t) idx]);
            if (p.name.isNotEmpty())
                eval.setVariable (p.name.toStdString(), knobValues[(size_t) idx]);
        }
    }

    out = eval.evaluate (0.f);
    return std::isfinite (out);
}

void FormulaDisplayComponent::setFontHeight (float heightPt)
{
    fontHeight = juce::jlimit (Config::kMinEditorFontPt, Config::kMaxEditorFontPt, heightPt);
    layoutDirty = true;
    refreshBodySize();
    repaint();
}

void FormulaDisplayComponent::rebuildAttributed()
{
    cachedLayout = {};
    cachedLayout.setLineSpacing (fontHeight * (Config::kFormulaLineHeight - 1.0f));
    // Formula live view uses embedded mono for readability (Apex is UI chrome only)
    const juce::Font font = NeuroCoreLookAndFeel::monoFont (fontHeight);
    const juce::Font mono = NeuroCoreLookAndFeel::monoFont (fontHeight);
    // Terminal palette: near-white text on black, red accents for knobs
    const auto textCol   = juce::Colour (0xfffff0f0);
    const auto mutedCol  = juce::Colour (0xff8a8a8a);
    const auto keyCol    = juce::Colour (0xffb0b0b0);

    if (error.isNotEmpty())
    {
        cachedLayout.append (error, font, juce::Colour (0xffff1a1a));
        layoutDirty = false;
        return;
    }

    juce::StringArray lines;
    lines.addLines (formula);
    if (lines.isEmpty())
        lines.add ("// empty formula");

    for (int li = 0; li < lines.size(); ++li)
    {
        auto line = lines[li];
        if (li > 0)
            cachedLayout.append ("\n", mono, textCol);

        // Split by ';' for per-assignment annotations while preserving structure
        juce::StringArray segments;
        // Keep colons with first segment of block lines
        int segStart = 0;
        for (int i = 0; i <= line.length(); ++i)
        {
            if (i == line.length() || line[i] == ';')
            {
                segments.add (line.substring (segStart, i));
                segStart = i + 1;
            }
        }
        if (segments.isEmpty())
            segments.add (line);

        for (int si = 0; si < segments.size(); ++si)
        {
            if (si > 0)
                cachedLayout.append (";", mono, mutedCol);

            const auto seg = segments[si];
            // Find '=' for possible live value
            juce::String prefix = seg;
            juce::String rhs;
            const int eq = seg.indexOfChar ('=');
            if (eq >= 0)
            {
                prefix = seg.substring (0, eq + 1);
                rhs    = seg.substring (eq + 1);
            }

            // Colour identifiers; after every knob (a..f or alias) append live mapped [value]
            auto appendColoured = [this, &mono, &textCol, &keyCol] (const juce::String& text,
                                                                    bool annotateKnobs)
            {
                int i = 0;
                while (i < text.length())
                {
                    if (! isIdentStart (text[i]))
                    {
                        int j = i;
                        while (j < text.length() && ! isIdentStart (text[j]))
                            ++j;
                        cachedLayout.append (text.substring (i, j), mono, textCol);
                        i = j;
                        continue;
                    }
                    int j = i + 1;
                    while (j < text.length() && isIdentChar (text[j]))
                        ++j;
                    auto tok = text.substring (i, j);
                    const int ki = knobIndexForToken (tok);
                    if (ki >= 0)
                    {
                        const auto col = varColours[(size_t) ki];
                        cachedLayout.append (tok, mono, col);
                        if (annotateKnobs)
                        {
                            // Live value for THIS knob (updates when the rotary moves)
                            cachedLayout.append ("[", mono, col.withAlpha (0.75f));
                            cachedLayout.append (formatValue (mappedKnobValue (ki)), mono, col);
                            cachedLayout.append ("]", mono, col.withAlpha (0.75f));
                        }
                    }
                    else if (tok.equalsIgnoreCase ("param")
                          || tok.equalsIgnoreCase ("filter")
                          || tok.equalsIgnoreCase ("stage")
                          || tok.equalsIgnoreCase ("comp")
                          || tok.equalsIgnoreCase ("delay")
                          || tok.equalsIgnoreCase ("reverb")
                          || tok.equalsIgnoreCase ("env")
                          || tok.equalsIgnoreCase ("osc")
                          || tok.equalsIgnoreCase ("type")
                          || tok.equalsIgnoreCase ("cutoff")
                          || tok.equalsIgnoreCase ("resonance")
                          || tok.equalsIgnoreCase ("center")
                          || tok.equalsIgnoreCase ("width")
                          || tok.equalsIgnoreCase ("feedback")
                          || tok.equalsIgnoreCase ("time")
                          || tok.equalsIgnoreCase ("mix")
                          || tok.equalsIgnoreCase ("highpass")
                          || tok.equalsIgnoreCase ("lowpass")
                          || tok.equalsIgnoreCase ("bandpass"))
                        cachedLayout.append (tok, mono, keyCol);
                    else
                        cachedLayout.append (tok, mono, textCol);
                    i = j;
                }
            };

            // param a = Drive [min,max]  -> also show live mapped value at end of line
            const bool isParamLine = seg.trimStart().startsWithIgnoreCase ("param");
            appendColoured (prefix, ! isParamLine);
            if (eq >= 0)
            {
                appendColoured (rhs, true);

                if (isParamLine)
                {
                    // Live mapped value for this param declaration
                    auto lhs = prefix.upToFirstOccurrenceOf ("=", false, false).trim().toLowerCase();
                    // "param a" / "param b"
                    juce::String letter;
                    if (lhs.startsWith ("param"))
                        letter = lhs.fromFirstOccurrenceOf ("param", false, false).trim();
                    if (letter.length() == 1 && letter[0] >= 'a'
                        && letter[0] < 'a' + Config::kNumUserParams)
                    {
                        const int ki = letter[0] - 'a';
                        const auto col = varColours[(size_t) ki];
                        cachedLayout.append ("  => ", mono, mutedCol);
                        cachedLayout.append (formatValue (mappedKnobValue (ki)), mono, col);
                    }
                }
                else
                {
                    // Complex pure expression result (e.g. a*b without sample vars)
                    float value = 0.f;
                    int dominant = -1;
                    const auto rhsTrim = rhs.trim();
                    const bool singleKnob = (knobIndexForToken (rhsTrim) >= 0);
                    if (! singleKnob && tryEvalPureExpr (rhs, value, dominant))
                    {
                        juce::Colour bracketCol = mutedCol;
                        if (dominant >= 0)
                            bracketCol = varColours[(size_t) dominant];
                        else if (dominant == -2)
                            bracketCol = juce::Colour (0xffe8ecf4);

                        cachedLayout.append (" [= ", mono, bracketCol.withAlpha (0.85f));
                        cachedLayout.append (formatValue (value), mono, bracketCol);
                        cachedLayout.append ("]", mono, bracketCol.withAlpha (0.85f));
                    }
                }
            }
        }
    }

    layoutDirty = false;
}

int FormulaDisplayComponent::getContentHeight() const noexcept
{
    return body != nullptr ? body->getHeight() : 0;
}

void FormulaDisplayComponent::refreshBodySize()
{
    if (layoutDirty)
        rebuildAttributed();

    const int viewW = juce::jmax (1, viewport.getMaximumVisibleWidth());
    const int textW = juce::jmax (1, viewW - 16);
    cachedTextLayout.createLayout (cachedLayout, (float) textW);
    const int h = (int) std::ceil (cachedTextLayout.getHeight()) + 16;
    if (body != nullptr)
        body->setSize (viewW, juce::jmax (h, viewport.getHeight()));
}

void FormulaDisplayComponent::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (18);
    viewport.setBounds (r.reduced (1, 0));
    refreshBodySize();
}

void FormulaDisplayComponent::paintBody (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff000000));
    if (layoutDirty)
        refreshBodySize();
    cachedTextLayout.draw (g, juce::Rectangle<float> (8.f, 6.f,
                                                      (float) juce::jmax (1, getWidth() - 16),
                                                      cachedTextLayout.getHeight() + 4.f));
}

void FormulaDisplayComponent::paintOverChildren (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    for (int y = 20; y < getHeight(); y += 3)
        g.drawHorizontalLine (y, 0.f, (float) getWidth());
}

void FormulaDisplayComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff000000));

    // Header strip: formula terminal identity
    g.setColour (juce::Colour (0xff0a0000));
    g.fillRect (0, 0, getWidth(), 18);
    g.setColour (juce::Colour (0xffff1a1a).withAlpha (0.85f));
    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 10.f, juce::Font::bold));
    const int lines = juce::StringArray::fromLines (formula).size();
    const bool blink = ((int) (juce::Time::getMillisecondCounter() / 400) % 2) == 0;
    g.drawText (juce::String ("DSP_CORE // ")
                    + juce::String (lines) + " LN"
                    + (error.isNotEmpty() ? " // ERR" : (blink ? " // LIVE_" : " // LIVE ")),
                8, 2, getWidth() - 16, 14, juce::Justification::centredLeft, false);
    // scan line under header
    g.setColour (juce::Colour (0xffff1a1a).withAlpha (0.45f));
    g.fillRect (0, 17, getWidth(), 1);

    g.setColour (juce::Colour (0xff3a0000));
    g.drawRect (getLocalBounds().toFloat().reduced (0.5f), 1.f);

}
