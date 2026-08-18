/**
 * Compile tester feedback (sheet, screenshots, routing spec) into
 * feedback/NeuroKore Feedback.pdf
 */
import { createRequire } from "module";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const require = createRequire(import.meta.url);
const { PDFDocument, StandardFonts, rgb } = require("pdf-lib");

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, "..");
const outDir = path.join(root, "feedback");
const outFile = path.join(outDir, "NeuroKore Feedback.pdf");

const A4 = [595.28, 841.89];
const M = 42;
const ink = rgb(0.08, 0.08, 0.1);
const muted = rgb(0.32, 0.32, 0.36);
const accent = rgb(0.78, 0.08, 0.08);
const rule = rgb(0.82, 0.82, 0.84);
const rowAlt = rgb(0.97, 0.97, 0.97);
const done = rgb(0.12, 0.45, 0.22);
const open = rgb(0.65, 0.28, 0.05);

function ascii(s) {
  return String(s || "")
    .replace(/[\u2012\u2013\u2014\u2212]/g, "-")
    .replace(/[\u2018\u2019]/g, "'")
    .replace(/[\u201c\u201d]/g, '"')
    .replace(/\u2026/g, "...")
    .replace(/\u00a0/g, " ")
    .replace(/\u2260/g, "!=")
    .replace(/[^\x09\x0a\x0d\x20-\x7e\xA0-\xFF]/g, "?");
}

function wrap(text, font, size, maxW) {
  const words = ascii(text).replace(/\s+/g, " ").trim().split(" ");
  const lines = [];
  let cur = "";
  for (const w of words) {
    const trial = cur ? cur + " " + w : w;
    if (font.widthOfTextAtSize(trial, size) <= maxW) cur = trial;
    else {
      if (cur) lines.push(cur);
      cur = w;
    }
  }
  if (cur) lines.push(cur);
  return lines.length ? lines : [""];
}

const doc = await PDFDocument.create();
doc.setTitle("NeuroKore Feedback");
doc.setAuthor("Neuroklast / tester notes");
doc.setSubject("Compiled tester sheet, screenshots, and routing rules");
const font = await doc.embedFont(StandardFonts.Helvetica);
const bold = await doc.embedFont(StandardFonts.HelveticaBold);

let page = doc.addPage(A4);
let y = A4[1] - M;

function newPage() {
  page = doc.addPage(A4);
  y = A4[1] - M;
}

function need(h) {
  if (y - h < M + 16) newPage();
}

function text(str, { size = 10, f = font, color = ink, x = M } = {}) {
  page.drawText(ascii(str), { x, y, size, font: f, color });
}

function para(str, { size = 10, f = font, color = ink, leading = 13, maxW = A4[0] - 2 * M } = {}) {
  const lines = wrap(str, f, size, maxW);
  for (const line of lines) {
    need(leading);
    text(line, { size, f, color });
    y -= leading;
  }
}

function heading(str, size = 14) {
  need(size + 18);
  y -= 8;
  text(str, { size, f: bold, color: accent });
  y -= 4;
  page.drawLine({
    start: { x: M, y },
    end: { x: A4[0] - M, y },
    thickness: 0.8,
    color: accent,
  });
  y -= size + 4;
}

function subhead(str) {
  need(18);
  y -= 4;
  text(str, { size: 11, f: bold, color: ink });
  y -= 16;
}

async function embedImage(rel) {
  const p = path.isAbsolute(rel) ? rel : path.join(root, rel);
  if (!fs.existsSync(p)) return null;
  const bytes = fs.readFileSync(p);
  if (p.toLowerCase().endsWith(".png")) return doc.embedPng(bytes);
  return doc.embedJpg(bytes);
}

async function figure(rel, caption, maxH = 220) {
  const img = await embedImage(rel);
  if (!img) {
    para("[missing image] " + rel, { size: 9, color: muted });
    return;
  }
  const maxW = A4[0] - 2 * M;
  let w = maxW;
  let h = (img.height / img.width) * w;
  if (h > maxH) {
    h = maxH;
    w = (img.width / img.height) * h;
  }
  need(h + 28);
  page.drawImage(img, { x: M, y: y - h, width: w, height: h });
  y -= h + 4;
  para(caption, { size: 8, color: muted, leading: 11 });
  y -= 6;
}

