#pragma once

#include <JuceHeader.h>
#include "../core/Config.h"
#include "../dsl/GraphModel.h"
#include "GraphCanvasComponent.h"
#include "PluginLookAndFeel.h"

/** Clean overlay to edit one circuit block. ASCII labels only. */
class NodeInspectComponent : public juce::Component
{
public:
    NodeInspectComponent (NeuroKoreAudioProcessor& proc,
                          GraphCanvasComponent& canvas,
                          int nodeIndex)
        : processor (proc), owner (canvas), index (nodeIndex)
    {
        setOpaque (true);
        if (! juce::isPositiveAndBelow (index, (int) owner.document.nodes.size()))
            return;

        const auto& n = owner.document.nodes[(size_t) index];
        title = kindTitle (n);

        auto keys = dsl::editableArgKeys (n, &owner.document);
        if (n.type.startsWithIgnoreCase ("stage") && ! keys.contains ("y"))
            keys.insert (0, "y");

        for (const auto& key : keys)
        {
            auto lab = std::make_unique<juce::Label>();
            lab->setText (pretty (key), juce::dontSendNotification);
            lab->setFont (NeuroKoreLookAndFeel::monoFont (14.f));
            lab->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
            lab->setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (*lab);

            auto ed = std::make_unique<juce::TextEditor>();
            juce::String cur;
            const auto it = n.args.find (key);
            if (it != n.args.end())
                cur = it->second;
            ed->setText (cur, false);
            ed->setFont (NeuroKoreLookAndFeel::monoFont (16.f));
            ed->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff050505));
            ed->setColour (juce::TextEditor::textColourId, NeuroKoreLookAndFeel::ink());
            ed->setColour (juce::TextEditor::outlineColourId, NeuroKoreLookAndFeel::accent().withAlpha (0.45f));
            ed->setColour (juce::TextEditor::focusedOutlineColourId, NeuroKoreLookAndFeel::accent());
            ed->setJustification (juce::Justification::centredLeft);
            addAndMakeVisible (*ed);

            rows.push_back ({ key, std::move (lab), std::move (ed) });
        }

        for (int k = 0; k < Config::kNumUserParams; ++k)
        {
            auto b = std::make_unique<juce::TextButton> (
                juce::String::charToString ((juce::juce_wchar) ('a' + k)));
            const auto name = processor.getVariableName (k);
            b->setTooltip (name.isNotEmpty() ? name : b->getButtonText());
            const int idx = k;
            b->onClick = [this, idx]
            {
                int which = 0;
                for (int i = 0; i < (int) rows.size(); ++i)
                    if (rows[(size_t) i].field->hasKeyboardFocus (true))
                        which = i;
                if (juce::isPositiveAndBelow (which, (int) rows.size()))
                    rows[(size_t) which].field->setText (
                        juce::String::charToString ((juce::juce_wchar) ('a' + idx)), false);
            };
            addAndMakeVisible (*b);
            knobs.push_back (std::move (b));
        }

        applyBtn.setButtonText ("Apply");
        applyBtn.onClick = [this] { commit(); };
        addAndMakeVisible (applyBtn);

        if (n.type.startsWithIgnoreCase ("ir"))
        {
            irBtn.setButtonText ("Open IR");
            irBtn.onClick = [this]
            {
                if (owner.onOpenIr && juce::isPositiveAndBelow (index, (int) owner.document.nodes.size()))
                    owner.onOpenIr (owner.document.nodes[(size_t) index].name);
            };
            addAndMakeVisible (irBtn);
        }

        const bool canRemove = n.type != "out" && n.type != "bus";
        removeBtn.setButtonText ("Remove");
        removeBtn.setEnabled (canRemove);
        removeBtn.onClick = [this]
        {
            owner.selected = index;
            owner.removeSelected();
            if (onFinished)
                onFinished();
        };
        addAndMakeVisible (removeBtn);

        hint.setText ("Bind focused field to knob a-f", juce::dontSendNotification);
        hint.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
        hint.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::inkMuted());
        hint.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (hint);
    }

    std::function<void()> onFinished;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (NeuroKoreLookAndFeel::canvas());
        g.setColour (NeuroKoreLookAndFeel::ink());
        g.setFont (NeuroKoreLookAndFeel::monoFont (18.f));
        g.drawText (title, getLocalBounds().removeFromTop (32).reduced (16, 2),
                    juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 14);
        r.removeFromTop (32);
        auto actions = r.removeFromBottom (40);
        r.removeFromBottom (12);
        auto bind = r.removeFromBottom (28);
        r.removeFromBottom (8);
        hint.setBounds (r.removeFromBottom (16));
        r.removeFromBottom (8);
        for (auto& row : rows)
        {
            auto line = r.removeFromTop (36);
            row.label->setBounds (line.removeFromLeft (118));
            line.removeFromLeft (10);
            row.field->setBounds (line);
            r.removeFromTop (8);
        }
        const int gap = 8;
        const int bw = juce::jmax (36, (bind.getWidth() - gap * (Config::kNumUserParams - 1))
                                           / Config::kNumUserParams);
        for (int k = 0; k < (int) knobs.size(); ++k)
        {
            knobs[(size_t) k]->setBounds (bind.removeFromLeft (bw));
            bind.removeFromLeft (gap);
        }

        removeBtn.setBounds (actions.removeFromLeft (108));
        actions.removeFromLeft (10);
        if (irBtn.isVisible())
        {
            irBtn.setBounds (actions.removeFromLeft (108));
            actions.removeFromLeft (10);
        }
        applyBtn.setBounds (actions.removeFromRight (128));
    }

    int preferredHeight() const noexcept
    {
        return 14 + 32 + (int) rows.size() * 44 + 16 + 8 + 28 + 12 + 40 + 14;
    }

private:
    struct Row
    {
        juce::String key;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::TextEditor> field;
    };

    static juce::String kindTitle (const dsl::GraphNode& n)
    {
        auto t = n.type.toUpperCase();
        if (n.name.isNotEmpty())
            t << "   " << n.name;
        return t;
    }

    static juce::String pretty (const juce::String& key)
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
        if (k == "hyst") return "Hyst";
        if (k == "hold") return "Hold";
        if (k == "range") return "Range";
        if (k == "ceiling") return "Ceiling";
        if (k == "time") return "Time";
        if (k == "feedback") return "Feedback";
        if (k == "mix") return "Mix";
        if (k == "size") return "Size";
        if (k == "decay") return "Decay";
        if (k == "damp") return "Damp";
        if (k == "width") return "Width";
        if (k == "depth") return "Depth";
        if (k == "shape") return "Shape";
        if (k == "mode") return "Mode";
        if (k == "in") return "Input";
        if (k == "main") return "Main";
        if (k == "bass") return "Bass";
        return key;
    }

    void commit()
    {
        if (! juce::isPositiveAndBelow (index, (int) owner.document.nodes.size()))
            return;
        for (auto& row : rows)
            dsl::setNodeArg (owner.document, index, row.key, row.field->getText());
        owner.commitDocument();
        if (onFinished)
            onFinished();
    }

    NeuroKoreAudioProcessor& processor;
    GraphCanvasComponent& owner;
    int index { -1 };
    juce::String title;
    std::vector<Row> rows;
    std::vector<std::unique_ptr<juce::TextButton>> knobs;
    juce::TextButton applyBtn, removeBtn, irBtn;
    juce::Label hint;
};
