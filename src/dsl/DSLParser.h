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

class DSLParser
{
public:
    DSLParser() = default;
    bool parse(const juce::String& text,
               std::vector<BlockDesc>& blocks,
               std::unordered_map<juce::String, juce::String>& paramAliases,
               juce::String& error);
};

} // namespace dsl

#endif // DSLPARSER_H