function table(headers, rows, widths) {
  const size = 8;
  const pad = 3;
  const lineH = 10;
  const drawRow = (cells, header, bg) => {
    const wrapped = cells.map((c, i) => wrap(c, header ? bold : font, size, widths[i] - 6));
    const h = Math.max(...wrapped.map((l) => l.length)) * lineH + pad * 2;
    need(h + 2);
    if (bg) {
      page.drawRectangle({
        x: M,
        y: y - h,
        width: widths.reduce((a, b) => a + b, 0),
        height: h,
        color: bg,
      });
    }
    let x = M;
    for (let i = 0; i < cells.length; i++) {
      page.drawRectangle({
        x,
        y: y - h,
        width: widths[i],
        height: h,
        borderColor: rule,
        borderWidth: 0.4,
      });
      let ly = y - pad - size;
      for (const line of wrapped[i]) {
        page.drawText(line, {
          x: x + 3,
          y: ly,
          size,
          font: header ? bold : font,
          color: ink,
        });
        ly -= lineH;
      }
      x += widths[i];
    }
    y -= h;
  };
  drawRow(headers, true, rgb(0.93, 0.93, 0.94));
  rows.forEach((r, i) => drawRow(r, false, i % 2 ? rowAlt : undefined));
  y -= 8;
}

// --- Cover ---
text("NEUROKORE", { size: 22, f: bold, color: accent });
y -= 28;
text("Tester Feedback", { size: 18, f: bold });
y -= 22;
para("Compiled " + new Date().toISOString().slice(0, 10) + "  ·  Sources: Google Sheet, existing one-page dump, screenshots, WhatsApp notes, routing spec.", {
  size: 9,
  color: muted,
});
y -= 8;
para("This file replaces the unreadable single-page dump. Original wording is kept. Status is against 0.4.4 / 0.4.5-alpha.", {
  size: 10,
});

heading("1. Sources");
para("Google Sheet: https://docs.google.com/spreadsheets/d/1h563lKFO4HYXzPjPXmb3i6-MfG5ykxGL1WGqjKhIOE0");
para("Existing dump: feeback/NeuroKore Feedback.pdf (one page, text too small).");
para("Local media: feeback/ screenshots + WhatsApp videos; screenshots/ board captures.");
para("Routing spec: tester text 2026-08-16 plus Screenshot 204457 (wrap-in-gutter).");

heading("2. Sheet — UI");
table(
  ["What", "Reality", "Expected", "Status"],
  [
    [
      "Functions / Stages / Settings",
      "Opens all windows on top of each other",
      "Close the previous one; only the current visible. Second click closes.",
      "Done 0.4.4 exclusive overlay + toggle",
    ],
    [
      "Help / User Manual",
      "Only via Settings",
      "Button in the top bar with ?",
      "Done 0.4.4 ? in toolbar",
    ],
    [
      "Bottom info bar moving around",
      "Studio/Live offset OK; Buffer, BPM, HOST, OS jump",
      "Everything has its own space. Videos: BottomBarMoving.mp4",
      "Partial: brighter footer, less squash. Fixed slots still open.",
    ],
    [
      "Hover then bottom bar",
      "Repeats last hover info; empty if nothing hovered first",
      "Don't show anything. Video: HoverBottomBar.mp4",
      "Open — footer still one label",
    ],
    [
      "Mix UI",
      "Cut off",
      "Not cut off",
      "Done 0.4.4 Mix % min width",
    ],
    [
      "Mix percentage",
      "4 digits after the dot",
      "1 or 2 decimals",
      "Done — integer percent",
    ],
    [
      "Right-click a node",
      "Opens the node editor",
      "Menu: delete, duplicate, etc. Double-click = settings (already)",
      "Done 0.4.4",
    ],
    [
      "Adding a new node",
      "Always links into the existing path",
      "On a line = insert on that cable. Empty space = not connected.",
      "Partial: cable splice works. Empty space still joins the chain.",
    ],
    [
      "Connections",
      "Hard to see where they land",
      "Short stub away from the jack before any turn",
      "Done — east/west stubs + wrap gutter",
    ],
    [
      "Knob host menu",
      "Own menu only",
      "VST3 host menu like Melda, then our items",
      "Done 0.4.4",
    ],
    [
      "Hard to read texts",
      "Grey on black, tiny at 125% host scale",
      "Also white text maybe. Snap setting unreadable.",
      "Partial: brighter footer/labels. Scale still needs work.",
    ],
    [
      "Routing",
      "If something is connected to the output, no way to change it",
      "Change routing by hand",
      "Partial: grab cable to unplug. OUT jacks still awkward.",
    ],
    [
      "IR Preview",
      "No preview play button",
      "Play the IR",
      "Done 0.4.4 Play mixes a short quiet preview",
    ],
    [
      "Misaligned UI",
      "Functions title and X feel off-centre",
      "Centered",
      "Done 0.4.4 overlay title centered",
    ],
  ],
  [110, 130, 145, 126]
);

