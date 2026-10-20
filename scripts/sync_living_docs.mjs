// One operator manual. Plugin Help and the packaged guide are that file.
// docs/manual/NEUROKORE.md is the source. Do not edit the copies by hand.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const cmake = readFileSync(join(root, "CMakeLists.txt"), "utf8");
const label = cmake.match(/set\(NEUROKORE_VERSION_LABEL "([^"]+)"\)/)?.[1];
if (!label) {
  console.error("NEUROKORE_VERSION_LABEL missing from CMakeLists.txt");
  process.exit(1);
}

const manualPath = join(root, "docs", "manual", "NEUROKORE.md");
let manual = readFileSync(manualPath, "utf8").replace(/\r\n/g, "\n");
if (!manual.includes(label)) {
  console.error(`docs/manual/NEUROKORE.md does not contain ${label}`);
  process.exit(1);
}
if (!manual.endsWith("\n")) manual += "\n";

const targets = [
  join(root, "resources", "UserManual_en.txt"),
  join(root, "web", "src", "overlays", "userManual.gen.txt"),
];
for (const target of targets) {
  const current = readFileSync(target, "utf8").replace(/\r\n/g, "\n");
  if (current === manual) {
    console.log(`living docs current ${target}`);
    continue;
  }
  writeFileSync(target, manual, "utf8");
  console.log(`living docs wrote ${target}`);
}
