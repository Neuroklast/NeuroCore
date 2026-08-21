import { describe, expect, it } from "vitest";
import { subscribeVizClock, vizClockSubscriberCount } from "./vizClock";

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
});
