// WeightedLayout.h
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

namespace ui
{
    enum class LayoutType { Row, Column, Leaf };

    struct LayoutNode
    {
        // --- Layout-Parameter ---
        LayoutType type{ LayoutType::Leaf };
        float      weight{ 1.0f };
        int        margin{ 0 };       ///< Outer-Padding
        int        innerMargin{ 0 };  ///< Gap zwischen Kindern
        bool       drawBorder{ false };

        // --- für Border-Zeichnen ---
        juce::Rectangle<int> lastBounds;  ///< wird in performLayout() gesetzt :contentReference[oaicite:0]{index=0}

        // --- Sichtbarkeits-Flags ---
        bool                  showHideEnabled{ false };
        std::function<bool()> visibleWhen;

        // --- Größen-Beschränkungen ---
        int minWidth{ 0 }, minHeight{ 0 };
        int maxWidth{ std::numeric_limits<int>::max() },
            maxHeight{ std::numeric_limits<int>::max() };

        // --- Leaf-spezifisch ---
        juce::Component* component{ nullptr };
        float            aspectRatio{ 0.0f };
        std::function<void()> onLayoutFinished;

        // --- Kinder ---
        std::vector<std::unique_ptr<LayoutNode>> children;
        LayoutNode() = default;
        explicit LayoutNode(LayoutType t) : type(t) {}
        void addChild(std::unique_ptr<LayoutNode> child) { children.push_back(std::move(child)); }

        /** Layout + Border-Zeichnen in einem Rutsch. */
        void layoutAndDraw(juce::Graphics& g, juce::Rectangle<int> bounds) noexcept;
    };

    // Factory-Funktionen
    inline std::unique_ptr<LayoutNode> makeRow(float weight = 1.0f, int margin = 0, bool border = false)
    {
        auto n = std::make_unique<LayoutNode>(LayoutType::Row);
        n->weight = weight;
        n->margin = margin;
        n->drawBorder = border;
        return n;
    }
    inline std::unique_ptr<LayoutNode> makeColumn(float weight = 1.0f, int margin = 0, bool border = false)
    {
        auto n = std::make_unique<LayoutNode>(LayoutType::Column);
        n->weight = weight;
        n->margin = margin;
        n->drawBorder = border;
        return n;
    }
    inline std::unique_ptr<LayoutNode> makeLeaf(juce::Component* c, float weight = 1.0f, float aspect = 0.0f)
    {
        auto n = std::make_unique<LayoutNode>(LayoutType::Leaf);
        n->component = c;
        n->weight = weight;
        n->aspectRatio = aspect;
        return n;
    }

    /** Nur die Bounds-Berechnung (nimmt margin & innerMargin in account). */
    void performLayout(LayoutNode& node, juce::Rectangle<int> bounds);

    /** Zeichnet alle Borders anhand der in performLayout gesetzten lastBounds. */
    void drawBorders(const LayoutNode& node, juce::Graphics& g);
}
