#ifndef DSLPARSER_H
#define DSLPARSER_H

#include <JuceHeader.h>
#include <vector>
#include <unordered_map>

namespace dsl
{

struct BlockDesc
{
    juce::String type;    // stage, filter, comp, env, osc, bus, send, out
    juce::String name;    // stage1 / dirt / send / out
    juce::String busName; // "main" or named bus; empty for bus/out headers
    std::unordered_map<juce::String, juce::String> args; // raw arguments
};

struct ParamDesc
{
    juce::String alias;      ///< Single letter alias (a,b,c,...)
    juce::String name;       ///< Human readable name
    float min { 0.f };       ///< Minimum value (linear) or first whole-note (note)
    float max { 1.f };       ///< Maximum value (linear) or last whole-note (note)
    bool  isNote { false };  ///< Range is musical divisions (1/1 .. 1/16)
    std::vector<float> noteWholes;
    std::vector<juce::String> noteLabels;
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
};

/** One-line summary for stage/filter/comp/osc/env list items. */
juce::String formatBlockSummary(const BlockDesc& block);

/** Multi-line detail text with all block arguments. */
juce::String formatBlockDetails(const BlockDesc& block);

} // namespace dsl

#endif // DSLPARSER_H
