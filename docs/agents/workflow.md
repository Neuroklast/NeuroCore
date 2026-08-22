# Agent workflow

## Before code

1. Read `docs/DEVELOPMENT_STATUS.md` (one page).
2. Read only the topic file for the task (`ui-ux`, `code-quality`, `testing`).
3. Write the **failing contract** first. If you cannot name the contract, you do not understand the bug.
4. Stop. If the change is a layout/DSP/UI model, write three sentences: what is wrong, what the single model is, which tests prove it.

No code before that.

## During

- One concern. Circuit arrangement is not “also idle CPU and also Settings buttons”.
- Prefer deleting code (A*, dropMicroJogs, knob cables, per-sample TPT) over adding a helper next to it.
- Library first on the canvas. Read `@xyflow/react` (Background, snap, `nodesDraggable`, `onConnect`, `Handle`, `getStraightPath`, `getSmoothStepPath`) before inventing a grid, a router, a drag, or a connect overlay. **Chip drag and jack connect are RF.** If RF already does it, use it. Custom code needs the RF API you rejected and the contract it failed.
- If a test fails: stop. Do not stack another workaround.

## After

1. Run the **contract tests for the files you touched**, not the 28k-expect factory slog, unless you changed factory scripts.
2. Update only `docs/DEVELOPMENT_STATUS.md` (current truth). Add one rule to `docs/LESSONS_LEARNED.md` only if it is a **new** rule, not a session diary.
3. If a user can see the change (Circuit, Help, footer, install, license, knobs), update `docs/manual/NEUROKORE.md` and `resources/UserManual_en.txt` in the **same** run. A coding run does not end with “docs later”.
4. Rebuild Standalone/VST3 only when the user asked for a binary.

## Forbidden

- Checking boxes in DEVELOPMENT_STATUS for work the screenshot still contradicts.
- Appending another `#### Regel` that repeats “HVH / IN left / OUT right”.
- Reading `docs/archive/LESSONS_SESSION_LOG.md` as the daily brief (1.6k lines). Use it as a search index only.
