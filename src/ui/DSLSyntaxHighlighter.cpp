#include "DSLSyntaxHighlighter.h"

DSLSyntaxHighlighter::DSLSyntaxHighlighter()
{
}

int DSLSyntaxHighlighter::readNextToken(juce::CodeDocument::Iterator& source)
{
    auto start = source;
    auto c = source.nextChar();
    
    // Skip whitespace
    if (juce::CharacterFunctions::isWhitespace(c))
    {
        source.skipWhitespace();
        return tokenType_whitespace;
    }
    
    // Comments
    if (c == '#' || (c == '/' && source.peekNextChar() == '/'))
    {
        source.skipToEndOfLine();
        return tokenType_comment;
    }
    
    // Numbers
    if (juce::CharacterFunctions::isDigit(c) || (c == '.' && juce::CharacterFunctions::isDigit(source.peekNextChar())))
    {
        while (juce::CharacterFunctions::isDigit(source.peekNextChar()) || source.peekNextChar() == '.')
            source.skip();
        return tokenType_number;
    }
    
    // Strings (quoted values)
    if (c == '"' || c == '\'')
    {
        juce::juce_wchar quote = c;
        while (!source.isEOF() && source.peekNextChar() != quote)
        {
            if (source.nextChar() == '\\')
                source.skip(); // Skip escaped character
        }
        if (!source.isEOF())
            source.skip(); // Skip closing quote
        return tokenType_string;
    }
    
    // Operators and punctuation
    if (c == '=' || c == ':' || c == '[' || c == ']' || c == '(' || c == ')')
    {
        return (c == '=' || c == ':') ? tokenType_operator : tokenType_punctuation;
    }
    
    // Identifiers and keywords
    if (juce::CharacterFunctions::isLetter(c) || c == '_')
    {
        while (juce::CharacterFunctions::isLetterOrDigit(source.peekNextChar()) || source.peekNextChar() == '_')
            source.skip();
            
        juce::String token(start, source);
        
        // Check line position to determine context
        auto lineStart = start;
        lineStart.skipToStartOfLine();
        int line = lineStart.getLine();
        int column = start.getIndexInLine();
        
        // Check for syntax errors at this position
        if (hasErrorAtPosition(line + 1, column + 1))
            return tokenType_error;
            
        if (token.startsWithIgnoreCase("param"))
            return tokenType_keyword;
        else if (isBlockType(token))
            return tokenType_blockType;
        else if (isKeyword(token))
            return tokenType_keyword;
        else
            return tokenType_identifier;
    }
    
    return tokenType_punctuation;
}

juce::CodeEditorComponent::ColourScheme DSLSyntaxHighlighter::getDefaultColourScheme()
{
    juce::CodeEditorComponent::ColourScheme scheme;
    
    scheme.set("Error", juce::Colour(0xffff4444));           // Bright red for errors
    scheme.set("Whitespace", juce::Colour(0xff000000));      // Black
    scheme.set("Comment", juce::Colour(0xff00aa00));         // Green
    scheme.set("Keyword", juce::Colour(0xff0000ff));         // Blue
    scheme.set("Block Type", juce::Colour(0xffaa00aa));      // Purple
    scheme.set("Parameter", juce::Colour(0xff006666));       // Teal
    scheme.set("Operator", juce::Colour(0xffaa6600));        // Orange
    scheme.set("Identifier", juce::Colour(0xff000000));      // Black
    scheme.set("String", juce::Colour(0xff880000));          // Dark red
    scheme.set("Number", juce::Colour(0xff880088));          // Magenta
    scheme.set("Punctuation", juce::Colour(0xff000000));     // Black
    
    return scheme;
}

void DSLSyntaxHighlighter::setSyntaxErrors(const std::vector<dsl::SyntaxError>& errors)
{
    syntaxErrors = errors;
}

bool DSLSyntaxHighlighter::hasErrorAtPosition(int line, int column) const
{
    for (const auto& error : syntaxErrors)
    {
        if (error.line == line && 
            column >= error.column && 
            column < (error.column + error.length))
        {
            return true;
        }
    }
    return false;
}

juce::String DSLSyntaxHighlighter::getErrorAtPosition(int line, int column) const
{
    for (const auto& error : syntaxErrors)
    {
        if (error.line == line && 
            column >= error.column && 
            column < (error.column + error.length))
        {
            return error.message;
        }
    }
    return {};
}

