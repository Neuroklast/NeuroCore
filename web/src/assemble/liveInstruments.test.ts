import { expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { NodeFace } from "./nodeFace/NodeFace";
import { MeterScreen } from "./nodeFace/MeterFace";

it("packs 64 host-driven steps into eight columns with theme LEDs", () => {
  const html = renderToStaticMarkup(createElement(NodeFace, {kind:"lfo", type:"stepseq", args:{steps:"64"}, host:{step:37}}));
  expect(html).toContain('class="nk-step-leds"');
  expect(html.match(/data-step-tick=/g)).toHaveLength(64);
  expect(html).toMatch(/data-step-tick="37" data-active="on"/);
  expect(html).not.toContain("var(--nk-cyan)");
});

it("meter chip displays its own measured level instead of utility gain/pan", () => {
  const html = renderToStaticMarkup(createElement(MeterScreen, {peak:0.5, rms:0.25, mode:"peak"}));
  expect(html).toContain('data-meter-screen=""');
  expect(html).toContain("-6.0");
  expect(html).toContain("-12.0");
  expect(html).not.toContain('data-tap="pan"');
});
