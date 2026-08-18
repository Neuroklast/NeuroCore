#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <vector>

namespace presetstep
{

struct Item
{
    juce::String name;
    juce::String category;
};

inline void sortItems (std::vector<Item>& items)
{
    std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b)
    {
        const int c = a.category.compareNatural (b.category);
        if (c != 0)
            return c < 0;
        return a.name.compareNatural (b.name) < 0;
    });
}

/** Category folders, then names. selectedCat is the start if current is outside it. */
inline int indexAfterStep (const std::vector<Item>& sorted,
                           const juce::String& current,
                           const juce::String& selectedCat,
                           int delta) noexcept
{
    const int n = (int) sorted.size();
    if (n <= 0 || delta == 0)
        return -1;

    int cur = -1;
    for (int i = 0; i < n; ++i)
        if (sorted[(size_t) i].name == current)
        {
            cur = i;
            break;
        }

    const auto cat = selectedCat.trim();
    if (cat.isNotEmpty())
    {
        const bool inCat = cur >= 0
                        && sorted[(size_t) cur].category.equalsIgnoreCase (cat);
        if (! inCat)
        {
            int first = -1, last = -1;
            for (int i = 0; i < n; ++i)
                if (sorted[(size_t) i].category.equalsIgnoreCase (cat))
                {
                    if (first < 0)
                        first = i;
                    last = i;
                }
            if (first >= 0)
                return delta > 0 ? first : last;
        }
    }

    if (cur < 0)
        return delta > 0 ? 0 : n - 1;
    return ((cur + delta) % n + n) % n;
}

} // namespace presetstep
