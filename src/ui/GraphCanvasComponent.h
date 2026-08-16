#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
#include <vector>
#include "../dsl/GraphModel.h"
#include "../core/Config.h"
#include "../core/PluginProcessor.h"
#include "fx/CyberFxTypes.h"

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
    std::function<void (int nodeIndex)> onInspectNode;

    void commitDocument();
    void setMotion (CyberMotion m) noexcept { motion = m; }
    void setHoverKnob (int knobIndex);
    CyberMotion getMotion() const noexcept { return motion; }

    /** Draw knob→node traces in editor space. `alphaMul` scales the 50% base. */
    void paintKnobCables (juce::Graphics& g, juce::Component& space, float alphaMul = 1.f) const;
    juce::Point<float> knobJackOnNode (int nodeIndex, int knobIndex) const;

    void setScript (const juce::String& script);
    juce::String getEmittedScript() const;
    bool hasValidGraph() const noexcept { return parseOk; }
    juce::String getParseError() const { return parseError; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void visibilityChanged() override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    static constexpr int kCardWidth = 208;
    static constexpr int kCardHeight = 84;
    static constexpr int kJackPitch = 16;
    static constexpr int kJackPad = 14;
    static constexpr int kIoWidth = 84;
    static constexpr int kIoHeight = 38;
    static constexpr int kChipMinH = 56;
    static constexpr int kChipArgRowH = 15;
    static constexpr int kGrid = 16;
    static constexpr float kZoomMin = 0.55f;
    static constexpr float kZoomMax = 2.4f;

    /** Folded = compact caption; expanded = every arg row plus jack pitch. */
    static int chipHeight (int nInJacks, int nOutJacks, int nArgs, int nBinds,
                           bool expanded) noexcept;
    /** Painted chevron box in local card pixels (title row, inset from jacks). */
    static juce::Rectangle<int> foldChevronRect (int cardW, int cardH,
                                                 bool hasSidechain, float zoom) noexcept;
    /** Click target around the chevron. Empty for IN/OUT. */
    static juce::Rectangle<int> foldHitRect (int cardW, int cardH,
                                             bool hasSidechain, float zoom) noexcept;

    bool isNodeExpanded (int nodeIndex) const;
    void setNodeExpanded (int nodeIndex, bool on);
    int nodeViewHeight (int nodeIndex) const;
    juce::Rectangle<int> nodeFoldHit (int nodeIndex) const;

    float getZoom() const noexcept { return zoom; }
    void setZoom (float z);
    float mappedKnobValue (int knobIndex) const;
    static juce::String formatLiveKnob (float v);
    static constexpr float kCableBeadGate = 0.022f;
    static bool cableBeadsVisible (float level) noexcept { return level >= kCableBeadGate; }
    static float loudnessToCableLevel (float db) noexcept;
    static juce::Colour cableTraceColour (bool hot, float alpha) noexcept;
    static float cableTapEnergy (const float* wave, int n) noexcept;
    void refreshCableMeters();
    float getCableInLevel() const noexcept { return inLevel; }
    float getCableOutLevel() const noexcept { return outLevel; }

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
    bool pendingAutoArrange { false };
    CyberMotion motion { CyberMotion::Full };
    float zoom { 1.f };
    static constexpr int kTapN = 64;
    std::array<float, kTapN> inWave {};
    std::array<float, kTapN> outWave {};
    std::map<juce::String, std::array<float, kTapN>> nodeWaves;
    juce::AudioBuffer<float> scopeCap { Config::kMaxChannels, Config::kWaveformDisplaySamples };
    std::vector<float> scopeMono;
    int hoverKnob { -1 };
    int hoverNode { -99 };
    int hoverDropNode { -99 };
    juce::String hoverDropJack;
    int dropInsertBefore { -99 };
    juce::String dropRail;
    int movingNode { -99 };
    mutable std::vector<dsl::GraphEdge> cachedEdges;
    mutable bool edgesDirty { true };
    juce::StringArray expandedNames;

    void timerCallback() override;
    void drawLiveCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                        float level, bool mix, bool hot, const float* wave, int waveN,
                        bool forceWave = false) const;
    void drawModLauflicht (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                           const float* wave, int waveN, bool hot) const;
    int scaled (int v) const noexcept { return (int) std::lround ((float) v * zoom); }
    void pullCableWaves();
    const float* waveForEdge (int fromIndex) const;

    juce::Viewport viewport;
    std::unique_ptr<Paper> paper;
    std::vector<std::unique_ptr<NodeView>> nodeViews;

    void rebuildViews();
    void autoLayout();
    void tidyCircuit();
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
    void removeSelected(); // used by NodeInspectComponent
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
    friend class NodeInspectComponent;
};
