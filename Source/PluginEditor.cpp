/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FormulaHelper.h"
#include "FormulaWaveComponent.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (1200, 800);

    static const juce::Colour defaultColours[4] = {
        juce::Colours::red, juce::Colours::green,
        juce::Colours::blue, juce::Colours::yellow };

    for (int i = 0; i < sliders.size(); ++i)
    {
        sliders[i] = std::make_unique<juce::Slider>();
        sliders[i]->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sliders[i]->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sliders[i]->setColour (juce::Slider::rotarySliderFillColourId, defaultColours[i]);
        sliderColours[i] = defaultColours[i];
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (
            audioProcessor.apvts, juce::String ("abcd"[i]), *sliders[i]));
        sliders[i]->onValueChange = [this, i]
        {
            if (valueEditors[i])
                valueEditors[i]->setText (juce::String (sliders[i]->getValue()), juce::dontSendNotification);
        };
        addAndMakeVisible (*sliders[i]);

        valueEditors[i] = std::make_unique<juce::TextEditor>();
        valueEditors[i]->setMultiLine (false);
        valueEditors[i]->setReadOnly (true);
        valueEditors[i]->setText ("0", juce::dontSendNotification);
        addAndMakeVisible (*valueEditors[i]);

        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText (audioProcessor.getVariableName(i), juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
        {
            audioProcessor.setVariableName(i, nameEditors[i]->getText());
            formulaDisplay->setVariableColours(audioProcessor.getVariableNames(), sliderColours);
        };
        addAndMakeVisible (*nameEditors[i]);
    }

    formulaInputEditor = std::make_unique<juce::TextEditor>();
    formulaInputEditor->setMultiLine (true, true);
    formulaInputEditor->setReturnKeyStartsNewLine (true);
    formulaInputEditor->setText ("tanh(x)", juce::dontSendNotification);
    formulaInputEditor->onTextChange = [this]
    {
        audioProcessor.setFormula (formulaInputEditor->getText());
        formulaDisplay->setFormula (formulaInputEditor->getText());
        formulaDisplay->setError (audioProcessor.getEvaluator().getLastError());
        showAutocomplete();
    };
    addAndMakeVisible (*formulaInputEditor);
    audioProcessor.setFormula (formulaInputEditor->getText());


    formulaDisplay = std::make_unique<FormulaDisplayComponent>();
    formulaDisplay->setVariableColours(audioProcessor.getVariableNames(), sliderColours);
    formulaDisplay->setFormula ("tanh(x)");
    addAndMakeVisible (*formulaDisplay);

    optimizeButton = std::make_unique<juce::TextButton>(TRANS("OptimizeButton"));
    optimizeButton->onClick = [this]
    {
        juce::String info;
        auto text = formulaInputEditor->getText();
        auto opt = optimizeFormula(text, info);
        if (opt != text)
            formulaInputEditor->setText (opt, juce::dontSendNotification);
        if (info.isNotEmpty())
            formulaDisplay->setError(info);
    };
    addAndMakeVisible (*optimizeButton);

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
        valueEditors[i]->setBounds (sliderSize, y, sliderSize, textHeight);
        nameEditors[i]->setBounds (sliderSize, y + textHeight, sliderSize, textHeight);
    }

    auto middleX = thirdWidth;
    int middleHeight = getHeight();
    int inputHeight = middleHeight * 2 / 3;

    formulaInputEditor->setBounds (middleX, 0, thirdWidth, inputHeight);

    formulaDisplay->setBounds (middleX, inputHeight, thirdWidth, middleHeight - inputHeight - textHeight);
    optimizeButton->setBounds (middleX, middleHeight - textHeight, thirdWidth, textHeight);
}

void NeuroCoreAudioProcessorEditor::showAutocomplete()
{
    auto caret = formulaInputEditor->getCaretPosition();
    auto text  = formulaInputEditor->getText();
    int start = caret; while (start > 0 && juce::CharacterFunctions::isLetterOrDigit(text[start-1])) --start;
    juce::String prefix = text.substring(start, caret);

    if (prefix.length() < 1)
        return;

    juce::PopupMenu menu;
    for (auto& t : formulaTemplates)
        if (t.name.startsWithIgnoreCase(prefix))
            menu.addItem(t.name, [this, caret, start, t]{
                formulaInputEditor->moveCaretToPosition(start);
                formulaInputEditor->insertTextAtCaret(t.name.substring(prefix.length()));
            });
    for (auto& f : builtinFunctions)
        if (f.startsWithIgnoreCase(prefix))
            menu.addItem(f, [this, caret, start, f]{
                formulaInputEditor->moveCaretToPosition(start);
                formulaInputEditor->insertTextAtCaret(f.substring(prefix.length()));
            });
    for (auto& n : audioProcessor.getVariableNames())
        if (n.startsWithIgnoreCase(prefix))
            menu.addItem(n, [this, caret, start, n]{
                formulaInputEditor->moveCaretToPosition(start);
                formulaInputEditor->insertTextAtCaret(n.substring(prefix.length()));
            });
    if (menu.getNumItems() > 0)
        menu.showAt(formulaInputEditor.get());

}
