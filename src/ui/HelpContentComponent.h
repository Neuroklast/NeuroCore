#pragma once
#include <JuceHeader.h>
#include <vector>

struct HelpChapter
{
    juce::String title;
    juce::String body;
    int startChar { 0 };
};

std::vector<HelpChapter> parseHelpChapters (const juce::String& markdown);
/** Drop markdown markers so Help shows prose, not **stars** or ### headings. */
juce::String stripMarkdownToPlain (const juce::String& markdown);

class HelpContentComponent : public juce::Component,
                             private juce::ListBoxModel,
                             private juce::TextEditor::Listener
{
public:
    explicit HelpContentComponent (const juce::String& markdown);
    ~HelpContentComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void showChapter (int index);
    juce::String getDisplayedText() const { return body.getText(); }
    int getVisibleChapterCount() const { return (int) visible.size(); }
    juce::String getBodyTypefaceName() const { return body.getFont().getTypefaceName(); }

private:
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void rebuildVisible();
    void showFirstVisible();
    void applyBodyFont();
    static juce::String readableChapter (const HelpChapter& ch);

    juce::String fullText;
    std::vector<HelpChapter> chapters;
    std::vector<int> visible;
    juce::TextEditor search;
    juce::Label searchLabel;
    juce::ListBox chapterList;
    juce::TextEditor body;
};
