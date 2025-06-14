/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "../core/PluginProcessor.h"
#include "PluginEditor.h"
#include "../core/Config.h"
#include "../utils/FormulaHelper.h"
#include "FormulaWaveComponent.h"
#include "PluginLookAndFeel.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
   

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
        auto paramId = juce::String::charToString (static_cast<juce_wchar>('a' + i));
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramId, *sliders[i]));
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

    compileButton = std::make_unique<juce::TextButton>(TRANS("CompileButton"));
    compileButton->onClick = [this]
    {
        if (formulaInputEditor != nullptr)
        {
            audioProcessor.setFormula (formulaInputEditor->getText());
            auto err = audioProcessor.getEvaluator().getLastError();
            errorLabel->setText (err.isNotEmpty() ? TRANS("CompileError") + ": " + err
                                              : juce::String(),
                                juce::dontSendNotification);
            formulaDisplay->setFormula (formulaInputEditor->getText());
            formulaDisplay->setError (err);
        }
    };
    addAndMakeVisible (*compileButton);

    errorLabel = std::make_unique<juce::Label>();
    errorLabel->setColour (juce::Label::textColourId, juce::Colours::red);
    addAndMakeVisible (*errorLabel);


    setSize(Config::kWindowWidth, Config::kWindowHeight);

}

NeuroCoreAudioProcessorEditor::~NeuroCoreAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

juce::String NeuroCoreAudioProcessorEditor::getFormulaText() const
{
    return formulaInputEditor ? formulaInputEditor->getText() : juce::String();
}

void NeuroCoreAudioProcessorEditor::setFormulaText(const juce::String& text)
{
    if (formulaInputEditor)
        formulaInputEditor->setText (text, juce::dontSendNotification);
}

//==============================================================================
void NeuroCoreAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

}

void NeuroCoreAudioProcessorEditor::resized()
{
    const int thirdWidth = Config::kMiddleColumnWidth;
    const int rowHeight  = Config::kKnobHeight + Config::kKnobSpacing;
    const int sliderSize = Config::kKnobHeight;
    const int textHeight = Config::kLabelHeight;

    for (int i = 0; i < sliders.size(); ++i)
    {
        int y = i * rowHeight;
        if (sliders[i])
            sliders[i]->setBounds (0, y, sliderSize, sliderSize);
        if (valueEditors[i])
            valueEditors[i]->setBounds (sliderSize, y, sliderSize, textHeight);
        if (nameEditors[i])
            nameEditors[i]->setBounds (sliderSize, y + textHeight, sliderSize, textHeight);
    }

    auto middleX = thirdWidth;
    int middleHeight = getHeight();
    int inputHeight = middleHeight * 2 / 3;

    formulaInputEditor->setBounds (middleX, 0, thirdWidth, inputHeight);
    if(formulaDisplay)
    formulaDisplay->setBounds (middleX, inputHeight, thirdWidth, textHeight);
	if (compileButton)
    compileButton->setBounds  (middleX, inputHeight + textHeight, thirdWidth, textHeight);
	if (optimizeButton)
    optimizeButton->setBounds (middleX, inputHeight + 2 * textHeight, thirdWidth, textHeight);
	if (errorLabel)
    errorLabel->setBounds     (middleX, inputHeight + 3 * textHeight, thirdWidth, textHeight);
}

void NeuroCoreAudioProcessorEditor::showAutocomplete()
{
    auto caret = formulaInputEditor->getCaretPosition();
    auto text  = formulaInputEditor->getText();
    int start = caret;
    while (start > 0 && juce::CharacterFunctions::isLetterOrDigit (text[start - 1]))
        --start;
    juce::String prefix = text.substring (start, caret);

    if (prefix.length() < 1)
        return;

    juce::PopupMenu menu;
    for (auto& t : formulaTemplates)
        if (t.name.startsWithIgnoreCase(prefix))
            menu.addItem (t.name, [this, caret, start, prefix, t]
            {
                formulaInputEditor->setCaretPosition (start);
                formulaInputEditor->insertTextAtCaret (t.name.substring (prefix.length()));
            });
    for (auto& f : builtinFunctions)
        if (f.startsWithIgnoreCase(prefix))
            menu.addItem (f, [this, caret, start, prefix, f]
            {
                formulaInputEditor->setCaretPosition (start);
                formulaInputEditor->insertTextAtCaret (f.substring (prefix.length()));
            });
    for (auto& n : audioProcessor.getVariableNames())
        if (n.startsWithIgnoreCase(prefix))
            menu.addItem (n, [this, caret, start, prefix, n]
            {
                formulaInputEditor->setCaretPosition (start);
                formulaInputEditor->insertTextAtCaret (n.substring (prefix.length()));
            });
    if (menu.getNumItems() > 0)
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (*formulaInputEditor));

}
