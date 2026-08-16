// ParameterComponent.cpp
#include "ParameterComponent.h"
#include "../../core/Config.h"
#include <cmath>
#include "../../utils/Localiser.h"
#include "../MidiLearnManager.h"
#include "../PluginLookAndFeel.h"

using namespace juce;

namespace ui
{

ParameterComponent::ParameterComponent (AudioProcessorValueTreeState& vts,
                                        const String& id,
                                        const String& alias)
    : valueTreeState (vts),
      paramID        (id),
      aliasName      (alias)
{
    // Default size: room for name above + rotary + min/max below
    setSize (Config::kParameterKnobSize,
             Config::kParameterKnobSize + 22);

    // Slider als großer Knob über gesamten Bereich
    slider.setSliderStyle      (Slider::RotaryHorizontalVerticalDrag);
    const float startAngle = MathConstants<float>::pi * 4.0f / 3.0f;
    const float endAngle   = MathConstants<float>::pi * 8.0f / 3.0f;
    slider.setRotaryParameters (startAngle, endAngle, true);
    slider.setTextBoxStyle     (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);

    // Labels sollen Klicks durchlassen und individuell justiert werden
    for (auto* lbl : { &valueLabel, &minLabel, &maxLabel })
    {
        lbl->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*lbl);
    }
    nameLabel.setInterceptsMouseClicks (true, false);
    nameLabel.setEditable (true, true, false);
    nameLabel.setTooltip ("Click the name to rename this knob (a–f stay in the formula)");
    nameLabel.setMouseCursor (juce::MouseCursor::IBeamCursor);
    nameLabel.onEditorShow = [this]
    {
        if (auto* ed = nameLabel.getCurrentTextEditor())
        {
            ed->setJustification (Justification::centred);
            ed->setColour (TextEditor::backgroundColourId, Colour (0xff141414));
            ed->setColour (TextEditor::textColourId, accentColour.brighter (0.2f));
            ed->setHighlightedRegion ({ 0, ed->getText().length() });
            ed->grabKeyboardFocus();
        }
    };
    nameLabel.onEditorHide = [this] { commitAlias(); };
    addAndMakeVisible (nameLabel);

    valueLabel.setJustificationType (Justification::centred);
    minLabel.setJustificationType (Justification::centredLeft);
    maxLabel.setJustificationType (Justification::centredRight);
    nameLabel.setJustificationType (Justification::centred);

    const auto muted  = Colour(0xffb0b0b0); // readable min/max
    valueLabel.setColour (Label::textColourId, accentColour);
    valueLabel.setFont (NeuroKoreLookAndFeel::monoFont (15.f));
    valueLabel.setJustificationType (Justification::centred);
    nameLabel.setColour(Label::textColourId, Colour (0xfffff5f5));
    nameLabel.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
    minLabel.setColour(Label::textColourId, muted);
    maxLabel.setColour(Label::textColourId, muted);
    minLabel.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
    maxLabel.setFont (NeuroKoreLookAndFeel::monoFont (11.f));
    // Values always painted above knob art
    valueLabel.setAlwaysOnTop (true);
    nameLabel.setAlwaysOnTop (true);
    slider.setColour (Slider::rotarySliderOutlineColourId, accentColour.withAlpha (0.75f));
    slider.setColour (Slider::thumbColourId, accentColour);

    // Bindung an den ValueTreeState
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, paramID, slider);

    // Callback für sofortiges Label-Update
    slider.onValueChange = [this] { updateLabel(); };

    valueTreeState.addParameterListener (paramID, this);

    // Initial einmal befüllen, sonst bleiben die Labels leer
    updateLabel();
}

ParameterComponent::~ParameterComponent()
{
    valueTreeState.removeParameterListener (paramID, this);
}

// externes Alias setzen
juce::String ParameterComponent::formatRangeBound (float v)
{
    if (! std::isfinite (v))
        return "0";
    if (std::abs (v - std::round (v)) < 1.0e-4f && std::abs (v) < 1.0e7f)
        return String ((int) std::lround (v));
    return String (v, 2);
}

void ParameterComponent::commitAlias()
{
    auto next = nameLabel.getText().trim();
    if (next.isEmpty())
        next = paramID;
    if (next == aliasName)
    {
        nameLabel.setText (aliasName, dontSendNotification);
        return;
    }
    aliasName = next;
    nameLabel.setText (aliasName, dontSendNotification);
    if (onAliasChanged)
        onAliasChanged (aliasName);
}

void ParameterComponent::setAliasName (const String& name)
{
    if (nameLabel.isBeingEdited())
        return;
    aliasName = name;
    updateLabel();
}

void ParameterComponent::setAccentColour (Colour colour)
{
    accentColour = colour;
    valueLabel.setColour (Label::textColourId, accentColour);
    nameLabel.setColour (Label::textColourId, accentColour.brighter (0.15f));
    slider.setColour (Slider::rotarySliderOutlineColourId, accentColour.withAlpha (0.75f));
    slider.setColour (Slider::thumbColourId, accentColour);
    repaint();
}

