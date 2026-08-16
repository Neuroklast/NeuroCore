#include "GraphCanvasComponent.h"
#include "PluginLookAndFeel.h"
#include "custom/ParameterComponent.h"
#include "../core/Config.h"
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
    return n.name;
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
    if (k == "feedback") return "Feedback";
    if (k == "mix") return "Mix";
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
    juce::Path p;
    const float dx = juce::jmax (40.f, std::abs (b.x - a.x) * 0.45f);
    p.startNewSubPath (a);
    p.cubicTo ({ a.x + dx, a.y }, { b.x - dx, b.y }, b);
    return p;
}

juce::Path makeSoftCable (juce::Point<float> a, juce::Point<float> b)
{
    juce::Path p;
    const float dx = juce::jlimit (28.f, 72.f, std::abs (b.x - a.x) * 0.18f);
    p.startNewSubPath (a);
    p.cubicTo ({ a.x + dx, a.y }, { b.x - dx, b.y }, b);
    return p;
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
        setRepaintsOnMouseActivity (true);
        if (juce::isPositiveAndBelow (idx, (int) o.document.nodes.size()))
            binds = dsl::knobBindings (o.document.nodes[(size_t) idx]);
        jacks = o.jacksOf (idx);
        applyCardSize();
        if (const auto* n = realNode())
        {
            const auto tip = compactSummary (*n);
            if (tip.isNotEmpty())
                setTooltip (n->name + "  " + tip);
        }
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

    void applyCardSize()
    {
        if (editing)
        {
            setSize (220, 30 + editKeys.size() * 40 + 30);
            return;
        }
        int nIn = 0, nOut = 0;
        for (const auto& j : jacks)
            (j.output ? nOut : nIn) += 1;
        const int rows = juce::jmax (nIn, nOut, 1);
        const int minH = (isIn() || isOut()) ? GraphCanvasComponent::kIoHeight : 44;
        const int h = juce::jmax (minH, 14 + rows * 16);
        const int w = (isIn() || isOut()) ? GraphCanvasComponent::kIoWidth
                                          : GraphCanvasComponent::kCardWidth;
        setSize (w, binds.empty() || isIn() || isOut() ? h
                       : juce::jmax (h, GraphCanvasComponent::kCardHeight));
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
        if (isIn() || isOut() || ! juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            return;
        owner.commitOpenEdits();
        const auto& n = owner.document.nodes[(size_t) nodeIndex];
        if (n.type == "bus")
            return;
        clearEditors();
        editing = true;
        owner.selected = nodeIndex;
        editKeys = onlyKey.isNotEmpty() ? juce::StringArray { onlyKey }
                                        : dsl::editableArgKeys (n);
        if (n.type.startsWithIgnoreCase ("stage") && onlyKey.isEmpty())
            editKeys = juce::StringArray { "y" };
        else if (editKeys.size() > 4)
            editKeys.removeRange (4, editKeys.size() - 4);

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

    juce::Point<float> jackCentre (int jackIndex) const
    {
        if (! juce::isPositiveAndBelow (jackIndex, (int) jacks.size()))
            return { (float) getWidth() * 0.5f, (float) getHeight() * 0.5f };
        const bool output = jacks[(size_t) jackIndex].output;
        int slot = 0, count = 0;
        for (int i = 0; i < (int) jacks.size(); ++i)
        {
            if (jacks[(size_t) i].output != output)
                continue;
            if (i == jackIndex)
                slot = count;
            ++count;
        }
        const float pitch = 16.f;
        const float span = (float) juce::jmax (0, count - 1) * pitch;
        const float y = (float) getHeight() * 0.5f - span * 0.5f + (float) slot * pitch;
        return { output ? (float) getWidth() - 2.f : 2.f, y };
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
        return { outputIfEmpty ? (float) getWidth() - 2.f : 2.f,
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
            g.drawHorizontalLine ((int) (r.getY() + 22.f), r.getX() + 12.f, r.getRight() - 12.f);
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
            g.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
            auto titleArea = getLocalBounds().reduced (10, 2);
            if (jacks.size() <= 1)
                g.drawText (title, getLocalBounds(), juce::Justification::centred, false);
            else
                g.drawText (title, titleArea.removeFromTop (12), juce::Justification::centred, false);
        }
        else
        {
            auto body = r.reduced (16.f, 5.f);
            auto titleRow = body.removeFromTop (16.f);
            if (! binds.empty())
            {
                auto chipRow = titleRow.removeFromRight ((int) binds.size() * 18);
                g.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
                for (const auto& b : binds)
                {
                    auto chip = chipRow.removeFromLeft (16.f).reduced (1.f);
                    const bool hot = (owner.hoverKnob == b.knobIndex);
                    g.setColour (hot ? accent : accent.withAlpha (0.8f));
                    g.fillRect (chip);
                    g.setColour (NeuroKoreLookAndFeel::ink());
                    g.drawText (juce::String::charToString ((juce::juce_wchar) ('a' + b.knobIndex)),
                                chip.toNearestInt(), juce::Justification::centred, false);
                    chipRow.removeFromLeft (2.f);
                }
            }
            g.setColour (NeuroKoreLookAndFeel::ink());
            g.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
            g.drawText (title, titleRow.toNearestInt(), juce::Justification::centredLeft, true);
            if (node != nullptr)
            {
                const auto sub = compactSummary (*node);
                if (sub.isNotEmpty())
                {
                    g.setColour (NeuroKoreLookAndFeel::inkMuted());
                    g.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
                    g.drawFittedText (sub, body.toNearestInt(),
                                      juce::Justification::centredLeft, 2);
                }
            }
        }

        auto drawPort = [&] (juce::Point<float> c, bool hot, bool knobish, bool output)
        {
            const float pinW = 6.f, pinH = 4.f;
            auto pad = juce::Rectangle<float> (output ? c.x - 1.f : c.x - pinW + 1.f,
                                               c.y - pinH * 0.5f, pinW, pinH);
            g.setColour (hot || knobish ? accent.withAlpha (hot ? 0.95f : 0.55f)
                                        : NeuroKoreLookAndFeel::inkMuted().withAlpha (0.55f));
            g.fillRect (pad);
            g.setColour (NeuroKoreLookAndFeel::canvas());
            g.fillEllipse (c.x - 3.2f, c.y - 3.2f, 6.4f, 6.4f);
            g.setColour (hot ? accent : NeuroKoreLookAndFeel::accent().withAlpha (0.7f));
            g.drawEllipse (c.x - 3.2f, c.y - 3.2f, 6.4f, 6.4f, 1.1f);
        };

        g.setFont (NeuroKoreLookAndFeel::monoFont (9.f));
        for (int i = 0; i < (int) jacks.size(); ++i)
        {
            const auto& j = jacks[(size_t) i];
            const auto c = jackCentre (i);
            const bool knobish = (j.kind == "knob" || j.kind == "mod");
            const bool dropHot = owner.hoverDropNode == nodeIndex && owner.hoverDropJack == j.id;
            drawPort (c, isMouseOver() || dropHot, knobish, j.output);
            if (dropHot)
            {
                g.setColour (accent);
                g.fillEllipse (c.x - 5.f, c.y - 5.f, 10.f, 10.f);
                g.setColour (NeuroKoreLookAndFeel::ink());
                g.drawEllipse (c.x - 6.f, c.y - 6.f, 12.f, 12.f, 1.6f);
            }
            const bool labelSide = (j.kind == "mix" || j.kind == "sc" || j.kind == "mod"
                                 || j.id == "mid" || j.id == "side" || j.id == "low"
                                 || j.id == "high" || j.id == "left" || j.id == "right");
            if (! labelSide || j.label.isEmpty())
                continue;
            auto lr = juce::Rectangle<float> (j.output ? c.x - 36.f : c.x + 7.f,
                                              c.y - 6.f, 30.f, 12.f);
            g.setColour (NeuroKoreLookAndFeel::inkMuted());
            g.drawText (j.label, lr.toNearestInt(),
                        j.output ? juce::Justification::centredRight
                                 : juce::Justification::centredLeft, false);
        }
    }

    void resized() override
    {
        if (editing)
            layoutEditors();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.selected = nodeIndex;
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
        if (e.getNumberOfClicks() >= 2
            && juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
        {
            const auto& n = owner.document.nodes[(size_t) nodeIndex];
            if (n.type.startsWithIgnoreCase ("ir") && owner.onOpenIr)
                owner.onOpenIr (n.name);
            else
                startEdit();
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
        owner.dropRail = owner.nearestRailAt (getY() + getHeight() / 2);
        owner.dropInsertBefore = -99;
        if (e.getDistanceFromDragStart() < kDragSlop && e.getPosition() == dragAnchor)
            return;
        auto p = getPosition() + (e.getPosition() - dragAnchor);
        p = GraphCanvasComponent::snapPoint (p);
        p.x = juce::jmax (kGrid, p.x);
        p.y = juce::jmax (kGrid, p.y);
        setTopLeftPosition (p);
        if (isIn())
            owner.inPos = p.toFloat();
        else if (isOut() && ! juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            owner.outPos = p.toFloat();
        else if (juce::isPositiveAndBelow (nodeIndex, (int) owner.document.nodes.size()))
            dsl::setPosition (owner.document, nodeIndex, (float) p.x, (float) p.y);
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
            owner.commitNodeDrop (nodeIndex);
        moving = false;
        owner.movingNode = -99;
        owner.dropRail.clear();
        owner.dropInsertBefore = -99;
        owner.repaintPaper();
    }

private:
    GraphCanvasComponent& owner;
};

class GraphCanvasComponent::Paper : public juce::Component
{
public:
    explicit Paper (GraphCanvasComponent& o) : owner (o)
    {
        setOpaque (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (NeuroKoreLookAndFeel::canvas());
        const auto rose = NeuroKoreLookAndFeel::accent();
        const int step = kGrid * 2;
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
                                                            : "Graph: assemble the parts.\nScript: hack the construct.",
                              getLocalBounds().reduced (40), juce::Justification::centred, 4);
            return;
        }

        if (owner.edgesDirty)
        {
            owner.cachedEdges = dsl::visualAudioEdges (owner.document);
            if (owner.outIndex() == kOutIndex)
            {
                int lastMain = -99;
                for (const auto& e : owner.cachedEdges)
                    if (e.kind == "audio" && e.toIndex >= 0)
                        lastMain = e.toIndex;
                if (lastMain == -99)
                {
                    for (int i = 0; i < (int) owner.document.nodes.size(); ++i)
                    {
                        const auto& n = owner.document.nodes[(size_t) i];
                        if (n.type != "bus" && ! dsl::isModulator (n)
                            && (n.busName.isEmpty() || n.busName == "main"))
                            lastMain = i;
                    }
                }
                owner.cachedEdges.push_back ({ lastMain != -99 ? lastMain : kInIndex,
                                               kOutIndex, "audio", "out", "main" });
            }
            owner.edgesDirty = false;
        }

        for (const auto& e : owner.cachedEdges)
        {
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
            owner.drawLiveCable (g, a, b, lvl, mix, hot);
        }

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
                auto path = makeCable (a, b);
                g.setColour (NeuroKoreLookAndFeel::inkMuted()
                                 .withAlpha (srcHot || destHot ? 0.55f : 0.18f));
                float dash[] = { 5.f, 4.f };
                juce::Path dest;
                juce::PathStrokeType (1.0f).createDashedStroke (dest, path, dash, 2);
                g.strokePath (dest, juce::PathStrokeType (1.0f));
            }
        }

        if (owner.cableFrom != -99)
        {
            const auto a = owner.portCentre (owner.cableFrom, owner.cableFromJack, true);
            auto path = makeCable (a, owner.rubber);
            g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.85f));
            g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        if (owner.movingNode != -99 && owner.dropRail.isNotEmpty())
        {
            int y = 160;
            for (auto& nv : owner.nodeViews)
            {
                if (! juce::isPositiveAndBelow (nv->nodeIndex, (int) owner.document.nodes.size()))
                    continue;
                if (dsl::visualRail (owner.document.nodes[(size_t) nv->nodeIndex]) == owner.dropRail)
                {
                    y = nv->getY();
                    break;
                }
            }
            g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.10f));
            g.fillRect (0, y - 8, getWidth(), GraphCanvasComponent::kCardHeight + 16);
            g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.85f));
            g.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
            g.drawText (owner.dropRail.toUpperCase(), 12, y - 22, 80, 16,
                        juce::Justification::centredLeft, false);
            if (auto* mv = owner.viewFor (owner.movingNode))
            {
                const int x = mv->getX() - 4;
                g.setColour (NeuroKoreLookAndFeel::accent());
                g.fillRect (x, y - 6, 3, GraphCanvasComponent::kCardHeight + 12);
            }
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
    viewport.setScrollBarsShown (true, true);
    startTimerHz (24);
}

