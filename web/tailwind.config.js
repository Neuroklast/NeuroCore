/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        ink: "#f4f1ea",
        muted: "#7a7a86",
        accent: {
          DEFAULT: "#ff003c",
          dim: "#8a0021",
        },
        warn: "#fcee0a",
        cyan: "#00f0ff",
        surface: {
          DEFAULT: "#0e0e12",
          high: "#16161c",
        },
        panel: "#3a1420",
        well: "#07070a",
      },
      fontFamily: {
        brand: ["Apex", "sans-serif"],
        mono: ["JetBrains Mono", "ui-monospace", "Consolas", "monospace"],
      },
    },
  },
  plugins: [],
};