bool DSLSyntaxHighlighter::isKeyword(const juce::String& token) const
{
    static const juce::StringArray keywords = {
        "param", "stage", "filter", "comp", "env", "osc"
    };
    return keywords.contains(token.toLowerCase());
}

bool DSLSyntaxHighlighter::isBlockType(const juce::String& token) const
{
    static const juce::StringArray blockTypes = {
        "stage", "filter", "comp", "env", "osc",
        "hpf", "hp", "highpass", "lpf", "lp", "lowpass", 
        "bpf", "bp", "bandpass"
    };
    
    juce::String baseType = token.retainCharacters("abcdefghijklmnopqrstuvwxyz").toLowerCase();
    return blockTypes.contains(baseType);
}

bool DSLSyntaxHighlighter::isOperator(juce::CodeDocument::Iterator& source) const
{
    juce::juce_wchar c = source.peekPreviousChar();
    return c == '=' || c == ':';
}

DSLSyntaxHighlighter::TokenType DSLSyntaxHighlighter::getTokenTypeAtPosition(int line, int column) const
{
    if (hasErrorAtPosition(line, column))
        return tokenType_error;
    return tokenType_identifier; // Default
}

//==============================================================================

DSLCodeEditor::DSLCodeEditor(juce::CodeDocument& doc, DSLSyntaxHighlighter* highlighter)
    : juce::CodeEditorComponent(doc, highlighter), dslHighlighter(highlighter)
{
    setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
    setTabSize(4, true);
    setLineNumbersShown(true);
    
    // Set up colors for better visibility
    setColourScheme(highlighter->getDefaultColourScheme());
}

void DSLCodeEditor::paint(juce::Graphics& g)
{
    juce::CodeEditorComponent::paint(g);
    
    if (!showSyntaxErrors || !dslHighlighter)
        return;
        
    // Draw error backgrounds
    g.setColour(juce::Colour(0xffff4444).withAlpha(0.2f)); // Semi-transparent red
    
    auto doc = &getDocument();
    for (int line = 0; line < doc->getNumLines(); ++line)
    {
        auto lineText = doc->getLine(line);
        for (int col = 0; col < lineText.length(); ++col)
        {
            if (dslHighlighter->hasErrorAtPosition(line + 1, col + 1))
            {
                auto charBounds = getCharacterBounds({line, col});
                if (!charBounds.isEmpty())
                {
                    g.fillRect(charBounds);
                }
            }
        }
    }
}

void DSLCodeEditor::mouseMove(const juce::MouseEvent& e)
{
    juce::CodeEditorComponent::mouseMove(e);
    
    if (!showSyntaxErrors || !dslHighlighter)
        return;
        
    auto pos = getPositionAt(e.getMouseDownPosition());
    auto errorMsg = dslHighlighter->getErrorAtPosition(pos.getLineNumber() + 1, pos.getIndexInLine() + 1);
    
    if (errorMsg.isNotEmpty())
    {
        showErrorTooltip(e, errorMsg);
    }
    else
    {
        hideErrorTooltip();
    }
}

bool DSLCodeEditor::keyPressed(const juce::KeyPress& key)
{
    bool result = juce::CodeEditorComponent::keyPressed(key);
    
    // Update syntax errors after text changes
    if (key != juce::KeyPress::upKey && key != juce::KeyPress::downKey &&
        key != juce::KeyPress::leftKey && key != juce::KeyPress::rightKey)
    {
        updateSyntaxErrors();
    }
    
    return result;
}

void DSLCodeEditor::updateSyntaxErrors()
{
    if (!dslHighlighter)
        return;
        
    std::vector<dsl::SyntaxError> errors;
    dsl::DSLParser parser;
    parser.checkSyntax(getDocument().getAllContent(), errors);
    
    dslHighlighter->setSyntaxErrors(errors);
    repaint();
}

void DSLCodeEditor::showErrorTooltip(const juce::MouseEvent& e, const juce::String& errorMessage)
{
    if (errorMessage.isNotEmpty())
    {
        setTooltip(errorMessage);
    }
}

void DSLCodeEditor::hideErrorTooltip()
{
    setTooltip("");
}