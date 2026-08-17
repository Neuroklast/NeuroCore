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

enum class PcbFacing : unsigned char { East, North, West, South };

enum class PcbNetClass : unsigned char { Audio, Mix, Mod, Control };

/** Jack on a chip outline. `pin` is a grid point. Escape is one cell along `facing`. */
struct PcbPort
{
    PcbPoint pin {};
    PcbFacing facing { PcbFacing::East };
};

struct PcbNet
{
    PcbPort src {};
    PcbPort dst {};
    PcbNetClass cls { PcbNetClass::Audio };
};

/** Inclusive vertex grid. World (origin + (i,j)*cell), 0<=i<=cols, 0<=j<=rows. */
struct PcbBoard
{
    float cell { 16.f };
    PcbPoint origin { 0.f, 0.f };
    int cols { 0 };
    int rows { 0 };
};

/**
    Pattern Manhattan router. No UI types. No A*.

    Every net is HVH (or a wrap-U when the dest sits west of the source):
    east stub, one vertical, west stub. If that run hits a chip, the
    vertical moves to the next free column or the path goes around the
    chip box. Parallel nets take the next free track. Fan-in shares the
    dest escape. Corners are filleted by `cell/2`.
*/
struct PcbRouter
{
    float cellSize { 16.f };
    float cornerRadius { 8.f };

    PcbRoute route (PcbPoint start, PcbPoint end,
                    const std::vector<PcbRect>& obstacles) const;

    PcbRoute route (const PcbNet& net,
                    const std::vector<PcbRect>& obstacles,
                    const PcbBoard& board) const;

    std::vector<PcbRoute> routeAll (const std::vector<std::pair<PcbPoint, PcbPoint>>& nets,
                                    const std::vector<PcbRect>& obstacles) const;

    std::vector<PcbRoute> routeAll (const std::vector<PcbNet>& nets,
                                    const std::vector<PcbRect>& obstacles,
                                    const PcbBoard& board) const;

    static PcbPoint escapeOf (const PcbPort& port, float cell) noexcept;
    static PcbBoard inferBoard (const std::vector<PcbNet>& nets,
                                const std::vector<PcbRect>& obstacles,
                                float cell,
                                int haloCells = 1);
    static bool onGrid (const PcbPoint& p, float cell) noexcept;
    static std::vector<PcbPoint> collapseColinear (const std::vector<PcbPoint>& wp);
    static bool isOrthogonal (const PcbPoint& a, const PcbPoint& b) noexcept;
    static int countTurns (const std::vector<PcbPoint>& wp) noexcept;
    /** cells + 12 * turns. Long and straight beats short and kinked. */
    static float pathCost (const std::vector<PcbPoint>& wp, float cell) noexcept;
    std::vector<PcbCmd> roundCorners (const std::vector<PcbPoint>& wp) const;
};

} // namespace dsl
