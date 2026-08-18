import { copyFileSync, mkdirSync } from "node:fs";
import { spawnSync } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";
import react from "@vitejs/plugin-react";

const dir = path.dirname(fileURLToPath(import.meta.url));

function embedFactoryCatalog() {
  const script = path.resolve(dir, "../scripts/embed_factory_catalog.mjs");
  const r = spawnSync(process.execPath, [script], { stdio: "inherit" });
  if (r.status !== 0) {
    throw new Error("embed_factory_catalog failed");
  }
}

function embedUserManual() {
  const src = path.resolve(dir, "../resources/UserManual_en.txt");
  const dest = path.resolve(dir, "src/overlays/userManual.gen.txt");
  mkdirSync(path.dirname(dest), { recursive: true });
  copyFileSync(src, dest);
}

function embedWebAssets() {
  embedFactoryCatalog();
  embedUserManual();
}

export default defineConfig({
  plugins: [
    react(),
    {
      name: "embed-web-assets",
      buildStart: embedWebAssets,
      configureServer: embedWebAssets,
    },
  ],
  base: "./",
  server: {
    port: 5173,
    strictPort: true,
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
    assetsDir: "assets",
  },
  test: {
    environment: "node",
    include: ["src/**/*.test.ts"],
  },
});
