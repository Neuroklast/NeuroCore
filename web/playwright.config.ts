import { defineConfig } from "@playwright/test";

export default defineConfig({
  testDir: "./e2e",
  testMatch: "*.spec.ts",
  timeout: 60000,
  projects: [
    { name: "chromium-1x", use: { deviceScaleFactor: 1 } },
    { name: "chromium-2x", use: { deviceScaleFactor: 2 } },
  ],
  use: {
    launchOptions: process.env.NK_CHROMIUM ? { executablePath: process.env.NK_CHROMIUM, args: ["--no-sandbox", ...(process.env.NK_SINGLE_PROCESS ? ["--single-process", "--no-zygote", "--in-process-gpu"] : []), ...(process.env.NK_DISABLE_WEBGL ? ["--disable-webgl"] : ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"])] } : {},
    baseURL: "http://127.0.0.1:4173",
    viewport: { width: 1280, height: 860 },
    deviceScaleFactor: 1,
    reducedMotion: "reduce",
  },
  webServer: {
    command: "npm run preview -- --host 127.0.0.1 --port 4173 --strictPort",
    url: "http://127.0.0.1:4173",
    reuseExistingServer: !process.env.CI,
    timeout: 60000,
  },
});
