#ifndef DSLPARSER_H
#define DSLPARSER_H

#include <JuceHeader.h>
#include <vector>
#include <unordered_map>

namespace dsl
{

enum class Scope
{
    Global = 0,
    Left,
    Right,
    Mid,
    Side,
    Low,
    MidBand,
    High,
    Count
};

struct BlockDesc
{
    juce::String type;   // stage, filter, comp, env, osc
    juce::String name;   // stage1 etc
    std::unordered_map<juce::String, juce::String> args; // raw arguments
    Scope scope { Scope::Global };
};

struct ScopeRange
{
    juce::String low;
    juce::String high;
};

class DSLParser
{
public:
    DSLParser() = default;
    bool parse(const juce::String& text,
               std::vector<BlockDesc>& blocks,
               std::unordered_map<juce::String, juce::String>& paramAliases,
               std::unordered_map<Scope, ScopeRange>& ranges,
               juce::String& error);
};

} // namespace dsl

#endif // DSLPARSER_H
