#ifndef WEIGHTEDLAYOUTTEST_H
#define WEIGHTEDLAYOUTTEST_H

#include <JuceHeader.h>
#include "../src/ui/WeightedLayout.h"

class WeightedLayoutTest : public juce::UnitTest
{
public:
    WeightedLayoutTest() : juce::UnitTest("WeightedLayoutTest", "UI") {}

    void runTest() override
    {
        using namespace ui;
        beginTest("Basic Row Layout");
        juce::Component a, b;
        auto root = makeRow();
        root->addChild(makeLeaf(&a, 1.0f));
        root->addChild(makeLeaf(&b, 1.0f));
        performLayout(*root, {0,0,200,100});
        expectEquals(a.getBounds().getX(), 0);
        expectEquals(a.getBounds().getWidth(), 100);
        expectEquals(b.getBounds().getX(), 100);
        expectEquals(b.getBounds().getWidth(), 100);

        beginTest("Aspect Ratio");
        juce::Component c;
        auto col = makeColumn();
        col->addChild(makeLeaf(&c, 1.0f, 1.0f));
        performLayout(*col, {0,0,100,80});
        expectEquals(c.getBounds().getWidth(), c.getBounds().getHeight());

        beginTest("Show Hide");
        juce::Component d, e;
        auto row = makeRow();
        auto n1 = makeLeaf(&d, 1.0f);
        auto n2 = makeLeaf(&e, 1.0f);
        n2->showHideEnabled = true;
        n2->visibleWhen = []{ return false; };
        row->addChild(std::move(n1));
        row->addChild(std::move(n2));
        performLayout(*row, {0,0,150,50});
        expect(d.isVisible());
        expect(!e.isVisible());
        expectEquals(d.getBounds().getWidth(), 150);

        beginTest("Min Width");
        juce::Component f, g;
        auto row2 = makeRow();
        auto c1 = makeLeaf(&f, 1.0f); c1->minWidth = 40;
        auto c2 = makeLeaf(&g, 1.0f);
        row2->addChild(std::move(c1));
        row2->addChild(std::move(c2));
        performLayout(*row2, {0,0,60,40});
        expectEquals(f.getBounds().getWidth(), 40);
        expectEquals(g.getBounds().getWidth(), 20);
    }
};

#endif // WEIGHTEDLAYOUTTEST_H

