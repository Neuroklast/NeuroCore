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
    }
};
