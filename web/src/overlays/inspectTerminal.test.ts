import { expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { InspectBody } from "./Overlays";

it("inspects the synthesized output terminal even when it is absent from AST nodes", () => {
  const html = renderToStaticMarkup(createElement(InspectBody, {nodeId:"OUT"}));
  expect(html).toContain("Plugin output");
  expect(html).not.toContain("No node selected");
});
