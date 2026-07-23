# Research-mode tooling

This document is the detailed reference for `tools/research/research`, the canonical entry point
that turns the policy in [`AGENTS.md`](../AGENTS.md) §"Research-mode access to internals" and this
directory's [`README.md`](./README.md) into runnable, auditable commands. Read those first for
*why* research-mode access is allowed here; this document is *how* to actually use it.

## Architecture

```
Prismatic or another authorized orchestrator (external, optional)
        |  opaque references only: --orchestrator-run-id, --retrieval-ref, ...
        v
tools/research/research   (this fork's own CLI -- no Prismatic dependency)
        |  configure / build / run / validate / report
        v
OpenTTD research fork, optionally built with OPTION_RESEARCH_INSTRUMENTATION=ON
        |  manifest.json / summary.md / logs / diagnostics
        v
build-research-runs/<experiment-id>/   (local evidence bundle, gitignored)
```

`tools/research/research` has no build-time or run-time dependency on Prismatic, and no code here
reads or writes `~/dev/prismatic-platform`. References to an external orchestrator are opaque
strings, stored verbatim in the manifest and never interpreted.

## Prerequisites

- `cmake`, `ctest`, a C++20 compiler (same as building OpenTTD itself — see `CLAUDE.md`).
- `git`.
- Optional, used only if present (never installed by this tooling): `jq` or `python3` for a real
  JSON syntax check of the generated manifest (falls back to a brace-balance check otherwise);
  `timeout`/`gtimeout` for the headless capture step (falls back to a background+watchdog wrapper
  otherwise).

Run `tools/research/research doctor` to check all of the above without mutating anything.

## Canonical commands

| Command | Purpose | Mutates? |
|---|---|---|
| `doctor` | Report repo/tool/profile state | No |
| `init --experiment-id ID [metadata flags]` | Create the evidence bundle + initial manifest | Creates the experiment directory only |
| `configure --profile P [--experiment-id ID]` | `cmake -S . -B <profile build dir> ...`, or reuse if already configured to match | Configures the profile's build dir only |
| `build --profile P --experiment-id ID` | `cmake --build <profile build dir>` | Builds the profile's build dir only |
| `run --profile P --experiment-id ID` | Headless dedicated-server capture of the `research_status` demonstrator | Runs the built binary (read-only game state; a fresh in-memory game, never an existing save) |
| `validate --gate G [--gate G2 ...] --experiment-id ID` | `smoke`/`change`/`full` (wraps `tools/gate.sh`) and/or `research` (manifest completeness lint) | Depends on gate |
| `report --experiment-id ID` | Finalize `manifest.json`, write `summary.md` | Writes summary + finalizes manifest |
| `clean --experiment-id ID` | Remove exactly that experiment's evidence directory | Deletes that one directory only |

Every command supports `--help`, `--dry-run` (prints planned commands, executes nothing), and
`--output-dir` (default `build-research-runs/`). None of them commit, push, or make network calls.
`--experiment-id` is restricted to `[A-Za-z0-9._-]+` (no `/`, no `..`) so it can never be used to
escape the evidence-bundle root.

### Prismatic / orchestrator metadata (all optional, on `init`)

`--task`, `--research-question`, `--hypothesis`, `--scope`, `--acceptance-criteria`,
`--orchestrator-run-id`, `--agent-run-id`, `--agent-role`, `--task-id`, `--retrieval-ref`,
`--blackboard-ref`, `--evaluation-ref` (or the matching `RESEARCH_*` env vars). Stored verbatim in
the manifest under `orchestrator` / `agent_configuration` / `retrieval_context` /
`blackboard_context` / `evaluation_reference`. Absent values are recorded as the literal string
`"NOT APPLICABLE"`, never guessed. This tooling does not parse or validate these values against
any Prismatic schema — see README.md §0.9/0.10 for why RAG and Blackboard-style coordination are
still only *conceptual* integration points from this fork's side.

## Build profiles

| Profile | Build dir | Build type | `OPTION_RESEARCH_INSTRUMENTATION` | Notes |
|---|---|---|---|---|
| `research-debug` | `build-research/` | Debug | ON | Asserts on. The experimental configuration; compiles in `research_status` and any future research-only hooks. |
| `baseline-release` | `build-research-baseline-release/` | RelWithDebInfo | OFF | `OPTION_USE_ASSERTS=OFF` explicitly, for a clean comparison baseline distinct from `tools/gate.sh`'s own `build/` (which keeps asserts on for its dev-loop purpose). |
| `research-asan` | `build-research-asan/` | Debug | ON | `-fsanitize=address,undefined` passed directly (no dedicated CMake option exists in this codebase — see `AGENTS.md`). **Configure-only in this fork so far** — `build`/`run` refuse this profile outright rather than silently taking a very expensive third full build; see Limitations. |

