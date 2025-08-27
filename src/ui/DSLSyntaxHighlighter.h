#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "../dsl/DSLParser.h"

/**
    @class DSLSyntaxHighlighter
    @brief Syntax highlighter for the NeuroCore DSL with error highlighting support.
    
    Extends JUCE's CodeTokeniser to provide syntax highlighting for the DSL
    and supports highlighting syntax errors in red.
*/
class DSLSyntaxHighlighter : public juce::CodeTokeniser
{
public:
    DSLSyntaxHighlighter();
    
    // CodeTokeniser interface
    int readNextToken(juce::CodeDocument::Iterator& source) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;
    
    /**
     * Update the syntax errors for highlighting
     * @param errors Vector of syntax errors to highlight
     */
    void setSyntaxErrors(const std::vector<dsl::SyntaxError>& errors);
    
    /**
     * Check if a position is within a syntax error
     * @param line Line number (1-based)
     * @param column Column number (1-based)
     * @return true if position has a syntax error
     */
    bool hasErrorAtPosition(int line, int column) const;
    
    /**
     * Get error message at position if any
     * @param line Line number (1-based)
     * @param column Column number (1-based)
     * @return Error message or empty string if no error
     */
    juce::String getErrorAtPosition(int line, int column) const;

private:
    enum TokenType
    {
        tokenType_error = 0,
        tokenType_whitespace,
        tokenType_comment,
        tokenType_keyword,
        tokenType_blockType,
        tokenType_paramName,
        tokenType_operator,
        tokenType_identifier,
        tokenType_string,
        tokenType_number,
        tokenType_punctuation
    };
    
    std::vector<dsl::SyntaxError> syntaxErrors;
    dsl::DSLParser parser;
    
    bool isKeyword(const juce::String& token) const;
    bool isBlockType(const juce::String& token) const;
    bool isOperator(juce::CodeDocument::Iterator& source) const;
    TokenType getTokenTypeAtPosition(int line, int column) const;
};

/**
    @class DSLCodeEditor
    @brief Enhanced code editor with DSL syntax highlighting and error display.
    
    Extends JUCE's CodeEditorComponent with DSL-specific features including
    real-time syntax error highlighting and tooltip error messages.
*/
class DSLCodeEditor : public juce::CodeEditorComponent
{
public:
    DSLCodeEditor(juce::CodeDocument& doc, DSLSyntaxHighlighter* highlighter);
    
    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    /**
     * Update syntax checking and error highlighting
     */
    void updateSyntaxErrors();
    
    /**
     * Set whether to show syntax errors in real-time
     * @param show true to show errors, false to hide
     */
    void setShowSyntaxErrors(bool show) { showSyntaxErrors = show; }

private:
    DSLSyntaxHighlighter* dslHighlighter;
    bool showSyntaxErrors{true};
    juce::TooltipWindow tooltipWindow{this};
    
    void showErrorTooltip(const juce::MouseEvent& e, const juce::String& errorMessage);
    void hideErrorTooltip();
};