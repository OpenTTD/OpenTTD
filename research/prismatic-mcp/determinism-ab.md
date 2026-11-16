# Determinism A/B (I0026) — design and current blocker

**Status: NOT RUN / BLOCKED.** The experiment harness is designed and the mechanics
are understood, but the OpenTTD state-comparison mechanism it relies on does not emit
output in this environment, so no result has been produced. This record exists so the
work is not lost and a future session (or the owner) can resume from the exact blocker,
rather than re-deriving it. Per `research/README.md`, a step that was not run is
recorded as NOT RUN, never asserted as PASS.

## Intent

Demonstrate — not assert — that the read-only MCP path does not perturb the
deterministic simulation: run one map seed twice, one run hammered with MCP
`resources/read`, the other idle, and show the two end with **identical game state**
(I0026 acceptance). The read path must be exercised through the same seam a client uses.

## What is understood and works

- **Read load, in process.** The `mcp` console command (I0067) calls the MCP server's
  `Dispatch()` directly: `mcp read openttd://<uri>` runs the exact resource-read handlers
  the HTTP transport's `resources/read` uses, on the same main thread. Feeding
  `mcp read …` through a dedicated server's console (a FIFO on stdin) is a faithful,
  HTTP-free way to generate the load. Confirmed working: `mcp resources` lists all 23
  resources; `mcp read openttd://game/state` returns live state.
- **The game advances headless.** A dedicated server (`-D -g`) with
  `min_active_clients = 0` runs `StateGameLoop` (via `NetworkGameLoop`) and the economy
  date progresses (observed 1950-01-02 → 1950-02-10, `paused: false`).
- **The research-mcp build** (profile from I0027) builds and runs
  (`build-research-mcp/openttd`, 65 MB, Debug + both gates ON).

## The blocker

The intended tick-exact comparison point is OpenTTD's own desync state dump:
`src/openttd.cpp` (`StateGameLoop`, ~line 1246) writes
`save/autosave/dmp_cmds_<seed>_<date>.sav` — a full savegame including `_random.state`
— when `IsVisibleSeverity(Facility::Desync, Severity::Error)` and
`date_fract == 0` and `(TimerGameEconomy::date.base() & 0x1F) == 0` (every 32 game days).
Two runs of one seed whose dumps at matching dates are byte-identical would prove
determinism directly.

**No `dmp_cmds` dump is ever written**, across every combination tried:

| Tried | Result |
|---|---|
| dedicated, `-ddesync=2` (attached, optional-value form) | no dump |
| dedicated, console `debug_level desync=2` (confirmed set: `desync=2` echoed) | no dump |
| single-player `-g -v null` (the canonical desync scenario) | no dump (empty log) |
| `[economy] timekeeping_units = 0` (calendar days, so `date % 32` aligns with `date_fract == 0`) | no dump |
| 40–75 game days per run (crosses two+ `date % 32 == 0` boundaries) | no dump |

`Severity` ordering (`src/debug_type.h`) is `Critical=0, Error=1, …`, so
`desync=2 ≥ Error` makes the severity visible; `debug_level` confirms the level is set.
`NetworkGameLoop` does call `StateGameLoop`, so the dump path is reached. The dump still
does not appear anywhere under the personal dir (`find … -name 'dmp_cmds*'` is empty).
The cause is not yet identified — candidates: the `SaveOrLoad(..., Subdirectory::Autosave)`
failing silently, a fork/rebase change to this path, or a condition not visible in the
read of `StateGameLoop`. This needs a source-level or debugger investigation, not more
black-box runs.

## How to resume

1. Resolve why the desync dump does not fire (add a trace at `src/openttd.cpp:1246`, or
   confirm `SaveOrLoad` to `Subdirectory::Autosave` succeeds in a headless dedicated run).
   Alternatively, add a small research-only console command that prints or saves a state
   checksum / `_random.state` at a named tick, giving a tick-exact comparison point that
   does not depend on the desync dump at all.
2. Then the A/B is: for `idle` and `load` arms, run the same seed to N dump dates (or N
   checksummed ticks), compare byte-for-byte; a matching **control** (idle vs idle) first,
   to prove the comparison is sound, then idle vs load for the result.
3. Wrap it through `tools/research/research` (`init` done:
   `build-research-runs/determinism-ab-20261116`) so the run leaves a manifest, and finish
   with `research report --experiment-id determinism-ab-20261116`.

A draft harness exploring the dedicated + FIFO-console + dump approach was written during
this session but is **not committed** — it depends on the dump that does not fire and on
an HTTP read path that `mcp read` supersedes, so it needs rework once the comparison point
above exists.
