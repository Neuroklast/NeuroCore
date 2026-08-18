# Task 11 report — Terminal View/Edit, a[0.34], comments, autocomplete

**Status:** DONE  
**Commit:** `b93175d`  
**Branch:** refine-engine  
**Version:** 0.4.8-alpha (unchanged)

## Contract

1. View vs Edit: `termFrame` → view muted/read-only/no caret; edit accent/caret.
2. Header comments: `# <preset>` + `# How it sounds: …` only (`withHeaderComments` strips `# a Drive:…` header lines).
3. Inlay: `annotateKnobInlays` appends live mapped `a[0.34]` (2 decimals) after each knob token; skips comments.
4. Autocomplete: `stillParsesAfterInsert` + filter; after `filter1: type = ` only `lowpass|highpass|bandpass`.
5. Task 12 `validateOnSave` on Save kept.

## TDD

### RED

```
npx vitest run src/hack/dslLanguage.test.ts
```

- Missing `headerComments` / `withHeaderComments` / `annotateKnobInlays` / `termFrame` / `stillParsesAfterInsert`
- 6 failed / 6 passed

### GREEN

Same command:

```
Test Files  1 passed (1)
     Tests  12 passed (12)
```

Also green: `validateModel.test.ts` + `optimizeModel.test.ts` (24 together).

## Files

| File | Role |
|---|---|
| `web/src/hack/dslLanguage.ts` | `headerComments`, `withHeaderComments`, `annotateKnobInlays`, `termFrame` |
| `web/src/hack/dslComplete.ts` | `stillParsesAfterInsert`, compile-safe `complete` |
| `web/src/hack/dslLanguage.test.ts` | Contracts above |
| `web/src/hack/HackView.tsx` | View/edit chrome, view inlays + header; Save → `validateOnSave` |
| `web/src/theme/tailwind.css` | `.nk-term--view` / `.nk-term--edit` frames |
| `docs/DEVELOPMENT_STATUS.md` | WP4 note |

Not touched: C++ FormulaDisplay, factory JSON, version, Assemble, Task 12 validate model.

## Model

- View shows derived text (header normalize + inlays); Edit/Save use raw draft. Inlays never written back.
- Compile-safe = brace balance + chipSpec enum check after splice at caret.
- Type-value context is only `/type\s*=\s*$/` so `type = lowpass; cut` still gets properties.

## Self-review

- One concern: Terminal hack UX.
- TDD: failing Vitest first.
- `validateOnSave` still called on Save.
- Parallel dirty tree left unstaged.

## Concerns

- View rewrites header from `presetName` + catalog description each paint; raw script on Edit may still carry old param-range header lines until Save path normalizes (not required by brief).
