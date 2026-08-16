#pragma once

#include <JuceHeader.h>
#include "../dsl/GraphModel.h"
#include "../core/PluginProcessor.h"

/** Free 2D node patcher. Audio is still the DSL chain; positions are visual. */
class GraphCanvasComponent : public juce::Component,
                             private juce::Timer
{
public:
    explicit GraphCanvasComponent (NeuroKoreAudioProcessor& processor);
    ~GraphCanvasComponent() override;

    std::function<void()> onScriptChanged;
    std::function<void (juce::String slot)> onOpenIr;
    std::function<juce::Component*(int)> knobForIndex;

    /** Draw knob→node traces in editor space (clipped by the caller). */
    void paintKnobCables (juce::Graphics& g, juce::Component& space) const;
    juce::Point<float> knobJackOnNode (int nodeIndex, int knobIndex) const;

    void setScript (const juce::String& script);
    juce::String getEmittedScript() const;
    bool hasValidGraph() const noexcept { return parseOk; }
    juce::String getParseError() const { return parseError; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void visibilityChanged() override;

    static constexpr int kCardWidth = 160;
    static constexpr int kCardHeight = 62;
    static constexpr int kIoWidth = 72;
    static constexpr int kIoHeight = 32;
    static constexpr int kGrid = 16;

    static int snap (int v) noexcept
    {
        const int g = kGrid;
        if (v >= 0)
            return ((v + g / 2) / g) * g;
        return -(((-v) + g / 2) / g) * g;
    }
    static juce::Point<int> snapPoint (juce::Point<int> p) noexcept
    {
        return { snap (p.x), snap (p.y) };
    }

private:
    static constexpr int kInIndex = -1;
    static constexpr int kOutIndex = -2;
    static constexpr int kNodeW = kCardWidth;
    static constexpr int kNodeH = kCardHeight;
    static constexpr int kPortR = 6;

    class NodeView;
    class Paper;

    NeuroKoreAudioProcessor& processor;
    dsl::GraphDocument document;
    bool parseOk { false };
    juce::String parseError;
    juce::String lastScript;
    juce::Point<float> inPos { 32.f, 160.f };
    juce::Point<float> outPos { 512.f, 160.f };
    int selected { -99 };
    int cableFrom { -99 };
    juce::String cableFromJack;
    bool cablePickup { false };
    juce::Point<float> rubber;
    bool panning { false };
    juce::Point<int> panStart;
    float inLevel { 0.f };
    float outLevel { 0.f };
    float cablePhase { 0.f };
    bool rebuilding { false };
    int hoverKnob { -1 };
    int hoverNode { -99 };
    int hoverDropNode { -99 };
    juce::String hoverDropJack;
    int dropInsertBefore { -99 };
    juce::String dropRail;
    int movingNode { -99 };
    mutable std::vector<dsl::GraphEdge> cachedEdges;
    mutable bool edgesDirty { true };

    void timerCallback() override;
    void drawLiveCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                        float level, bool mix, bool hot) const;

    juce::Viewport viewport;
    std::unique_ptr<Paper> paper;
    std::vector<std::unique_ptr<NodeView>> nodeViews;

    void rebuildViews();
    void autoLayout();
    void applyGraph();
    void addBlock (const juce::String& type, juce::Point<int> at);
    void addRouting (const juce::String& kind, juce::Point<int> at);
    void addBlockMenu (juce::Point<int> at);
    void updateCableHover (juce::Point<int> paperPos);
    void commitNodeDrop (int nodeIndex);
    juce::String nearestRailAt (int y) const;
    void showNodeMenu (int nodeIndex);
    void editNodeArgs (int nodeIndex, const juce::String& onlyKey = {});
    void bindKnob (int nodeIndex, const juce::String& key, int knobIndex);
    void removeSelected();
    void commitOpenEdits();
    juce::String nextName (const juce::String& typeStem) const;
    int outIndex() const;
    NodeView* viewAt (juce::Point<int> paperPos) const;
    NodeView* viewFor (int nodeIndex) const;
    std::vector<dsl::GraphJack> jacksOf (int nodeIndex) const;
    juce::Point<float> portCentre (int nodeIndex, bool output) const;
    juce::Point<float> portCentre (int nodeIndex, const juce::String& jackId, bool outputIfEmpty) const;
    void beginCable (int fromIndex, bool pickup = false, juce::String fromJack = {});
    void pickupIncoming (int toIndex, const juce::String& toJack = {});
    void finishCable (int toIndex, const juce::String& toJack = {});
    int hitCable (juce::Point<float> paperPos) const;
    void markEdgesDirty() noexcept { edgesDirty = true; }
    void layoutPaper();
    void repaintPaper();
    juce::Point<int> toPaper (juce::Component& from, juce::Point<int> p) const;

    friend class NodeView;
    friend class Paper;
};
