import { createElement } from "react";
import { expect, it } from "vitest";
import { renderToStaticMarkup } from "react-dom/server";
import { FaceView } from "./FaceView";
it("shows meaningful meters without invented thermal readings or data rain",()=>{
 const html=renderToStaticMarkup(createElement(FaceView));
 expect(html).not.toContain('CORE_TEMP');
 expect(html).not.toContain('AST_CHECKSUM');
 expect(html).not.toContain('nk-face-rain-col');
 expect(html).toContain('INPUT PEAK');
 expect(html).toContain('OUTPUT PEAK');
});
