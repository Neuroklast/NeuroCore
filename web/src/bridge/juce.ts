type JuceBackend = {
  emitEvent: (id: string, payload: unknown) => void;
  addEventListener: (id: string, fn: (payload: unknown) => void) => number;
};

type JuceGlobal = {
  backend: JuceBackend;
  initialisationData?: { __juce__functions?: string[] };
};

declare global {
  interface Window {
    __JUCE__?: JuceGlobal;
  }
}

export function getNativeFunction(name: string): (...args: unknown[]) => Promise<unknown> {
  return (...args: unknown[]) => {
    const juce = window.__JUCE__;
    if (! juce?.backend) {
      return Promise.reject(new Error("JUCE bridge missing"));
    }
    return new Promise((resolve) => {
      const resultId = Math.floor(Math.random() * 1e9);
      const done = (payload: unknown) => {
        const rec = payload as { promiseId?: number; result?: unknown };
        if (rec && rec.promiseId === resultId) {
          juce.backend.addEventListener("__juce__complete", () => undefined);
          resolve(rec.result);
        }
      };
      juce.backend.addEventListener("__juce__complete", done);
      juce.backend.emitEvent("__juce__invoke", {
        name,
        params: args,
        resultId,
      });
      window.setTimeout(() => resolve(undefined), 50);
    });
  };
}

export function onNativeEvent(id: string, fn: (payload: unknown) => void): void {
  const juce = window.__JUCE__;
  if (! juce?.backend) {
    return;
  }
  juce.backend.addEventListener(id, fn);
}

export function hasJuceBridge(): boolean {
  return typeof window !== "undefined" && window.__JUCE__?.backend != null;
}
