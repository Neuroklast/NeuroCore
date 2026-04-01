// ParameterComponent.cpp
#include "ParameterComponent.h"
#include "../../core/Config.h"
#include "../../utils/Localiser.h"
#include "../MidiLearnManager.h"

using namespace juce;

namespace ui
{
bool ParameterComponent::infoMode = true;

ParameterComponent::ParameterComponent (AudioProcessorValueTreeState& vts,
                                        const String& id,
                                        const String& alias)
    : valueTreeState (vts),
      paramID        (id),
      aliasName      (alias)
{
    // Optional: Default-Größe (kann vom Parent überschrieben werden)
    setSize (Config::kParameterKnobSize,
             Config::kParameterKnobSize);

    // Slider als großer Knob über gesamten Bereich
    slider.setSliderStyle      (Slider::RotaryHorizontalVerticalDrag);
    const float startAngle = MathConstants<float>::pi * 4.0f / 3.0f;
    const float endAngle   = MathConstants<float>::pi * 8.0f / 3.0f;
    slider.setRotaryParameters (startAngle, endAngle, true);
    slider.setTextBoxStyle     (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);

    // Labels sollen Klicks durchlassen und individuell justiert werden
    for (auto* lbl : { &nameLabel, &valueLabel, &minLabel, &maxLabel })
    {
        lbl->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*lbl);
    }

    valueLabel.setJustificationType (Justification::centred);
    minLabel. setJustificationType (Justification::bottomLeft);
    maxLabel. setJustificationType (Justification::bottomRight);
    nameLabel.setJustificationType (Justification::centred);

    valueLabel.setColour (Label::textColourId, Colours::red);
	nameLabel.setColour(Label::textColourId, Colours::red);
	minLabel.setColour(Label::textColourId, Colours::red);
	maxLabel.setColour(Label::textColourId, Colours::red);

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
void ParameterComponent::setAliasName (const String& name)
{
    aliasName = name;
    updateLabel();
}

void ParameterComponent::paint (Graphics& g)
{
    g.fillAll (Colours::transparentBlack);
}

void ParameterComponent::resized()
{
    // Slider füllt den gesamten Komponent-Bereich
    auto bounds = getLocalBounds();
    slider.setBounds (bounds);

    // Label-Größen und Abstände
    const int labelH  = 16;
    const int margin  = 2;
    const int labelW  = jmin (bounds.getWidth() / 2, 40);

    // nameLabel oben zentriert
    nameLabel.setBounds (bounds.getX(),
                         bounds.getY() + margin,
                         bounds.getWidth(),
                         labelH);

    // minLabel unten links
    minLabel.setBounds (bounds.getX()     + margin,
                        bounds.getBottom() - labelH - margin,
                        labelW,
                        labelH);

    // maxLabel unten rechts
    maxLabel.setBounds (bounds.getRight() - labelW - margin,
                        bounds.getBottom() - labelH - margin,
                        labelW,
                        labelH);

    // valueLabel deckt den ganzen Knob (zentriert roten Wert)
    valueLabel.setBounds (bounds);
}

void ParameterComponent::paintOverChildren (Graphics& g)
{
    // steuert, ob die Labels gezeichnet werden
    nameLabel .setVisible (infoMode);
    valueLabel.setVisible (infoMode);
    minLabel  .setVisible (infoMode);
    maxLabel  .setVisible (infoMode);

    if (! isEnabled())
    {
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRect (getLocalBounds());
    }
}

// wird auf Audio-Thread gerufen, wir delegieren per MessageLoop
void ParameterComponent::parameterChanged (const String& id, float)
{
    if (id == paramID)
        MessageManager::callAsync ([this] { updateLabel(); });
}

// Labeltexte setzen
void ParameterComponent::updateLabel()
{
    if (auto* p = valueTreeState.getParameter (paramID))
    {
        nameLabel .setText (aliasName.isNotEmpty() ? aliasName : p->getName (64),
                            dontSendNotification);
        valueLabel.setText (String (slider.getValue(), 2),
                            dontSendNotification);
        minLabel  .setText (String (slider.getMinimum(), 2),
                            dontSendNotification);
        maxLabel  .setText (String (slider.getMaximum(), 2),
                            dontSendNotification);
    }
}

void ParameterComponent::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        PopupMenu menu;
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
    Component::setEnabled (shouldBeEnabled);
    slider.setEnabled   (shouldBeEnabled);
    if (! shouldBeEnabled)
    {
        // Keep the parameter value but hide it visually
        slider.setValue (0.0f, dontSendNotification);
    }
    else
    {
        // Sync the slider with the parameter when re-enabled
        if (auto* v = valueTreeState.getRawParameterValue (paramID))
            slider.setValue (v->load(), dontSendNotification);
        updateLabel();
    }
}
} // namespace ui