GraphCanvasComponent::~GraphCanvasComponent()
{
    stopTimer();
    nodeViews.clear();
}

void GraphCanvasComponent::addBlockMenu (juce::Point<int> at)
{
    juce::PopupMenu m;
    m.addSectionHeader ("Add a module");
    m.addSeparator();
    m.addSectionHeader ("Tone");
    m.addItem (1, "Filter     low / high / band");
    m.addItem (2, "EQ     peak / shelf");
    m.addSectionHeader ("Drive");
    m.addItem (3, "Drive     formula, tube, clip");
    m.addSectionHeader ("Dynamics");
    m.addItem (4, "Compressor");
    m.addItem (5, "Gate");
    m.addItem (6, "Limiter");
    m.addSectionHeader ("Space");
    m.addItem (7, "Delay");
    m.addItem (8, "Reverb");
    m.addItem (9, "Cabinet IR");
    m.addSectionHeader ("Routing");
    m.addItem (13, "Bus     parallel send / return");
    m.addItem (14, "Mid-Side Split     encode, mid rail, side rail, decode");
    m.addItem (15, "Left / Right Split     two channel rails");
    m.addItem (16, "Crossover     band split to LOW / MID / HIGH");
    m.addSectionHeader ("Stereo");
    m.addItem (10, "OTT");
    m.addItem (11, "Stereo Width     Haas / stereo image, not mid-side");
    m.addSectionHeader ("Modulation");
    m.addItem (12, "LFO     does not sit on the audio line");
    m.showMenuAsync (juce::PopupMenu::Options()
                         .withMousePosition()
                         .withMinimumWidth (320)
                         .withStandardItemHeight (28),
                     [this, at] (int r)
    {
        if (r == 13) { addRouting ("bus", at); return; }
        if (r == 14) { addRouting ("split_ms", at); return; }
        if (r == 15) { addRouting ("split_lr", at); return; }
        if (r == 16) { addRouting ("xover", at); return; }
        const char* t[] = { nullptr, "filter", "eq", "stage", "comp", "gate",
                            "limit", "delay", "reverb", "ir", "ott", "widen", "osc" };
        if (r >= 1 && r <= 12)
            addBlock (t[r], at);
    });
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
            sub.addItem (juce::PopupMenu::Item ("Edit “" + prettyArgName (key) + "”")
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
                + (cur.isNotEmpty() ? cur : juce::String ("—"));
            m.addSubMenu (row, std::move (sub));
        }
    }

    m.addSeparator();
    const bool canRemove = n.type != "out" && n.type != "bus";
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
    if (auto* v = viewFor (nodeIndex))
        v->startEdit (onlyKey);
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

    std::map<juce::String, juce::Point<float>> kept;
    for (const auto& n : document.nodes)
        if (n.name.isNotEmpty() && std::isfinite (n.x) && std::isfinite (n.y))
            kept[n.name] = { n.x, n.y };

    document = std::move (incoming);
    parseOk = true;
    lastScript = script;
    selected = -99;
    cableFrom = -99;
    cableFromJack.clear();
    markEdgesDirty();

    for (auto& n : document.nodes)
    {
        if (std::isfinite (n.x) && std::isfinite (n.y))
            continue;
        const auto it = kept.find (n.name);
        if (it != kept.end())
        {
            n.x = it->second.x;
            n.y = it->second.y;
        }
    }
    if (! dsl::hasAllPositions (document))
        autoLayout();
    rebuildViews();
}

