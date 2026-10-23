import { readFileSync } from "node:fs";
import { expect,it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { hydrateBoard } from "./boardModel";

const fixture = process.env.NK_NATIVE_AST_AUDIT;
it.skipIf(!fixture)("native host ASTs resolve every board connection",()=>{
 const rows = JSON.parse(readFileSync(fixture!,"utf8")) as {name:string;ast:AstDocument}[];
 expect(rows.length).toBeGreaterThan(0);
 expect(rows.flatMap(row=>(hydrateBoard(row.ast).connectionIssues??[]).map(issue=>({name:row.name,...issue})))).toEqual([]);
});
