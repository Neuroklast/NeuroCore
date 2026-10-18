import { perfEnabled } from "./marks";
import { recordGpuMs } from "./stats";

type TimerExt = {
  TIME_ELAPSED_EXT: number;
  GPU_DISJOINT_EXT: number;
};

type Pending = { q: WebGLQuery; name: string };

export type GpuTimer = {
  begin(name: string): void;
  end(): void;
  poll(): void;
  dispose(): void;
};

/**
 * Async GPU elapsed-time queries. Never spin-waits.
 * Disjoint results are discarded. No-ops when nkperf is off or the extension is missing.
 */
export function createGpuTimer(gl: WebGL2RenderingContext): GpuTimer | null {
  const ext = gl.getExtension("EXT_disjoint_timer_query_webgl2") as TimerExt | null;
  if (! ext) {
    return null;
  }
  const pool: WebGLQuery[] = [];
  const pending: Pending[] = [];
  let active: WebGLQuery | null = null;

  const take = (): WebGLQuery | null => {
    const reused = pool.pop();
    if (reused) {
      return reused;
    }
    return gl.createQuery();
  };

  return {
    begin(name) {
      if (! perfEnabled() || active) {
        return;
      }
      const q = take();
      if (! q) {
        return;
      }
      gl.beginQuery(ext.TIME_ELAPSED_EXT, q);
      active = q;
      pending.push({ q, name });
    },
    end() {
      if (! active) {
        return;
      }
      gl.endQuery(ext.TIME_ELAPSED_EXT);
      active = null;
    },
    poll() {
      if (! perfEnabled() || pending.length === 0) {
        return;
      }
      if (gl.getParameter(ext.GPU_DISJOINT_EXT)) {
        while (pending.length > 0) {
          const row = pending.shift()!;
          pool.push(row.q);
        }
        return;
      }
      while (pending.length > 0) {
        const row = pending[0]!;
        if (! gl.getQueryParameter(row.q, gl.QUERY_RESULT_AVAILABLE)) {
          return;
        }
        pending.shift();
        const ns = gl.getQueryParameter(row.q, gl.QUERY_RESULT) as number;
        if (Number.isFinite(ns) && ns > 0) {
          recordGpuMs(ns / 1e6);
        }
        pool.push(row.q);
      }
    },
    dispose() {
      if (active) {
        gl.endQuery(ext.TIME_ELAPSED_EXT);
        active = null;
      }
      for (const row of pending) {
        gl.deleteQuery(row.q);
      }
      pending.length = 0;
      for (const q of pool) {
        gl.deleteQuery(q);
      }
      pool.length = 0;
    },
  };
}
