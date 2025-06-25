#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include <memory>

namespace ui
{
    /** Type of layout node. */
    enum class LayoutType { Row, Column, Leaf };

    /** Layout node for weighted, recursive layout. */
    struct LayoutNode
    {
        LayoutType type { LayoutType::Leaf };
        float weight { 1.0f };
        int margin { 0 };
        bool drawBorder { false };
        juce::Component* component { nullptr }; ///< Only for leaf nodes
        float aspectRatio { 0.0f };
        bool showHideEnabled { false };
        std::function<bool()> visibleWhen; ///< Returns true if node should be visible
        int minWidth { 0 };
        int minHeight { 0 };
        int maxWidth { std::numeric_limits<int>::max() };
        int maxHeight { std::numeric_limits<int>::max() };
        int innerMargin { 0 };
        std::function<void()> onLayoutFinished;
        std::vector<std::unique_ptr<LayoutNode>> children;

        LayoutNode() = default;
        explicit LayoutNode(LayoutType t) : type(t) {}
        /** Adds a child node. */
        void addChild(std::unique_ptr<LayoutNode> child) { children.push_back(std::move(child)); }
    };

    /** Creates a row container node. */
    inline std::unique_ptr<LayoutNode> makeRow(float weight = 1.0f, int margin = 0, bool border = false)
    {
        auto n = std::make_unique<LayoutNode>(LayoutType::Row);
        n->weight = weight;
        n->margin = margin;
        n->drawBorder = border;
        return n;
    }

    /** Creates a column container node. */
    inline std::unique_ptr<LayoutNode> makeColumn(float weight = 1.0f, int margin = 0, bool border = false)
    {
        auto n = std::make_unique<LayoutNode>(LayoutType::Column);
        n->weight = weight;
        n->margin = margin;
        n->drawBorder = border;
        return n;
    }

    /** Creates a leaf node for a JUCE component. */
    inline std::unique_ptr<LayoutNode> makeLeaf(juce::Component* c, float weight = 1.0f, float aspect = 0.0f)
    {
        auto n = std::make_unique<LayoutNode>(LayoutType::Leaf);
        n->component = c;
        n->weight = weight;
        n->aspectRatio = aspect;
        return n;
    }

    /** Performs layout on the given node. */
    void performLayout(LayoutNode& node, juce::Rectangle<int> bounds);
}

