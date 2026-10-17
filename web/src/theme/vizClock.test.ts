import { describe, expect, it } from "vitest";
import { clockShouldTick, shouldEmitFrame, subscribeVizClock, vizClockSubscriberCount } from "./vizClock";

describe("viz clock", () => {
  it("is one loop: last unsubscribe leaves zero listeners", () => {
    const a = subscribeVizClock(() => undefined);
    const b = subscribeVizClock(() => undefined);
    expect(vizClockSubscriberCount()).toBe(2);
    a();
    expect(vizClockSubscriberCount()).toBe(1);
    b();
    expect(vizClockSubscriberCount()).toBe(0);
  });

  it("caps 30 fps so a 10 ms tick is skipped", () => {
    expect(shouldEmitFrame(10, 0, 30)).toBe(false);
    expect(shouldEmitFrame(34, 0, 30)).toBe(true);
    expect(shouldEmitFrame(8, 0, 0)).toBe(true);
  });

  it("does not tick subscribers while the document is hidden", () => {
    expect(clockShouldTick(34, 0, 30, true)).toBe(false);
    expect(clockShouldTick(34, 0, 30, false)).toBe(true);
  });
});
