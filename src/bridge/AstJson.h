#pragma once

#include "../dsl/GraphModel.h"

namespace dsl
{

/** Serialize GraphDocument to AstDocument JSON (version 1). */
juce::String toJson (const GraphDocument& doc);

/** Parse AstDocument JSON into GraphDocument. On failure dest is cleared. */
bool fromJson (const juce::String& json, GraphDocument& dest, juce::String& error);

} // namespace dsl