`configure` refuses to silently reconfigure a build directory whose existing `CMakeCache.txt`
doesn't match the requested profile — it fails with a clear message rather than picking a side.
All three build directories match the existing `.gitignore` rule `/build*`, so no new ignore rule
was needed.

## Instrumentation demonstrator

`research_status` (`src/console_cmds.cpp`, gated by `#ifdef OTTD_RESEARCH_INSTRUMENTATION`) prints
one line: current tick, in-game date, vehicle count, company count. Read-only — it never mutates
game state. `run` captures it by starting the profile's binary as a headless dedicated server
against a throwaway config file (`-D -x -g -G <seed> -c <throwaway.cfg>`) and piping
`research_status` then `quit` via stdin (verified: `src/video/dedicated_v.cpp`'s
`VideoDriver_Dedicated::MainLoop` reads console commands line-by-line from stdin via
`std::getline(std::cin, ...)`). Output is captured to `runtime/stdout.log`, with the matching line
extracted to `diagnostics/research_status.txt`.

When research instrumentation is off (`baseline-release`), `research_status` is not compiled in at
all — the console command is simply unknown, which is itself the evidence that the default build
is unaffected.

**Known startup race, and its workaround.** The dedicated server's stdin console loop starts
consuming input before console-command registration and map generation finish — a command piped
in immediately after process start can get a false "not found" even though it's compiled in
(reproduced directly: even the built-in `help` command was lost without a delay). `run` works
around this with a fixed `sleep 5` before writing `research_status`/`quit` to stdin — confirmed
empirically (not a documented engine guarantee) against runs as short as a 3s delay succeeding
under normal load. **This is a fixed delay, not a readiness signal**: under heavy concurrent system
load (observed `load average` in the 30–90 range from several simultaneous builds on the same
machine) a single `run` can still race and produce an empty `diagnostics/research_status.txt` even
with the 5s margin — `run` appends a `limitations` entry automatically when this happens rather
than silently reporting success, and a retry has reliably succeeded in every case observed so far.
If this becomes a recurring problem, the real fix is polling `runtime/stderr.log` for the "Map
generated, starting game" line instead of a fixed sleep, not simply increasing the delay further.

## Manifest schema

`tools/research/schema/experiment-manifest.schema.json`, `schema_version` `"1.0.0"`. JSON, not
YAML (no new parsing dependency). Adapts the field shape already established in
[`experiment-template.md`](./experiment-template.md) (base revision, goal/hypothesis, changed
files, build/debug configuration, a validation table, result, limitations, status) rather than
inventing a parallel taxonomy, extended with structured `commands[]`/`validations[]` arrays,
`internal_access`/`instrumentation` fields, and the opaque orchestrator/retrieval/Blackboard
references above. The schema file itself never hardcodes this checkout's private remote URL or
absolute paths — those only ever appear in a *generated* `manifest.json`, which is local-only
(gitignored).

Bump `schema_version` on any breaking field change; keep the filename and `$id` stable so older
manifests remain self-describing via the field itself.

## Evidence bundle layout

```
build-research-runs/<experiment-id>/
├── manifest.json          <- canonical machine-readable output (see schema above)
├── summary.md             <- human-readable, generated from manifest.json + the jsonl files below
├── commands.jsonl         <- one JSON object per executed/dry-run command (redacted)
├── validations.jsonl      <- one JSON object per gate run (smoke/change/full/research)
├── observations.jsonl     <- one JSON string per directly-doctorable fact
├── limitations.jsonl      <- one JSON string per known gap
├── .state.sh              <- internal plumbing only (shell-quoted KEY=value facts); not versioned data
├── git/
│   ├── status-before.txt, status-after.txt, diff-stat.txt
├── build/configure.log, build/build.log
├── validation/<gate>.log
├── runtime/stdout.log, stderr.log, openttd-research.cfg
└── diagnostics/research_status.txt
```

`build-research-runs/` matches the existing `/build*` `.gitignore` rule, so generated runs are
never accidentally staged — no new ignore rule was needed. Templates/schema (`tools/research/`
itself) are versioned; everything under `build-research-runs/` and the profile build directories
is generated and gitignored.

## Status semantics

