const PREFIX = "nk:";

let on = false;

export function setPerfEnabled(v: boolean): void {
  on = v;
}

export function perfEnabled(): boolean {
  return on;
}

export function perfFlagFromSearch(
  search: string,
  storage?: { getItem(key: string): string | null },
): boolean {
  if (/(?:^|[?&])nkperf=1(?:&|$)/.test(search)) {
    return true;
  }
  if (/(?:^|[?&])nkperf=0(?:&|$)/.test(search)) {
    return false;
  }
  return storage?.getItem("nkperf") === "1";
}

export function syncPerfFlag(): void {
  if (typeof window === "undefined") {
    return;
  }
  setPerfEnabled(perfFlagFromSearch(window.location.search, window.localStorage));
}

export function perfBegin(name: string): void {
  if (! on) {
    return;
  }
  performance.mark(`${PREFIX}${name}:start`);
}

export function perfEnd(name: string): void {
  if (! on) {
    return;
  }
  const start = `${PREFIX}${name}:start`;
  const end = `${PREFIX}${name}:end`;
  performance.mark(end);
  try {
    performance.measure(`${PREFIX}${name}`, start, end);
  } catch {
    // missing start mark
  }
}

export function perfMeasures(name?: string): PerformanceEntry[] {
  if (name) {
    return performance.getEntriesByName(`${PREFIX}${name}`);
  }
  return performance.getEntriesByType("measure").filter((e) => e.name.startsWith(PREFIX));
}
