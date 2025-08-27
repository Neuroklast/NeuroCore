#ifndef DSLPARSER_H
#define DSLPARSER_H

#include <JuceHeader.h>
#include <vector>
#include <unordered_map>

namespace dsl
{

struct BlockDesc
{
    juce::String type;   // stage, filter, comp, env, osc
    juce::String name;   // stage1 etc
    std::unordered_map<juce::String, juce::String> args; // raw arguments
};

struct ParamDesc
{
    juce::String alias;      ///< Single letter alias (a,b,c,...)
    juce::String name;       ///< Human readable name
    float min { 0.f };       ///< Minimum value
    float max { 1.f };       ///< Maximum value
};

struct SyntaxError
{
    int line;                ///< Line number (1-based)
    int column;              ///< Column number (1-based)
    int length;              ///< Length of error span
    juce::String message;    ///< Error description
    enum Severity { Warning, Error, Critical } severity;
};

class DSLParser
{
public:
    DSLParser() = default;
    bool parse(const juce::String& text,
               std::vector<BlockDesc>& blocks,
               std::unordered_map<juce::String, juce::String>& paramAliases,
               std::vector<ParamDesc>& params,
               juce::String& error);
    
    /**
     * Check syntax without full parsing, providing detailed error information
     * @param text The DSL text to check
     * @param errors Vector to store found syntax errors
     * @return true if syntax is valid, false if errors were found
     */
    bool checkSyntax(const juce::String& text, std::vector<SyntaxError>& errors);
    
private:
    void addSyntaxError(std::vector<SyntaxError>& errors, int line, int column, 
                       int length, const juce::String& message, 
                       SyntaxError::Severity severity = SyntaxError::Error);
    bool validateBlockSyntax(const juce::String& line, int lineNumber, std::vector<SyntaxError>& errors);
    bool validateParamSyntax(const juce::String& line, int lineNumber, std::vector<SyntaxError>& errors);
};

} // namespace dsl

#endif // DSLPARSER_H
