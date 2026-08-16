#include "PcbRouter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>

namespace dsl
{
namespace
{

constexpr int kEast = 0, kNorth = 1, kWest = 2, kSouth = 3, kNoDir = 4;
constexpr int kDx[4] = { 1,  0, -1, 0 };
constexpr int kDy[4] = { 0, -1,  0, 1 };

int cellOf (float v, float cell) noexcept
{
    return (int) std::floor ((double) v / (double) cell);
}

float cellOrigin (int c, float cell) noexcept
{
    return (float) c * cell;
}

uint64_t packCell (int x, int y) noexcept
{
    return ((uint64_t) (uint32_t) x << 32) | (uint32_t) y;
}

uint64_t packState (int x, int y, int dir) noexcept
{
    return packCell (x, y) ^ ((uint64_t) (dir & 7) << 60);
}

struct HeapNode
{
    float f { 0.f };
    float g { 0.f };
    int x { 0 };
    int y { 0 };
    int dir { kNoDir };
    bool operator> (const HeapNode& o) const noexcept { return f > o.f; }
};

bool rectContains (const PcbRect& r, PcbPoint p) noexcept
{
    return p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
}

bool blockedAt (int cx, int cy, float cell, const std::vector<PcbRect>& obs,
                PcbPoint start, PcbPoint end) noexcept
{
    const float ox = cellOrigin (cx, cell);
    const float oy = cellOrigin (cy, cell);
    for (const auto& r : obs)
    {
        if (! r.intersectsCell (ox, oy, cell))
            continue;
        if (rectContains (r, start) || rectContains (r, end))
            continue;
        return true;
    }
    return false;
}

int manhattan (int ax, int ay, int bx, int by) noexcept
{
    return std::abs (ax - bx) + std::abs (ay - by);
}

float snapGrid (float v, float cell) noexcept
{
    return std::round (v / cell) * cell;
}

bool segHitsRect (PcbPoint a, PcbPoint b, const PcbRect& r, float inset) noexcept
{
    const float x1 = r.x + inset, y1 = r.y + inset;
    const float x2 = r.x + r.w - inset, y2 = r.y + r.h - inset;
    if (x2 <= x1 || y2 <= y1)
        return false;
    const float minx = std::min (a.x, b.x), maxx = std::max (a.x, b.x);
    const float miny = std::min (a.y, b.y), maxy = std::max (a.y, b.y);
    if (maxx < x1 || minx > x2 || maxy < y1 || miny > y2)
        return false;
    if (std::abs (a.y - b.y) < 0.5f)
        return a.y > y1 && a.y < y2 && maxx > x1 && minx < x2;
    if (std::abs (a.x - b.x) < 0.5f)
        return a.x > x1 && a.x < x2 && maxy > y1 && miny < y2;
    return true;
}

bool pathHitsObstacles (const std::vector<PcbPoint>& wp, const std::vector<PcbRect>& obs,
                        PcbPoint start, PcbPoint end) noexcept
{
    for (size_t i = 1; i < wp.size(); ++i)
    {
        for (const auto& r : obs)
        {
            if (rectContains (r, start) || rectContains (r, end))
                continue;
            if (segHitsRect (wp[i - 1], wp[i], r, 4.f))
                return true;
        }
    }
    return false;
}

std::vector<PcbPoint> manhattanStubs (PcbPoint start, PcbPoint end, float stub, float cell)
{
    std::vector<PcbPoint> wp;
    const float s1x = start.x + stub;
    const float e1x = end.x - stub;
    wp.push_back (start);
    wp.push_back ({ s1x, start.y });

    if (std::abs (start.y - end.y) < 0.51f)
    {
        if (e1x > s1x + 0.5f)
            wp.push_back ({ e1x, end.y });
        wp.push_back (end);
        return wp;
    }

    if (e1x >= s1x + cell)
    {
        float mid = snapGrid (0.5f * (s1x + e1x), cell);
        mid = std::min (std::max (mid, s1x), e1x);
        wp.push_back ({ mid, start.y });
        wp.push_back ({ mid, end.y });
        wp.push_back ({ e1x, end.y });
        wp.push_back (end);
        return wp;
    }

    float midY = snapGrid (0.5f * (start.y + end.y), cell);
    if (std::abs (midY - start.y) < 0.51f || std::abs (midY - end.y) < 0.51f)
        midY = start.y + (end.y > start.y ? cell : -cell);
    wp.push_back ({ s1x, midY });
    wp.push_back ({ e1x, midY });
    wp.push_back ({ e1x, end.y });
    wp.push_back (end);
    return wp;
}

std::vector<PcbPoint> dropMicroJogs (const std::vector<PcbPoint>& wp, float minSeg)
{
    if (wp.size() < 4)
        return wp;
    std::vector<PcbPoint> out = wp;
    bool changed = true;
    while (changed && out.size() >= 4)
    {
        changed = false;
        for (size_t i = 1; i + 2 < out.size(); ++i)
        {
            const float dx = std::abs (out[i].x - out[i + 1].x);
            const float dy = std::abs (out[i].y - out[i + 1].y);
            const float len = std::sqrt (dx * dx + dy * dy);
            if (len + 1.0e-3f >= minSeg)
                continue;
            // Remove a tiny stair: keep the longer axis of the neighbours.
            out.erase (out.begin() + (int) i + 1);
            changed = true;
            break;
        }
    }
    return out;
}

void forceJackStubs (std::vector<PcbPoint>& wp, PcbPoint start, PcbPoint end, float stub)
{
    if (wp.size() < 2)
    {
        wp = { start, { start.x + stub, start.y }, { end.x - stub, end.y }, end };
        return;
    }
    wp.front() = start;
    wp.back() = end;
    if (std::abs (wp[1].y - start.y) > 0.5f || wp[1].x < start.x + stub * 0.5f)
        wp.insert (wp.begin() + 1, PcbPoint { start.x + stub, start.y });
    else if (wp[1].x < start.x + stub)
        wp[1] = { start.x + stub, start.y };

    if (wp.size() >= 3)
    {
        auto& prev = wp[wp.size() - 2];
        if (std::abs (prev.y - end.y) > 0.5f || prev.x > end.x - stub * 0.5f)
            wp.insert (wp.end() - 1, PcbPoint { end.x - stub, end.y });
        else if (prev.x > end.x - stub)
            prev = { end.x - stub, end.y };
    }

    // Repair any non-ortho pair introduced by the stubs.
    for (size_t i = 0; i + 1 < wp.size();)
    {
        if (PcbRouter::isOrthogonal (wp[i], wp[i + 1]))
        {
            ++i;
            continue;
        }
        wp.insert (wp.begin() + (int) i + 1, PcbPoint { wp[i + 1].x, wp[i].y });
        ++i;
    }
}

} // namespace

bool PcbRouter::isOrthogonal (const PcbPoint& a, const PcbPoint& b) noexcept
{
    const float dx = std::abs (a.x - b.x);
    const float dy = std::abs (a.y - b.y);
    return (dx < 1.0e-3f) || (dy < 1.0e-3f);
}

int PcbRouter::countTurns (const std::vector<PcbPoint>& wp) noexcept
{
    int n = 0;
    for (size_t i = 2; i < wp.size(); ++i)
    {
        const float ix = wp[i - 1].x - wp[i - 2].x;
        const float iy = wp[i - 1].y - wp[i - 2].y;
        const float ox = wp[i].x - wp[i - 1].x;
        const float oy = wp[i].y - wp[i - 1].y;
        if (std::abs (ix * oy - iy * ox) > 1.0e-3f)
            ++n;
    }
    return n;
}

std::vector<PcbPoint> PcbRouter::collapseColinear (const std::vector<PcbPoint>& wp)
{
    if (wp.size() < 3)
        return wp;
    std::vector<PcbPoint> out;
    out.reserve (wp.size());
    out.push_back (wp.front());
    for (size_t i = 1; i + 1 < wp.size(); ++i)
    {
        const float ix = wp[i].x - out.back().x;
        const float iy = wp[i].y - out.back().y;
        const float ox = wp[i + 1].x - wp[i].x;
        const float oy = wp[i + 1].y - wp[i].y;
        const bool colinear = std::abs (ix * oy - iy * ox) < 1.0e-3f
                           && (ix * ox + iy * oy) >= 0.f;
        if (! colinear)
            out.push_back (wp[i]);
    }
    out.push_back (wp.back());
    return out;
}

std::vector<PcbCmd> PcbRouter::roundCorners (const std::vector<PcbPoint>& wp) const
{
    std::vector<PcbCmd> cmds;
    if (wp.empty())
        return cmds;
    if (wp.size() == 1)
    {
        cmds.push_back ({ PcbCmdKind::Move, wp[0], {} });
        return cmds;
    }

    cmds.push_back ({ PcbCmdKind::Move, wp[0], {} });
    for (size_t i = 1; i + 1 < wp.size(); ++i)
    {
        const PcbPoint a = wp[i - 1], b = wp[i], c = wp[i + 1];
        const float ix = b.x - a.x, iy = b.y - a.y;
        const float ox = c.x - b.x, oy = c.y - b.y;
        const float inLen = std::sqrt (ix * ix + iy * iy);
        const float outLen = std::sqrt (ox * ox + oy * oy);
        const float r = std::min (cornerRadius, std::min (inLen, outLen) * 0.49f);
        if (r < 1.5f || inLen < 1.0e-3f || outLen < 1.0e-3f)
        {
            cmds.push_back ({ PcbCmdKind::Line, b, {} });
            continue;
        }
        const PcbPoint enter { b.x - (ix / inLen) * r, b.y - (iy / inLen) * r };
        const PcbPoint leave { b.x + (ox / outLen) * r, b.y + (oy / outLen) * r };
        cmds.push_back ({ PcbCmdKind::Line, enter, {} });
        cmds.push_back ({ PcbCmdKind::Quad, leave, b });
    }
    cmds.push_back ({ PcbCmdKind::Line, wp.back(), {} });
    return cmds;
}

PcbRoute PcbRouter::route (PcbPoint start, PcbPoint end,
                           const std::vector<PcbRect>& obstacles) const
{
    PcbRoute out;
    const float cell = std::max (8.f, cellSize);
    const float stub = cell;

    auto finish = [&] (std::vector<PcbPoint> wp)
    {
        forceJackStubs (wp, start, end, stub);
        wp = collapseColinear (wp);
        wp = dropMicroJogs (wp, 4.f);
        wp = collapseColinear (wp);
        forceJackStubs (wp, start, end, stub);
        if (wp.size() == 2 && std::abs (start.y - end.y) < 0.51f)
        {
            wp = { start, { start.x + stub, start.y }, { end.x - stub, end.y }, end };
        }
        wp = collapseColinear (wp);
        if (wp.size() == 2 && std::abs (start.y - end.y) < 0.51f)
            wp = { start, { start.x + stub, start.y }, { end.x - stub, end.y }, end };
        out.waypoints = std::move (wp);
        out.cmds = roundCorners (out.waypoints);
    };

    auto simple = manhattanStubs (start, end, stub, cell);
    simple = collapseColinear (simple);
    if (! pathHitsObstacles (simple, obstacles, start, end))
    {
        finish (std::move (simple));
        return out;
    }

    const PcbPoint s1 { start.x + stub, start.y };
    const PcbPoint e1 { end.x - stub, end.y };
    const int sx = cellOf (s1.x, cell);
    const int sy = cellOf (s1.y, cell);
    const int gx = cellOf (e1.x, cell);
    const int gy = cellOf (e1.y, cell);

    int minX = std::min (sx, gx), maxX = std::max (sx, gx);
    int minY = std::min (sy, gy), maxY = std::max (sy, gy);
    for (const auto& r : obstacles)
    {
        minX = std::min (minX, cellOf (r.x, cell));
        minY = std::min (minY, cellOf (r.y, cell));
        maxX = std::max (maxX, cellOf (r.x + r.w, cell));
        maxY = std::max (maxY, cellOf (r.y + r.h, cell));
    }
    minX -= padCells; minY -= padCells;
    maxX += padCells; maxY += padCells;

    std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> open;
    std::unordered_map<uint64_t, float> bestG;
    struct Parent { int x { 0 }, y { 0 }, dir { kNoDir }; bool ok { false }; };
    std::unordered_map<uint64_t, Parent> came;

    auto pushStart = [&] (int dir, float extra)
    {
        const float g = extra;
        const float h = (float) manhattan (sx, sy, gx, gy);
        open.push ({ g + h, g, sx, sy, dir });
        bestG[packState (sx, sy, dir)] = g;
    };
    pushStart (kEast, 0.f);
    pushStart (kNorth, turnPenalty);
    pushStart (kSouth, turnPenalty);
    pushStart (kWest, turnPenalty * 2.f);

    bool found = false;
    int fx = gx, fy = gy, fd = kEast;
    int expansions = 0;

    while (! open.empty() && expansions < maxExpansions)
    {
        const HeapNode cur = open.top();
        open.pop();
        const auto ck = packState (cur.x, cur.y, cur.dir);
        auto itG = bestG.find (ck);
        if (itG != bestG.end() && cur.g > itG->second + 1.0e-4f)
            continue;
        ++expansions;

        if (cur.x == gx && cur.y == gy)
        {
            found = true;
            fx = cur.x; fy = cur.y; fd = cur.dir;
            break;
        }

        for (int nd = 0; nd < 4; ++nd)
        {
            const int nx = cur.x + kDx[nd];
            const int ny = cur.y + kDy[nd];
            if (nx < minX || nx > maxX || ny < minY || ny > maxY)
                continue;
            if (blockedAt (nx, ny, cell, obstacles, s1, e1))
                continue;
            const float step = 1.f + (nd != cur.dir ? turnPenalty : 0.f);
            const float ng = cur.g + step;
            const auto nk = packState (nx, ny, nd);
            auto prev = bestG.find (nk);
            if (prev != bestG.end() && ng >= prev->second)
                continue;
            bestG[nk] = ng;
            came[nk] = Parent { cur.x, cur.y, cur.dir, true };
            open.push ({ ng + (float) manhattan (nx, ny, gx, gy), ng, nx, ny, nd });
        }
    }

    std::vector<PcbPoint> wp;
    wp.push_back (start);
    wp.push_back (s1);
    if (found)
    {
        std::vector<PcbPoint> cells;
        int x = fx, y = fy, d = fd;
        while (true)
        {
            cells.push_back ({ cellOrigin (x, cell) + cell * 0.5f,
                               cellOrigin (y, cell) + cell * 0.5f });
            const auto k = packState (x, y, d);
            auto it = came.find (k);
            if (it == came.end() || ! it->second.ok)
                break;
            x = it->second.x;
            y = it->second.y;
            d = it->second.dir;
            if (x == sx && y == sy)
            {
                cells.push_back ({ cellOrigin (x, cell) + cell * 0.5f,
                                   cellOrigin (y, cell) + cell * 0.5f });
                break;
            }
        }
        std::reverse (cells.begin(), cells.end());
        for (auto& c : cells)
        {
            // Keep interior on the grid; never pin to jack coords (that is the jog).
            wp.push_back (c);
        }
    }
    wp.push_back (e1);
    wp.push_back (end);
    finish (std::move (wp));
    return out;
}

void PcbRouter::offsetSharedRuns (std::vector<PcbRoute>& routes) const
{
    struct Run
    {
        size_t route { 0 };
        size_t i0 { 0 };
        bool horiz { true };
        int track { 0 };
        float a { 0.f };
        float b { 0.f };
        int lane { 0 };
    };

    const float q = std::max (1.f, cellSize);
    const float gap = std::max (q, laneGap);
    std::vector<Run> runs;
    for (size_t r = 0; r < routes.size(); ++r)
    {
        auto& wp = routes[r].waypoints;
        if (wp.size() < 4)
            continue;
        // Never offset jack stubs (first and last segments).
        for (size_t i = 1; i + 2 < wp.size(); ++i)
        {
            const bool horiz = std::abs (wp[i].y - wp[i + 1].y) < 0.75f;
            const bool vert  = std::abs (wp[i].x - wp[i + 1].x) < 0.75f;
            if (! horiz && ! vert)
                continue;
            Run run;
            run.route = r;
            run.i0 = i;
            run.horiz = horiz;
            if (horiz)
            {
                run.track = (int) std::lround (wp[i].y / q);
                run.a = std::min (wp[i].x, wp[i + 1].x);
                run.b = std::max (wp[i].x, wp[i + 1].x);
            }
            else
            {
                run.track = (int) std::lround (wp[i].x / q);
                run.a = std::min (wp[i].y, wp[i + 1].y);
                run.b = std::max (wp[i].y, wp[i + 1].y);
            }
            if (run.b - run.a < q)
                continue;
            runs.push_back (run);
        }
    }

    auto overlaps = [] (const Run& u, const Run& v) -> bool
    {
        return u.horiz == v.horiz && u.track == v.track
            && u.a < v.b - 1.f && v.a < u.b - 1.f;
    };

    for (size_t i = 0; i < runs.size(); ++i)
    {
        std::vector<char> used (24, 0);
        for (size_t j = 0; j < i; ++j)
            if (overlaps (runs[i], runs[j]) && runs[j].lane >= 0
                && runs[j].lane < (int) used.size())
                used[(size_t) runs[j].lane] = 1;
        int lane = 0;
        while (lane < (int) used.size() && used[(size_t) lane])
            ++lane;
        runs[i].lane = lane;
    }

    // 0 stays on the jack axis; 1 = +1 cell, 2 = -1, 3 = +2… Full-cell
    // spacing — half-gap centering produced 3 px stairs that looked like
    // the micro-jogs and were then deleted by dropMicroJogs.
    auto signedLane = [] (int lane) noexcept -> int
    {
        if (lane <= 0)
            return 0;
        const int n = (lane + 1) / 2;
        return (lane & 1) ? n : -n;
    };

    struct Insert
    {
        size_t route { 0 };
        size_t a { 0 };
        bool horiz { true };
        float off { 0.f };
    };
    std::vector<Insert> inserts;
    for (const auto& run : runs)
    {
        int groupMax = run.lane;
        for (const auto& other : runs)
            if (overlaps (run, other))
                groupMax = std::max (groupMax, other.lane);
        if (groupMax <= 0)
            continue;
        const float off = (float) signedLane (run.lane) * gap;
        if (std::abs (off) < 0.25f)
            continue;
        inserts.push_back ({ run.route, run.i0, run.horiz, off });
    }

    std::sort (inserts.begin(), inserts.end(),
               [] (const Insert& u, const Insert& v)
               {
                   if (u.route != v.route)
                       return u.route < v.route;
                   return u.a > v.a;
               });

    for (const auto& ins : inserts)
    {
        auto& wp = routes[ins.route].waypoints;
        if (ins.a + 1 >= wp.size())
            continue;
        if (ins.horiz)
        {
            const PcbPoint p0 { wp[ins.a].x, wp[ins.a].y + ins.off };
            const PcbPoint p1 { wp[ins.a + 1].x, wp[ins.a + 1].y + ins.off };
            wp.insert (wp.begin() + (int) ins.a + 1, p1);
            wp.insert (wp.begin() + (int) ins.a + 1, p0);
        }
        else
        {
            const PcbPoint p0 { wp[ins.a].x + ins.off, wp[ins.a].y };
            const PcbPoint p1 { wp[ins.a + 1].x + ins.off, wp[ins.a + 1].y };
            wp.insert (wp.begin() + (int) ins.a + 1, p1);
            wp.insert (wp.begin() + (int) ins.a + 1, p0);
        }
    }
}

std::vector<PcbRoute> PcbRouter::routeAll (const std::vector<std::pair<PcbPoint, PcbPoint>>& nets,
                                           const std::vector<PcbRect>& obstacles) const
{
    std::vector<PcbRoute> routes;
    routes.reserve (nets.size());
    for (const auto& n : nets)
    {
        auto r = route (n.first, n.second, obstacles);
        r.cmds.clear();
        routes.push_back (std::move (r));
    }
    offsetSharedRuns (routes);
    for (auto& r : routes)
    {
        if (r.waypoints.size() < 2)
            continue;
        const PcbPoint s = r.waypoints.front();
        const PcbPoint e = r.waypoints.back();
        // Do not dropMicroJogs here: a one-cell lane riser is intentional.
        r.waypoints = collapseColinear (r.waypoints);
        forceJackStubs (r.waypoints, s, e, std::max (8.f, cellSize));
        r.waypoints = collapseColinear (r.waypoints);
        // Straight same-Y bus: collapse eats the stubs. Put them back so
        // plugs stay orthogonal. Offset paths stay size > 2.
        if (r.waypoints.size() == 2 && std::abs (s.y - e.y) < 0.51f)
        {
            const float stub = std::max (8.f, cellSize);
            r.waypoints = { s, { s.x + stub, s.y }, { e.x - stub, e.y }, e };
        }
        r.cmds = roundCorners (r.waypoints);
    }
    return routes;
}

} // namespace dsl
