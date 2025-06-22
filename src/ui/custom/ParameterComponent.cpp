#include "ParameterComponent.h"

using namespace juce;

namespace ui
{
    ParameterComponent::ParameterComponent(AudioProcessorValueTreeState& vts,
                                           const String& id,
                                           const String& alias)
        : valueTreeState(vts), paramID(id), aliasName(alias)
    {
        slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        slider.addMouseListener(this, true);
        addAndMakeVisible(slider);

        nameLabel.setJustificationType(Justification::centred);
        addAndMakeVisible(nameLabel);

        for (auto* l : { &minLabel, &valueLabel, &maxLabel })
        {
            l->setJustificationType(Justification::left);
            addAndMakeVisible(*l);
        }

        mappingBox.onChange = [this]
        {
            currentMapping = mappingBox.getSelectedItemIndex();
            updateLabels();
        };
        addAndMakeVisible(mappingBox);

        attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, paramID, slider);

        slider.onValueChange = [this]
        {
            updateLabels();
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
        updateLabels();
    }

    void ParameterComponent::paint(Graphics& g)
    {
        g.fillAll(Colours::transparentBlack);
    }

    void ParameterComponent::resized()
    {
        auto area = getLocalBounds();
        auto left = area.removeFromLeft(60);
        minLabel.setBounds(left.removeFromTop(20));
        valueLabel.setBounds(left.removeFromTop(20));
        maxLabel.setBounds(left.removeFromTop(20));

        area.removeFromTop(5);
        nameLabel.setBounds(area.removeFromTop(20));
        slider.setBounds(area.reduced(5));
        mappingBox.setBounds(area.removeFromBottom(20));
    }

    void ParameterComponent::addMapping(const ParameterMapping& m)
    {
        mappings.push_back(m);
        mappingBox.addItem(m.context, mappings.size());
        if (mappings.size() == 1)
            mappingBox.setSelectedItemIndex(0, dontSendNotification);
        updateLabels();
    }

    void ParameterComponent::parameterChanged(const String& id, float)
    {
        if (id == paramID)
        {
            MessageManager::callAsync([this] { updateLabels(); });
        }
    }

    void ParameterComponent::updateLabels()
    {
        auto* param = valueTreeState.getParameter(paramID);
        if (!param)
            return;

        auto value = param->getValueForText(String(slider.getValue()));
        valueLabel.setText(String(value, 2) + (mappings.empty() ? String() : mappings[(size_t)currentMapping].unit), dontSendNotification);

        if (!aliasName.isEmpty())
            nameLabel.setText(aliasName, dontSendNotification);
        else
            nameLabel.setText(param->getName(64), dontSendNotification);

        if (!mappings.empty())
        {
            auto& m = mappings[(size_t)currentMapping];
            minLabel.setText(String(m.min) + " " + m.unit, dontSendNotification);
            maxLabel.setText(String(m.max) + " " + m.unit, dontSendNotification);
        }
        else
        {
            minLabel.setText(String(param->getNormalisableRange().start), dontSendNotification);
            maxLabel.setText(String(param->getNormalisableRange().end), dontSendNotification);
        }
    }

    void ParameterComponent::showContextMenu()
    {
        if (mappings.empty())
            return;

        PopupMenu menu;
        menu.addItem("Edit", [this]
        {
            editMapping(mappings[(size_t)currentMapping]);
        });
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&slider));
    }

    void ParameterComponent::mouseUp(const MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            showContextMenu();
    }

    void ParameterComponent::editMapping(ParameterMapping& mapping)
    {
        auto* w = new AlertWindow("Edit Mapping", {}, AlertWindow::NoIcon);
        w->addTextEditor("min", String(mapping.min));
        w->addTextEditor("max", String(mapping.max));
        w->addTextEditor("unit", mapping.unit);
        w->addButton("OK", 1, KeyPress(KeyPress::returnKey));
        w->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
        w->enterModalState(true, ModalCallbackFunction::create([this, &mapping, w](int result)
        {
            std::unique_ptr<AlertWindow> owned(w);
            if (result == 1)
            {
                mapping.min  = w->getTextEditor("min")->getText().getDoubleValue();
                mapping.max  = w->getTextEditor("max")->getText().getDoubleValue();
                mapping.unit = w->getTextEditor("unit")->getText();
                updateLabels();
            }
        }), false);
    }
} // namespace ui

