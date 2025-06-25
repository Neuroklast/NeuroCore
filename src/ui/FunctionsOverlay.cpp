#include "FunctionsOverlay.h"
#include "../third_party/nlohmann/json.hpp"
#include "../utils/ExpressionEvaluator.h"
#include "../core/Config.h"

using json = nlohmann::json;

void FunctionPlotComponent::setFormula(const juce::String& f)
{
    formula = f;
    values.clear();
    ExpressionEvaluator eval;
    eval.parseFormula(formula.toStdString());
    const int num = 64;
    values.resize(num);
    for (int i=0;i<num;++i)
    {
        float x = juce::jmap((float)i, 0.f, (float)(num-1), -1.f, 1.f);
        eval.setVariable("x", x);
        values[i] = eval.evaluate(x);
    }
    repaint();
}

void FunctionPlotComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    if (values.empty()) return;
    juce::Path p;
    auto area = getLocalBounds().toFloat();
    float midY = area.getCentreY();
    p.startNewSubPath(area.getX(), midY - values[0]*area.getHeight()/2);
    for (size_t i=1;i<values.size();++i)
    {
        float x = area.getX() + ((float)i / (values.size()-1)) * area.getWidth();
        float y = midY - values[i]*area.getHeight()/2;
        p.lineTo(x,y);
    }
    g.setColour(juce::Colours::white);
    g.strokePath(p, juce::PathStrokeType(1.2f));
}

FunctionsOverlay::FunctionsOverlay(NeuroCoreAudioProcessor& p) : processor(p)
{
    setOpaque(false);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(searchField);
    addAndMakeVisible(listBox);
    addAndMakeVisible(insertButton);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(descLabel);
    addAndMakeVisible(exampleLabel);
    addAndMakeVisible(extraLabel);
    addAndMakeVisible(plot);

    searchField.onTextChange = [this]{ filterList(); };
    listBox.setRowHeight(20);
    insertButton.onClick = [this]{ if(currentIndex>=0 && onInsert) onInsert(allFunctions[filtered[currentIndex]].example); if(onClose) onClose(); };
    closeButton.onClick = [this]{ if(onClose) onClose(); };

    loadFunctions();
    filterList();
}

void FunctionsOverlay::loadFunctions()
{
    juce::File res = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                        .getSiblingFile(Config::kResourceFolder)
                        .getChildFile("locale");
    juce::File f = res.getChildFile(processor.getCurrentLanguage().startsWithIgnoreCase("de") ? "functions_de.txt" : "functions_en.txt");
    juce::String content;
    if (f.existsAsFile())
        content = f.loadFileAsString();
    else if (processor.getCurrentLanguage().startsWithIgnoreCase("de"))
        content = juce::String::fromUTF8(BinaryData::functions_de_txt, BinaryData::functions_de_txtSize);
    else
        content = juce::String::fromUTF8(BinaryData::functions_en_txt, BinaryData::functions_en_txtSize);

    auto j = json::parse(content.toStdString(), nullptr, false);
    if (!j.is_object()) return;
    for (auto& v : j["functions"])
    {
        FunctionInfo info;
        info.name = v["name"].get<std::string>();
        info.description = v["description"].get<std::string>();
        info.soundCharacter = v["soundCharacter"].get<std::string>();
        info.example = v["example"].get<std::string>();
        for (auto& u : v["keywords"]) info.keywords.add(u.get<std::string>());
        for (auto& u : v["useCases"]) info.useCases.add(u.get<std::string>());
        info.domain = v["dangersAndLimits"]["domain"].get<std::string>();
        info.aliasing = v["dangersAndLimits"]["aliasing"].get<std::string>();
        info.performance = v["dangersAndLimits"]["performance"].get<std::string>();
        allFunctions.push_back(std::move(info));
    }
}

void FunctionsOverlay::filterList()
{
    juce::String q = searchField.getText().toLowerCase();
    filtered.clear();
    for (int i=0;i<(int)allFunctions.size();++i)
    {
        auto name = allFunctions[i].name.toLowerCase();
        bool match = name.contains(q);
        if (!match)
            for (auto& k : allFunctions[i].keywords)
                if (k.toLowerCase().contains(q)) { match=true; break; }
        if (match)
            filtered.push_back(i);
    }
    listBox.updateContent();
    if(!filtered.empty()) { listBox.selectRow(0); selectedRowsChanged(0); }
}

int FunctionsOverlay::getNumRows() { return (int)filtered.size(); }

void FunctionsOverlay::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (!juce::isPositiveAndBelow(row, filtered.size())) return;
    if (selected) g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    g.drawText(allFunctions[filtered[row]].name, 4,0,width,height, juce::Justification::centredLeft);
}

void FunctionsOverlay::selectedRowsChanged(int row)
{
    currentIndex = row;
    if (juce::isPositiveAndBelow(row, filtered.size()))
        updateDetails(filtered[row]);
}

void FunctionsOverlay::updateDetails(int index)
{
    auto& f = allFunctions[index];
    nameLabel.setText(f.name, juce::dontSendNotification);
    descLabel.setText(f.description + "\n" + f.soundCharacter, juce::dontSendNotification);
    exampleLabel.setText(f.example, juce::dontSendNotification);
    extraLabel.setText("Domain: " + f.domain + "\nAliasing: " + f.aliasing + "\nPerformance: " + f.performance, juce::dontSendNotification);
    juce::String formula = f.example.fromFirstOccurrenceOf("=", false, false).trim();
    plot.setFormula(formula);
}

void FunctionsOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.5f));
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(getLocalBounds().reduced(40));
    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds().reduced(40));
}

void FunctionsOverlay::resized()
{
    auto area = getLocalBounds().reduced(40);
    auto left = area.removeFromLeft(area.getWidth()*6/10);
    searchField.setBounds(left.removeFromTop(24));
    listBox.setBounds(left);

    auto right = area;
    nameLabel.setBounds(right.removeFromTop(24));
    descLabel.setBounds(right.removeFromTop(60));
    plot.setBounds(right.removeFromTop(80));
    exampleLabel.setBounds(right.removeFromTop(24));
    extraLabel.setBounds(right.removeFromTop(60));

    auto buttons = right.removeFromBottom(30);
    insertButton.setBounds(buttons.removeFromLeft(80));
    closeButton.setBounds(buttons.removeFromLeft(80));
}

bool FunctionsOverlay::keyPressed(const juce::KeyPress& kp)
{
    if (kp == juce::KeyPress::escapeKey) { if(onClose) onClose(); return true; }
    if (kp == juce::KeyPress::returnKey) { insertButton.triggerClick(); return true; }
    return false;
}
