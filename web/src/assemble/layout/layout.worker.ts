import { runLayout } from "./runLayout";
import type { LayoutRequest } from "./types";

type Req = LayoutRequest & { reqId: number };

self.onmessage = async (ev: MessageEvent<Req>) => {
  const msg = ev.data;
  try {
    const result = await runLayout(msg.type, msg.nodes, msg.edges, msg.view);
    (self as unknown as Worker).postMessage({ reqId: msg.reqId, ok: true, ...result });
  } catch (err) {
    (self as unknown as Worker).postMessage({
      reqId: msg.reqId,
      ok: false,
      error: err instanceof Error ? err.message : String(err),
    });
  }
};