heading("3. Sheet — Function lookup, shortcuts, nodes");
table(
  ["What", "Reality", "Expected", "Status"],
  [
    [
      "Functions / Stages / Settings click again",
      "Opens, lights up, does not close",
      "Toggle closed when already open",
      "Done 0.4.4",
    ],
    [
      "Functions",
      "No copy button",
      "Copy as well as insert at caret",
      "Done 0.4.4 Copy",
    ],
    [
      "Functions > Nodes",
      "Missing",
      "Look up nodes: what they do, jacks, args",
      "Done 0.4.4 Nodes category",
    ],
    [
      "Functions I/O animation",
      "Octaver, widen, vocoder all look like OTT",
      "Only show I/O where it matters. Distinct glyphs.",
      "Partial: Nodes text distinguishes them. Shared plot still generic.",
    ],
    ["CTRL+Z", "Does not undo positioning", "Undo chip moves", "Done 0.4.4 layout-only undo"],
    ["XOVER", "Creates weird artifacts", "No clicks / zipper", "Partial: slower coeff updates"],
    [
      "Node visual at host scale != 100%",
      "Misaligned",
      "Stay aligned",
      "Open",
    ],
    [
      "Moving nodes",
      "Forces path recalculation / playback artifact",
      "Do nothing if nothing in the graph changed",
      "Done 0.4.4 layout-only setFormula",
    ],
    [
      "Plugin CPU",
      "~12% with nothing happening",
      "Smart idle if no input — do not kill the next transient",
      "Partial: backdrop skips when Motion=Off or silent. DSP stays warm.",
    ],
    [
      "Crash",
      "Save on British Plexi Bark after validate",
      "Never crash",
      "Done 0.4.4 SafePointer + icon bounds",
    ],
    [
      "British Plexi Bark copy",
      "How it sounds: … no Marshall name.",
      "Sounds like an AI prompt leak; has the Marshall name anyway",
      "Done 0.4.4 copy rewritten",
    ],
    [
      "Min/Max",
      "Random numbers bottom left/right",
      "Label MIN / MAX",
      "Done 0.4.4",
    ],
  ],
  [110, 130, 145, 126]
);

heading("4. Feature request — Stages Window");
para("From the original PDF (the sheet row was empty in CSV export):");
para("Should have a way to re-order the stages, so you can click on one, move/position with arrow keys or a button, or drag and drop. That would add another layer of patch feeling.");
para("Status: OPEN. Not built. Circuit already lets you drag chips; this asks for an ordered stage list as well.");

heading("5. Screenshots — board and chrome");
await figure(
  "screenshots/Screenshot 2026-08-16 183757.png",
  "183757 — MS ENC/DEC labels on pins, OUT in the middle, knob cables stacked, chips overlapping the story. Fixed in 0.4.4 (title row, no overlap, OUT to the right, curved knobs).",
  200
);
await figure(
  "screenshots/Screenshot 2026-08-16 174608.png",
  "174608 — cables must enter jacks orthogonally, never diagonally through DRIVE.",
  170
);
await figure(
  "screenshots/Screenshot 2026-08-16 180407.png",
  "180407 — no mini stair-jogs on LFO/IN traces. Horizontal stub before any turn.",
  170
);
await figure(
  "screenshots/Screenshot 2026-08-16 204457.png",
  "204457 — RULE for a wrap between two stacked chips: leave the right jack, run in the gutter, enter the left jack. Not through the body.",
  160
);
await figure(
  "screenshots/Screenshot 2026-08-16 161455.png",
  "161455 — earlier board capture (reference).",
  150
);
await figure(
  "screenshots/Screenshot 2026-08-16 161727.png",
  "161727 — earlier board capture (reference).",
  150
);