Same fail-closed taxonomy as [`research/README.md`](./README.md): `PASS` / `FAIL` / `PARTIAL` /
`NOT RUN` / `NOT APPLICABLE` (`INCONCLUSIVE` also accepted by the schema). `report` aggregates the
final `status` itself, mechanically: `FAIL` if any recorded command or validation failed, `PASS`
only if at least one validation ran and none failed, `PARTIAL` if commands ran but no validation
gate was ever invoked, `NOT RUN` if nothing was recorded at all. It never rounds up.

## Redaction

`redact()` (`tools/research/lib/common.sh`) is an allowlist-style pattern replacement — not a
general secret scanner — applied to every command string before it is logged or written to
`commands.jsonl`: `--password=`, `--token=`, `--secret=`, `--api-key=`/`--apikey=`,
`Authorization:`, and `PASSWORD=`/`TOKEN=`/`SECRET=`/`API_KEY=`-style environment assignments are
replaced with `[REDACTED]`. This is a documented limitation, not a guarantee — don't pass secrets
on the command line to begin with. No environment variables are captured wholesale; only the
explicit `RESEARCH_*` allowlist above is ever read.

## Internal-API / instrumentation policy

Governed by `AGENTS.md` — this tooling doesn't add new policy, it records what the policy already
allows: which internal APIs were used, which internals were exposed, whether access was
mutating (always `false` for `research_status`), and the active compile-time gate. See
`manifest.json`'s `internal_access` and `instrumentation` fields.

## Observer effects

Recorded automatically when the build type is `Debug` (asserts on, unoptimized — not
representative of release-build timing), and when `research_status` reflects a freshly-started
headless game (tick near 0, `GENERATE_NEW_SEED`) rather than a loaded save. Report any
experiment-specific observer effect (e.g. sanitizer overhead, added synchronization) in the
`observer_effects` array via a filled-in `experiment-template.md` note alongside the manifest, or
extend `manifest_write` if it becomes a recurring, mechanically-derivable fact.

## Claims discipline

This tooling never auto-generates a sentence like "the experiment proved X" or "the agent
succeeded." The `claims` array is empty by default and stays that way unless a human deliberately
fills it in elsewhere — see the mission's "Claims discipline" phase and `research/README.md`'s
fail-closed taxonomy. `observations` only ever contains directly-doctorable facts ("command X
exited 0"); `preliminary_findings` is for optional interpretation that references a specific
observation, not a free-standing assertion.

## Prismatic integration boundary

See README.md §0.10 for the authoritative status table. From this document's own scope: this CLI
exposes *only* opaque reference fields (§"Prismatic / orchestrator metadata" above) — no shared
database, no hardcoded endpoint, no dependency on Prismatic's runtime, and no assumption about
what an external orchestrator does with a completed `manifest.json` beyond reading it as a file.

## Troubleshooting

- **`configure` refuses with a mismatch error**: another tool or session configured the profile's
  build directory with different flags. Inspect `<build dir>/CMakeCache.txt` yourself; if a fresh
  configure is really intended, remove the directory manually first (this tool will never do that
  for you implicitly).
- **`run` produces an empty `diagnostics/research_status.txt`**: check `runtime/stderr.log` first
  — a missing usable graphics/base set (see `CLAUDE.md` §Running) is the most likely cause of a
  headless start failing before the console command is even read; the startup-race workaround
  above (5s fixed delay) can also still lose the race under heavy system load — retry once before
  assuming anything is actually broken, and check `limitations.jsonl` for an automatic note.
- **`configure`/`build` fails with `'source_location' file not found`**: the documented macOS CLT
  gotcha from `CLAUDE.md` §Build — an Xcode Command Line Tools install older than Xcode 15 ships a
  libc++ without `<source_location>`. `configure` already prefers Homebrew's LLVM toolchain
  automatically on macOS when `CC`/`CXX` aren't already set in the environment (confirmed: this is
  what makes `baseline-release` buildable on such a machine at all — before this was wired in,
  `configure`'s own "already matches, reusing" fast path could mask the problem for a
  previously-good build dir while a *fresh* configure of a new profile hit it immediately). If it
  still fails, `brew install llvm` and retry, or export `CC`/`CXX` yourself to a working compiler.
- **Shared `build-research/` contention**: this build directory is also used directly by hand and
  potentially by other concurrent sessions in this fork. `configure`/`build` are safe to re-run
  (idempotent for identical inputs) but two full builds racing on a heavily loaded machine will
  simply be slow, not incorrect, since they compile identical sources with identical flags.

## Cleanup

`clean --experiment-id ID` removes exactly `build-research-runs/<experiment-id>/` and refuses any
path outside that root (including a `..`-shaped `--experiment-id`, rejected at argument-parsing
time). It does not touch build directories (`build-research*`) — remove those manually if a truly
fresh configure is needed, per the Troubleshooting note above.
