/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        ink: "rgb(var(--nk-ink-rgb) / <alpha-value>)",
        muted: "rgb(var(--nk-ink-muted-rgb) / <alpha-value>)",
        accent: {
          DEFAULT: "rgb(var(--nk-accent-rgb) / <alpha-value>)",
          dim: "rgb(var(--nk-accent-dim-rgb) / <alpha-value>)",
        },
        warn: "rgb(var(--nk-warn-rgb) / <alpha-value>)",
        cyan: "rgb(var(--nk-cyan-rgb) / <alpha-value>)",
        surface: {
          DEFAULT: "var(--nk-surface)",
          high: "var(--nk-surface-high)",
        },
        panel: "var(--nk-panel)",
        well: "var(--nk-well)",
      },
      fontFamily: {
        brand: ["Apex", "sans-serif"],
        mono: ["JetBrains Mono", "ui-monospace", "Consolas", "monospace"],
      },
    },
  },
  plugins: [],
};
