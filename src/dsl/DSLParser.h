#ifndef DSLPARSER_H
#define DSLPARSER_H

#include <JuceHeader.h>
#include <vector>
#include <unordered_map>
#include "AST.h"

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

class DSLParser
{
public:
    DSLParser() = default;
    bool parse(const juce::String& text,
               std::vector<BlockDesc>& blocks,
               std::unordered_map<juce::String, juce::String>& paramAliases,
               std::vector<ParamDesc>& params,
               juce::String& error);

    /** Parses the script into an AST structure. */
    bool parseAST(const juce::String& text,
                  Ast& ast,
                  std::unordered_map<juce::String, juce::String>& paramAliases,
                  std::vector<ParamDesc>& params,
                  juce::String& error);

    /** Converts the AST into the intermediate representation. */
    static void astToIR(const Ast& ast, std::vector<BlockDesc>& blocks);
};

} // namespace dsl

#endif // DSLPARSER_H
