#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace dsl
{

/** World-space point. Independent of any widget toolkit. */
struct PcbPoint
{
    float x { 0.f };
    float y { 0.f };
};

/** Axis-aligned obstacle (node chip). */
struct PcbRect
{
    float x { 0.f };
    float y { 0.f };
    float w { 0.f };
    float h { 0.f };

    bool intersectsCell (float cx, float cy, float cell) const noexcept
    {
        const float x1 = x, y1 = y, x2 = x + w, y2 = y + h;
        const float a1 = cx, b1 = cy, a2 = cx + cell, b2 = cy + cell;
        return a1 < x2 && a2 > x1 && b1 < y2 && b2 > y1;
    }
};

enum class PcbCmdKind : unsigned char { Move, Line, Quad };

/** Move/Line use `p`. Quad is quadratic Bezier to `p` with control `c`. */
struct PcbCmd
{
    PcbCmdKind kind { PcbCmdKind::Move };
    PcbPoint p {};
    PcbPoint c {};
};

struct PcbRoute
{
    std::vector<PcbPoint> waypoints;
    std::vector<PcbCmd> cmds;
};

/**
    Orthogonal A* router for circuit cables.
    No UI types. The canvas maps chips to PcbRect and cmds to a stroke.
*/
struct PcbRouter
{
    float cellSize { 16.f };
    float turnPenalty { 12.f };
    float cornerRadius { 8.f };
    float laneGap { 16.f };
    int   padCells { 16 };
    int   maxExpansions { 80000 };

    PcbRoute route (PcbPoint start, PcbPoint end,
                    const std::vector<PcbRect>& obstacles) const;

    std::vector<PcbRoute> routeAll (const std::vector<std::pair<PcbPoint, PcbPoint>>& nets,
                                    const std::vector<PcbRect>& obstacles) const;

    static std::vector<PcbPoint> collapseColinear (const std::vector<PcbPoint>& wp);
    static bool isOrthogonal (const PcbPoint& a, const PcbPoint& b) noexcept;
    static int countTurns (const std::vector<PcbPoint>& wp) noexcept;
    std::vector<PcbCmd> roundCorners (const std::vector<PcbPoint>& wp) const;
    void offsetSharedRuns (std::vector<PcbRoute>& routes) const;
};

} // namespace dsl
