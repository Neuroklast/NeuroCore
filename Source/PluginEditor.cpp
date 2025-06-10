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
        addAndMakeVisible (*sliders[i]);

        sliderEditors[i] = std::make_unique<juce::TextEditor>();
        sliderEditors[i]->setMultiLine (false);
        addAndMakeVisible (*sliderEditors[i]);
    }

    formulaInputEditor = std::make_unique<juce::TextEditor>();
    formulaInputEditor->setMultiLine (true, true);
    formulaInputEditor->setReturnKeyStartsNewLine (true);
    addAndMakeVisible (*formulaInputEditor);

    formulaDisplayEditor = std::make_unique<juce::TextEditor>();
    formulaDisplayEditor->setMultiLine (false);
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