void ParameterComponent::setMappedRange (float minVal, float maxVal)
{
    if (maxVal < minVal)
        std::swap (minVal, maxVal);
    mappedMin = minVal;
    mappedMax = maxVal;
    hasMappedRange = std::abs (maxVal - minVal) > 1.0e-9f;
    updateLabel();
}

void ParameterComponent::setNoteGrid (std::vector<juce::String> labels)
{
    noteLabels = std::move (labels);
    const int n = (int) noteLabels.size();
    if (n >= 2)
        slider.setRange (0.0, 1.0, 1.0 / (double) (n - 1));
    else
        slider.setRange (0.0, 1.0, 0.0);
    updateLabel();
}

void ParameterComponent::paint (Graphics& g)
{
    g.fillAll (Colours::transparentBlack);

    auto bounds = getLocalBounds().toFloat().reduced (1.f);
    const bool on = isEnabled();

    // Cyber panel under knob
    g.setColour (Colour (0xff050505).withAlpha (on ? 0.85f : 0.4f));
    g.fillRect (bounds);
    g.setColour (accentColour.withAlpha (on ? 0.65f : 0.18f));
    // cut corners
    Path frame;
    const float cut = 8.f;
    frame.startNewSubPath (bounds.getX() + cut, bounds.getY());
    frame.lineTo (bounds.getRight(), bounds.getY());
    frame.lineTo (bounds.getRight(), bounds.getBottom() - cut);
    frame.lineTo (bounds.getRight() - cut, bounds.getBottom());
    frame.lineTo (bounds.getX(), bounds.getBottom());
    frame.lineTo (bounds.getX(), bounds.getY() + cut);
    frame.closeSubPath();
    g.strokePath (frame, PathStrokeType (1.2f));

    // Corner ticks
    g.setColour (accentColour.withAlpha (on ? 0.9f : 0.2f));
    g.drawLine (bounds.getX(), bounds.getY() + cut, bounds.getX(), bounds.getY(), 1.5f);
    g.drawLine (bounds.getX(), bounds.getY(), bounds.getX() + cut, bounds.getY(), 1.5f);

    if (! on)
    {
        g.setColour (Colour (0x66ff1a1a));
        g.setFont (Font (Font::getDefaultMonospacedFontName(), 10.f, Font::bold));
        g.drawText ("// NO LINK", bounds.removeFromTop (14.f), Justification::centred, false);
    }
}

void ParameterComponent::resized()
{
    auto bounds = getLocalBounds().reduced (2);

    const int nameH = 18;
    nameLabel.setBounds (bounds.removeFromTop (nameH));
    nameLabel.setJustificationType (Justification::centred);

    // Remaining area = knob + min/max + centre value
    const int rangeH = 14;
    auto rangeRow = bounds.removeFromBottom (rangeH);
    const int labelW = jmax (28, rangeRow.getWidth() / 3);
    minLabel.setBounds (rangeRow.removeFromLeft (labelW));
    maxLabel.setBounds (rangeRow.removeFromRight (labelW));
    minLabel.setJustificationType (Justification::centredLeft);
    maxLabel.setJustificationType (Justification::centredRight);

    // Value sits under the disc so the needle never covers the readout
    const int valueH = 16;
    auto valueRow = bounds.removeFromBottom (valueH);
    valueLabel.setBounds (valueRow);
    valueLabel.setJustificationType (Justification::centred);

    auto knobArea = bounds.reduced (2);
    const int side = jmin (knobArea.getWidth(), knobArea.getHeight());
    auto rotary = juce::Rectangle<int> (0, 0, side, side)
                      .withCentre (knobArea.getCentre());
    slider.setBounds (rotary);
}

bool ParameterComponent::isActivelyUsed() const
{
    return slider.isMouseButtonDown()
        || slider.isMouseOverOrDragging()
        || isMouseOverOrDragging();
}

void ParameterComponent::paintOverChildren (Graphics& g)
{
    // Dim unlinked knobs lightly but keep text readable
    if (! isEnabled())
    {
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillRect (getLocalBounds());
    }
}

// wird auf Audio-Thread gerufen, wir delegieren per MessageLoop
void ParameterComponent::parameterChanged (const String& id, float)
{
    if (id == paramID)
        MessageManager::callAsync ([this] { updateLabel(); });
}

