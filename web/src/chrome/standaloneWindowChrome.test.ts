import { afterEach, describe, expect, it } from "vitest";
import { act, create, type ReactTestRenderer } from "react-test-renderer";
import { createElement } from "react";
import { Hud } from "./Chrome";
import { useHostStore } from "../store/hostStore";

const original = useHostStore.getState();
const mounted: ReactTestRenderer[] = [];

function renderHud(standalone: boolean) {
  act(() => useHostStore.setState({ standalone, licensed: true }));
  let renderer!: ReactTestRenderer;
  act(() => { renderer = create(createElement(Hud)); });
  mounted.push(renderer);
  return renderer.root;
}

afterEach(() => {
  act(() => { mounted.splice(0).forEach((renderer) => renderer.unmount()); });
  useHostStore.setState(original);
});

describe("Standalone window chrome", () => {
  it("never adds close/minimise controls to a VST3 or AU plugin editor", () => {
    const root = renderHud(false);
    expect(root.findAllByProps({ "data-window-drag-handle": "" })).toHaveLength(0);
    expect(root.findAllByProps({ "aria-label": "Close standalone window" })).toHaveLength(0);
    expect(root.findAllByProps({ "aria-label": "Minimise standalone window" })).toHaveLength(0);
  });

  it("replaces the standalone HUD with a movable dark header and both window controls", () => {
    const root = renderHud(true);
    const header = root.findByProps({ "data-window-drag-handle": "" });
    expect(header.props.onPointerDown).toBeTypeOf("function");
    expect(header.props.onPointerMove).toBeTypeOf("function");
    expect(header.props.onPointerUp).toBeTypeOf("function");
    expect(header.props.onPointerCancel).toBeTypeOf("function");
    expect(header.props.onLostPointerCapture).toBeTypeOf("function");
    expect(root.findByProps({ "aria-label": "Close standalone window" }).props.type).toBe("button");
    expect(root.findByProps({ "aria-label": "Minimise standalone window" }).props.type).toBe("button");
    expect(root.findAllByProps({ "data-window-control": "" })).toHaveLength(2);
  });
});
