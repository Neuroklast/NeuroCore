#include "ParameterComponent.h"
#include "../../core/Config.h"
#include "../../utils/Localiser.h"

using namespace juce;

namespace ui
{
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
        nameLabel.setInterceptsMouseClicks(false, false);
        attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, paramID, slider);

        slider.onValueChange = [this]
        {
            updateLabel();
        };

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
        slider.setBounds(getLocalBounds());
        nameLabel.setBounds(0, getHeight() - 24, getWidth(), 24);
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
    }


    void ParameterComponent::mouseUp(const MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            ;
    }
} // namespace ui