juce::String GraphCanvasComponent::getEmittedScript() const
{
    return parseOk ? dsl::emit (document) : lastScript;
}

void GraphCanvasComponent::autoLayout()
{
    float xMain = 200.f, yMain = 160.f;
    float xMod = 200.f, yMod = 32.f;
    std::map<juce::String, int> busRow;
    int nextBus = 1;
    inPos = GraphCanvasComponent::snapPoint ({ 32, 160 }).toFloat();

    for (int i = 0; i < (int) document.nodes.size(); ++i)
    {
        auto& n = document.nodes[(size_t) i];
        if (n.type == "bus")
        {
            const auto rail = n.name.isNotEmpty() ? n.name : juce::String ("bus");
            if (busRow.find (rail) == busRow.end())
                busRow[rail] = nextBus++;
            const int row = busRow[rail];
            const auto p = GraphCanvasComponent::snapPoint ({ 80, 160 + row * 112 });
            n.x = (float) p.x;
            n.y = (float) p.y;
            continue;
        }
        if (n.type == "out")
        {
            const auto p = GraphCanvasComponent::snapPoint ({ (int) std::lround (xMain + 32.f),
                                                              (int) std::lround (yMain) });
            n.x = (float) p.x;
            n.y = (float) p.y;
            outPos = { n.x, n.y };
            continue;
        }
        if (dsl::isModulator (n))
        {
            const auto p = GraphCanvasComponent::snapPoint ({ (int) std::lround (xMod),
                                                              (int) std::lround (yMod) });
            n.x = (float) p.x;
            n.y = (float) p.y;
            xMod += (float) (kNodeW + 32);
            continue;
        }
        const auto rail = dsl::visualRail (n);
        if (rail == "main")
        {
            const auto p = GraphCanvasComponent::snapPoint ({ (int) std::lround (xMain),
                                                              (int) std::lround (yMain) });
            n.x = (float) p.x;
            n.y = (float) p.y;
            xMain += (float) (kNodeW + 48);
        }
        else
        {
            if (busRow.find (rail) == busRow.end())
                busRow[rail] = nextBus++;
            const int row = busRow[rail];
            int count = 0;
            for (int j = 0; j < i; ++j)
                if (dsl::visualRail (document.nodes[(size_t) j]) == rail)
                    ++count;
            const auto p = GraphCanvasComponent::snapPoint ({ 192 + count * (kNodeW + 48),
                                                              160 + row * 112 });
            n.x = (float) p.x;
            n.y = (float) p.y;
        }
    }
    if (outIndex() < 0)
        outPos = GraphCanvasComponent::snapPoint ({ (int) std::lround (xMain + 32.f),
                                                    (int) std::lround (yMain) }).toFloat();
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
        v->setTopLeftPosition (snapped.x, snapped.y);
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

juce::Point<int> GraphCanvasComponent::toPaper (juce::Component& from, juce::Point<int> p) const
{
    if (paper == nullptr)
        return p;
    return paper->getLocalPoint (&from, p);
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
    if (edgesDirty)
    {
        cachedEdges = dsl::audioEdges (document);
        edgesDirty = false;
    }
    for (const auto& e : cachedEdges)
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
    if (edgesDirty)
    {
        cachedEdges = dsl::audioEdges (document);
        edgesDirty = false;
    }
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
        const float d = distSeg (paperPos, a, b);
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
    else if (type == "ms")
    {
        n.name = nextName ("ms");
        n.args["mode"] = "encode";
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
    applyGraph();
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
    g.fillAll (NeuroKoreLookAndFeel::canvas());
}

void GraphCanvasComponent::resized()
{
    viewport.setBounds (getLocalBounds());
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
    if (isShowing())
        startTimerHz (processor.isLiveMode() ? 12 : 24);
    else
        stopTimer();
}

void GraphCanvasComponent::timerCallback()
{
    const int wantHz = processor.isLiveMode() ? 12 : 24;
    if (getTimerInterval() != (1000 / wantHz))
        startTimerHz (wantHz);

    const float raw = juce::jlimit (0.f, 1.f,
        juce::Decibels::decibelsToGain (processor.getLoudnessDb() + 18.f) * 0.35f);
    inLevel = inLevel * 0.72f + raw * 0.28f;
    outLevel = inLevel;
    const bool live = outLevel > 0.018f;
    if (live)
    {
        cablePhase += 0.045f + outLevel * 0.18f;
        if (cablePhase > 64.f)
            cablePhase -= 64.f;
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
    if (parseOk && (live || hoverChanged))
        repaintPaper();
    if (hoverChanged)
        if (auto* ed = getParentComponent())
            ed->repaint();
}

void GraphCanvasComponent::drawLiveCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                                         float level, bool mix, bool hot) const
{
    auto path = makeCable (a, b);
    const float idle = mix ? 0.16f : 0.22f;
    const float alpha = hot ? 0.62f
                            : (idle + 0.14f * juce::jlimit (0.f, 1.f, level));
    g.setColour ((hot ? NeuroKoreLookAndFeel::ink() : NeuroKoreLookAndFeel::inkMuted())
                     .withAlpha (alpha));
    g.strokePath (path, juce::PathStrokeType (mix ? 1.0f : (hot ? 1.7f : 1.25f),
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    if (level < 0.022f)
        return;

    juce::PathFlatteningIterator it (path);
    float acc = 0.f;
    const float spacing = 28.f;
    float next = std::fmod (cablePhase * spacing * 0.55f, spacing);
    const float pr = hot ? 2.0f : 1.3f;
    g.setColour (NeuroKoreLookAndFeel::ink().withAlpha (hot ? 0.40f : 0.16f + level * 0.22f));
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
}

juce::Point<float> GraphCanvasComponent::knobJackOnNode (int nodeIndex, int knobIndex) const
{
    auto* v = viewFor (nodeIndex);
    if (v == nullptr || ! juce::isPositiveAndBelow (nodeIndex, (int) document.nodes.size()))
        return {};
    const auto letter = juce::String::charToString ((juce::juce_wchar) ('a' + knobIndex));
    return v->jackCentreById ("knob:" + letter, false);
}

void GraphCanvasComponent::paintKnobCables (juce::Graphics& g, juce::Component& space) const
{
    if (! parseOk)
        return;

    const auto accent = NeuroKoreLookAndFeel::accent();
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
            auto path = makeSoftCable (from, to);
            g.setColour (accent.withAlpha (hot ? 0.88f : 0.16f));
            g.strokePath (path, juce::PathStrokeType (hot ? 1.8f : 1.05f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
            if (hot)
                g.fillEllipse (to.x - 2.4f, to.y - 2.4f, 4.8f, 4.8f);
        }
    }
}