heading("6. WhatsApp / scale (feeback/)");
para("there is a snap setting, but I can't really read it.");
para("try 125% scaling for main gui — footer 48000 BUF 2048 BPM 130.0 HOST is tiny.");
para("NK lockup / version string extremely small, super hard to read.");
para("animations settings apply when after the window was closed, I would expect directly.");
para("Status: motion-live done 0.4.4. Readability at 125% host scale still OPEN.");
await figure("feeback/Screenshot 2026-08-16 163548.png", "163548 — Playback FFT / Heavy Audio Filter style reference (jacks + labels).", 140);
await figure("feeback/Screenshot 2026-08-16 163606.png", "163606 — WhatsApp: snap + 125% footer unreadable.", 180);
await figure("feeback/Screenshot 2026-08-16 163630.png", "163630 — WhatsApp: motion should apply while Settings is still open.", 180);

heading("7. Routing rules (binding)");
subhead("7.1 Grid and alignment");
para("The whole board is a fixed 16 px grid. Every port and every wire segment snaps to that grid. Jacks sit on the grid line of their row (not the cell centre). Opposite chips share jack Y if they share the same slot.");
subhead("7.2 Obstacle avoidance");
para("Each module registers its outer box as blocked cells. The router must not walk through those cells.");
subhead("7.3 Ports");
para("Every wire leaves a port in a straight line for a fixed number of grid cells (one cell stub). The first turn is only allowed after that stub. Outputs face +X, inputs face −X.");
subhead("7.4 Cost function");
para("Only axis-aligned moves (X or Y). Each bend gets a high penalty so the path prefers long straight runs. The chosen path is the cheapest mix of distance and turns.");
subhead("7.5 Channel routing");
para("A segment marks its cells used. Another signal in the same direction takes the next free parallel track. Crossings only at exact 90 degrees.");
subhead("7.6 Signal flow and layout");
para("IN has the lowest rank, far left. OUT has the highest rank, far right. Nodes in between sit in columns by their connections. Reading order: top-left, left-to-right, then the next row, until OUT bottom-right.");
subhead("7.7 Wrap between two rows (Screenshot 204457)");
para("When the next block is on the row below: stub east out of the upper-right jack, drop into the gutter BETWEEN the two chips, run left in that gutter, enter the lower-left jack from the west. Never route through a chip body. Row gap is 32 px so a grid line exists in the gutter.");
subhead("7.8 Bus splits and multi-target");
para("A bus / bundled mix uses a thicker stroke (and mix/bus ports are squares). A splitter turns the thick line into thin singles. If one output feeds several destinations, one line leaves the port and branches at the first free grid point. If several cables share one input, they merge one cell before the plug and enter as one line.");
subhead("7.9 Fold / expand");
para("A module has two fixed bounding boxes. Folding or expanding changes the obstacle and the whole grid must be routed again. Inner wires of an expanded module stay in their own coordinates and do not pollute the outer grid.");
subhead("7.10 Module size and port shapes");
para("Chips were too wide (208 px). Compact width is 144 px so horizontal runs stay short. Audio singles = circles. Ports that can take several cables (mix / OUT) = squares.");

heading("8. Videos (not embedded)");
para("feeback/WhatsApp Video 2026-08-16 at 16.23.30.mp4 — BottomBarMoving / hover footer.");
para("feeback/WhatsApp Video 2026-08-16 at 16.32.37.mp4 — Settings motion / scale.");

heading("9. Still open after 0.4.4 / 0.4.5");
para("1. Footer as real fixed slots (not one shrinking label). Hover must not rewrite the strip.");
para("2. Empty-space add-node stays disconnected.");
para("3. Functions plot glyphs: OTT ≠ widen ≠ octaver ≠ vocoder.");
para("4. Host scale 125%: snap setting, lockup, footer type size.");
para("5. Stages Window reorder list (arrow keys / drag).");
para("6. Idle CPU: backdrop only — do not sleep the DSP.");
para("7. Verify wrap-gutter + 144 px chips in the DAW on 0.4.5.");

need(40);
y -= 12;
para("End of compiled feedback. Keep this file in feedback/ and treat the sheet + this PDF as the source of truth.", {
  size: 9,
  color: muted,
});

fs.mkdirSync(outDir, { recursive: true });
const bytes = await doc.save();
fs.writeFileSync(outFile, bytes);
console.log("Wrote", outFile, bytes.length, "bytes", doc.getPageCount(), "pages");