// Labeltexte setzen — ALWAYS show live value (even when knob is unlinked)
void ParameterComponent::updateLabel()
{
    // Prefer raw APVTS so display stays correct when slider is disabled
    float norm = 0.f;
    if (auto* raw = valueTreeState.getRawParameterValue (paramID))
        norm = juce::jlimit (0.f, 1.f, raw->load());
    else
        norm = (float) slider.getValue();

    if (! nameLabel.isBeingEdited())
    {
        if (auto* p = valueTreeState.getParameter (paramID))
            nameLabel.setText (aliasName.isNotEmpty() ? aliasName : p->getName (64),
                               dontSendNotification);
        else if (aliasName.isNotEmpty())
            nameLabel.setText (aliasName, dontSendNotification);
        nameLabel.setFont (NeuroKoreLookAndFeel::monoFont (12.f));
    }

    if (noteLabels.size() >= 2)
    {
        const int last = (int) noteLabels.size() - 1;
        const int idx = juce::jlimit (0, last, (int) std::lround (norm * (float) last));
        valueLabel.setText (noteLabels[(size_t) idx], dontSendNotification);
        minLabel.setText (noteLabels.front(), dontSendNotification);
        maxLabel.setText (noteLabels.back(), dontSendNotification);
    }
    else if (hasMappedRange)
    {
        const float mapped = mappedMin + norm * (mappedMax - mappedMin);
        const float av = std::abs (mapped);
        String txt;
        if (av >= 1000.f)      txt = String (mapped, 0);
        else if (av >= 100.f)  txt = String (mapped, 1);
        else if (av >= 10.f)   txt = String (mapped, 2);
        else                   txt = String (mapped, 3);
        valueLabel.setText (txt, dontSendNotification);
        minLabel.setText (formatRangeBound (mappedMin), dontSendNotification);
        maxLabel.setText (formatRangeBound (mappedMax), dontSendNotification);
    }
    else
    {
        valueLabel.setText (String (norm, 3), dontSendNotification);
        minLabel  .setText ("0", dontSendNotification);
        maxLabel  .setText ("1", dontSendNotification);
    }

    valueLabel.setColour (Label::textColourId, accentColour.brighter (0.25f));
    valueLabel.setFont (NeuroKoreLookAndFeel::monoFont (15.f));
}

void ParameterComponent::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        PopupMenu menu;
        menu.addItem (5, "Rename knob");
        menu.addSeparator();
        menu.addItem (1, TRANS("Set Min"));
        menu.addItem (2, TRANS("Set Max"));

        // MIDI Learn options
        if (midiLearnMgr != nullptr)
        {
            menu.addSeparator();
            int mappedCC = midiLearnMgr->getMappedCC(paramID);
            if (mappedCC >= 0)
            {
                menu.addItem(3, "MIDI Learn (CC " + String(mappedCC) + ")");
                menu.addItem(4, "MIDI Unlearn");
            }
            else
            {
                menu.addItem(3, "MIDI Learn");
            }
        }

        PopupMenu::Options opts;
        opts.withTargetComponent (this);

        menu.showMenuAsync (opts,
            [this] (int res)
            {
                if (res == 5)
                {
                    nameLabel.showEditor();
                    return;
                }
                if (res == 3 && midiLearnMgr != nullptr)
                {
                    midiLearnMgr->startLearning(paramID);
                    return;
                }
                if (res == 4 && midiLearnMgr != nullptr)
                {
                    midiLearnMgr->clearMappingForParam(paramID);
                    return;
                }

                if (res != 1 && res != 2)
                    return;

                const bool setMin = (res == 1);
                auto* aw = new AlertWindow ("", TRANS("Enter value"), AlertWindow::NoIcon);
                aw->addTextEditor ("val",
                                   setMin ? String (slider.getMinimum())
                                          : String (slider.getMaximum()));
                aw->addButton      ("OK", 1, KeyPress (KeyPress::returnKey));
                aw->addButton      ("Cancel", 0, KeyPress (KeyPress::escapeKey));

                aw->enterModalState (true,
                    ModalCallbackFunction::create (
                        [this, awPtr = Component::SafePointer<AlertWindow>(aw), setMin] (int result)
                        {
                            if (result != 1 || awPtr == nullptr)
                                return;

                            const float v = awPtr->getTextEditor ("val")->getText().getFloatValue();
                            if (auto* p = dynamic_cast<AudioParameterFloat*> (valueTreeState.getParameter (paramID)))
                            {
                                const float newMin = setMin ? v : p->range.start;
                                const float newMax = setMin ? p->range.end : v;
                                if (newMax > newMin)
                                {
                                    p->range.start = newMin;
                                    p->range.end   = newMax;
                                    slider.setRange (newMin, newMax);
                                    updateLabel();
                                }
                            }
                        }),
                    true);
            });
    }
}

void ParameterComponent::setEnabled (bool shouldBeEnabled)
{
    // Only the slider is interactive when unlinked — labels always enabled so values stay readable
    Component::setEnabled (true); // keep component graph receiving paint
    slider.setEnabled (shouldBeEnabled);
    nameLabel.setEnabled (true);
    valueLabel.setEnabled (true);
    minLabel.setEnabled (true);
    maxLabel.setEnabled (true);

    // Dim the dial when unlinked, but NEVER hide the numeric value
    setAlpha (shouldBeEnabled ? 1.0f : 0.72f);
    slider.setAlpha (shouldBeEnabled ? 1.0f : 0.4f);
    valueLabel.setAlpha (1.0f);
    nameLabel.setAlpha (1.0f);
    slider.setInterceptsMouseClicks (shouldBeEnabled, shouldBeEnabled);

    // Sync slider thumb to APVTS even when not interactive
    if (auto* v = valueTreeState.getRawParameterValue (paramID))
        slider.setValue (v->load(), dontSendNotification);

    updateLabel(); // always show current mapped/norm value
    repaint();
}
} // namespace ui
