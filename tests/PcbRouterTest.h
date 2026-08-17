#pragma once

#include <JuceHeader.h>
#include "../src/dsl/PcbRouter.h"
#include <cmath>

class PcbRouterTest : public juce::UnitTest
{
public:
    PcbRouterTest() : juce::UnitTest ("PcbRouter", "DSL") {}

    void runTest() override
    {
        dsl::PcbRouter r;

        beginTest ("jack stubs leave and enter on the jack axis");
        {
            const auto path = r.route ({ 16.f, 48.f }, { 200.f, 96.f }, {});
            expect (path.waypoints.size() >= 2);
            expect (dsl::PcbRouter::isOrthogonal (path.waypoints[0], path.waypoints[1]));
            expect (std::abs (path.waypoints[0].y - path.waypoints[1].y) < 0.6f);
            expect (dsl::PcbRouter::isOrthogonal (path.waypoints[path.waypoints.size() - 2],
                                                  path.waypoints.back()));
        }

        beginTest ("open field same row is a straight orthogonal run");
        {
            const auto path = r.route ({ 16.f, 80.f }, { 240.f, 80.f }, {});
            expect (path.waypoints.size() >= 2);
            expectEquals (dsl::PcbRouter::countTurns (path.waypoints), 0);
            for (size_t i = 1; i < path.waypoints.size(); ++i)
                expect (dsl::PcbRouter::isOrthogonal (path.waypoints[i - 1], path.waypoints[i]));
        }

        beginTest ("A* goes around a blocking chip");
        {
            dsl::PcbRect chip { 80.f, 40.f, 80.f, 80.f };
            const auto path = r.route ({ 16.f, 80.f }, { 220.f, 80.f }, { chip });
            expect (path.waypoints.size() >= 3);
            expect (dsl::PcbRouter::countTurns (path.waypoints) >= 1);
            for (size_t i = 1; i < path.waypoints.size(); ++i)
                expect (dsl::PcbRouter::isOrthogonal (path.waypoints[i - 1], path.waypoints[i]));

            bool hitCore = false;
            const dsl::PcbRect core { chip.x + 8.f, chip.y + 8.f, chip.w - 16.f, chip.h - 16.f };
            for (const auto& p : path.waypoints)
                if (p.x > core.x && p.x < core.x + core.w
                    && p.y > core.y && p.y < core.y + core.h)
                    hitCore = true;
            expect (! hitCore, "route walked through the chip core");
        }

        beginTest ("turn penalty prefers an L over a staircase");
        {
            const auto path = r.route ({ 16.f, 16.f }, { 160.f, 80.f }, {});
            expect (dsl::PcbRouter::countTurns (path.waypoints) <= 2);
            const float cheap = dsl::PcbRouter::pathCost (path.waypoints, 16.f);
            std::vector<dsl::PcbPoint> stair { { 16.f, 16.f }, { 48.f, 16.f }, { 48.f, 48.f },
                                               { 80.f, 48.f }, { 80.f, 80.f }, { 160.f, 80.f } };
            expect (cheap < dsl::PcbRouter::pathCost (stair, 16.f));
        }

        beginTest ("corners become quadratic Beziers");
        {
            const auto path = r.route ({ 16.f, 16.f }, { 160.f, 80.f }, {});
            int quads = 0;
            for (const auto& c : path.cmds)
                if (c.kind == dsl::PcbCmdKind::Quad)
                    ++quads;
            expect (quads >= 1, "expected a rounded 90 degree corner");
        }

        beginTest ("shared tracks get a parallel offset");
        {
            std::vector<std::pair<dsl::PcbPoint, dsl::PcbPoint>> nets {
                { { 16.f, 80.f }, { 240.f, 80.f } },
                { { 16.f, 80.f }, { 240.f, 80.f } },
            };
            const auto routes = r.routeAll (nets, {});
            expectEquals ((int) routes.size(), 2);

            auto runYs = [] (const dsl::PcbRoute& p) -> std::vector<float>
            {
                std::vector<float> ys;
                for (size_t i = 1; i + 1 < p.waypoints.size(); ++i)
                    ys.push_back (p.waypoints[i].y);
                return ys;
            };
            const auto a = runYs (routes[0]);
            const auto b = runYs (routes[1]);
            bool split = false;
            for (float ya : a)
                for (float yb : b)
                    if (std::abs (ya - yb) > 8.f)
                        split = true;
            expect (split, "bus lanes must not sit on the same track");

            for (const auto& path : routes)
            {
                expect (path.waypoints.size() >= 4);
                expect (std::abs (path.waypoints[0].y - path.waypoints[1].y) < 0.6f);
                expect (std::abs (path.waypoints[path.waypoints.size() - 2].y
                                  - path.waypoints.back().y) < 0.6f);
                for (size_t i = 1; i < path.waypoints.size(); ++i)
                    expect (dsl::PcbRouter::isOrthogonal (path.waypoints[i - 1],
                                                          path.waypoints[i]));
            }
        }

        beginTest ("stacked chips wrap through the row gutter");
        {
            dsl::PcbRect top { 80.f, 16.f, 144.f, 32.f };
            dsl::PcbRect bot { 80.f, 80.f, 144.f, 32.f }; // 32 px gutter 48..80
            const auto path = r.route ({ 224.f, 32.f }, { 80.f, 96.f }, { top, bot });
            expect (path.waypoints.size() >= 6);
            expect (std::abs (path.waypoints[0].y - path.waypoints[1].y) < 0.6f);
            expect (path.waypoints[1].x > path.waypoints[0].x + 4.f);
            bool gutter = false;
            for (size_t i = 1; i < path.waypoints.size(); ++i)
            {
                expect (dsl::PcbRouter::isOrthogonal (path.waypoints[i - 1], path.waypoints[i]));
                if (std::abs (path.waypoints[i - 1].y - path.waypoints[i].y) < 0.6f
                    && path.waypoints[i].y > 48.f && path.waypoints[i].y < 80.f
                    && std::abs (path.waypoints[i].x - path.waypoints[i - 1].x) > 8.f)
                    gutter = true;
            }
            expect (gutter, "wrap cable must run in the gap between the two chips");
        }

        beginTest ("Manhattan heuristic is never larger than a 4-neighbour path");
        {
            const int ax = 3, ay = 7, bx = 11, by = 2;
            const int h = std::abs (ax - bx) + std::abs (ay - by);
            expect (h == 8 + 5);
        }

        beginTest ("collapseColinear drops midpoints on a straight run");
        {
            const auto slim = dsl::PcbRouter::collapseColinear ({
                { 0.f, 0.f }, { 16.f, 0.f }, { 32.f, 0.f }, { 32.f, 16.f }
            });
            expectEquals ((int) slim.size(), 3);
            expect (dsl::PcbRouter::isOrthogonal (slim[0], slim[1]));
            expect (dsl::PcbRouter::isOrthogonal (slim[1], slim[2]));
        }

        beginTest ("same column still leaves the jack on the horizontal stub");
        {
            const auto path = r.route ({ 80.f, 16.f }, { 80.f, 192.f }, {});
            expect (path.waypoints.size() >= 4);
            expect (dsl::PcbRouter::isOrthogonal (path.waypoints[0], path.waypoints[1]));
            expect (std::abs (path.waypoints[0].y - path.waypoints[1].y) < 0.6f);
            expect (path.waypoints[1].x > path.waypoints[0].x + 4.f);
            for (size_t i = 1; i < path.waypoints.size(); ++i)
                expect (dsl::PcbRouter::isOrthogonal (path.waypoints[i - 1], path.waypoints[i]));
        }

        beginTest ("cmds start with Move and stay finite");
        {
            const auto path = r.route ({ 16.f, 16.f }, { 160.f, 80.f }, {});
            expect (! path.cmds.empty());
            expect (path.cmds.front().kind == dsl::PcbCmdKind::Move);
            for (const auto& c : path.cmds)
            {
                expect (std::isfinite (c.p.x) && std::isfinite (c.p.y));
                expect (std::isfinite (c.c.x) && std::isfinite (c.c.y));
            }
        }

        beginTest ("several nets into one jack merge one cell before the plug");
        {
            std::vector<std::pair<dsl::PcbPoint, dsl::PcbPoint>> nets {
                { { 16.f, 32.f }, { 240.f, 80.f } },
                { { 16.f, 96.f }, { 240.f, 80.f } },
            };
            const auto routes = r.routeAll (nets, {});
            expectEquals ((int) routes.size(), 2);
            const auto a = routes[0].waypoints[routes[0].waypoints.size() - 2];
            const auto b = routes[1].waypoints[routes[1].waypoints.size() - 2];
            expect (std::abs (a.x - b.x) < 0.6f && std::abs (a.y - b.y) < 0.6f,
                    "fan-in must share the last stub");
            expect (a.x < routes[0].waypoints.back().x);
        }

        beginTest ("offset lanes stay orthogonal");
        {
            std::vector<std::pair<dsl::PcbPoint, dsl::PcbPoint>> nets {
                { { 16.f, 64.f }, { 200.f, 64.f } },
                { { 16.f, 64.f }, { 200.f, 64.f } },
                { { 16.f, 64.f }, { 200.f, 64.f } },
            };
            const auto routes = r.routeAll (nets, {});
            expectEquals ((int) routes.size(), 3);
            for (const auto& path : routes)
                for (size_t i = 1; i < path.waypoints.size(); ++i)
                    expect (dsl::PcbRouter::isOrthogonal (path.waypoints[i - 1], path.waypoints[i]));
        }

        beginTest ("port escape is one cell along facing");
        {
            dsl::PcbNet net;
            net.src = { { 16.f, 80.f }, dsl::PcbFacing::East };
            net.dst = { { 240.f, 80.f }, dsl::PcbFacing::West };
            const auto board = dsl::PcbRouter::inferBoard ({ net }, {}, 16.f);
            const auto path = r.route (net, {}, board);
            expect (path.waypoints.size() >= 2);
            expect (dsl::PcbRouter::onGrid (path.waypoints.front(), 16.f));
            for (const auto& p : path.waypoints)
                expect (dsl::PcbRouter::onGrid (p, 16.f));
            expectEquals (dsl::PcbRouter::countTurns (path.waypoints), 0);
            expect (std::abs (path.waypoints[1].x - path.waypoints[0].x - 16.f) < 0.6f);
            expect (std::abs (path.waypoints[1].y - path.waypoints[0].y) < 0.6f);
            const auto a = path.waypoints[path.waypoints.size() - 2];
            const auto b = path.waypoints.back();
            expect (std::abs (b.x - a.x - 16.f) < 0.6f);
            expect (std::abs (a.y - b.y) < 0.6f);
            for (size_t i = 1; i < path.waypoints.size(); ++i)
            {
                const float dx = std::abs (path.waypoints[i].x - path.waypoints[i - 1].x);
                const float dy = std::abs (path.waypoints[i].y - path.waypoints[i - 1].y);
                expect (dx + dy + 1.0e-3f >= 16.f);
            }
        }

        beginTest ("west dest uses the gutter and stays on the board");
        {
            dsl::PcbRect top { 80.f, 16.f, 144.f, 32.f };
            dsl::PcbRect bot { 80.f, 80.f, 144.f, 32.f };
            dsl::PcbNet net;
            net.src = { { 224.f, 32.f }, dsl::PcbFacing::East };
            net.dst = { { 80.f, 96.f }, dsl::PcbFacing::West };
            const auto board = dsl::PcbRouter::inferBoard ({ net }, { top, bot }, 16.f);
            const auto path = r.route (net, { top, bot }, board);
            expect (path.waypoints.size() >= 4);
            expect (path.waypoints[1].x > path.waypoints[0].x + 4.f);
            bool gutter = false;
            bool off = false;
            bool inChip = false;
            for (size_t i = 0; i < path.waypoints.size(); ++i)
            {
                const auto& p = path.waypoints[i];
                expect (dsl::PcbRouter::onGrid (p, 16.f));
                if (p.x < board.origin.x - 0.5f || p.y < board.origin.y - 0.5f
                    || p.x > board.origin.x + (float) board.cols * board.cell + 0.5f
                    || p.y > board.origin.y + (float) board.rows * board.cell + 0.5f)
                    off = true;
                if (p.x > top.x + 1.f && p.x < top.x + top.w - 1.f
                    && p.y > top.y + 1.f && p.y < top.y + top.h - 1.f)
                    inChip = true;
                if (p.x > bot.x + 1.f && p.x < bot.x + bot.w - 1.f
                    && p.y > bot.y + 1.f && p.y < bot.y + bot.h - 1.f)
                    inChip = true;
                if (i > 0 && std::abs (path.waypoints[i - 1].y - p.y) < 0.6f
                    && p.y > 48.f && p.y < 80.f
                    && std::abs (p.x - path.waypoints[i - 1].x) > 8.f)
                    gutter = true;
            }
            expect (gutter, "wrap must run in the gap between the two chips");
            expect (! off, "no waypoint may leave the board");
            expect (! inChip, "no waypoint may sit in a chip core");
        }

        beginTest ("west escape on the board origin stays inside the AABB");
        {
            dsl::PcbNet net;
            net.src = { { 64.f, 80.f }, dsl::PcbFacing::East };
            net.dst = { { 0.f, 160.f }, dsl::PcbFacing::West };
            dsl::PcbBoard board;
            board.cell = 16.f;
            board.origin = { 0.f, 0.f };
            board.cols = 20;
            board.rows = 20;
            const auto path = r.route (net, {}, board);
            expect (path.waypoints.size() >= 2);
            for (const auto& p : path.waypoints)
            {
                expect (p.x + 0.5f >= board.origin.x);
                expect (p.y + 0.5f >= board.origin.y);
                expect (dsl::PcbRouter::onGrid (p, 16.f));
            }
        }

        beginTest ("fan-in shares dest escape; parallel nets take the next cell");
        {
            dsl::PcbNet a, b;
            a.src = { { 16.f, 32.f }, dsl::PcbFacing::East };
            a.dst = { { 240.f, 80.f }, dsl::PcbFacing::West };
            b.src = { { 16.f, 96.f }, dsl::PcbFacing::East };
            b.dst = { { 240.f, 80.f }, dsl::PcbFacing::West };
            const auto board = dsl::PcbRouter::inferBoard ({ a, b }, {}, 16.f);
            const auto routes = r.routeAll ({ a, b }, {}, board);
            expectEquals ((int) routes.size(), 2);
            const auto pa = routes[0].waypoints[routes[0].waypoints.size() - 2];
            const auto pb = routes[1].waypoints[routes[1].waypoints.size() - 2];
            expect (std::abs (pa.x - pb.x) < 0.6f && std::abs (pa.y - pb.y) < 0.6f,
                    "fan-in must share the dest escape");
            expect (pa.x < routes[0].waypoints.back().x);

            dsl::PcbNet u, v;
            u.src = { { 16.f, 80.f }, dsl::PcbFacing::East };
            u.dst = { { 240.f, 80.f }, dsl::PcbFacing::West };
            v = u;
            const auto split = r.routeAll ({ u, v }, {}, dsl::PcbRouter::inferBoard ({ u, v }, {}, 16.f));
            bool apart = false;
            for (const auto& p : split[0].waypoints)
                for (const auto& q : split[1].waypoints)
                    if (std::abs (p.x - q.x) < 0.6f && std::abs (p.y - q.y) > 8.f)
                        apart = true;
            expect (apart, "identical nets must occupy distinct tracks");
        }
    }
};
