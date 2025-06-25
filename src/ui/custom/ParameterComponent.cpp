#include "ParameterComponent.h"
#include "../../core/Config.h"
#include "../../utils/Localiser.h"

using namespace juce;

namespace ui
{
    bool ParameterComponent::infoMode = true;

    ParameterComponent::ParameterComponent(AudioProcessorValueTreeState& vts,
                                           const String& id,
                                           const String& alias)
        : valueTreeState(vts), paramID(id), aliasName(alias)
    {
        setSize(Config::kParameterKnobSize, Config::kParameterKnobSize);

        slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        const float startAngle = MathConstants<float>::pi * 4.0f / 3.0f;
        const float endAngle   = MathConstants<float>::pi * 8.0f / 3.0f;
        slider.setRotaryParameters(startAngle, endAngle, true);
        slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        nameLabel.setInterceptsMouseClicks(false, false);
        valueLabel.setInterceptsMouseClicks(false, false);
        minLabel.setInterceptsMouseClicks(false, false);
        maxLabel.setInterceptsMouseClicks(false, false);
        valueLabel.setColour(Label::textColourId, Colours::red);
        valueLabel.setJustificationType(Justification::centred);
        minLabel.setJustificationType(Justification::bottomLeft);
        maxLabel.setJustificationType(Justification::bottomRight);
        nameLabel.setJustificationType(Justification::centred);

        attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, paramID, slider);

        slider.onValueChange = [this]
        {
            updateLabel();
        };

        addAndMakeVisible(slider);
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(minLabel);
        addAndMakeVisible(maxLabel);

        valueTreeState.addParameterListener(paramID, this);
    }

    ParameterComponent::~ParameterComponent()
    {
        valueTreeState.removeParameterListener(paramID, this);
    }

    void ParameterComponent::setAliasName(const String& name)
    {
        aliasName = name;
        updateLabel();
    }

    void ParameterComponent::paint(Graphics& g)
    {
        g.fillAll(Colours::transparentBlack);
    }

    void ParameterComponent::resized()
    {
        auto area = getLocalBounds();
        auto knobSize = area.getWidth();
        slider.setBounds(area.removeFromTop(knobSize));

        valueLabel.setBounds(slider.getBounds());

        auto bottom = area.removeFromTop(20);
        minLabel.setBounds(bottom.removeFromLeft(bottom.getWidth() / 2));
        maxLabel.setBounds(bottom);
        nameLabel.setBounds(area);
    }

    void ParameterComponent::paintOverChildren(Graphics& g)
    {
        valueLabel.setVisible(infoMode);
        minLabel.setVisible(infoMode);
        maxLabel.setVisible(infoMode);
        nameLabel.setVisible(infoMode);

        if (!isEnabled())
        {
            g.setColour(Colours::black.withAlpha(0.5f));
            g.fillAll();
        }
    }


    void ParameterComponent::parameterChanged(const String& id, float)
    {
        if (id == paramID)
        {
            MessageManager::callAsync([this] { updateLabel(); });
        }
    }
       
    void ParameterComponent::updateLabel(){
        auto* param = valueTreeState.getParameter(paramID);
        if (!param)
            return;

        if (!aliasName.isEmpty())
            nameLabel.setText(aliasName, dontSendNotification);
        else
            nameLabel.setText(param->getName(64), dontSendNotification);

        valueLabel.setText(String(slider.getValue(), 2), dontSendNotification);
        minLabel.setText(String(slider.getMinimum(), 2), dontSendNotification);
        maxLabel.setText(String(slider.getMaximum(), 2), dontSendNotification);
    }


    void ParameterComponent::mouseUp(const MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            PopupMenu menu;
            menu.addItem(1, TRANS("Set Min"));
            menu.addItem(2, TRANS("Set Max"));

            PopupMenu::Options opts;
            opts.withTargetComponent(this);
            auto res = menu.showMenu(opts);
            if (res == 1 || res == 2)
            {
                bool setMin = res == 1;
                auto* aw = new AlertWindow("", TRANS("Enter value"), AlertWindow::NoIcon);
                aw->addTextEditor("val", setMin ? String(slider.getMinimum()) : String(slider.getMaximum()));
                aw->addButton("OK", 1, KeyPress(KeyPress::returnKey));
                aw->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

                aw->enterModalState(true,
                                     ModalCallbackFunction::create([this, awPtr = Component::SafePointer<AlertWindow>(aw), setMin](int result)
                                     {
                                         if (result != 1 || awPtr == nullptr)
                                             return;

                                         const auto v = awPtr->getTextEditor("val")->getText().getFloatValue();

                                         if (auto* p = dynamic_cast<AudioParameterFloat*>(valueTreeState.getParameter(paramID)))
                                         {
                                             auto min = setMin ? v : p->range.start;
                                             auto max = setMin ? p->range.end : v;

                                             if (max > min)
                                             {
                                                 p->range.start = min;
                                                 p->range.end = max;
                                                 slider.setRange(min, max);
                                             }
                                         }
                                     }),
                                     true);
            }
        }
    }

    void ParameterComponent::setEnabled(bool shouldBeEnabled)
    {
        Component::setEnabled(shouldBeEnabled);
        slider.setEnabled(shouldBeEnabled);
        if (!shouldBeEnabled)
            slider.setValue(0.0, dontSendNotification);
    }
} // namespace ui

