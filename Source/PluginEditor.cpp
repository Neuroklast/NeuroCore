/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (1200, 800);

    for (int i = 0; i < sliders.size(); ++i)
    {
        sliders[i] = std::make_unique<juce::Slider>();
        sliders[i]->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sliders[i]->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sliders[i]->setRange (0.0, 1.0);
        sliders[i]->onValueChange = [this, i]
        {
            const float v = static_cast<float> (sliders[i]->getValue());
            sliderEditors[i]->setText (juce::String (v), juce::dontSendNotification);
            audioProcessor.setParameter (i, v);
        };
        sliders[i]->setValue (0.0);
        addAndMakeVisible (*sliders[i]);

        sliderEditors[i] = std::make_unique<juce::TextEditor>();
        sliderEditors[i]->setMultiLine (false);
        sliderEditors[i]->setText ("0", juce::dontSendNotification);
        addAndMakeVisible (*sliderEditors[i]);
    }

    formulaInputEditor = std::make_unique<juce::TextEditor>();
    formulaInputEditor->setMultiLine (true, true);
    formulaInputEditor->setReturnKeyStartsNewLine (true);
    formulaInputEditor->setText ("tanh(x)", juce::dontSendNotification);
    formulaInputEditor->onTextChange = [this]
    {
        audioProcessor.setFormula (formulaInputEditor->getText());
        formulaDisplayEditor->setText (formulaInputEditor->getText(), juce::dontSendNotification);
    };
    addAndMakeVisible (*formulaInputEditor);
    audioProcessor.setFormula (formulaInputEditor->getText());

    formulaDisplayEditor = std::make_unique<juce::TextEditor>();
    formulaDisplayEditor->setMultiLine (false);
    formulaDisplayEditor->setText ("tanh(x)", juce::dontSendNotification);
    addAndMakeVisible (*formulaDisplayEditor);
}

NeuroCoreAudioProcessorEditor::~NeuroCoreAudioProcessorEditor()
{
}

//==============================================================================
void NeuroCoreAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

}

void NeuroCoreAudioProcessorEditor::resized()
{
    const int thirdWidth = 400;
    const int rowHeight  = getHeight() / 4;
    const int sliderSize = rowHeight;
    const int textHeight = 30;

    for (int i = 0; i < sliders.size(); ++i)
    {
        int y = i * rowHeight;
        sliders[i]->setBounds (0, y, sliderSize, sliderSize);
        sliderEditors[i]->setBounds (sliderSize, y + (rowHeight - textHeight) / 2,
                                     sliderSize, textHeight);
    }

    auto middleX = thirdWidth;
    int middleHeight = getHeight();
    int inputHeight = middleHeight * 2 / 3;

    formulaInputEditor->setBounds (middleX, 0, thirdWidth, inputHeight);
    formulaDisplayEditor->setBounds (middleX, inputHeight, thirdWidth,
                                     middleHeight - inputHeight);
}
