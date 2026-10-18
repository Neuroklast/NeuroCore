import { afterEach, describe, expect, it, vi } from "vitest";
import { getNativeFunction, nativeCallResult, onNativeEvent } from "./juce";

function backend() {
  const listeners = new Map<string, Array<(p: unknown) => void>>();
  const requests: Array<{ resultId: number }> = [];
  const native = {
    addEventListener: (id: string, fn: (p: unknown) => void) => {
      const a = listeners.get(id) ?? [];
      a.push(fn);
      listeners.set(id, a);
      return [id, a.length - 1];
    },
    emitEvent: (_id: string, p: unknown) => requests.push(p as { resultId: number }),
  };
  vi.stubGlobal("window", { __JUCE__: { backend: native }, setTimeout, clearTimeout });
  return {
    listeners,
    requests,
    emit: (id: string, p: unknown) => listeners.get(id)?.forEach((fn) => fn(p)),
  };
}

afterEach(() => {
  vi.useRealTimers();
  vi.unstubAllGlobals();
});

it("waits for native compile completion beyond 50 ms", async () => {
  vi.useFakeTimers();
  const b = backend();
  let settled = false;
  const call = getNativeFunction("compile")({ script: "stage1: y = x" }).then((v) => {
    settled = true;
    return v;
  });
  await vi.advanceTimersByTimeAsync(100);
  expect(settled).toBe(false);
  b.emit("__juce__complete", { promiseId: b.requests[0]!.resultId, result: { ok: true } });
  expect(await call).toEqual({ ok: true });
});

it("uses one completion listener for repeated parameter updates", async () => {
  const b = backend();
  const calls = Array.from({ length: 100 }, (_, value) => getNativeFunction("setParam")({ id: "a", value: value / 100 }));
  for (const r of b.requests) b.emit("__juce__complete", { promiseId: r.resultId, result: true });
  expect(await Promise.all(calls)).toHaveLength(100);
  expect(b.listeners.get("__juce__complete")).toHaveLength(1);
  expect(new Set(b.requests.map((r) => r.resultId)).size).toBe(100);
});

it("unsubscribes mounted UI callbacks without accumulating backend listeners", () => {
  const b = backend();
  const fn = vi.fn();
  for (let i = 0; i < 5; i++) {
    const off = onNativeEvent("host", fn);
    b.emit("host", {});
    if (typeof off === "function") off();
  }
  b.emit("host", {});
  expect(fn).toHaveBeenCalledTimes(5);
  expect(b.listeners.get("host")).toHaveLength(1);
});

describe("nativeCallResult", () => {
  it("accepts boolean true", () => {
    expect(nativeCallResult(true)).toEqual({ ok: true });
  });

  it("reads failVar diagnostics", () => {
    expect(nativeCallResult({
      ok: false,
      diagnostics: [{ line: 1, column: 1, message: "Could not save preset" }],
    })).toEqual({ ok: false, error: "Could not save preset" });
  });

  it("treats missing or false replies as failure", () => {
    expect(nativeCallResult({ ok: true })).toEqual({ ok: true });
    expect(nativeCallResult(undefined)).toEqual({ ok: false, error: "Native call failed" });
    expect(nativeCallResult(false)).toEqual({ ok: false, error: "Native call failed" });
  });
});
