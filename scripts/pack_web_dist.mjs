/** Pack web/dist into a store-only zip + Windows .rc so VST3/Standalone ship the UI. */
import fs from "node:fs";
import path from "node:path";

const distDir = process.argv[2];
const zipPath = process.argv[3];
const rcPath = process.argv[4];

if (!distDir || !zipPath || !rcPath) {
  console.error("usage: pack_web_dist.mjs <distDir> <zipPath> <rcPath>");
  process.exit(1);
}

const crcTable = new Uint32Array(256);
for (let n = 0; n < 256; n++) {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  crcTable[n] = c >>> 0;
}

function crc32(buf) {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function u16(n) {
  const b = Buffer.alloc(2);
  b.writeUInt16LE(n);
  return b;
}

function u32(n) {
  const b = Buffer.alloc(4);
  b.writeUInt32LE(n);
  return b;
}

function walk(dir, prefix, files) {
  for (const name of fs.readdirSync(dir)) {
    const full = path.join(dir, name);
    const rel = prefix ? `${prefix}/${name}` : name;
    if (fs.statSync(full).isDirectory()) walk(full, rel.replaceAll("\\", "/"), files);
    else files.push({ rel: rel.replaceAll("\\", "/"), data: fs.readFileSync(full) });
  }
}

if (!fs.existsSync(path.join(distDir, "index.html"))) {
  console.error("web/dist/index.html missing — npm run build failed");
  process.exit(1);
}

const files = [];
walk(distDir, "", files);
if (files.length === 0) {
  console.error("web/dist is empty");
  process.exit(1);
}

const now = new Date();
const dosTime =
  ((now.getHours() & 0x1f) << 11) |
  ((now.getMinutes() & 0x3f) << 5) |
  (Math.floor(now.getSeconds() / 2) & 0x1f);
const dosDate = ((now.getFullYear() - 1980) << 9) | ((now.getMonth() + 1) << 5) | now.getDate();

const locals = [];
const centrals = [];
let offset = 0;

for (const file of files) {
  const name = Buffer.from(file.rel, "utf8");
  const crc = crc32(file.data);
  const local = Buffer.concat([
    u32(0x04034b50),
    u16(20),
    u16(0),
    u16(0),
    u16(dosTime),
    u16(dosDate),
    u32(crc),
    u32(file.data.length),
    u32(file.data.length),
    u16(name.length),
    u16(0),
    name,
    file.data,
  ]);
  const central = Buffer.concat([
    u32(0x02014b50),
    u16(20),
    u16(20),
    u16(0),
    u16(0),
    u16(dosTime),
    u16(dosDate),
    u32(crc),
    u32(file.data.length),
    u32(file.data.length),
    u16(name.length),
    u16(0),
    u16(0),
    u16(0),
    u16(0),
    u32(0),
    u32(offset),
    name,
  ]);
  locals.push(local);
  centrals.push(central);
  offset += local.length;
}

const central = Buffer.concat(centrals);
const eocd = Buffer.concat([
  u32(0x06054b50),
  u16(0),
  u16(0),
  u16(files.length),
  u16(files.length),
  u32(central.length),
  u32(offset),
  u16(0),
]);

fs.mkdirSync(path.dirname(zipPath), { recursive: true });
fs.writeFileSync(zipPath, Buffer.concat([...locals, central, eocd]));

const zipForRc = zipPath.replaceAll("\\", "/");
fs.mkdirSync(path.dirname(rcPath), { recursive: true });
// Integer ID — quoted RC string names keep the quotes and FindResource misses them.
fs.writeFileSync(rcPath, `41001 RCDATA "${zipForRc}"\n`);
console.log(`packed ${files.length} web files -> ${zipPath}`);
