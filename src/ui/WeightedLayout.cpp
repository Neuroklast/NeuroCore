// WeightedLayout.cpp
#include "WeightedLayout.h"

using namespace juce;

namespace ui
{
    // Hilfsfunktionen: Min/Max und Aspect-Ratio
    static Rectangle<int> applyMinMax(const Rectangle<int>& r, const LayoutNode& n)
    {
        auto w = jlimit(n.minWidth, n.maxWidth, r.getWidth());
        auto h = jlimit(n.minHeight, n.maxHeight, r.getHeight());
        return r.withSizeKeepingCentre(w, h);
    }

    static Rectangle<int> applyAspectRatio(const Rectangle<int>& r, float ratio)
    {
        if (ratio <= 0.0f) return r;
        float w = (float)r.getWidth(), h = (float)r.getHeight(), cur = w / h;
        if (cur > ratio) w = h * ratio; else h = w / ratio;
        return r.withSizeKeepingCentre(roundToInt(w), roundToInt(h));
    }

    // Kern-Routine: rekursives Layout
    static void layoutNode(LayoutNode& node, Rectangle<int> bounds)
    {
        // 1) Outer-Margin und speichern für Border
        jassert(node.margin >= 0);
        bounds = bounds.reduced(node.margin);
        node.lastBounds = bounds;  // :contentReference[oaicite:1]{index=1}

        // 2) Sichtbarkeit prüfen
        bool visible = !node.showHideEnabled || (node.visibleWhen && node.visibleWhen());
        if (!visible && node.type == LayoutType::Leaf && node.component)
            node.component->setVisible(false);

        // 3) Leaf-Fall?
        if (node.type == LayoutType::Leaf)
        {
            if (visible && node.component)
            {
                auto area = applyMinMax(bounds, node);
                area = applyAspectRatio(area, node.aspectRatio);
                node.component->setBounds(area);
                node.component->setVisible(true);
            }
            if (node.onLayoutFinished) node.onLayoutFinished();
            return;
        }

        if (!visible)
        {
            for (auto& c : node.children)
                if (c->component) c->component->setVisible(false);
            return;
        }

        // 4) Kinder sammeln, Gesamtgewicht und verfügbaren Platz berechnen
        std::vector<LayoutNode*> kids;
        float totalW = 0.0f;
        int   avail = (node.type == LayoutType::Row) ? bounds.getWidth()
            : bounds.getHeight();
        int   cross = (node.type == LayoutType::Row) ? bounds.getHeight()
            : bounds.getWidth();

        for (auto& c : node.children)
        {
            bool kidVis = !c->showHideEnabled || (c->visibleWhen && c->visibleWhen());
            if (!kidVis)
            {
                if (c->component) c->component->setVisible(false);
                continue;
            }
            kids.push_back(c.get());
            totalW += c->weight;
        }

        if (kids.empty())
        {
            if (node.onLayoutFinished) node.onLayoutFinished();
            return;
        }
        if (totalW <= 0.0f) totalW = (float)kids.size();

        // 5) Inner-Margin **einmal** zwischen den Kindern abziehen
        const int gap = node.innerMargin;
        const int numGaps = std::max(0, (int)kids.size() - 1);
        avail -= gap * numGaps;  // :contentReference[oaicite:2]{index=2}

        // 6) Längen proportional verteilen (mit Min/Max-Loop)
        std::vector<int> lengths(kids.size(), 0);
        std::vector<bool> fixed(kids.size(), false);
        int remaining = avail;
        float remW = totalW;
        bool changed;
        do
        {
            changed = false;
            for (size_t i = 0; i < kids.size(); ++i)
            {
                if (fixed[i]) continue;
                int len = roundToInt((remaining * kids[i]->weight) / remW);
                int minL = (node.type == LayoutType::Row) ? kids[i]->minWidth
                    : kids[i]->minHeight;
                int maxL = (node.type == LayoutType::Row) ? kids[i]->maxWidth
                    : kids[i]->maxHeight;
                if (maxL <= 0) maxL = std::numeric_limits<int>::max();

                if (len < minL || len > maxL)
                {
                    lengths[i] = jlimit(minL, maxL, len);
                    fixed[i] = true;
                    remaining -= lengths[i];
                    remW -= kids[i]->weight;
                    changed = true;
                }
            }
        } while (changed && remW > 0.0f);

        // noch nicht fixierte verteilen
        for (size_t i = 0; i < kids.size(); ++i)
            if (!fixed[i])
                lengths[i] = roundToInt((remaining * kids[i]->weight) / remW);

        // Rundungsfehler ausgleichen
        int used = std::accumulate(lengths.begin(), lengths.end(), 0);
        lengths.back() += avail - used;

        // 7) Kinder positionieren, Gap nur zwischen
        int pos = (node.type == LayoutType::Row) ? bounds.getX() : bounds.getY();
        for (size_t i = 0; i < kids.size(); ++i)
        {
            Rectangle<int> cb = (node.type == LayoutType::Row)
                ? Rectangle<int>(pos, bounds.getY(), lengths[i], cross)
                : Rectangle<int>(bounds.getX(), pos, cross, lengths[i]);

            layoutNode(*kids[i], cb);
            pos += lengths[i] + ((i + 1 < kids.size()) ? gap : 0);
        }

        if (node.onLayoutFinished) node.onLayoutFinished();
    }

    void performLayout(LayoutNode& node, Rectangle<int> bounds)
    {
        layoutNode(node, bounds);
    }

    // Rekursive Border-Zeichnung
    static void drawBordersRec(const LayoutNode& node, Graphics& g)
    {
        if (node.drawBorder)
        {
            g.setColour(Colours::red);
            g.drawRect(node.lastBounds, 1);
        }
        for (auto& c : node.children)
            drawBordersRec(*c, g);
    }

    void drawBorders(const LayoutNode& node, juce::Graphics& g)
    {
        drawBordersRec(node, g);
    }

    void LayoutNode::layoutAndDraw(Graphics& g, Rectangle<int> bounds) noexcept
    {
        performLayout(*this, bounds);
        drawBorders(*this, g);
    }
}
