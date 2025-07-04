#ifndef DSL_AST_H
#define DSL_AST_H

#include <JuceHeader.h>
#include <unordered_map>
#include <memory>
#include <vector>

namespace dsl
{

struct DslNode
{
    juce::String name;
    virtual ~DslNode() = default;
};

struct StageNode : DslNode
{
    juce::String formula;
};

struct FilterNode : DslNode
{
    juce::String type;
    std::unordered_map<juce::String, juce::String> args;
};

struct OscNode : DslNode
{
    juce::String shape;
    std::unordered_map<juce::String, juce::String> args;
};

struct CompNode : DslNode
{
    std::unordered_map<juce::String, juce::String> args;
};

struct EnvNode : DslNode
{
    juce::String mode;
    std::unordered_map<juce::String, juce::String> args;
};

struct ParamNode
{
    juce::String alias;
    juce::String name;
    float min{0.f};
    float max{1.f};
};

using Ast = std::vector<std::unique_ptr<DslNode>>;

} // namespace dsl

#endif // DSL_AST_H
