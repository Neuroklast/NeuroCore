type JuceBackend = {
  emitEvent: (id: string, payload: unknown) => void;
  addEventListener: (id: string, fn: (payload: unknown) => void) => unknown;
};
type JuceGlobal = {
  backend: JuceBackend;
  initialisationData?: { __juce__functions?: string[] };
};
declare global { interface Window { __JUCE__?: JuceGlobal; } }

type Pending = { resolve: (value: unknown) => void; reject: (error: Error) => void; timer: ReturnType<typeof setTimeout> };
type Session = {
  nextId: number;
  pending: Map<number, Pending>;
  events: Map<string, Set<(payload: unknown) => void>>;
};
const sessions = new WeakMap<JuceBackend, Session>();

/** One dispatcher per WebView backend; knob gestures cannot grow native listeners. */
function sessionFor(backend: JuceBackend): Session {
  let session = sessions.get(backend);
  if (session) return session;
  session = { nextId: 1, pending: new Map(), events: new Map() };
  sessions.set(backend, session);
  const state = session;
  backend.addEventListener("__juce__complete", (payload) => {
    const reply = payload as { promiseId?: number; result?: unknown } | null;
    if (typeof reply?.promiseId !== "number") return;
    const pending = state.pending.get(reply.promiseId);
    if (!pending) return;
    state.pending.delete(reply.promiseId);
    clearTimeout(pending.timer);
    pending.resolve(reply.result);
  });
  return state;
}

export function getNativeFunction(name: string): (...args: unknown[]) => Promise<unknown> {
  return (...args) => {
    const backend = typeof window !== "undefined" ? window.__JUCE__?.backend : undefined;
    if (!backend) return Promise.reject(new Error("JUCE bridge missing"));
    const session = sessionFor(backend);
    const resultId = session.nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        session.pending.delete(resultId);
        reject(new Error(`Native ${name} did not respond within 30 seconds`));
      }, 30000);
      session.pending.set(resultId, { resolve, reject, timer });
      try {
        backend.emitEvent("__juce__invoke", { name, params: args, resultId });
      } catch (error) {
        session.pending.delete(resultId);
        clearTimeout(timer);
        reject(error);
      }
    });
  };
}

/** Subscribe logically; the bounded backend dispatcher survives Strict Mode remounts. */
export function onNativeEvent(id: string, fn: (payload: unknown) => void): () => void {
  const backend = typeof window !== "undefined" ? window.__JUCE__?.backend : undefined;
  if (!backend) return () => undefined;
  const session = sessionFor(backend);
  let callbacks = session.events.get(id);
  if (!callbacks) {
    callbacks = new Set();
    session.events.set(id, callbacks);
    const subscribers = callbacks;
    backend.addEventListener(id, payload => subscribers.forEach(callback => callback(payload)));
  }
  callbacks.add(fn);
  return () => { callbacks.delete(fn); };
}

export function hasJuceBridge(): boolean {
  return typeof window !== "undefined" && window.__JUCE__?.backend != null;
}
