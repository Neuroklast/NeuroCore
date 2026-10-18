/** Compile-time: `npm run build` / `tsc --noEmit` must accept these production call sites. */
import type { CompileResultPayload } from "../bridge/ast";
import { publishScript } from "./addBlock";
import { inspectorBox } from "./sourceInspectorLayout";

const storedWidth: number = 400;
const storedHeight: number = 180;
void inspectorBox("side", 1280, 640, storedWidth, storedHeight);

const published: CompileResultPayload | Promise<CompileResultPayload> = publishScript("in: x", "canvas");
void published;
