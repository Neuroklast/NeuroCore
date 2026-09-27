export function setSplashProgress(pct: number): void {
  const bar = document.getElementById("nk-splash-bar");
  if (! bar) {
    return;
  }
  const n = Math.max(0, Math.min(100, Math.round(pct)));
  bar.style.width = `${n}%`;
  bar.setAttribute("aria-valuenow", String(n));
}

export function dismissSplash(): void {
  document.documentElement.dataset.nkReady = "1";
  document.getElementById("nk-splash")?.remove();
}
