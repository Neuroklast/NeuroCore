#include "GraphCanvasComponent.h"
#include "../dsl/PcbRouter.h"
#include "PluginLookAndFeel.h"
#include "custom/ParameterComponent.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include "../utils/UiSettings.h"
#include <cmath>
#include <map>

namespace
{
constexpr int kDragSlop = 6;
constexpr int kGrid = GraphCanvasComponent::kGrid;

juce::String kindLabel (const dsl::GraphNode& n)
{
    const auto t = n.type.toLowerCase();
    if (t.startsWith ("filter")) return "FILTER";
    if (t == "eq") return "EQ";
    if (t.startsWith ("stage")) return "DRIVE";
    if (t.startsWith ("comp")) return "COMP";
    if (t.startsWith ("ngate") || t.startsWith ("noisegate") || t == "noise_gate")
        return "NGATE";
    if (t.startsWith ("gate")) return "GATE";
    if (t.startsWith ("limit")) return "LIMIT";
    if (t.startsWith ("delay")) return "DELAY";
    if (t.startsWith ("reverb")) return "REVERB";
    if (t.startsWith ("ir")) return "CAB";
    if (t == "ott") return "OTT";
    if (t.startsWith ("widen")) return "WIDTH";
    if (t == "ms") return dsl::isMsEncode (n) ? "MS ENC" : "MS DEC";
    if (t == "bus") return "BUS";
    if (t == "send") return "SEND";
    if (t.startsWith ("xover") || t.startsWith ("crossover")) return "XOVER";
    if (t == "send") return "SEND";
    if (t == "out") return "OUT";
    if (t.startsWith ("octav")) return "OCT";
    if (t.startsWith ("vocod")) return "VOC";
    if (t.startsWith ("env")) return "ENV";
    if (t.startsWith ("osc")) return "LFO";
    if (t.startsWith ("meter") || t == "probe") return "METER";
    if (t.startsWith ("sidechain") || t == "sc" || t == "scin") return "SC IN";
    return t.toUpperCase().substring (0, 6);
}

juce::String compactSummary (const dsl::GraphNode& n)
{
    auto get = [&n] (const char* key) -> juce::String
    {
        const auto it = n.args.find (key);
        return it == n.args.end() ? juce::String() : it->second;
    };
    if (n.type == "send") return "tap";
    if (n.type == "bus") return n.name;
    if (n.type == "ms")
        return dsl::isMsEncode (n) ? "L/R  ->  M/S" : "M/S  ->  L/R";
    if (n.type.startsWithIgnoreCase ("xover"))
        return (get ("f1") + (get ("f2").isNotEmpty() ? (" / " + get ("f2")) : juce::String())).trim();
    if (n.type == "out")
    {
        juce::StringArray bits;
        for (const auto& kv : n.args)
            if (kv.first.isNotEmpty())
                bits.add (kv.first);
        return bits.isEmpty() ? juce::String ("mix") : bits.joinIntoString ("+");
    }
    if (n.type == "stage")
    {
        return get ("y");
    }
    if (n.type.startsWithIgnoreCase ("filter"))
    {
        auto t = get ("type");
        if (t.startsWithIgnoreCase ("high")) t = "HP";
        else if (t.startsWithIgnoreCase ("low")) t = "LP";
        else if (t.startsWithIgnoreCase ("band")) t = "BP";
        return (t + " " + get ("cutoff")).trim();
    }
    if (n.type.startsWithIgnoreCase ("osc"))
        return (get ("shape") + " " + get ("freq")).trim();
    if (n.type.startsWithIgnoreCase ("reverb"))
        return ("sz " + get ("size")).trim();
    if (n.type.startsWithIgnoreCase ("comp"))
        return ("th " + get ("threshold")).trim();
    if (n.type.startsWithIgnoreCase ("ngate") || n.type.startsWithIgnoreCase ("noisegate")
        || n.type.equalsIgnoreCase ("noise_gate"))
        return ("th " + get ("threshold")).trim();
    if (n.type.startsWithIgnoreCase ("gate"))
        return ("th " + get ("threshold")).trim();
    if (n.type.startsWithIgnoreCase ("meter") || n.type.equalsIgnoreCase ("probe"))
        return get ("mode").isNotEmpty() ? get ("mode") : juce::String ("loudness");
    return {};
}

juce::String prettyArgName (const juce::String& key)
{
    const auto k = key.toLowerCase();
    if (k == "y") return "Formula";
    if (k == "channel") return "Channel";
    if (k == "type") return "Type";
    if (k == "cutoff") return "Cutoff";
    if (k == "resonance") return "Resonance";
    if (k == "freq") return "Freq";
    if (k == "q") return "Q";
    if (k == "gain") return "Gain";
    if (k == "threshold") return "Threshold";
    if (k == "ratio") return "Ratio";
    if (k == "attack") return "Attack";
    if (k == "release") return "Release";
    if (k == "makeup") return "Makeup";
    if (k == "time") return "Time";
    if (k == "sync") return "Sync";
    if (k == "feedback") return "Feedback";
    if (k == "mix") return "Mix";
    if (k == "pingpong") return "Ping-pong";
    if (k == "+") return "Plus";
    if (k == "*") return "Times";
    if (k == "size") return "Size";
    if (k == "decay") return "Decay";
    if (k == "damp") return "Damp";
    if (k == "width") return "Width";
    if (k == "depth") return "Depth";
    if (k == "shape") return "Shape";
    if (k == "in") return "Input";
    if (k == "main") return "Main";
    return key;
}

juce::Path makeCable (juce::Point<float> a, juce::Point<float> b)
{
    return GraphCanvasComponent::makeOrthoCable (a, b);
}

void pathEnds (const juce::Path& path, juce::Point<float>& start, juce::Point<float>& end)
{
    start = {};
    end = {};
    bool have = false;
    juce::PathFlatteningIterator it (path);
    while (it.next())
    {
        if (! have)
        {
            start = { it.x1, it.y1 };
            have = true;
        }
        end = { it.x2, it.y2 };
    }
}

void drawCablePlug (juce::Graphics& g, juce::Point<float> jack, bool leaveRight, juce::Colour body)
{
    const float bodyW = 7.2f, bodyH = 5.4f;
    const float pinW = 5.2f, pinH = 2.4f;
    const float bodyX = leaveRight ? jack.x + 0.6f : jack.x - bodyW - 0.6f;
    const float pinX  = leaveRight ? jack.x - 2.2f : jack.x - pinW + 2.2f;
    g.setColour (body);
    g.fillRoundedRectangle (bodyX, jack.y - bodyH * 0.5f, bodyW, bodyH, 1.1f);
    g.setColour (body.brighter (0.18f));
    g.fillRoundedRectangle (pinX, jack.y - pinH * 0.5f, pinW, pinH, 0.6f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.fillRect (bodyX + 1.2f, jack.y - 1.5f, 2.2f, 1.0f);
}

bool nodeUsesToken (const dsl::GraphNode& n, const juce::String& token)
{
    if (token.isEmpty())
        return false;
    const auto t = token.toLowerCase();
    for (const auto& kv : n.args)
    {
        const auto s = kv.second.toLowerCase();
        int i = 0;
        while ((i = s.indexOf (i, t)) >= 0)
        {
            const auto prev = (i == 0) ? 0 : s[i - 1];
            const auto next = (i + t.length() >= s.length()) ? 0 : s[i + t.length()];
            if (! juce::CharacterFunctions::isLetterOrDigit (prev) && prev != '_'
                && ! juce::CharacterFunctions::isLetterOrDigit (next) && next != '_')
                return true;
            ++i;
        }
    }
    return false;
}
}

class GraphCanvasComponent::NodeView : public juce::Component,
                                       public juce::SettableTooltipClient
{
public:
    NodeView (GraphCanvasComponent& o, int idx) : owner (o), nodeIndex (idx)
    {
        setPaintingIsUnclipped (true);
        setRepaintsOnMouseActivity (true);
        if (juce::isPositiveAndBelow (idx, (int) o.document.nodes.size()))
            binds = dsl::knobBindings (o.document.nodes[(size_t) idx]);
        jacks = o.jacksOf (idx);
        if (const auto* n = realNode())
            expanded = owner.expandedNames.contains (n->name);
        applyCardSize();
        refreshTooltip();
    }

    int nodeIndex { -1 };
    juce::Point<int> dragAnchor;
    bool moving { false };
    bool editing { false };
    std::vector<dsl::KnobBinding> binds;
    std::vector<dsl::GraphJack> jacks;
    juce::StringArray editKeys;
    std::vector<std::unique_ptr<juce::Label>> editLabels;
    std::vector<std::unique_ptr<juce::TextEditor>> editFields;
    std::vector<std::unique_ptr<juce::TextButton>> knobLetters;
    int focusField { 0 };
    bool expanded { false };

    juce::Rectangle<int> foldHit() const
    {
        if (isIn() || isOut() || editing)
            return {};
        auto r = getLocalBounds().toFloat().reduced (1.f);
        const float titleH = (float) owner.scaled (GraphCanvasComponent::kTitleRows
                                                   * GraphCanvasComponent::kGrid);
        auto titleRow = juce::Rectangle<float> (r.getX() + 8.f, r.getY() + 1.f,
                                                r.getWidth() - 16.f, titleH - 2.f);
        bool hasSc = false;
        for (const auto& j : jacks)
            if (j.kind == "sc")
                hasSc = true;
        if (hasSc)
            titleRow.removeFromRight (22.f);
        return titleRow.removeFromRight (14.f).expanded (6.f, 4.f).toNearestInt();
    }

    void refreshTooltip()
    {
        if (const auto* n = realNode())
        {
            juce::String tip;
            tip << kindLabel (*n) << "  " << n->name;
            const auto sum = compactSummary (*n);
            if (sum.isNotEmpty())
                tip << "\n" << sum;
            for (const auto& key : dsl::editableArgKeys (*n))
            {
                const auto it = n->args.find (key);
                tip << "\n" << prettyArgName (key) << "  "
                    << (it == n->args.end() ? juce::String ("-") : it->second);
            }
            tip << "\n" << (expanded ? "Click the arrow to fold." : "Click the arrow to expand.")
                << " Double-click to edit.";
            setTooltip (tip);
            return;
        }
        if (isIn())
            setTooltip ("IN  Audio input");
        else if (isOut())
            setTooltip ("OUT  Mix output");
    }

    void setExpanded (bool on)
    {
        if (isIn() || isOut())
            return;
        expanded = on;
        if (const auto* n = realNode())
        {
            if (on)
                owner.expandedNames.addIfNotAlreadyThere (n->name);
            else
                owner.expandedNames.removeString (n->name);
        }
        applyCardSize();
        owner.markEdgesDirty();
        owner.layoutPaper();
        repaint();
        owner.repaintPaper();
    }

    void applyCardSize()
    {
        if (editing)
        {
            setSize (owner.scaled (220), owner.scaled (30 + editKeys.size() * 40 + 30));
            return;
        }
        int nIn = 0, nOut = 0;
        for (const auto& j : jacks)
        {
            if (isControlJack (j))
                continue;
            (j.output ? nOut : nIn) += 1;
        }
        const bool io = isIn() || isOut();
        const int w = io ? GraphCanvasComponent::kIoWidth : GraphCanvasComponent::kCardWidth;
        int wantH = GraphCanvasComponent::kIoHeight;
        if (io)
        {
            wantH = GraphCanvasComponent::chipHeight (nIn, nOut, 0, 0, false);
        }
        else
        {
            const auto* n = realNode();
            const int nArgs = n != nullptr ? dsl::editableArgKeys (*n).size() : 0;
            wantH = GraphCanvasComponent::chipHeight (nIn, nOut, nArgs, (int) binds.size(),
                                                      expanded);
        }
        setSize (owner.scaled (w), owner.scaled (wantH));
    }

    void clearEditors()
    {
        for (auto& e : editFields)
            removeChildComponent (e.get());
        for (auto& l : editLabels)
            removeChildComponent (l.get());
        for (auto& b : knobLetters)
            removeChildComponent (b.get());
        editFields.clear();
        editLabels.clear();
        knobLetters.clear();
        editKeys.clear();
    }

    void layoutEditors()
    {
        auto r = getLocalBounds().reduced (8, 6);
        r.removeFromTop (4);
        for (size_t i = 0; i < editFields.size(); ++i)
        {
            auto row = r.removeFromTop (36);
            if (i < editLabels.size())
                editLabels[i]->setBounds (row.removeFromTop (12));
            editFields[i]->setBounds (row);
            r.removeFromTop (4);
        }
        auto letters = r.removeFromTop (22);
        const int w = letters.getWidth() / Config::kNumUserParams;
        for (int k = 0; k < (int) knobLetters.size(); ++k)
            knobLetters[(size_t) k]->setBounds (letters.removeFromLeft (w).reduced (1));
    }

    void startEdit (const juce::String& onlyKey = {})
    {
        if (isIn() || nodeIndex == kOutIndex
            || ! juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            return;
        owner.commitOpenEdits();
        const auto& n = owner.document.nodes[(size_t) nodeIndex];
        if (n.type == "bus")
            return;
        clearEditors();
        editing = true;
        owner.selected = nodeIndex;
        editKeys = onlyKey.isNotEmpty() ? juce::StringArray { onlyKey }
                                        : dsl::editableArgKeys (n, &owner.document);

        for (const auto& key : editKeys)
        {
            auto lab = std::make_unique<juce::Label>();
            lab->setText (key == "y" ? "formula" : key, juce::dontSendNotification);
            lab->setFont (NeuroKoreLookAndFeel::monoFont (11.f));
            lab->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::inkMuted());
            addAndMakeVisible (*lab);
            editLabels.push_back (std::move (lab));

            auto ed = std::make_unique<juce::TextEditor>();
            juce::String cur;
            const auto it = n.args.find (key);
            if (it != n.args.end())
                cur = it->second;
            ed->setText (cur, false);
            ed->setFont (NeuroKoreLookAndFeel::monoFont (13.f));
            ed->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff050505));
            ed->setColour (juce::TextEditor::textColourId, NeuroKoreLookAndFeel::ink());
            ed->setColour (juce::TextEditor::outlineColourId, NeuroKoreLookAndFeel::inkMuted().withAlpha (0.4f));
            ed->setColour (juce::TextEditor::focusedOutlineColourId, NeuroKoreLookAndFeel::accent());
            ed->onReturnKey = [this] { commitEdit(); };
            ed->onEscapeKey = [this] { cancelEdit(); };
            ed->onFocusLost = [this] {};
            addAndMakeVisible (*ed);
            editFields.push_back (std::move (ed));
        }
        for (int k = 0; k < Config::kNumUserParams; ++k)
        {
            auto b = std::make_unique<juce::TextButton> (
                juce::String::charToString ((juce::juce_wchar) ('a' + k)));
            b->setTooltip ("Bind this knob to the focused field");
            const int idx = k;
            b->onClick = [this, idx]
            {
                int which = focusField;
                for (int i = 0; i < (int) editFields.size(); ++i)
                    if (editFields[(size_t) i]->hasKeyboardFocus (true))
                        which = i;
                if (juce::isPositiveAndBelow (which, (int) editFields.size()))
                    editFields[(size_t) which]->setText (
                        juce::String::charToString ((juce::juce_wchar) ('a' + idx)), false);
            };
            addAndMakeVisible (*b);
            knobLetters.push_back (std::move (b));
        }
        applyCardSize();
        layoutEditors();
        owner.layoutPaper();
        if (! editFields.empty())
            editFields[0]->grabKeyboardFocus();
        owner.repaintPaper();
    }

    void commitEdit()
    {
        if (! editing)
            return;
        for (int i = 0; i < editKeys.size() && i < (int) editFields.size(); ++i)
            dsl::setNodeArg (owner.document, nodeIndex, editKeys[i],
                             editFields[(size_t) i]->getText());
        clearEditors();
        editing = false;
        applyCardSize();
        owner.applyGraph();
    }

    void cancelEdit()
    {
        if (! editing)
            return;
        clearEditors();
        editing = false;
        applyCardSize();
        owner.layoutPaper();
        owner.repaintPaper();
    }

    bool isIn() const noexcept { return nodeIndex == kInIndex; }
    bool isOut() const
    {
        if (nodeIndex == kOutIndex)
            return true;
        if (! juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            return false;
        return owner.document.nodes[(size_t) nodeIndex].type == "out";
    }
    const dsl::GraphNode* realNode() const
    {
        if (! juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            return nullptr;
        return &owner.document.nodes[(size_t) nodeIndex];
    }

    static bool isControlJack (const dsl::GraphJack& j) noexcept
    {
        return j.kind == "knob" || j.kind == "param";
    }

    juce::Point<float> jackCentre (int jackIndex) const
    {
        if (! juce::isPositiveAndBelow (jackIndex, (int) jacks.size()))
            return { (float) getWidth() * 0.5f, (float) getHeight() * 0.5f };
        const auto& jack = jacks[(size_t) jackIndex];
        // Knob/param pins share the first signal-in row so they do not
        // stretch the chip and throw opposite jacks off the grid.
        if (isControlJack (jack))
        {
            const float y = (float) owner.scaled (GraphCanvasComponent::jackLocalY (0));
            return { 0.f, y };
        }
        const bool output = jack.output;
        int slot = 0, count = 0;
        for (int i = 0; i < (int) jacks.size(); ++i)
        {
            if (isControlJack (jacks[(size_t) i]))
                continue;
            if (jacks[(size_t) i].output != output)
                continue;
            if (i == jackIndex)
                slot = count;
            ++count;
        }
        const float y = (float) owner.scaled (GraphCanvasComponent::jackLocalY (slot));
        return { output ? (float) getWidth() : 0.f, y };
    }

    juce::Point<float> jackCentreById (const juce::String& id, bool outputIfEmpty) const
    {
        if (id.isNotEmpty())
        {
            for (int i = 0; i < (int) jacks.size(); ++i)
                if (jacks[(size_t) i].id == id)
                    return jackCentre (i);
        }
        for (int i = 0; i < (int) jacks.size(); ++i)
            if (jacks[(size_t) i].output == outputIfEmpty)
                return jackCentre (i);
        return { outputIfEmpty ? (float) getWidth() : 0.f,
                 (float) getHeight() * 0.5f };
    }

    juce::Point<float> inCentre() const
    {
        return jackCentreById ({}, false);
    }

    juce::Point<float> outCentre() const
    {
        return jackCentreById ({}, true);
    }

    bool hitPort (juce::Point<int> p, bool& output, juce::String& jackId) const
    {
        if (jacks.empty())
            return false;
        const auto pf = p.toFloat();
        int best = -1;
        float bestD = 14.f;
        for (int i = 0; i < (int) jacks.size(); ++i)
        {
            const auto c = jackCentre (i);
            float d = pf.getDistanceFrom (c);
            if (jacks[(size_t) i].output && p.x >= getWidth() - 18)
                d = juce::jmin (d, std::abs (pf.y - c.y));
            if (! jacks[(size_t) i].output && p.x <= 18)
                d = juce::jmin (d, std::abs (pf.y - c.y));
            if (d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
        if (best < 0)
            return false;
        output = jacks[(size_t) best].output;
        jackId = jacks[(size_t) best].id;
        return true;
    }

    void paint (juce::Graphics& g) override
    {
        const auto accent = NeuroKoreLookAndFeel::accent();
        auto r = getLocalBounds().toFloat().reduced (1.f);
        const bool sel = (owner.selected == nodeIndex);
        const auto* node = realNode();
        bool hoverBind = false;
        for (const auto& b : binds)
            if (b.knobIndex == owner.hoverKnob)
                hoverBind = true;

        const bool moving = owner.movingNode == nodeIndex;
        const bool chip = ! isIn() && ! isOut();
        auto chamfer = [] (juce::Rectangle<float> box, float cut)
        {
            juce::Path p;
            p.startNewSubPath (box.getX() + cut, box.getY());
            p.lineTo (box.getRight() - cut, box.getY());
            p.lineTo (box.getRight(), box.getY() + cut);
            p.lineTo (box.getRight(), box.getBottom() - cut);
            p.lineTo (box.getRight() - cut, box.getBottom());
            p.lineTo (box.getX() + cut, box.getBottom());
            p.lineTo (box.getX(), box.getBottom() - cut);
            p.lineTo (box.getX(), box.getY() + cut);
            p.closeSubPath();
            return p;
        };
        const auto package = chamfer (r, chip ? 5.f : 3.f);
        g.setColour (editing || sel || moving ? juce::Colour (0xff141414) : juce::Colour (0xff0a0a0a));
        g.fillPath (package);
        if (chip)
        {
            const auto inner = chamfer (r.reduced (3.2f, 3.2f), 3.f);
            g.setColour (juce::Colour (0xff111111));
            g.fillPath (inner);
            g.setColour (NeuroKoreLookAndFeel::canvas());
            g.fillEllipse (r.getCentreX() - 5.5f, r.getY() - 3.4f, 11.f, 7.4f);
            g.setColour (accent.withAlpha (0.62f));
            g.fillEllipse (r.getX() + 7.f, r.getY() + 7.f, 3.4f, 3.4f);
            g.setColour (accent.withAlpha (0.10f));
            g.drawHorizontalLine ((int) (r.getY() + (float) owner.scaled (GraphCanvasComponent::kTitleRows
                                                                         * GraphCanvasComponent::kGrid)),
                                  r.getX() + 12.f, r.getRight() - 12.f);
        }
        g.setColour (sel || hoverBind || moving ? accent : accent.withAlpha (0.40f));
        g.strokePath (package, juce::PathStrokeType (sel || moving ? 1.6f : 1.05f));
        if (moving)
        {
            g.setColour (accent.withAlpha (0.16f));
            g.fillPath (package);
        }

        juce::String title = isIn() ? "IN" : (isOut() ? "OUT" : (node != nullptr ? kindLabel (*node) : "?"));
        if (node != nullptr)
        {
            const auto ch = dsl::channelRail (*node);
            if (ch.isNotEmpty())
                title << "  " << ch.toUpperCase();
        }
        if (editing)
            return;

        if (isIn() || isOut())
        {
            g.setColour (NeuroKoreLookAndFeel::ink());
            g.setFont (NeuroKoreLookAndFeel::monoFont (16.f));
            auto titleArea = getLocalBounds().reduced (10, 2);
            if (jacks.size() <= 1)
                g.drawText (title, getLocalBounds(), juce::Justification::centred, false);
            else
                g.drawText (title, titleArea.removeFromTop (12), juce::Justification::centred, false);
        }
        else
        {
            const float titleH = (float) owner.scaled (GraphCanvasComponent::kTitleRows
                                                       * GraphCanvasComponent::kGrid);
            auto titleRow = juce::Rectangle<float> (r.getX() + 8.f, r.getY() + 1.f,
                                                    r.getWidth() - 16.f, titleH - 2.f);
            bool hasSc = false;
            for (const auto& j : jacks)
                if (j.kind == "sc")
                    hasSc = true;
            if (hasSc)
            {
                auto badge = titleRow.removeFromRight (22.f);
                g.setColour (accent.withAlpha (0.85f));
                g.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
                g.drawText ("SC", badge.toNearestInt(), juce::Justification::centredRight, false);
                titleRow.removeFromRight (4.f);
            }
            auto foldR = titleRow.removeFromRight (14.f);
            g.setColour (NeuroKoreLookAndFeel::inkMuted());
            juce::Path chev;
            const float cx = foldR.getCentreX(), cy = foldR.getCentreY();
            if (expanded)
                chev.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
            else
                chev.addTriangle (cx - 2.f, cy - 4.f, cx + 3.f, cy, cx - 2.f, cy + 4.f);
            g.fillPath (chev);
            if (! binds.empty())
            {
                juce::String bits;
                for (const auto& b : binds)
                {
                    if (bits.isNotEmpty()) bits << "  ";
                    const auto letter = juce::String::charToString ((juce::juce_wchar) ('a' + b.knobIndex));
                    auto name = owner.processor.getVariableName (b.knobIndex);
                    if (name.isEmpty())
                        name = letter;
                    bits << letter << " " << name;
                }
                auto valR = titleRow.removeFromRight (juce::jmin (titleRow.getWidth() * 0.58f, 148.f));
                g.setColour (accent);
                g.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
                g.drawFittedText (bits, valR.toNearestInt(), juce::Justification::centredRight, 1);
                titleRow.removeFromRight (4.f);
            }
            g.setColour (NeuroKoreLookAndFeel::ink());
            g.setFont (NeuroKoreLookAndFeel::monoFont (15.f));
            g.drawText (title, titleRow.toNearestInt(), juce::Justification::centredLeft, true);

            auto body = r;
            body.removeFromTop (titleH);
            body.removeFromLeft (16.f);
            body.removeFromRight (16.f);
            body.removeFromBottom (3.f);
            if (node != nullptr
                && (node->type.startsWithIgnoreCase ("meter") || node->type.equalsIgnoreCase ("probe")))
            {
                float db = -100.f;
                if (owner.processor.copyMeterReading (node->name, db))
                {
                    juce::String unit = "LU";
                    const auto mode = node->args.count ("mode") ? node->args.at ("mode").toLowerCase()
                                                                : juce::String ("loudness");
                    if (mode == "peak") unit = "PK";
                    else if (mode == "rms") unit = "RMS";
                    auto line = body.removeFromTop (14.f);
                    g.setColour (accent);
                    g.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
                    g.drawText (juce::String (db, 1) + " " + unit, line.toNearestInt(),
                                juce::Justification::centredLeft, true);
                }
            }
            if (node != nullptr && expanded)
            {
                g.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
                for (const auto& key : dsl::editableArgKeys (*node, &owner.document))
                {
                    auto line = body.removeFromTop ((float) owner.scaled (GraphCanvasComponent::kGrid));
                    if (line.getHeight() < 8.f)
                        break;
                    juce::String val;
                    const auto it = node->args.find (key);
                    if (it != node->args.end())
                        val = it->second;
                    juce::String bindBit;
                    for (const auto& b : binds)
                        if (b.key == key)
                        {
                            const auto letter = juce::String::charToString (
                                (juce::juce_wchar) ('a' + b.knobIndex));
                            auto name = owner.processor.getVariableName (b.knobIndex);
                            bindBit = letter + (name.isNotEmpty() ? (" " + name) : juce::String())
                                    + "  "
                                    + GraphCanvasComponent::formatLiveKnob (
                                          owner.mappedKnobValue (b.knobIndex));
                            break;
                        }
                    g.setColour (NeuroKoreLookAndFeel::inkMuted());
                    g.drawText (prettyArgName (key), line.removeFromLeft (64.f).toNearestInt(),
                                juce::Justification::centredLeft, true);
                    g.setColour (bindBit.isNotEmpty() ? accent : NeuroKoreLookAndFeel::ink());
                    g.drawText (bindBit.isNotEmpty() ? bindBit
                                                     : (val.isEmpty() ? juce::String ("-") : val),
                                line.toNearestInt(), juce::Justification::centredLeft, true);
                }
            }
        }

        auto drawPort = [&] (juce::Point<float> c, bool hot, bool knobish, bool output,
                             bool patched, bool hub)
        {
            const float rOut = hub ? 5.6f : 5.2f;
            const float rIn  = patched ? 2.4f : 3.1f;
            g.setColour (NeuroKoreLookAndFeel::inkMuted().withAlpha (hot ? 0.85f : 0.55f));
            if (hub)
                g.fillRect (c.x - rOut, c.y - rOut, rOut * 2.f, rOut * 2.f);
            else
                g.fillEllipse (c.x - rOut, c.y - rOut, rOut * 2.f, rOut * 2.f);
            g.setColour (NeuroKoreLookAndFeel::canvas().brighter (0.04f));
            if (hub)
                g.drawRect (c.x - rOut, c.y - rOut, rOut * 2.f, rOut * 2.f, 1.15f);
            else
                g.drawEllipse (c.x - rOut, c.y - rOut, rOut * 2.f, rOut * 2.f, 1.15f);
            g.setColour (juce::Colour::fromRGB (12, 12, 14));
            if (hub)
                g.fillRect (c.x - rIn, c.y - rIn, rIn * 2.f, rIn * 2.f);
            else
                g.fillEllipse (c.x - rIn, c.y - rIn, rIn * 2.f, rIn * 2.f);
            if (patched)
            {
                const float pinW = 7.f, pinH = 3.2f;
                auto pin = juce::Rectangle<float> (output ? c.x - 1.2f : c.x - pinW + 1.2f,
                                                   c.y - pinH * 0.5f, pinW, pinH);
                g.setColour (hot || knobish ? accent.withAlpha (hot ? 0.95f : 0.75f)
                                            : NeuroKoreLookAndFeel::ink().withAlpha (0.82f));
                g.fillRoundedRectangle (pin, 0.8f);
                g.setColour (NeuroKoreLookAndFeel::ink().withAlpha (0.9f));
                if (hub)
                    g.fillRect (c.x - 2.1f, c.y - 2.1f, 4.2f, 4.2f);
                else
                    g.fillEllipse (c.x - 2.1f, c.y - 2.1f, 4.2f, 4.2f);
                g.setColour (accent.withAlpha (hot ? 0.95f : 0.55f));
                if (hub)
                    g.drawRect (c.x - 2.1f, c.y - 2.1f, 4.2f, 4.2f, 0.9f);
                else
                    g.drawEllipse (c.x - 2.1f, c.y - 2.1f, 4.2f, 4.2f, 0.9f);
            }
            else
            {
                g.setColour ((hot || knobish ? accent : NeuroKoreLookAndFeel::accent())
                                 .withAlpha (hot ? 0.85f : 0.45f));
                if (hub)
                    g.drawRect (c.x - rOut, c.y - rOut, rOut * 2.f, rOut * 2.f, 1.0f);
                else
                    g.drawEllipse (c.x - rOut, c.y - rOut, rOut * 2.f, rOut * 2.f, 1.0f);
            }
        };

        g.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
        for (int i = 0; i < (int) jacks.size(); ++i)
        {
            const auto& j = jacks[(size_t) i];
            const auto c = jackCentre (i);
            const bool knobish = (j.kind == "knob" || j.kind == "mod");
            const bool dropHot = owner.hoverDropNode == nodeIndex && owner.hoverDropJack == j.id;
            const bool patched = owner.jackIsPatched (nodeIndex, j.id, j.output);
            const bool hub = (j.kind == "mix" || j.kind == "send" || isOut());
            drawPort (c, isMouseOver() || dropHot, knobish, j.output, patched, hub);
            if (dropHot)
            {
                g.setColour (accent);
                g.fillEllipse (c.x - 5.f, c.y - 5.f, 10.f, 10.f);
                g.setColour (NeuroKoreLookAndFeel::ink());
                g.drawEllipse (c.x - 6.f, c.y - 6.f, 12.f, 12.f, 1.6f);
            }
            if (j.label.isEmpty() || j.kind == "sc" || isControlJack (j))
                continue;
            const bool namedSplit = j.id == "mid" || j.id == "side" || j.id == "low"
                                 || j.id == "high" || j.id == "left" || j.id == "right"
                                 || j.kind == "mix" || j.kind == "mod";
            if (! expanded && ! namedSplit)
                continue;
            const float titleH = (float) owner.scaled (GraphCanvasComponent::kTitleRows
                                                       * GraphCanvasComponent::kGrid);
            if (c.y < titleH + 1.f)
                continue;
            const bool outside = j.output ? (c.x > r.getRight() - 8.f) : (c.x < r.getX() + 8.f);
            if (! outside)
                continue;
            auto lr = juce::Rectangle<float> (j.output ? c.x - 38.f : c.x + 8.f,
                                              c.y - 6.f, 32.f, 12.f);
            g.setColour (NeuroKoreLookAndFeel::inkMuted());
            g.setFont (NeuroKoreLookAndFeel::monoFont (9.f));
            g.drawFittedText (j.label, lr.toNearestInt(),
                              j.output ? juce::Justification::centredRight
                                       : juce::Justification::centredLeft, 1);
        }
    }

    void resized() override
    {
        if (editing)
            layoutEditors();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool output = false;
        juce::String jackId;
        if (hitPort (e.getPosition(), output, jackId))
        {
            juce::String tip = jackId;
            for (const auto& j : jacks)
                if (j.id == jackId)
                {
                    tip = (j.label.isNotEmpty() ? j.label : j.id)
                        + "  " + j.kind
                        + (j.output ? " out" : " in");
                    break;
                }
            setTooltip (tip);
            return;
        }
        refreshTooltip();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.selected = nodeIndex;
        if (foldHit().contains (e.getPosition()))
        {
            setExpanded (! expanded);
            return;
        }
        bool output = false;
        juce::String jackId;
        if (hitPort (e.getPosition(), output, jackId))
        {
            if (output)
                owner.beginCable (nodeIndex, false, jackId);
            else
                owner.pickupIncoming (nodeIndex, jackId);
            owner.repaintPaper();
            return;
        }
        if (e.mods.isPopupMenu())
        {
            owner.selected = nodeIndex;
            owner.showNodeMenu (nodeIndex);
            owner.repaintPaper();
            return;
        }
        if (e.getNumberOfClicks() >= 2)
        {
            owner.selected = nodeIndex;
            if (owner.onInspectNode
                && juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
                owner.onInspectNode (nodeIndex);
            else
                owner.showNodeMenu (nodeIndex);
            owner.repaintPaper();
            return;
        }
        if (editing)
            return;
        moving = true;
        dragAnchor = e.getPosition();
        toFront (true);
        owner.grabKeyboardFocus();
        owner.repaintPaper();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (owner.cableFrom != -99)
        {
            owner.rubber = owner.toPaper (*this, e.getPosition()).toFloat();
            owner.updateCableHover (owner.rubber.roundToInt());
            owner.repaintPaper();
            return;
        }
        if (! moving)
            return;
        owner.movingNode = nodeIndex;
        owner.dropRail.clear();
        owner.dropInsertBefore = -99;
        if (e.getDistanceFromDragStart() < kDragSlop && e.getPosition() == dragAnchor)
            return;
        auto p = getPosition() + (e.getPosition() - dragAnchor);
        const float z = juce::jmax (0.2f, owner.zoom);
        auto design = GraphCanvasComponent::snapPoint ({ (int) std::lround ((float) p.x / z),
                                                         (int) std::lround ((float) p.y / z) });
        design.x = juce::jmax (kGrid, design.x);
        design.y = juce::jmax (kGrid, design.y);
        design = owner.firstFreeDesign (nodeIndex, design);
        setTopLeftPosition (owner.scaled (design.x), owner.scaled (design.y));
        if (isIn())
            owner.inPos = design.toFloat();
        else if (isOut() && ! juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            owner.outPos = design.toFloat();
        else if (juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            dsl::setPosition (owner.document, nodeIndex, (float) design.x, (float) design.y);
        owner.markEdgesDirty();
        owner.layoutPaper();
        owner.repaintPaper();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (owner.cableFrom != -99)
        {
            bool output = false;
            juce::String jackId;
            int dest = -99;
            if (hitPort (e.getPosition(), output, jackId) && ! output)
                dest = nodeIndex;
            else if (auto* hit = owner.viewAt (owner.toPaper (*this, e.getPosition())))
            {
                const auto local = hit->getLocalPoint (this, e.getPosition());
                bool inPort = false;
                juce::String hitJack;
                if ((hit->hitPort (local, inPort, hitJack) && ! inPort) || ! hit->isIn())
                {
                    dest = hit->nodeIndex;
                    jackId = hitJack;
                }
            }
            owner.finishCable (dest, jackId);
            owner.hoverDropNode = -99;
            owner.hoverDropJack.clear();
            moving = false;
            owner.movingNode = -99;
            return;
        }
        if (moving && e.getDistanceFromDragStart() >= kDragSlop)
            owner.commitDocument();
        moving = false;
        owner.movingNode = -99;
        owner.dropRail.clear();
        owner.dropInsertBefore = -99;
        owner.repaintPaper();
    }

private:
    GraphCanvasComponent& owner;
};

class GraphCanvasComponent::Paper : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
    explicit Paper (GraphCanvasComponent& o) : owner (o)
    {
        setOpaque (true);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int edge = owner.hitCable (e.getPosition().toFloat());
        if (edge >= 0 && edge < (int) owner.cachedEdges.size())
        {
            const auto& c = owner.cachedEdges[(size_t) edge];
            juce::String a = (c.fromIndex == -1) ? "IN"
                           : (juce::isPositiveAndBelow (c.fromIndex, (int) owner.document.nodes.size())
                                  ? owner.document.nodes[(size_t) c.fromIndex].name
                                  : "OUT");
            juce::String b = (c.toIndex == -2 || c.toIndex == owner.outIndex()) ? "OUT"
                           : (juce::isPositiveAndBelow (c.toIndex, (int) owner.document.nodes.size())
                                  ? owner.document.nodes[(size_t) c.toIndex].name
                                  : "?");
            setTooltip (a + "  ->  " + b + "  (" + c.kind + ")");
        }
        else
            setTooltip ({});
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (NeuroKoreLookAndFeel::canvas());
        const auto rose = NeuroKoreLookAndFeel::accent();
        const int step = juce::jmax (8, (int) std::lround ((float) kGrid * 2.f * owner.zoom));
        for (int y = step; y < getHeight(); y += step)
        {
            for (int x = step; x < getWidth(); x += step)
            {
                const bool major = ((x / step) % 4 == 0) && ((y / step) % 4 == 0);
                g.setColour (rose.withAlpha (major ? 0.20f : 0.08f));
                const float arm = major ? 3.5f : 2.f;
                const float xf = (float) x, yf = (float) y;
                g.drawLine (xf - arm, yf, xf + arm, yf, major ? 1.0f : 0.7f);
                g.drawLine (xf, yf - arm, xf, yf + arm, major ? 1.0f : 0.7f);
            }
        }

        if (! owner.parseOk)
        {
            g.setColour (NeuroKoreLookAndFeel::ink());
            g.setFont (NeuroKoreLookAndFeel::monoFont (16.f));
            g.drawFittedText (owner.parseError.isNotEmpty() ? owner.parseError
                                                            : "Circuit: assemble the parts.\nTerminal: hack the construct.",
                              getLocalBounds().reduced (40), juce::Justification::centred, 4);
            return;
        }

        if (owner.motion != CyberMotion::Off)
        {
            const auto spec = owner.processor.getCurrentSpec();
            juce::String hud;
            hud << "SNAP " << kGrid
                << "   SR " << juce::String ((int) spec.sampleRate)
                << "   BUF " << (int) spec.maximumBlockSize
                << "   BPM " << juce::String (owner.processor.getEffectiveBpm(), 1)
                << (owner.processor.isHostTempo() ? " HOST" : " USER")
                << "   NODES " << (int) owner.document.nodes.size();
            g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (
                owner.motion == CyberMotion::Full ? 0.42f : 0.28f));
            g.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
            g.drawText (hud, getLocalBounds().removeFromTop (18).reduced (8, 0),
                        juce::Justification::centredLeft, false);
        }

        owner.refreshEdgesIfDirty();

        for (int ei = 0; ei < (int) owner.cachedEdges.size(); ++ei)
        {
            const auto& e = owner.cachedEdges[(size_t) ei];
            const auto a = owner.portCentre (e.fromIndex, e.fromJack, true);
            const auto b = owner.portCentre (e.toIndex, e.toJack, false);
            if (a.x < 1.f && a.y < 1.f && b.x < 1.f)
                continue;
            const bool mix = (e.kind == "mix" || e.kind == "send");
            const float lvl = (e.fromIndex == kInIndex) ? owner.inLevel
                            : (e.toIndex == owner.outIndex() || e.toIndex == kOutIndex) ? owner.outLevel
                            : juce::jmax (owner.inLevel, owner.outLevel);
            const bool hot = owner.hoverNode == e.fromIndex || owner.hoverNode == e.toIndex
                          || owner.selected == e.fromIndex || owner.selected == e.toIndex;
            const juce::Path path = (ei < (int) owner.cachedRoutes.size())
                ? owner.pathFromPcb (owner.cachedRoutes[(size_t) ei])
                : makeCable (a, b);
            owner.drawLiveCable (g, path, lvl, mix, hot,
                                 owner.waveForEdge (e.fromIndex), GraphCanvasComponent::kTapN);
        }

        int modI = 0;
        for (const auto& nv : owner.nodeViews)
        {
            if (nv->isIn() || nv->isOut())
                continue;
            if (! juce::isPositiveAndBelow (nv->nodeIndex, (int) owner.document.nodes.size()))
                continue;
            const auto& src = owner.document.nodes[(size_t) nv->nodeIndex];
            if (! dsl::isModulator (src))
                continue;
            const bool srcHot = nv->isMouseOver() || owner.selected == nv->nodeIndex
                             || owner.hoverNode == nv->nodeIndex;
            for (const auto& destv : owner.nodeViews)
            {
                if (destv->isIn() || destv->isOut())
                    continue;
                if (! juce::isPositiveAndBelow (destv->nodeIndex, (int) owner.document.nodes.size()))
                    continue;
                const auto& dst = owner.document.nodes[(size_t) destv->nodeIndex];
                if (! nodeUsesToken (dst, src.name))
                    continue;
                const bool destHot = destv->isMouseOver() || owner.selected == destv->nodeIndex
                                  || owner.hoverNode == destv->nodeIndex;
                const auto a = owner.portCentre (nv->nodeIndex, "mod", true);
                const auto b = owner.portCentre (destv->nodeIndex, src.name, false);
                const auto it = owner.nodeWaves.find (src.name);
                const float* wave = (it != owner.nodeWaves.end()) ? it->second.data() : nullptr;
                const int waveN = (wave != nullptr) ? GraphCanvasComponent::kTapN : 0;
                float hz = 0.f;
                owner.processor.copyLfoHz (src.name, hz);
                const auto ampIt = owner.lfoAmp.find (src.name);
                const float amp = (ampIt != owner.lfoAmp.end())
                    ? ampIt->second
                    : GraphCanvasComponent::lfoChaseBrightness (wave, waveN);
                const auto phIt = owner.lfoPhase.find (src.name);
                const float phase = (phIt != owner.lfoPhase.end()) ? phIt->second : 0.f;
                const juce::Path path = (modI < (int) owner.cachedModRoutes.size())
                    ? owner.pathFromPcb (owner.cachedModRoutes[(size_t) modI])
                    : makeCable (a, b);
                ++modI;
                owner.drawModLauflicht (g, path, hz, amp, srcHot || destHot, phase);
            }
        }

        if (owner.cableFrom != -99)
        {
            const auto a = owner.portCentre (owner.cableFrom, owner.cableFromJack, true);
            juce::Path drag;
            drag.startNewSubPath (a);
            drag.lineTo (owner.rubber);
            g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.92f));
            g.strokePath (drag, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown() || e.mods.isCommandDown())
        {
            owner.panning = true;
            owner.panStart = e.getPosition();
            return;
        }
        if (e.mods.isPopupMenu())
        {
            owner.addBlockMenu (e.getPosition());
            return;
        }
        {
            const int edge = owner.hitCable (e.getPosition().toFloat());
            if (edge >= 0 && edge < (int) owner.cachedEdges.size())
            {
                const auto c = owner.cachedEdges[(size_t) edge];
                juce::String err;
                dsl::disconnectAudio (owner.document, c.fromIndex, c.toIndex, err);
                owner.markEdgesDirty();
                owner.beginCable (c.fromIndex, true, c.fromJack);
                owner.rubber = e.getPosition().toFloat();
                return;
            }
        }
        owner.commitOpenEdits();
        owner.selected = -99;
        owner.grabKeyboardFocus();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! owner.panning)
            return;
        auto* vp = &owner.viewport;
        auto cur = vp->getViewPosition() - (e.getPosition() - owner.panStart);
        vp->setViewPosition (cur);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        owner.panning = false;
        if (owner.cableFrom != -99)
            owner.finishCable (-99);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        owner.mouseWheelMove (e, w);
    }

private:
    GraphCanvasComponent& owner;
};

GraphCanvasComponent::GraphCanvasComponent (NeuroKoreAudioProcessor& proc)
    : processor (proc)
{
    setWantsKeyboardFocus (true);
    paper = std::make_unique<Paper> (*this);
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (paper.get(), false);
    viewport.setScrollBarsShown (false, false);
    startTimerHz (30);
}

GraphCanvasComponent::~GraphCanvasComponent()
{
    stopTimer();
    nodeViews.clear();
}

void GraphCanvasComponent::addBlockMenu (juce::Point<int> at)
{
    auto addItem = [this, at] (juce::PopupMenu& dest, const char* label, const juce::String& type,
                               bool routing = false, const juce::String& route = {})
    {
        dest.addItem (juce::PopupMenu::Item (label).setAction ([this, at, type, routing, route]
        {
            if (routing)
                addRouting (route, at);
            else
                addBlock (type, at);
        }));
    };

    juce::PopupMenu tone, drive, dyn, space, route, stereo, pitch, mod, measure;
    addItem (tone, "Filter     low / high / band", "filter");
    addItem (tone, "EQ     peak / shelf", "eq");
    addItem (drive, "Drive     formula, tube, clip", "stage");
    addItem (dyn, "Compressor", "comp");
    addItem (dyn, "Gate     expander / range", "gate");
    addItem (dyn, "Noise Gate     threshold, attack, release", "noisegate");
    addItem (dyn, "Limiter", "limit");
    addItem (space, "Delay", "delay");
    addItem (space, "Reverb", "reverb");
    addItem (space, "Cabinet IR", "ir");
    addItem (route, "Bus     parallel send / return", {}, true, "bus");
    addItem (route, "Mid-Side Split     encode / mid / side / decode", {}, true, "split_ms");
    addItem (route, "Left / Right Split     two channel rails", {}, true, "split_lr");
    addItem (route, "Crossover     LOW / MID / HIGH", {}, true, "xover");
    addItem (route, "MS Encode     L/R to mid/side", "ms");
    addItem (route, "MS Decode     mid/side to L/R", "ms_decode");
    addItem (route, "Sidechain     host extra input on this cable", "sidechain");
    addItem (stereo, "OTT     3-band up + down", "ott");
    addItem (stereo, "Stereo Width     Haas / image", "widen");
    addItem (pitch, "Octaver", "octaver");
    addItem (pitch, "Vocoder     voice on sidechain", "vocoder");
    addItem (mod, "LFO     does not sit on the audio line", "osc");
    addItem (mod, "Envelope follower", "env");
    addItem (measure, "Meter     loudness / peak / RMS, dry", "meter");

    juce::PopupMenu add;
    add.addSubMenu ("Tone", std::move (tone));
    add.addSubMenu ("Drive", std::move (drive));
    add.addSubMenu ("Dynamics", std::move (dyn));
    add.addSubMenu ("Space", std::move (space));
    add.addSubMenu ("Routing", std::move (route));
    add.addSubMenu ("Stereo", std::move (stereo));
    add.addSubMenu ("Pitch", std::move (pitch));
    add.addSubMenu ("Modulation", std::move (mod));
    add.addSubMenu ("Measure", std::move (measure));

    juce::PopupMenu m;
    m.addItem (juce::PopupMenu::Item ("Auto-arrange")
                   .setAction ([this] { tidyCircuit(); }));
    m.addSeparator();
    m.addSubMenu ("Add", std::move (add));
    m.showMenuAsync (juce::PopupMenu::Options()
                         .withMousePosition()
                         .withMinimumWidth (280)
                         .withStandardItemHeight (28));
}

void GraphCanvasComponent::bindKnob (int nodeIndex, const juce::String& key, int knobIndex)
{
    if (! dsl::setNodeArg (document, nodeIndex, key,
                           juce::String::charToString ((juce::juce_wchar) ('a' + knobIndex))))
        return;
    applyGraph();
}

void GraphCanvasComponent::showNodeMenu (int nodeIndex)
{
    if (nodeIndex == kInIndex)
    {
        addBlockMenu (viewFor (kInIndex) != nullptr
                          ? viewFor (kInIndex)->getBounds().getBottomLeft()
                          : juce::Point<int> (80, 160));
        return;
    }
    if (! juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
        return;

    const auto& n = document.nodes[(size_t) nodeIndex];
    juce::PopupMenu m;
    const auto title = kindLabel (n)
        + (n.name.isNotEmpty() ? ("   " + n.name) : juce::String());
    m.addSectionHeader (title);
    m.addItem (juce::PopupMenu::Item ("Edit this block")
                   .setEnabled (n.type != "bus")
                   .setAction ([this, nodeIndex] { editNodeArgs (nodeIndex); }));
    if (n.type.startsWithIgnoreCase ("ir") && onOpenIr)
        m.addItem (juce::PopupMenu::Item ("Open cabinet IR")
                       .setAction ([this, nodeIndex]
        {
            const auto name = document.nodes[(size_t) nodeIndex].name;
            onOpenIr (name);
        }));

    const auto keys = dsl::editableArgKeys (n);
    if (keys.size() > 0)
    {
        m.addSeparator();
        m.addSectionHeader ("Parameters");
        for (const auto& key : keys)
        {
            juce::String cur;
            const auto it = n.args.find (key);
            if (it != n.args.end())
                cur = it->second.trim();
            juce::PopupMenu sub;
            sub.addItem (juce::PopupMenu::Item ("Edit " + prettyArgName (key))
                             .setAction ([this, nodeIndex, key] { editNodeArgs (nodeIndex, key); }));
            sub.addSeparator();
            sub.addSectionHeader ("Connect a knob");
            for (int k = 0; k < Config::kNumUserParams; ++k)
            {
                const auto letter = juce::String::charToString ((juce::juce_wchar) ('a' + k));
                const auto knobName = processor.getVariableName (k);
                const auto label = letter + "   " + (knobName.isNotEmpty() ? knobName : letter);
                const bool on = cur.equalsIgnoreCase (letter);
                sub.addItem (juce::PopupMenu::Item (label)
                                 .setTicked (on)
                                 .setAction ([this, nodeIndex, key, k] { bindKnob (nodeIndex, key, k); }));
            }
            const auto row = prettyArgName (key)
                + "    "
                + (cur.isNotEmpty() ? cur : juce::String ("-"));
            m.addSubMenu (row, std::move (sub));
        }
    }

    m.addSeparator();
    const bool canRemove = n.type != "out" && n.type != "bus";
    m.addItem (juce::PopupMenu::Item ("Duplicate")
                   .setEnabled (canRemove)
                   .setAction ([this, nodeIndex]
    {
        if (! juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
            return;
        const auto copy = document.nodes[(size_t) nodeIndex];
        addBlock (copy.type, { (int) copy.x + kGrid * 2, (int) copy.y + kGrid * 2 });
    }));
    m.addItem (juce::PopupMenu::Item ("Remove this block")
                   .setEnabled (canRemove)
                   .setAction ([this, nodeIndex]
    {
        selected = nodeIndex;
        removeSelected();
    }));

    m.showMenuAsync (juce::PopupMenu::Options()
                         .withMousePosition()
                         .withMinimumWidth (280)
                         .withStandardItemHeight (28));
}

void GraphCanvasComponent::editNodeArgs (int nodeIndex, const juce::String& onlyKey)
{
    juce::ignoreUnused (onlyKey);
    if (onInspectNode && juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
    {
        onInspectNode (nodeIndex);
        return;
    }
    if (auto* v = viewFor (nodeIndex))
        v->startEdit ({});
}

void GraphCanvasComponent::commitOpenEdits()
{
    for (auto& v : nodeViews)
        if (v->editing)
            v->commitEdit();
}

void GraphCanvasComponent::setScript (const juce::String& script)
{
    dsl::GraphDocument incoming;
    juce::String err;
    if (! dsl::parse (script, incoming, err))
    {
        lastScript = script;
        parseOk = false;
        parseError = err;
        document = {};
        selected = -99;
        cableFrom = -99;
        cableFromJack.clear();
        markEdgesDirty();
        rebuildViews();
        return;
    }

    parseError.clear();
    if (parseOk && dsl::semanticallyEqual (document, incoming))
    {
        for (size_t i = 0; i < document.nodes.size(); ++i)
        {
            if (std::isfinite (incoming.nodes[i].x) && std::isfinite (incoming.nodes[i].y))
            {
                document.nodes[i].x = incoming.nodes[i].x;
                document.nodes[i].y = incoming.nodes[i].y;
            }
        }
        lastScript = script;
        return;
    }

    document = std::move (incoming);
    parseOk = true;
    lastScript = script;
    selected = -99;
    cableFrom = -99;
    cableFromJack.clear();
    markEdgesDirty();

    // A new graph must not inherit coordinates from the previous preset just
    // because names collide (stage1 in A is not stage1 in B). Factory text
    // has no @x,y — tidy at the current viewport/zoom.
    if (! dsl::hasAllPositions (document))
    {
        autoLayout();
        pendingAutoArrange = viewport.getWidth() <= 0 || viewport.getHeight() <= 0;
    }
    else
    {
        pendingAutoArrange = false;
    }
    rebuildViews();
}

void GraphCanvasComponent::tidyCircuit()
{
    if (! parseOk)
        return;
    autoLayout();
    applyGraph();
}

juce::String GraphCanvasComponent::getEmittedScript() const
{
    return parseOk ? dsl::emit (document) : lastScript;
}

void GraphCanvasComponent::autoLayout()
{
    const float z = juce::jmax (0.2f, zoom);
    const int viewW = juce::jmax (0, (int) std::lround ((float) viewport.getWidth() / z));
    const int viewH = juce::jmax (0, (int) std::lround ((float) viewport.getHeight() / z));
    const auto hint = dsl::tidyLayout (document, viewW, viewH);
    inPos = GraphCanvasComponent::snapPoint ({ (int) std::lround (hint.inX),
                                               (int) std::lround (hint.inY) }).toFloat();
    outPos = GraphCanvasComponent::snapPoint ({ (int) std::lround (hint.outX),
                                                (int) std::lround (hint.outY) }).toFloat();
}

int GraphCanvasComponent::outIndex() const
{
    for (int i = 0; i < (int) document.nodes.size(); ++i)
        if (document.nodes[(size_t) i].type == "out")
            return i;
    return -2;
}

void GraphCanvasComponent::rebuildViews()
{
    if (rebuilding)
        return;
    rebuilding = true;
    auto dying = std::move (nodeViews);
    if (paper != nullptr)
        paper->removeAllChildren();
    dying.clear();
    if (! parseOk)
    {
        layoutPaper();
        rebuilding = false;
        return;
    }

    auto addView = [this] (int idx, float x, float y)
    {
        auto v = std::make_unique<NodeView> (*this, idx);
        const auto snapped = GraphCanvasComponent::snapPoint ({ (int) std::lround (x),
                                                                (int) std::lround (y) });
        v->setTopLeftPosition (scaled (snapped.x), scaled (snapped.y));
        if (idx == kInIndex)
            inPos = snapped.toFloat();
        else if (idx == kOutIndex)
            outPos = snapped.toFloat();
        else if (juce::isPositiveAndBelow (idx, (int) document.nodes.size()))
        {
            document.nodes[(size_t) idx].x = (float) snapped.x;
            document.nodes[(size_t) idx].y = (float) snapped.y;
        }
        paper->addAndMakeVisible (*v);
        nodeViews.push_back (std::move (v));
    };

    addView (kInIndex, inPos.x, inPos.y);
    for (int i = 0; i < (int) document.nodes.size(); ++i)
    {
        const auto& n = document.nodes[(size_t) i];
        if (n.type == "bus" && n.name == dsl::kParkRail)
            continue;
        const float x = std::isfinite (n.x) ? n.x : 200.f;
        const float y = std::isfinite (n.y) ? n.y : 160.f;
        addView (i, x, y);
    }
    if (outIndex() == kOutIndex)
        addView (kOutIndex, outPos.x, outPos.y);
    layoutPaper();
    rebuilding = false;
    if (paper != nullptr)
        paper->repaint();
}

juce::Path GraphCanvasComponent::pathFromPcb (const dsl::PcbRoute& route) const
{
    juce::Path p;
    const float z = juce::jmax (0.2f, zoom);
    auto sx = [z] (dsl::PcbPoint q) -> juce::Point<float>
    {
        return { q.x * z, q.y * z };
    };
    for (const auto& c : route.cmds)
    {
        const auto pt = sx (c.p);
        if (c.kind == dsl::PcbCmdKind::Move)
            p.startNewSubPath (pt.x, pt.y);
        else if (c.kind == dsl::PcbCmdKind::Line)
            p.lineTo (pt.x, pt.y);
        else
        {
            const auto ct = sx (c.c);
            p.quadraticTo (ct.x, ct.y, pt.x, pt.y);
        }
    }
    return p;
}

void GraphCanvasComponent::refreshEdgesIfDirty() const
{
    if (! edgesDirty)
        return;
    cachedEdges = dsl::visualAudioEdges (document);
    if (outIndex() == kOutIndex)
    {
        int lastMain = -99;
        for (const auto& e : cachedEdges)
            if (e.kind == "audio" && e.toIndex >= 0)
                lastMain = e.toIndex;
        if (lastMain == -99)
        {
            for (int i = 0; i < (int) document.nodes.size(); ++i)
            {
                const auto& n = document.nodes[(size_t) i];
                if (n.type != "bus" && ! dsl::isModulator (n)
                    && (n.busName.isEmpty() || n.busName == "main"))
                    lastMain = i;
            }
        }
        cachedEdges.push_back ({ lastMain != -99 ? lastMain : kInIndex,
                                 kOutIndex, "audio", "out", "main" });
    }
    rebuildPcbRoutes();
    edgesDirty = false;
}

void GraphCanvasComponent::rebuildPcbRoutes() const
{
    const float cell = (float) kGrid;
    const float z = juce::jmax (0.2f, zoom);
    pcbRouter.cellSize = cell;
    pcbRouter.cornerRadius = cell * 0.5f;

    auto toDesign = [z] (juce::Point<float> p) -> dsl::PcbPoint
    {
        return { p.x / z, p.y / z };
    };

    std::vector<dsl::PcbRect> obs;
    obs.reserve (nodeViews.size());
    for (const auto& v : nodeViews)
    {
        const auto b = designBoundsOf (v->nodeIndex);
        if (b.isEmpty())
            continue;
        obs.push_back ({ (float) b.getX(), (float) b.getY(),
                         (float) b.getWidth(), (float) b.getHeight() });
    }

    std::vector<dsl::PcbNet> audio;
    audio.reserve (cachedEdges.size());
    for (const auto& e : cachedEdges)
    {
        const auto a = toDesign (portCentre (e.fromIndex, e.fromJack, true));
        const auto b = toDesign (portCentre (e.toIndex, e.toJack, false));
        const bool mix = (e.kind == "mix" || e.kind == "send");
        audio.push_back ({ { a, dsl::PcbFacing::East }, { b, dsl::PcbFacing::West },
                           mix ? dsl::PcbNetClass::Mix : dsl::PcbNetClass::Audio });
    }

    std::vector<dsl::PcbNet> mods;
    for (const auto& nv : nodeViews)
    {
        if (nv->isIn() || nv->isOut())
            continue;
        if (! juce::isPositiveAndBelow (nv->nodeIndex, (int) document.nodes.size()))
            continue;
        const auto& src = document.nodes[(size_t) nv->nodeIndex];
        if (! dsl::isModulator (src))
            continue;
        int dests = 0;
        for (const auto& destv : nodeViews)
        {
            if (destv->isIn() || destv->isOut())
                continue;
            if (! juce::isPositiveAndBelow (destv->nodeIndex, (int) document.nodes.size()))
                continue;
            if (nodeUsesToken (document.nodes[(size_t) destv->nodeIndex], src.name))
                ++dests;
        }
        int slot = 0;
        for (const auto& destv : nodeViews)
        {
            if (destv->isIn() || destv->isOut())
                continue;
            if (! juce::isPositiveAndBelow (destv->nodeIndex, (int) document.nodes.size()))
                continue;
            const auto& dst = document.nodes[(size_t) destv->nodeIndex];
            if (! nodeUsesToken (dst, src.name))
                continue;
            const auto fromJack = dests <= 1 ? juce::String ("mod")
                                             : ("mod:" + juce::String (slot++));
            const auto a = toDesign (portCentre (nv->nodeIndex, fromJack, true));
            const auto b = toDesign (portCentre (destv->nodeIndex, src.name, false));
            mods.push_back ({ { a, dsl::PcbFacing::East }, { b, dsl::PcbFacing::West },
                              dsl::PcbNetClass::Mod });
        }
    }

    std::vector<dsl::PcbNet> all = audio;
    all.insert (all.end(), mods.begin(), mods.end());
    const auto board = dsl::PcbRouter::inferBoard (all, obs, cell);
    cachedRoutes = pcbRouter.routeAll (audio, obs, board);
    cachedModRoutes = pcbRouter.routeAll (mods, obs, board);
    cachedControlRoutes.clear();
}

void GraphCanvasComponent::layoutPaper()
{
    int maxX = viewport.getWidth();
    int maxY = viewport.getHeight();
    for (auto& v : nodeViews)
    {
        maxX = juce::jmax (maxX, v->getRight() + 48);
        maxY = juce::jmax (maxY, v->getBottom() + 48);
    }
    paper->setSize (juce::jmax (1, maxX), juce::jmax (1, maxY));
}

void GraphCanvasComponent::repaintPaper()
{
    if (paper != nullptr)
        paper->repaint();
}

juce::Rectangle<int> GraphCanvasComponent::designBoundsOf (int nodeIndex) const
{
    auto* v = viewFor (nodeIndex);
    if (v == nullptr)
        return {};
    const float z = juce::jmax (0.2f, zoom);
    return { (int) std::lround ((float) v->getX() / z),
             (int) std::lround ((float) v->getY() / z),
             juce::jmax (kGrid, (int) std::lround ((float) v->getWidth() / z)),
             juce::jmax (kGrid, (int) std::lround ((float) v->getHeight() / z)) };
}

juce::Point<int> GraphCanvasComponent::firstFreeDesign (int nodeIndex, juce::Point<int> want) const
{
    auto boxAt = [this, nodeIndex] (juce::Point<int> p) -> juce::Rectangle<int>
    {
        auto b = designBoundsOf (nodeIndex);
        if (b.isEmpty())
            b = { 0, 0, kCardWidth, kCardHeight };
        return { p.x, p.y, b.getWidth(), b.getHeight() };
    };
    auto clashes = [this, nodeIndex] (juce::Rectangle<int> mine) -> bool
    {
        const int g = kGrid;
        const auto inflated = mine.expanded (g / 2, g / 2);
        for (const auto& v : nodeViews)
        {
            if (v->nodeIndex == nodeIndex)
                continue;
            const auto other = designBoundsOf (v->nodeIndex).expanded (g / 2, g / 2);
            if (inflated.intersects (other))
                return true;
        }
        return false;
    };

    auto p = want;
    p.x = juce::jmax (kGrid, p.x);
    p.y = juce::jmax (kGrid, p.y);
    for (int step = 0; step < 96; ++step)
    {
        if (! clashes (boxAt (p)))
            return snapPoint (p);
        p.y += kGrid;
        if ((step % 16) == 15)
        {
            p.y = want.y;
            p.x += kGrid;
        }
    }
    return snapPoint (p);
}

juce::Point<int> GraphCanvasComponent::toPaper (juce::Component& from, juce::Point<int> p) const
{
    if (paper == nullptr)
        return p;
    return paper->getLocalPoint (&from, p);
}

bool GraphCanvasComponent::jackIsPatched (int nodeIndex, const juce::String& jackId,
                                          bool output) const
{
    refreshEdgesIfDirty();
    for (const auto& e : cachedEdges)
    {
        if (output && e.fromIndex == nodeIndex)
        {
            if (jackId.isEmpty() || e.fromJack.isEmpty() || e.fromJack == jackId
                || e.fromJack == "out" || e.fromJack == "audio")
                return true;
        }
        if (! output && e.toIndex == nodeIndex)
        {
            if (jackId.isEmpty() || e.toJack.isEmpty() || e.toJack == jackId)
                return true;
        }
    }
    if (output && nodeIndex == kInIndex)
        return ! cachedEdges.empty();
    if (! output && nodeIndex == kOutIndex)
        return ! cachedEdges.empty();

    if (juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
    {
        const auto& n = document.nodes[(size_t) nodeIndex];
        if (jackId.startsWith ("knob:"))
        {
            const auto letter = jackId.fromFirstOccurrenceOf (":", false, false);
            for (const auto& kv : n.args)
                if (kv.second.containsWholeWord (letter))
                    return true;
        }
        if (output && dsl::isModulator (n))
        {
            for (const auto& other : document.nodes)
                if (other.name != n.name && nodeUsesToken (other, n.name))
                    return true;
        }
        if (! output)
        {
            for (const auto& other : document.nodes)
                if (dsl::isModulator (other) && (jackId == other.name || nodeUsesToken (n, other.name)))
                    return true;
        }
    }
    return false;
}

GraphCanvasComponent::NodeView* GraphCanvasComponent::viewFor (int nodeIndex) const
{
    for (auto& v : nodeViews)
        if (v->nodeIndex == nodeIndex)
            return v.get();
    return nullptr;
}

GraphCanvasComponent::NodeView* GraphCanvasComponent::viewAt (juce::Point<int> paperPos) const
{
    for (auto& v : nodeViews)
        if (v->getBounds().contains (paperPos))
            return v.get();
    return nullptr;
}

std::vector<dsl::GraphJack> GraphCanvasComponent::jacksOf (int nodeIndex) const
{
    if (nodeIndex == kInIndex)
        return dsl::jacksForInput();
    if (nodeIndex == kOutIndex)
        return dsl::jacksForVirtualOut (document);
    if (juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
        return dsl::jacksFor (document.nodes[(size_t) nodeIndex], &document);
    return {};
}

juce::Point<float> GraphCanvasComponent::portCentre (int nodeIndex, bool output) const
{
    return portCentre (nodeIndex, {}, output);
}

juce::Point<float> GraphCanvasComponent::portCentre (int nodeIndex, const juce::String& jackId,
                                                    bool outputIfEmpty) const
{
    if (auto* v = viewFor (nodeIndex))
    {
        const auto local = v->jackCentreById (jackId, outputIfEmpty);
        return paper->getLocalPoint (v, local.roundToInt()).toFloat();
    }
    return {};
}

void GraphCanvasComponent::beginCable (int fromIndex, bool pickup, juce::String fromJack)
{
    cableFrom = fromIndex;
    cableFromJack = std::move (fromJack);
    cablePickup = pickup;
    rubber = portCentre (fromIndex, cableFromJack, true);
}

void GraphCanvasComponent::pickupIncoming (int toIndex, const juce::String& toJack)
{
    const auto edges = dsl::audioEdges (document);
    for (const auto& e : edges)
    {
        if (e.toIndex != toIndex)
            continue;
        if (toJack.isNotEmpty() && e.toJack.isNotEmpty() && e.toJack != toJack)
            continue;
        if (e.kind != "audio" && e.kind != "send" && e.kind != "mix")
            continue;
        juce::String err;
        dsl::disconnectAudio (document, e.fromIndex, toIndex, err);
        markEdgesDirty();
        beginCable (e.fromIndex, true, e.fromJack);
        return;
    }
}

int GraphCanvasComponent::hitCable (juce::Point<float> paperPos) const
{
    refreshEdgesIfDirty();
    auto distSeg = [] (juce::Point<float> p, juce::Point<float> a, juce::Point<float> b)
    {
        const auto ab = b - a;
        const float den = ab.x * ab.x + ab.y * ab.y;
        if (den < 1.0f)
            return p.getDistanceFrom (a);
        float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / den;
        t = juce::jlimit (0.f, 1.f, t);
        return p.getDistanceFrom (a + ab * t);
    };
    int best = -1;
    float bestD = 8.f;
    for (int i = 0; i < (int) cachedEdges.size(); ++i)
    {
        const auto& e = cachedEdges[(size_t) i];
        const auto a = portCentre (e.fromIndex, e.fromJack, true);
        const auto b = portCentre (e.toIndex, e.toJack, false);
        float d = distSeg (paperPos, a, b);
        if (i < (int) cachedRoutes.size())
        {
            const float z = juce::jmax (0.2f, zoom);
            const auto& wp = cachedRoutes[(size_t) i].waypoints;
            for (size_t s = 1; s < wp.size(); ++s)
                d = juce::jmin (d, distSeg (paperPos,
                                            { wp[s - 1].x * z, wp[s - 1].y * z },
                                            { wp[s].x * z, wp[s].y * z }));
        }
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void GraphCanvasComponent::finishCable (int toIndex, const juce::String& toJack)
{
    const int from = cableFrom;
    const auto fromJack = cableFromJack;
    const bool pickup = cablePickup;
    cableFrom = -99;
    cableFromJack.clear();
    cablePickup = false;
    if (from == -99)
    {
        if (paper != nullptr)
            paper->repaint();
        return;
    }
    if (toIndex == -99 || toIndex == from)
    {
        if (pickup)
            applyGraph();
        else if (paper != nullptr)
            paper->repaint();
        return;
    }
    juce::String err;
    if (! dsl::connectJack (document, from, fromJack, toIndex, toJack, err))
    {
        parseError = err;
        if (pickup)
            applyGraph();
        else if (paper != nullptr)
            paper->repaint();
        return;
    }
    applyGraph();
}

void GraphCanvasComponent::commitDocument()
{
    applyGraph();
}

void GraphCanvasComponent::setHoverKnob (int knobIndex)
{
    if (hoverKnob == knobIndex)
        return;
    hoverKnob = knobIndex;
    repaintPaper();
    if (auto* ed = getParentComponent())
        ed->repaint();
}

void GraphCanvasComponent::applyGraph()
{
    if (! parseOk)
        return;
    markEdgesDirty();
    juce::String err;
    const auto script = dsl::emit (document);
    if (processor.setFormula (script, err, false))
    {
        lastScript = script;
        parseError.clear();
        if (onScriptChanged)
            onScriptChanged();
    }
    else
    {
        parseError = err;
        parseOk = dsl::parse (lastScript, document, err);
    }
    rebuildViews();
}

juce::String GraphCanvasComponent::nextName (const juce::String& typeStem) const
{
    int maxN = 0;
    for (const auto& n : document.nodes)
    {
        if (! n.name.startsWithIgnoreCase (typeStem))
            continue;
        const auto tail = n.name.substring (typeStem.length());
        if (tail.containsOnly ("0123456789"))
            maxN = juce::jmax (maxN, tail.getIntValue());
    }
    return typeStem + juce::String (maxN + 1);
}

void GraphCanvasComponent::addBlock (const juce::String& type, juce::Point<int> at)
{
    if (! parseOk)
    {
        parseOk = true;
        document = {};
    }

    const auto snappedAt = GraphCanvasComponent::snapPoint (at);
    dsl::GraphNode n;
    n.type = type;
    n.busName = "main";
    n.x = (float) snappedAt.x;
    n.y = (float) snappedAt.y;
    if (type == "filter")
    {
        n.name = nextName ("filter");
        n.args["type"] = "lowpass";
        n.args["cutoff"] = "4000";
        n.args["resonance"] = "0.3";
    }
    else if (type == "eq")
    {
        n.name = nextName ("eq");
        n.args["type"] = "peak";
        n.args["freq"] = "1000";
        n.args["q"] = "0.7";
        n.args["gain"] = "0";
    }
    else if (type == "stage")
    {
        n.name = nextName ("stage");
        n.args["y"] = "tube(x, 1.2)";
    }
    else if (type == "comp")
    {
        n.name = nextName ("comp");
        n.args["threshold"] = "-18";
        n.args["ratio"] = "4";
        n.args["attack"] = "0.01";
        n.args["release"] = "0.12";
    }
    else if (type == "gate")
    {
        n.name = nextName ("gate");
        n.args["threshold"] = "-48";
        n.args["hyst"] = "6";
        n.args["hold"] = "0.03";
        n.args["range"] = "-80";
    }
    else if (type == "noisegate" || type == "ngate")
    {
        n.type = "noisegate";
        n.name = nextName ("ngate");
        n.args["threshold"] = "-48";
        n.args["attack"] = "0.001";
        n.args["release"] = "0.08";
    }
    else if (type == "limit")
    {
        n.name = nextName ("limit");
        n.args["ceiling"] = "-0.3";
        n.args["release"] = "0.08";
    }
    else if (type == "delay")
    {
        n.name = nextName ("delay");
        n.args["time"] = "250";
        n.args["feedback"] = "0.25";
        n.args["mix"] = "0.3";
    }
    else if (type == "reverb")
    {
        n.name = nextName ("reverb");
        n.args["size"] = "0.45";
        n.args["decay"] = "0.4";
        n.args["mix"] = "0.25";
    }
    else if (type == "ir")
    {
        n.name = nextName ("ir");
        n.args["mix"] = "0.45";
        n.args["gain"] = "0";
    }
    else if (type == "ott")
    {
        n.name = nextName ("ott");
        n.args["depth"] = "0.55";
        n.args["time"] = "0.4";
    }
    else if (type == "widen")
    {
        n.name = nextName ("widen");
        n.args["width"] = "0.45";
        n.args["bass"] = "140";
    }
    else if (type == "ms" || type == "ms_decode")
    {
        n.type = "ms";
        n.name = nextName ("ms");
        n.args["mode"] = (type == "ms_decode") ? "decode" : "encode";
    }
    else if (type == "xover")
    {
        n.name = nextName ("xover");
        n.args["f1"] = "200";
        n.args["f2"] = "2500";
    }
    else if (type == "bus")
    {
        n.name = nextName ("bus");
        n.busName = {};
    }
    else if (type == "osc")
    {
        n.name = nextName ("osc");
        n.args["shape"] = "sine";
        n.args["freq"] = "0.4";
        n.args["depth"] = "1";
    }
    else if (type == "env")
    {
        n.name = nextName ("env");
        n.args["attack"] = "0.01";
        n.args["release"] = "0.12";
        n.args["depth"] = "1";
    }
    else if (type == "octaver")
    {
        n.name = nextName ("octaver");
        n.args["sub"] = "0.4";
        n.args["up"] = "0.2";
        n.args["mix"] = "0.5";
    }
    else if (type == "vocoder")
    {
        n.name = nextName ("vocoder");
        n.args["mix"] = "1";
        n.args["bands"] = "16";
    }
    else if (type == "meter")
    {
        n.name = nextName ("meter");
        n.args["mode"] = "loudness";
    }
    else if (type == "sidechain" || type == "sc" || type == "scin")
    {
        n.type = "sidechain";
        n.name = nextName ("sidechain");
        n.args["mix"] = "1";
    }
    else
    {
        n.name = nextName (type);
    }

    int insertAt = (int) document.nodes.size();
    for (int i = 0; i < (int) document.nodes.size(); ++i)
        if (document.nodes[(size_t) i].type == "out")
        {
            insertAt = i;
            break;
        }
    document.nodes.insert (document.nodes.begin() + insertAt, std::move (n));

    if (type == "bus")
    {
        const auto busName = document.nodes[(size_t) insertAt].name;
        dsl::GraphNode send;
        send.type = "send";
        send.name = "send";
        send.busName = busName;
        send.args["in"] = "1";
        const auto sendAt = GraphCanvasComponent::snapPoint ({ snappedAt.x + kNodeW + 32, snappedAt.y });
        send.x = (float) sendAt.x;
        send.y = (float) sendAt.y;
        document.nodes.insert (document.nodes.begin() + insertAt + 1, std::move (send));

        bool hasOut = false;
        for (auto& node : document.nodes)
            if (node.type == "out")
            {
                if (! node.args.count (busName))
                    node.args[busName] = "0.35";
                hasOut = true;
            }
        if (! hasOut)
        {
            dsl::GraphNode out;
            out.type = "out";
            out.name = "out";
            out.args["main"] = "1";
            out.args[busName] = "0.35";
            document.nodes.push_back (std::move (out));
        }
    }

    const juce::String newName = document.nodes[(size_t) insertAt].name;
    const int edgeHit = hitCable (snappedAt.toFloat());
    if (edgeHit >= 0 && edgeHit < (int) cachedEdges.size())
    {
        juce::String spliceErr;
        dsl::connectAudio (document, cachedEdges[(size_t) edgeHit].fromIndex, insertAt, spliceErr);
    }
    else if (type != "bus" && type != "send" && type != "out")
    {
        const int parkIdx = [&]
        {
            for (int i = 0; i < (int) document.nodes.size(); ++i)
                if (document.nodes[(size_t) i].name == newName)
                    return i;
            return insertAt;
        }();
        dsl::parkNode (document, parkIdx);
    }

    applyGraph();
    int idx = -1;
    for (int i = 0; i < (int) document.nodes.size(); ++i)
        if (document.nodes[(size_t) i].name == newName)
            idx = i;
    if (idx >= 0)
    {
        const auto want = GraphCanvasComponent::snapPoint ({
            (int) std::lround (document.nodes[(size_t) idx].x),
            (int) std::lround (document.nodes[(size_t) idx].y) });
        const auto free = firstFreeDesign (idx, want);
        dsl::setPosition (document, idx, (float) free.x, (float) free.y);
        applyGraph();
    }
}

void GraphCanvasComponent::addRouting (const juce::String& kind, juce::Point<int> at)
{
    if (kind == "bus" || kind == "xover")
    {
        addBlock (kind, at);
        return;
    }
    if (! parseOk)
    {
        parseOk = true;
        document = {};
    }

    auto insert = [this, at] (dsl::GraphNode n, int dx, int dy)
    {
        const auto p = GraphCanvasComponent::snapPoint ({ at.x + dx, at.y + dy });
        n.x = (float) p.x;
        n.y = (float) p.y;
        int insertAt = (int) document.nodes.size();
        for (int i = 0; i < (int) document.nodes.size(); ++i)
            if (document.nodes[(size_t) i].type == "out")
            {
                insertAt = i;
                break;
            }
        document.nodes.insert (document.nodes.begin() + insertAt, std::move (n));
    };

    if (kind == "split_ms")
    {
        dsl::GraphNode enc, mid, side, dec;
        enc.type = "ms"; enc.name = nextName ("ms"); enc.args["mode"] = "encode"; enc.busName = "main";
        mid.type = "stage"; mid.name = nextName ("stage"); mid.args["y"] = "x";
        mid.args["channel"] = "mid"; mid.busName = "main";
        side.type = "stage"; side.name = nextName ("stage"); side.args["y"] = "x";
        side.args["channel"] = "side"; side.busName = "main";
        dec.type = "ms"; dec.name = nextName ("ms"); dec.args["mode"] = "decode"; dec.busName = "main";
        insert (std::move (enc), 0, 0);
        insert (std::move (mid), kNodeW + 48, -70);
        insert (std::move (side), kNodeW + 48, 70);
        insert (std::move (dec), (kNodeW + 48) * 2, 0);
    }
    else if (kind == "split_lr")
    {
        dsl::GraphNode left, right;
        left.type = "stage"; left.name = nextName ("stage"); left.args["y"] = "x";
        left.args["channel"] = "left"; left.busName = "main";
        right.type = "stage"; right.name = nextName ("stage"); right.args["y"] = "x";
        right.args["channel"] = "right"; right.busName = "main";
        insert (std::move (left), 0, -70);
        insert (std::move (right), 0, 70);
    }
    applyGraph();
}

void GraphCanvasComponent::updateCableHover (juce::Point<int> paperPos)
{
    hoverDropNode = -99;
    hoverDropJack.clear();
    if (auto* hit = viewAt (paperPos))
    {
        const auto local = hit->getLocalPoint (paper.get(), paperPos);
        bool output = false;
        juce::String jackId;
        if (hit->hitPort (local, output, jackId) && ! output)
        {
            hoverDropNode = hit->nodeIndex;
            hoverDropJack = jackId;
        }
        else if (! hit->isIn() && hit->nodeIndex != cableFrom)
        {
            hoverDropNode = hit->nodeIndex;
            for (const auto& j : hit->jacks)
                if (! j.output)
                {
                    hoverDropJack = j.id;
                    break;
                }
        }
    }
}

juce::String GraphCanvasComponent::nearestRailAt (int y) const
{
    struct Row { juce::String rail; int y; };
    std::vector<Row> rows;
    rows.push_back ({ "main", 160 });
    for (auto& v : nodeViews)
    {
        if (! juce::isPositiveAndBelow (v->nodeIndex, (int) document.nodes.size()))
            continue;
        const auto rail = dsl::visualRail (document.nodes[(size_t) v->nodeIndex]);
        if (rail.isEmpty())
            continue;
        bool have = false;
        for (auto& r : rows)
            if (r.rail == rail)
            {
                r.y = (r.y + v->getY()) / 2;
                have = true;
            }
        if (! have)
            rows.push_back ({ rail, v->getY() });
    }
    int best = 0;
    int bestD = std::abs (rows[0].y - y);
    for (int i = 1; i < (int) rows.size(); ++i)
    {
        const int d = std::abs (rows[(size_t) i].y - y);
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return rows[(size_t) best].rail;
}

void GraphCanvasComponent::commitNodeDrop (int nodeIndex)
{
    if (! juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
        return;
    const auto name = document.nodes[(size_t) nodeIndex].name;
    const auto rail = dropRail.isNotEmpty() ? dropRail : nearestRailAt (
        viewFor (nodeIndex) != nullptr ? viewFor (nodeIndex)->getY() : 160);

    if (rail == "mid" || rail == "side" || rail == "left" || rail == "right")
    {
        dsl::assignNodeToBus (document, nodeIndex, "main");
        int idx = -1;
        for (int i = 0; i < (int) document.nodes.size(); ++i)
            if (document.nodes[(size_t) i].name == name)
                idx = i;
        if (idx >= 0)
            dsl::setNodeArg (document, idx, "channel", rail);
    }
    else if (rail != "mod" && rail != "main")
    {
        dsl::assignNodeToBus (document, nodeIndex, rail);
        int idx = -1;
        for (int i = 0; i < (int) document.nodes.size(); ++i)
            if (document.nodes[(size_t) i].name == name)
                idx = i;
        if (idx >= 0)
            dsl::setNodeArg (document, idx, "channel", {});
    }
    else if (rail == "main")
    {
        dsl::assignNodeToBus (document, nodeIndex, "main");
        int idx = -1;
        for (int i = 0; i < (int) document.nodes.size(); ++i)
            if (document.nodes[(size_t) i].name == name)
                idx = i;
        if (idx >= 0)
            dsl::setNodeArg (document, idx, "channel", {});
    }

    int idx = -1;
    for (int i = 0; i < (int) document.nodes.size(); ++i)
        if (document.nodes[(size_t) i].name == name)
            idx = i;
    if (idx < 0)
        return;

    const int dropX = viewFor (idx) != nullptr ? viewFor (idx)->getX() : 0;
    int after = -1;
    for (int i = 0; i < (int) document.nodes.size(); ++i)
    {
        if (i == idx)
            continue;
        if (dsl::visualRail (document.nodes[(size_t) i]) != dsl::visualRail (document.nodes[(size_t) idx]))
            continue;
        auto* v = viewFor (i);
        if (v != nullptr && v->getX() < dropX)
            after = i;
    }
    if (after >= 0)
        dsl::moveNode (document, idx, after + (after < idx ? 1 : 0));
    applyGraph();
}

void GraphCanvasComponent::setZoom (float z)
{
    const float next = juce::jlimit (kZoomMin, kZoomMax, z);
    if (std::abs (next - zoom) < 0.001f)
        return;
    zoom = next;
    markEdgesDirty();
    rebuildViews();
}

void GraphCanvasComponent::mouseWheelMove (const juce::MouseEvent& e,
                                           const juce::MouseWheelDetails& wheel)
{
    if (! (e.mods.isCtrlDown() || e.mods.isCommandDown()))
    {
        if (auto* p = getParentComponent())
            p->mouseWheelMove (e.getEventRelativeTo (p), wheel);
        return;
    }
    const float factor = wheel.deltaY > 0.f ? 1.12f : 0.89f;
    setZoom (zoom * (wheel.isReversed ? 1.f / factor : factor));
}

float GraphCanvasComponent::mappedKnobValue (int knobIndex) const
{
    if (knobIndex < 0 || knobIndex >= Config::kNumUserParams)
        return 0.f;
    float n01 = 0.f;
    if (auto* p = processor.apvts.getRawParameterValue (EffectParameters::userParams[knobIndex]))
        n01 = p->load();
    const auto letter = juce::String::charToString ((juce::juce_wchar) ('a' + knobIndex));
    for (const auto& pd : processor.getParamInfo())
    {
        if (! pd.alias.equalsIgnoreCase (letter))
            continue;
        if (pd.isNote && ! pd.noteLabels.empty())
        {
            const int idx = juce::jlimit (0, (int) pd.noteLabels.size() - 1,
                                          (int) std::lround (n01 * (float) (pd.noteLabels.size() - 1)));
            juce::ignoreUnused (idx);
        }
        return pd.min + n01 * (pd.max - pd.min);
    }
    return n01;
}

juce::String GraphCanvasComponent::formatLiveKnob (float v)
{
    if (! std::isfinite (v))
        return "--";
    if (std::abs (v) >= 100.f)
        return juce::String (v, 0);
    if (std::abs (v - std::round (v)) < 0.005f && std::abs (v) >= 1.f)
        return juce::String ((int) std::lround (v));
    return juce::String (v, 2);
}

int GraphCanvasComponent::alignToGrid (int v) noexcept
{
    if (v <= 0)
        return kGrid;
    return ((v + kGrid - 1) / kGrid) * kGrid;
}

int GraphCanvasComponent::jackLocalY (int slot) noexcept
{
    // Title occupies row 0. Jack slot i sits on the grid line of row 1+i.
    return (kTitleRows + juce::jmax (0, slot)) * kJackPitch + kJackPad;
}

int GraphCanvasComponent::chipHeight (int nInJacks, int nOutJacks, int nArgs, int nBinds,
                                      bool expanded) noexcept
{
    juce::ignoreUnused (nBinds);
    const int rows = juce::jmax (nInJacks, nOutJacks, 1);
    int units = kTitleRows + rows + kBottomRows;
    if (expanded)
        units += juce::jmax (0, nArgs);
    return units * kGrid;
}

juce::Path GraphCanvasComponent::makeOrthoCable (juce::Point<float> from, juce::Point<float> to)
{
    juce::Path p;
    const float stub = (float) kGrid;
    const float ax = from.x + stub;
    const float bx = to.x - stub;
    p.startNewSubPath (from);
    p.lineTo (ax, from.y);
    if (std::abs (from.y - to.y) < 0.6f && bx >= ax - 0.5f)
    {
        p.lineTo (bx, to.y);
    }
    else if (bx >= ax + 8.f)
    {
        const float mx = 0.5f * (ax + bx);
        p.lineTo (mx, from.y);
        p.lineTo (mx, to.y);
        p.lineTo (bx, to.y);
    }
    else
    {
        const float midY = 0.5f * (from.y + to.y);
        p.lineTo (ax, midY);
        p.lineTo (bx, midY);
        p.lineTo (bx, to.y);
    }
    p.lineTo (to);
    return p;
}

juce::Path GraphCanvasComponent::makeKnobCable (juce::Point<float> from, juce::Point<float> to)
{
    return makeOrthoCable (from, to);
}

juce::Rectangle<int> GraphCanvasComponent::foldChevronRect (int cardW, int cardH,
                                                            bool hasSidechain, float zoom) noexcept
{
    juce::ignoreUnused (cardH);
    const float z = zoom > 0.2f ? zoom : 1.f;
    const int titleH = juce::jmax (2, (int) std::lround ((float) (kTitleRows * kGrid) * z));
    auto title = juce::Rectangle<int> (8, 1, juce::jmax (16, cardW - 16), titleH - 2);
    if (hasSidechain)
        title.removeFromRight (22);
    return title.removeFromRight (14);
}

juce::Rectangle<int> GraphCanvasComponent::foldHitRect (int cardW, int cardH,
                                                        bool hasSidechain, float zoom) noexcept
{
    juce::ignoreUnused (cardH, zoom);
    return foldChevronRect (cardW, cardH, hasSidechain, zoom).expanded (6, 4);
}

bool GraphCanvasComponent::isNodeExpanded (int nodeIndex) const
{
    if (! juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
        return false;
    return expandedNames.contains (document.nodes[(size_t) nodeIndex].name);
}

float GraphCanvasComponent::loudnessToCableLevel (float db) noexcept
{
    if (! std::isfinite (db) || db < -80.f)
        return 0.f;
    return juce::jlimit (0.f, 1.f,
        juce::Decibels::decibelsToGain (db + 18.f) * 0.35f);
}

void GraphCanvasComponent::refreshCableMeters()
{
    const float raw = loudnessToCableLevel (processor.getLoudnessDb());
    inLevel = inLevel * 0.72f + raw * 0.28f;
    outLevel = inLevel;
    pullCableWaves();
}

void GraphCanvasComponent::pullCableWaves()
{
    if (scopeCap.getNumChannels() < 1
        || scopeCap.getNumSamples() != Config::kWaveformDisplaySamples)
        scopeCap.setSize (Config::kMaxChannels, Config::kWaveformDisplaySamples, false, false, true);

    auto fillFromScope = [this] (bool input, std::array<float, kTapN>& dest)
    {
        if (input)
            processor.getInputWaveform (scopeCap);
        else
            processor.getOutputWaveform (scopeCap);

        const int n = scopeCap.getNumSamples();
        const int chs = scopeCap.getNumChannels();
        if (n <= 0 || chs <= 0)
            return;

        if ((int) scopeMono.size() != n)
            scopeMono.assign ((size_t) n, 0.f);

        const float* L = scopeCap.getReadPointer (0);
        const float* R = chs > 1 ? scopeCap.getReadPointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i];
            const float r = R != nullptr ? R[i] : l;
            const float al = std::abs (l);
            const float ar = std::abs (r);
            float m = 0.f;
            if (al < 1.0e-6f && ar < 1.0e-6f)
                m = 0.f;
            else if (al >= ar * 4.0f)
                m = l;
            else if (ar >= al * 4.0f)
                m = r;
            else
                m = 0.5f * (l + r);
            scopeMono[(size_t) i] = m;
        }

        const float step = (float) n / (float) kTapN;
        for (int i = 0; i < kTapN; ++i)
        {
            const int idx = juce::jmin (n - 1, (int) (i * step));
            dest[(size_t) i] = scopeMono[(size_t) idx];
        }
    };

    fillFromScope (true, inWave);
    fillFromScope (false, outWave);

    nodeWaves.clear();
    for (const auto& n : document.nodes)
    {
        if (n.name.isEmpty())
            continue;
        std::array<float, kTapN> w {};
        if (n.type.startsWithIgnoreCase ("osc")
            && processor.copyLfoViz (n.name, w.data(), kTapN))
            nodeWaves[n.name] = w;
        else if (processor.copyCircuitTap (n.name, w.data(), kTapN))
            nodeWaves[n.name] = w;
    }
}

const float* GraphCanvasComponent::waveForEdge (int fromIndex) const
{
    if (fromIndex == kInIndex)
        return inWave.data();
    if (! juce::isPositiveAndBelow (fromIndex, (int) document.nodes.size()))
        return outWave.data();
    const auto name = document.nodes[(size_t) fromIndex].name;
    const auto it = nodeWaves.find (name);
    if (it != nodeWaves.end()
        && cableTapEnergy (it->second.data(), kTapN) >= kCableBeadGate)
        return it->second.data();
    return inWave.data();
}

void GraphCanvasComponent::removeSelected()
{
    if (! parseOk || ! juce::isPositiveAndBelow (selected, (int) document.nodes.size()))
        return;
    const auto type = document.nodes[(size_t) selected].type;
    if (type == "out" || type == "bus")
        return;
    document.nodes.erase (document.nodes.begin() + selected);
    selected = -99;
    applyGraph();
}

void GraphCanvasComponent::paint (juce::Graphics& g)
{
    if (! isTimerRunning())
        startTimerHz (30);
    g.fillAll (NeuroKoreLookAndFeel::canvas());
}

void GraphCanvasComponent::resized()
{
    viewport.setBounds (getLocalBounds());
    if (pendingAutoArrange && parseOk
        && viewport.getWidth() > 0 && viewport.getHeight() > 0)
    {
        pendingAutoArrange = false;
        autoLayout();
        rebuildViews();
        return;
    }
    layoutPaper();
}

bool GraphCanvasComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        bool any = false;
        for (auto& v : nodeViews)
            if (v->editing)
            {
                v->cancelEdit();
                any = true;
            }
        return any;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        removeSelected();
        return true;
    }
    return false;
}

void GraphCanvasComponent::visibilityChanged()
{
    // Never stop — WaveformDisplay keeps its timer for the same reason.
    startTimerHz (30);
}

void GraphCanvasComponent::timerCallback()
{
    if (getTimerInterval() != (1000 / 30))
        startTimerHz (30);

    const float raw = loudnessToCableLevel (processor.getLoudnessDb());
    inLevel = inLevel * 0.72f + raw * 0.28f;
    outLevel = inLevel;
    bool haveMod = false;
    for (const auto& n : document.nodes)
        if (dsl::isModulator (n))
        {
            haveMod = true;
            break;
        }
    const bool live = inLevel > 0.018f;
    if (live || haveMod)
    {
        cablePhase += 0.045f + (live ? inLevel * 0.18f : 0.07f);
        if (cablePhase > 64.f)
            cablePhase -= 64.f;
        pullCableWaves();
        const float fps = 30.f;
        for (const auto& n : document.nodes)
        {
            if (! n.type.startsWithIgnoreCase ("osc"))
                continue;
            float hz = 0.f;
            processor.copyLfoHz (n.name, hz);
            auto& ph = lfoPhase[n.name];
            ph += lfoChaseStep (hz, fps);
            if (ph > 100000.f)
                ph -= 100000.f;
            const auto it = nodeWaves.find (n.name);
            const float peak = (it != nodeWaves.end())
                ? lfoChaseBrightness (it->second.data(), kTapN) : 0.f;
            auto& amp = lfoAmp[n.name];
            amp = amp * 0.72f + peak * 0.28f;
        }
    }

    int hover = -1;
    if (knobForIndex)
        for (int i = 0; i < Config::kNumUserParams; ++i)
            if (auto* k = knobForIndex (i))
            {
                if (auto* pc = dynamic_cast<ui::ParameterComponent*> (k))
                {
                    if (pc->isActivelyUsed())
                        hover = i;
                }
                else if (k->isMouseOverOrDragging() || k->isMouseButtonDown())
                    hover = i;
            }
    int node = -99;
    for (auto& v : nodeViews)
        if (v->isMouseOver() || v->editing)
            node = v->nodeIndex;
    const bool hoverChanged = (hover != hoverKnob) || (node != hoverNode);
    hoverKnob = hover;
    hoverNode = node;
    repaintPaper();
    if (hoverChanged)
        if (auto* ed = getParentComponent())
            ed->repaint();
}

juce::Colour GraphCanvasComponent::cableTraceColour (bool hot, float alpha) noexcept
{
    return (hot ? NeuroKoreLookAndFeel::ink() : NeuroKoreLookAndFeel::inkMuted())
        .withAlpha (alpha);
}

float GraphCanvasComponent::cableTapEnergy (const float* wave, int n) noexcept
{
    if (wave == nullptr || n < 1)
        return 0.f;
    float peak = 0.f;
    for (int i = 0; i < n; ++i)
        peak = juce::jmax (peak, std::abs (wave[i]));
    return juce::jlimit (0.f, 1.f, peak * 2.5f);
}

float GraphCanvasComponent::lfoChaseHz (float hz) noexcept
{
    if (! std::isfinite (hz) || hz <= 0.f)
        return 0.f;
    return juce::jlimit (kLfoChaseHzMin, kLfoChaseHzMax, hz);
}

float GraphCanvasComponent::lfoChaseStep (float hz, float fps) noexcept
{
    const float rate = lfoChaseHz (hz);
    if (rate <= 0.f || ! std::isfinite (fps) || fps < 1.f)
        return 0.f;
    return (kLfoLedPxPerSec * rate / 2.f) / fps;
}

float GraphCanvasComponent::lfoPulseAlpha (float amp) noexcept
{
    if (! std::isfinite (amp) || amp <= 0.f)
        return 0.f;
    return juce::jlimit (0.f, 0.44f, amp * 0.44f);
}

float GraphCanvasComponent::lfoLedPulse (float hz, float timeSec) noexcept
{
    const float rate = lfoChaseHz (hz);
    if (rate <= 0.f || ! std::isfinite (timeSec))
        return 1.f;
    return 0.38f + 0.62f * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * rate * timeSec));
}

float GraphCanvasComponent::lfoChaseAlong (float phasePx, float pathLength) noexcept
{
    if (! (pathLength > 1.f) || ! std::isfinite (phasePx))
        return 0.f;
    float u = phasePx / pathLength;
    u -= std::floor (u);
    return u;
}

float GraphCanvasComponent::lfoChaseBrightness (const float* wave, int n) noexcept
{
    if (wave == nullptr || n < 1)
        return 0.f;
    float peak = 0.f;
    for (int i = 0; i < n; ++i)
        peak = juce::jmax (peak, std::abs (wave[i]));
    return juce::jlimit (0.f, 1.f, peak);
}

void GraphCanvasComponent::drawLiveCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                                         float level, bool mix, bool hot,
                                         const float* wave, int waveN, bool forceWave) const
{
    drawLiveCable (g, makeCable (a, b), level, mix, hot, wave, waveN, forceWave);
}

void GraphCanvasComponent::drawLiveCable (juce::Graphics& g, const juce::Path& path,
                                         float level, bool mix, bool hot,
                                         const float* wave, int waveN, bool forceWave) const
{
    if (path.isEmpty())
        return;
    const float idle = mix ? 0.28f : 0.22f;
    const float alpha = hot ? 0.62f
                            : (idle + 0.14f * juce::jlimit (0.f, 1.f, level));
    g.setColour (cableTraceColour (hot, alpha));
    g.strokePath (path, juce::PathStrokeType (mix ? (hot ? 2.6f : 2.2f) : (hot ? 1.7f : 1.25f),
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    {
        juce::Point<float> s, e;
        pathEnds (path, s, e);
        const auto plug = cableTraceColour (hot, juce::jmin (1.f, alpha + 0.25f));
        if (s.getDistanceFrom (e) > 6.f)
        {
            drawCablePlug (g, s, true, plug);
            drawCablePlug (g, e, false, plug);
        }
    }

    if (! cableBeadsVisible (level) && ! forceWave)
        return;

    const bool wantWave = forceWave || UiSettings::get().cableWaveform();
    if (! wantWave)
    {
        juce::PathFlatteningIterator it (path);
        float acc = 0.f;
        const float spacing = 28.f;
        float next = std::fmod (cablePhase * spacing * 0.55f, spacing);
        const float pr = hot ? 2.0f : 1.3f;
        g.setColour (NeuroKoreLookAndFeel::ink()
                         .withAlpha (hot ? 0.40f : 0.16f + level * 0.22f));
        bool have = false;
        juce::Point<float> prev;
        while (it.next())
        {
            const juce::Point<float> p0 (it.x1, it.y1);
            const juce::Point<float> p1 (it.x2, it.y2);
            if (! have)
            {
                prev = p0;
                have = true;
            }
            const float seg = p0.getDistanceFrom (p1);
            float t = 0.f;
            while (acc + (seg - t) >= next)
            {
                const float remain = next - acc;
                const float u = seg > 1.0e-4f ? juce::jlimit (0.f, 1.f, (t + remain) / seg) : 0.f;
                const auto pt = p0 + (p1 - p0) * u;
                g.fillEllipse (pt.x - pr, pt.y - pr, pr * 2.f, pr * 2.f);
                next += spacing;
            }
            acc += seg;
            t = seg;
            prev = p1;
        }
        juce::ignoreUnused (prev);
        return;
    }

    if (wave == nullptr || waveN < 4)
        return;

    float peak = 1.0e-6f;
    for (int i = 0; i < waveN; ++i)
        peak = juce::jmax (peak, std::abs (wave[i]));
    float length = 0.f;
    {
        juce::PathFlatteningIterator it (path);
        while (it.next())
            length += juce::Point<float> (it.x1, it.y1)
                          .getDistanceFrom ({ it.x2, it.y2 });
    }
    if (length < 4.f)
        return;

    const float amp = (hot ? 11.f : 7.f) * juce::jlimit (0.25f, 1.f, 0.2f + level * 1.4f)
                    / juce::jmax (0.08f, peak);
    juce::Path wavePath;
    bool started = false;
    juce::PathFlatteningIterator it2 (path);
    float acc = 0.f;
    while (it2.next())
    {
        const juce::Point<float> p0 (it2.x1, it2.y1), p1 (it2.x2, it2.y2);
        const float seg = p0.getDistanceFrom (p1);
        if (seg < 1.0e-4f)
            continue;
        const auto dir = (p1 - p0) / seg;
        const juce::Point<float> nrm (-dir.y, dir.x);
        const int steps = juce::jmax (1, (int) (seg / 2.f));
        for (int s = 0; s <= steps; ++s)
        {
            const float u = (float) s / (float) steps;
            const auto pt = p0 + (p1 - p0) * u;
            const float along = juce::jlimit (0.f, 1.f, (acc + seg * u) / length);
            const float idx = along * (float) (waveN - 1);
            const int i0 = (int) idx;
            const int i1 = juce::jmin (waveN - 1, i0 + 1);
            const float f = idx - (float) i0;
            const float smp = wave[i0] * (1.f - f) + wave[i1] * f;
            const auto wp = pt + nrm * (smp * amp);
            if (! started) { wavePath.startNewSubPath (wp); started = true; }
            else            wavePath.lineTo (wp);
        }
        acc += seg;
    }
    g.setColour (cableTraceColour (hot, hot ? 0.78f : 0.42f + level * 0.28f));
    g.strokePath (wavePath, juce::PathStrokeType (hot ? 1.6f : 1.15f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
}

void GraphCanvasComponent::drawModLauflicht (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                                            float hz, float amp, bool hot, float phase) const
{
    drawModLauflicht (g, makeCable (a, b), hz, amp, hot, phase);
}

void GraphCanvasComponent::drawModLauflicht (juce::Graphics& g, const juce::Path& path,
                                            float hz, float amp, bool hot, float phase) const
{
    if (path.isEmpty())
        return;
    g.setColour (NeuroKoreLookAndFeel::inkMuted().withAlpha (hot ? 0.42f : 0.22f));
    g.strokePath (path, juce::PathStrokeType (hot ? 1.35f : 1.05f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    {
        juce::Point<float> s, e;
        pathEnds (path, s, e);
        const auto plug = NeuroKoreLookAndFeel::ink().withAlpha (hot ? 0.72f : 0.45f);
        if (s.getDistanceFrom (e) > 6.f)
        {
            drawCablePlug (g, s, true, plug);
            drawCablePlug (g, e, false, plug);
        }
    }

    if (lfoChaseHz (hz) <= 0.f)
        return;

    struct Seg { juce::Point<float> p0, p1; float len; };
    std::vector<Seg> segs;
    float length = 0.f;
    {
        juce::PathFlatteningIterator it (path);
        while (it.next())
        {
            const juce::Point<float> p0 (it.x1, it.y1), p1 (it.x2, it.y2);
            const float len = p0.getDistanceFrom (p1);
            if (len < 1.0e-4f)
                continue;
            segs.push_back ({ p0, p1, len });
            length += len;
        }
    }
    if (length < 8.f || segs.empty())
        return;

    auto alongAt = [&] (float dist) -> juce::Point<float>
    {
        float d = dist;
        while (d < 0.f) d += length;
        while (d >= length) d -= length;
        float w = 0.f;
        for (const auto& s : segs)
        {
            if (w + s.len >= d)
            {
                const float u = (d - w) / s.len;
                return s.p0 + (s.p1 - s.p0) * u;
            }
            w += s.len;
        }
        return segs.back().p1;
    };

    const float a01 = juce::jlimit (0.f, 1.f, amp);
    const float pulse = lfoPulseAlpha (a01);
    if (pulse < 0.01f)
        return;

    const float spacing = juce::jmax (28.f, length * 0.28f);
    const int nPulse = juce::jlimit (1, 6, (int) std::floor (length / spacing) + 1);
    const float head = lfoChaseAlong (phase, length) * length;
    const auto ink = NeuroKoreLookAndFeel::ink().interpolatedWith (
        NeuroKoreLookAndFeel::accent(), 0.35f);

    for (int i = 0; i < nPulse; ++i)
    {
        const float d = head - (float) i * spacing;
        const auto p = alongAt (d);
        const float fade = 1.f - (float) i / (float) juce::jmax (1, nPulse);
        const float al = pulse * fade * 0.55f;
        const float r = 1.4f + a01 * 1.1f;
        g.setColour (ink.withAlpha (al));
        g.fillEllipse (p.x - r, p.y - r, r * 2.f, r * 2.f);
    }
}

juce::Point<float> GraphCanvasComponent::knobJackOnNode (int nodeIndex, int knobIndex) const
{
    auto* v = viewFor (nodeIndex);
    if (v == nullptr || ! juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
        return {};
    const auto letter = juce::String::charToString ((juce::juce_wchar) ('a' + knobIndex));
    return v->jackCentreById ("knob:" + letter, false);
}

void GraphCanvasComponent::paintKnobCables (juce::Graphics& g, juce::Component& space, float alphaMul) const
{
    juce::ignoreUnused (g, space, alphaMul);
    return;
    if (! parseOk)
        return;

    const auto accent = NeuroKoreLookAndFeel::accent();
    const float mul = juce::jlimit (0.05f, 1.f, alphaMul);

    // One shared vertical rail at the left edge of the circuit paper.
    // Every knob walks east to that rail, then north/south, then east to the jack.
    // Per-net midpoints were the spaghetti beside the knob column.
    float railX = 0.f;
    if (paper != nullptr)
        railX = (float) space.getLocalPoint (paper.get(), juce::Point<int> ()).x;
    else
        railX = (float) space.getWidth() * 0.28f;

    for (auto& nv : nodeViews)
    {
        if (! juce::isPositiveAndBelow (nv->nodeIndex, (int) document.nodes.size()))
            continue;
        for (const auto& b : nv->binds)
        {
            if (knobForIndex == nullptr)
                continue;
            auto* knob = knobForIndex (b.knobIndex);
            if (knob == nullptr || ! knob->isShowing())
                continue;

            const auto from = space.getLocalPoint (knob,
                juce::Point<int> (knob->getWidth(), knob->getHeight() / 2)).toFloat();
            const auto to = space.getLocalPoint (nv.get(),
                knobJackOnNode (nv->nodeIndex, b.knobIndex).roundToInt()).toFloat();

            const bool nodeHot = nv->isMouseOver() || selected == nv->nodeIndex
                              || hoverNode == nv->nodeIndex;
            const bool hot = (hoverKnob == b.knobIndex) || nodeHot;

            const float rx = (railX > from.x + 2.f) ? railX
                                                    : 0.5f * (from.x + to.x);

            juce::Path path;
            path.startNewSubPath (from);
            if (std::abs (from.y - to.y) < 0.75f && to.x >= from.x)
            {
                path.lineTo (to);
            }
            else
            {
                path.lineTo (rx, from.y);
                path.lineTo (rx, to.y);
                path.lineTo (to);
            }

            g.setColour (accent.withAlpha ((hot ? 0.70f : 0.50f) * mul));
            g.strokePath (path, juce::PathStrokeType (hot ? 2.0f : 1.2f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
            const auto plug = accent.withAlpha ((hot ? 0.88f : 0.62f) * mul);
            drawCablePlug (g, to, false, plug);
        }
    }
}


