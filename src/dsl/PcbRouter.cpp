#include "PcbRouter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace dsl
{
namespace
{

PcbPoint facingDelta (PcbFacing f, float cell) noexcept
{
    switch (f)
    {
        case PcbFacing::East:  return {  cell, 0.f };
        case PcbFacing::North: return { 0.f, -cell };
        case PcbFacing::West:  return { -cell, 0.f };
        case PcbFacing::South: return { 0.f,  cell };
    }
    return { cell, 0.f };
}

float snapGrid (float v, float cell) noexcept
{
    return std::round (v / cell) * cell;
}

PcbPoint snapPoint (PcbPoint p, float cell) noexcept
{
    return { snapGrid (p.x, cell), snapGrid (p.y, cell) };
}

bool samePoint (PcbPoint a, PcbPoint b, float cell) noexcept
{
    const float t = cell * 0.01f;
    return std::abs (a.x - b.x) <= t && std::abs (a.y - b.y) <= t;
}

void collectPoints (PcbPoint p, float& minX, float& minY, float& maxX, float& maxY) noexcept
{
    minX = std::min (minX, p.x);
    minY = std::min (minY, p.y);
    maxX = std::max (maxX, p.x);
    maxY = std::max (maxY, p.y);
}

bool closedContains (const PcbRect& r, PcbPoint p, float inset) noexcept
{
    return p.x > r.x + inset && p.x < r.x + r.w - inset
        && p.y > r.y + inset && p.y < r.y + r.h - inset;
}

bool segHitsRect (PcbPoint a, PcbPoint b, const PcbRect& r) noexcept
{
    const float inset = 1.f;
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

bool pathHitsChips (const std::vector<PcbPoint>& wp, const std::vector<PcbRect>& obs)
{
    if (wp.size() < 2)
        return false;
    for (size_t i = 0; i + 1 < wp.size(); ++i)
    {
        const bool stub = (wp.size() > 2) && (i == 0 || i + 2 >= wp.size());
        if (stub)
            continue;
        for (const auto& r : obs)
            if (segHitsRect (wp[i], wp[i + 1], r))
                return true;
    }
    for (size_t i = 1; i + 1 < wp.size(); ++i)
        for (const auto& r : obs)
            if (closedContains (r, wp[i], 1.f))
                return true;
    return false;
}

void pushUnique (std::vector<PcbPoint>& wp, PcbPoint p, float cell)
{
    if (wp.empty() || ! samePoint (wp.back(), p, cell))
        wp.push_back (p);
}

std::vector<PcbPoint> buildHvh (PcbPoint s, PcbPoint s1, PcbPoint e1, PcbPoint e,
                                float midX, float runYs, float runYe, float cell)
{
    std::vector<PcbPoint> wp;
    pushUnique (wp, s, cell);
    pushUnique (wp, s1, cell);
    if (std::abs (runYs - s1.y) > cell * 0.01f)
        pushUnique (wp, { s1.x, runYs }, cell);
    pushUnique (wp, { midX, runYs }, cell);
    if (std::abs (runYe - runYs) > cell * 0.01f)
        pushUnique (wp, { midX, runYe }, cell);
    pushUnique (wp, { e1.x, runYe }, cell);
    if (std::abs (e1.y - runYe) > cell * 0.01f)
        pushUnique (wp, e1, cell);
    pushUnique (wp, e, cell);
    return PcbRouter::collapseColinear (wp);
}

std::vector<PcbPoint> buildWrap (PcbPoint s, PcbPoint s1, PcbPoint e1, PcbPoint e,
                                 float gutterY, float cell)
{
    std::vector<PcbPoint> wp;
    pushUnique (wp, s, cell);
    pushUnique (wp, s1, cell);
    pushUnique (wp, { s1.x, gutterY }, cell);
    pushUnique (wp, { e1.x, gutterY }, cell);
    pushUnique (wp, e1, cell);
    pushUnique (wp, e, cell);
    return PcbRouter::collapseColinear (wp);
}

float gutterBetween (PcbPoint start, PcbPoint end, const std::vector<PcbRect>& obs, float cell)
{
    float srcBot = start.y, srcTop = start.y, dstBot = end.y, dstTop = end.y;
    bool srcHit = false, dstHit = false;
    for (const auto& r : obs)
    {
        if (start.y >= r.y - 1.f && start.y <= r.y + r.h + 1.f
            && std::abs (start.x - (r.x + r.w)) < cell * 1.5f)
        {
            srcTop = r.y;
            srcBot = r.y + r.h;
            srcHit = true;
        }
        if (end.y >= r.y - 1.f && end.y <= r.y + r.h + 1.f
            && std::abs (end.x - r.x) < cell * 1.5f)
        {
            dstTop = r.y;
            dstBot = r.y + r.h;
            dstHit = true;
        }
    }
    if (srcHit && dstHit)
    {
        if (dstTop >= srcBot - 0.5f)
        {
            const float gy = snapGrid (0.5f * (srcBot + dstTop), cell);
            if (gy > srcBot + 0.5f && gy < dstTop - 0.5f)
                return gy;
            return 0.5f * (srcBot + dstTop);
        }
        if (srcTop >= dstBot - 0.5f)
        {
            const float gy = snapGrid (0.5f * (dstBot + srcTop), cell);
            if (gy > dstBot + 0.5f && gy < srcTop - 0.5f)
                return gy;
            return 0.5f * (dstBot + srcTop);
        }
    }
    float mid = snapGrid (0.5f * (start.y + end.y), cell);
    if (std::abs (mid - start.y) < 0.51f || std::abs (mid - end.y) < 0.51f)
        mid = start.y + (end.y > start.y ? cell : -cell);
    return mid;
}

float chipTop (const std::vector<PcbRect>& obs, float fallback)
{
    float t = fallback;
    bool any = false;
    for (const auto& r : obs)
    {
        t = any ? std::min (t, r.y) : r.y;
        any = true;
    }
    return t;
}

float chipBottom (const std::vector<PcbRect>& obs, float fallback)
{
    float b = fallback;
    bool any = false;
    for (const auto& r : obs)
    {
        const float y2 = r.y + r.h;
        b = any ? std::max (b, y2) : y2;
        any = true;
    }
    return b;
}

std::vector<PcbPoint> patternRoute (PcbPoint s, PcbPoint e,
                                    PcbFacing srcFace, PcbFacing dstFace,
                                    const std::vector<PcbRect>& obs,
                                    float cell, float laneShift)
{
    auto clampEsc = [cell] (PcbPoint pin, PcbPoint esc) -> PcbPoint
    {
        if (esc.x < 0.f && pin.x >= 0.f)
            esc.x = pin.x;
        if (esc.y < 0.f && pin.y >= 0.f)
            esc.y = pin.y;
        return snapPoint (esc, cell);
    };
    const PcbPoint s1 = clampEsc (s, { s.x + facingDelta (srcFace, cell).x,
                                       s.y + facingDelta (srcFace, cell).y });
    const PcbPoint e1 = clampEsc (e, { e.x + facingDelta (dstFace, cell).x,
                                       e.y + facingDelta (dstFace, cell).y });

    const float runYs = s.y + laneShift;
    const float runYe = e.y + laneShift;

    std::vector<PcbPoint> best;
    float bestCost = 1.0e9f;
    auto consider = [&] (std::vector<PcbPoint> wp)
    {
        if (wp.size() < 2 || pathHitsChips (wp, obs))
            return;
        wp = PcbRouter::collapseColinear (wp);
        const float cost = PcbRouter::pathCost (wp, cell);
        if (cost + 1.0e-3f < bestCost)
        {
            bestCost = cost;
            best = std::move (wp);
        }
    };

    // Same row, dest to the east: one straight bus.
    if (std::abs (s.y - e.y) < 0.51f && e1.x + 0.5f >= s1.x)
    {
        if (std::abs (laneShift) < 0.25f)
        {
            std::vector<PcbPoint> wp { s, s1, e1, e };
            auto clean = PcbRouter::collapseColinear (wp);
            if (clean.size() == 2)
                clean = { s, s1, e1, e };
            consider (clean);
            if (! best.empty() && PcbRouter::countTurns (best) == 0)
                return best;
        }
        consider (buildHvh (s, s1, e1, e, s1.x, runYs, runYe, cell));
    }

    // Dest east of source: HVH. Prefer a mid column that misses chips.
    if (e1.x + 0.5f >= s1.x)
    {
        const float lo = std::min (s1.x, e1.x);
        const float hi = std::max (s1.x, e1.x);
        float mid = snapGrid (0.5f * (s1.x + e1.x), cell);
        mid = std::min (std::max (mid, lo), hi);

        consider (buildHvh (s, s1, e1, e, mid, runYs, runYe, cell));
        for (float mx = lo; mx <= hi + 0.5f; mx += cell)
            consider (buildHvh (s, s1, e1, e, mx, runYs, runYe, cell));

        const float above = snapGrid (chipTop (obs, s.y) - cell, cell);
        const float below = snapGrid (chipBottom (obs, s.y) + cell, cell);
        consider (buildHvh (s, s1, e1, e, s1.x, above, above, cell));
        consider (buildHvh (s, s1, e1, e, s1.x, below, below, cell));
        if (! best.empty())
            return best;
        return buildHvh (s, s1, e1, e, mid, runYs, runYe, cell);
    }

    // Dest west of source: east stub, U through the row gutter, west stub.
    const float gy = snapGrid (gutterBetween (s, e, obs, cell) + laneShift, cell);
    consider (buildWrap (s, s1, e1, e, gy, cell));
    const float below = snapGrid (std::max (s.y, e.y)
                                  + std::max (cell, chipBottom (obs, std::max (s.y, e.y)) - std::max (s.y, e.y) + cell),
                                  cell);
    consider (buildWrap (s, s1, e1, e, below, cell));
    if (! best.empty())
        return best;
    return buildWrap (s, s1, e1, e, below, cell);
}

int signedLane (int lane) noexcept
{
    if (lane <= 0)
        return 0;
    const int n = (lane + 1) / 2;
    return (lane & 1) ? n : -n;
}

} // namespace

PcbPoint PcbRouter::escapeOf (const PcbPort& port, float cell) noexcept
{
    const float c = std::max (1.f, cell);
    const PcbPoint pin = snapPoint (port.pin, c);
    const PcbPoint d = facingDelta (port.facing, c);
    return { pin.x + d.x, pin.y + d.y };
}

bool PcbRouter::onGrid (const PcbPoint& p, float cell) noexcept
{
    const float c = std::max (1.f, cell);
    return std::abs (p.x - snapGrid (p.x, c)) < 1.0e-3f
        && std::abs (p.y - snapGrid (p.y, c)) < 1.0e-3f;
}

PcbBoard PcbRouter::inferBoard (const std::vector<PcbNet>& nets,
                                const std::vector<PcbRect>& obstacles,
                                float cell,
                                int haloCells)
{
    const float c = std::max (1.f, cell);
    const int halo = std::max (1, haloCells);
    float minX =  1.0e9f, minY =  1.0e9f;
    float maxX = -1.0e9f, maxY = -1.0e9f;
    auto add = [&] (PcbPoint p) { collectPoints (p, minX, minY, maxX, maxY); };

    for (const auto& n : nets)
    {
        add (snapPoint (n.src.pin, c));
        add (snapPoint (n.dst.pin, c));
        add (escapeOf (n.src, c));
        add (escapeOf (n.dst, c));
    }
    for (const auto& r : obstacles)
    {
        add ({ r.x, r.y });
        add ({ r.x + r.w, r.y + r.h });
    }
    PcbBoard b;
    b.cell = c;
    if (minX > maxX)
        return b;
    const float pad = (float) halo * c;
    minX -= pad; minY -= pad;
    maxX += pad; maxY += pad;
    b.origin = { snapGrid (minX, c), snapGrid (minY, c) };
    if (b.origin.x > minX) b.origin.x -= c;
    if (b.origin.y > minY) b.origin.y -= c;
    b.cols = std::max (1, (int) std::ceil ((maxX - b.origin.x) / c));
    b.rows = std::max (1, (int) std::ceil ((maxY - b.origin.y) / c));
    return b;
}

bool PcbRouter::isOrthogonal (const PcbPoint& a, const PcbPoint& b) noexcept
{
    return (std::abs (a.x - b.x) < 1.0e-3f) || (std::abs (a.y - b.y) < 1.0e-3f);
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

float PcbRouter::pathCost (const std::vector<PcbPoint>& wp, float cell) noexcept
{
    if (wp.size() < 2)
        return 1.0e9f;
    const float c = std::max (1.f, cell);
    float cells = 0.f;
    for (size_t i = 1; i < wp.size(); ++i)
    {
        const float dx = wp[i].x - wp[i - 1].x;
        const float dy = wp[i].y - wp[i - 1].y;
        cells += std::sqrt (dx * dx + dy * dy) / c;
    }
    return cells + 12.f * (float) countTurns (wp);
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

PcbRoute PcbRouter::route (const PcbNet& net,
                           const std::vector<PcbRect>& obstacles,
                           const PcbBoard& board) const
{
    std::vector<PcbNet> one { net };
    auto all = routeAll (one, obstacles, board);
    if (all.empty())
        return {};
    return std::move (all.front());
}

PcbRoute PcbRouter::route (PcbPoint start, PcbPoint end,
                           const std::vector<PcbRect>& obstacles) const
{
    PcbNet net;
    net.src = { start, PcbFacing::East };
    net.dst = { end, PcbFacing::West };
    return route (net, obstacles, inferBoard ({ net }, obstacles, std::max (1.f, cellSize)));
}

std::vector<PcbRoute> PcbRouter::routeAll (const std::vector<std::pair<PcbPoint, PcbPoint>>& nets,
                                           const std::vector<PcbRect>& obstacles) const
{
    std::vector<PcbNet> typed;
    typed.reserve (nets.size());
    for (const auto& n : nets)
        typed.push_back ({ { n.first, PcbFacing::East }, { n.second, PcbFacing::West },
                           PcbNetClass::Audio });
    return routeAll (typed, obstacles, inferBoard (typed, obstacles, std::max (1.f, cellSize)));
}

std::vector<PcbRoute> PcbRouter::routeAll (const std::vector<PcbNet>& nets,
                                           const std::vector<PcbRect>& obstacles,
                                           const PcbBoard& boardIn) const
{
    const float cell = std::max (1.f, boardIn.cell > 0.f ? boardIn.cell : cellSize);
    const float radius = (cornerRadius > 0.f) ? cornerRadius : cell * 0.5f;

    struct Track
    {
        float y { 0.f };
        float a { 0.f };
        float b { 0.f };
    };
    std::vector<Track> used;

    auto overlaps = [] (const Track& u, float y, float a, float b) -> bool
    {
        return std::abs (u.y - y) < 0.75f && a < u.b - 1.f && b > u.a - 1.f;
    };

    std::vector<PcbRoute> routes;
    routes.reserve (nets.size());

    for (const auto& net : nets)
    {
        const PcbPoint s = snapPoint (net.src.pin, cell);
        const PcbPoint e = snapPoint (net.dst.pin, cell);

        int lane = 0;
        const float y0 = s.y;
        const float xa = std::min (s.x, e.x);
        const float xb = std::max (s.x, e.x);
        for (;;)
        {
            const float y = y0 + (float) signedLane (lane) * cell;
            bool taken = false;
            for (const auto& t : used)
                if (overlaps (t, y, xa, xb))
                {
                    taken = true;
                    break;
                }
            if (! taken)
                break;
            ++lane;
            if (lane > 16)
                break;
        }
        const float shift = (float) signedLane (lane) * cell;

        auto wp = patternRoute (s, e, net.src.facing, net.dst.facing, obstacles, cell, shift);
        wp = collapseColinear (wp);

        auto keepEsc = [cell] (PcbPoint pin, PcbFacing face) -> PcbPoint
        {
            PcbPoint esc = PcbRouter::escapeOf ({ pin, face }, cell);
            if (esc.x < 0.f && pin.x >= 0.f) esc.x = pin.x;
            if (esc.y < 0.f && pin.y >= 0.f) esc.y = pin.y;
            return snapPoint (esc, cell);
        };
        const PcbPoint s1 = keepEsc (s, net.src.facing);
        const PcbPoint e1 = keepEsc (e, net.dst.facing);
        if (wp.size() >= 2)
        {
            if (! samePoint (wp[1], s1, cell) && ! samePoint (s, s1, cell))
                wp.insert (wp.begin() + 1, s1);
            if (! samePoint (wp[wp.size() - 2], e1, cell) && ! samePoint (e, e1, cell))
                wp.insert (wp.end() - 1, e1);
        }
        for (auto& p : wp)
            p = snapPoint (p, cell);
        if (wp.size() == 2 && std::abs (s.y - e.y) < 0.51f)
            wp = { s, s1, e1, e };

        for (size_t i = 1; i + 1 < wp.size(); ++i)
        {
            if (std::abs (wp[i].y - wp[i + 1].y) > 0.75f)
                continue;
            const float a = std::min (wp[i].x, wp[i + 1].x);
            const float b = std::max (wp[i].x, wp[i + 1].x);
            if (b - a >= cell)
                used.push_back ({ wp[i].y, a, b });
        }

        PcbRoute out;
        out.waypoints = std::move (wp);
        PcbRouter tmp;
        tmp.cornerRadius = radius;
        out.cmds = tmp.roundCorners (out.waypoints);
        routes.push_back (std::move (out));
    }
    return routes;
}

} // namespace dsl
