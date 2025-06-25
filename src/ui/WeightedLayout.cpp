#include "WeightedLayout.h"

using namespace juce;

namespace ui
{
    static Rectangle<int> applyAspectRatio(Rectangle<int> r, float ratio)
    {
        if (ratio <= 0.0f)
            return r;
        float w = (float) r.getWidth();
        float h = (float) r.getHeight();
        float current = w / h;
        if (current > ratio)
            w = h * ratio;
        else
            h = w / ratio;
        return r.withSizeKeepingCentre(roundToInt(w), roundToInt(h));
    }

    static Rectangle<int> applyMinMax(Rectangle<int> r, const LayoutNode& n)
    {
        auto w = jlimit(n.minWidth, n.maxWidth, r.getWidth());
        auto h = jlimit(n.minHeight, n.maxHeight, r.getHeight());
        r.setSize(w, h);
        return r;
    }

    static void layoutNode(LayoutNode& node, Rectangle<int> bounds)
    {
        jassert(node.margin >= 0);
        bounds = bounds.reduced(node.margin);

        bool visible = true;
        if (node.showHideEnabled && node.visibleWhen)
            visible = node.visibleWhen();

        if (node.type == LayoutType::Leaf)
        {
            if (node.component)
            {
                node.component->setVisible(visible);
                if (!visible)
                    return;
                auto area = applyMinMax(bounds, node);
                area = applyAspectRatio(area, node.aspectRatio);
                node.component->setBounds(area);
            }
            if (node.onLayoutFinished)
                node.onLayoutFinished();
            return;
        }

        if (!visible)
        {
            for (auto& c : node.children)
                if (c->component)
                    c->component->setVisible(false);
            return;
        }

        std::vector<LayoutNode*> children;
        float totalWeight = 0.0f;
        int available = node.type == LayoutType::Row ? bounds.getWidth() : bounds.getHeight();
        int cross = node.type == LayoutType::Row ? bounds.getHeight() : bounds.getWidth();

        for (auto& ch : node.children)
        {
            bool childVisible = true;
            if (ch->showHideEnabled && ch->visibleWhen)
                childVisible = ch->visibleWhen();
            if (!childVisible)
            {
                if (ch->component)
                    ch->component->setVisible(false);
                continue;
            }
            children.push_back(ch.get());
            totalWeight += ch->weight;
            available -= ch->innerMargin;
        }

        if (children.empty())
        {
            if (node.onLayoutFinished)
                node.onLayoutFinished();
            return;
        }

        if (totalWeight == 0.0f)
            totalWeight = (float) children.size();

        int remaining = available;
        float remWeight = totalWeight;
        std::vector<int> lengths(children.size(), 0);
        std::vector<bool> fixed(children.size(), false);
        bool changed;

        do
        {
            changed = false;
            for (size_t i = 0; i < children.size(); ++i)
            {
                if (fixed[i])
                    continue;
                int len = roundToInt((remaining * children[i]->weight) / remWeight);
                int minL = node.type == LayoutType::Row ? children[i]->minWidth : children[i]->minHeight;
                int maxL = node.type == LayoutType::Row ? children[i]->maxWidth : children[i]->maxHeight;
                if (maxL <= 0)
                    maxL = std::numeric_limits<int>::max();
                if (len < minL)
                {
                    lengths[i] = minL;
                    fixed[i] = true;
                    remaining -= minL;
                    remWeight -= children[i]->weight;
                    changed = true;
                }
                else if (len > maxL)
                {
                    lengths[i] = maxL;
                    fixed[i] = true;
                    remaining -= maxL;
                    remWeight -= children[i]->weight;
                    changed = true;
                }
            }
        } while (changed && remWeight > 0.0f);

        for (size_t i = 0; i < children.size(); ++i)
        {
            if (!fixed[i])
            {
                int len = roundToInt((remaining * children[i]->weight) / remWeight);
                lengths[i] = len;
            }
        }

        int used = 0;
        for (auto l : lengths)
            used += l;
        lengths.back() += available - used;

        int pos = node.type == LayoutType::Row ? bounds.getX() : bounds.getY();
        for (size_t i = 0; i < children.size(); ++i)
        {
            int len = lengths[i];
            Rectangle<int> childBounds;
            if (node.type == LayoutType::Row)
                childBounds = { pos, bounds.getY(), len, cross };
            else
                childBounds = { bounds.getX(), pos, cross, len };

            layoutNode(*children[i], childBounds);
            pos += len + children[i]->innerMargin;
        }

        if (node.onLayoutFinished)
            node.onLayoutFinished();
    }

    void performLayout(LayoutNode& node, Rectangle<int> bounds)
    {
        layoutNode(node, bounds);
    }
}

