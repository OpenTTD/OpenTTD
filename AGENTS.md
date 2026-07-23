# AGENTS.md

Vendor-neutral instructions for any coding agent (Claude Code, or any other AI coding tool) working in this repository.

## What this repository actually is

`korczis/OpenTTD` is **not** the official OpenTTD project and is **not** staged for upstream contribution. It is korczis's private academic research fork: a real, complex, long-lived C++ codebase used as a **test/validation target for the Prismatic platform** — a testbed for evaluating coding-agent workflows, gating design, change/refactor quality, and reproducibility of agent-driven engineering. The goal of work here is never "prepare an OpenTTD pull request."

Two things live in this repository, and they play different roles:

- **OpenTTD itself** (`src/`, `regression/`, `docs/`, the build system) — the system under test / technical substrate. Its own engineering conventions (`CODINGSTYLE.md`, savegame compatibility rules, the command-pattern architecture described in `CLAUDE.md`, etc.) remain real technical constraints *when you touch that code* — they're part of what makes the substrate meaningful to experiment against, not upstream red tape.
- **This fork's own research/process layer** (`AGENTS.md`, `CLAUDE.md`, `research/`, `tools/gate.sh`, and the locally-excluded `.claude/`/`.aiad/`) — the experimental scaffolding around it.

Upstream's own contribution process — `CONTRIBUTING.md`, the PR template, the "AI-generated contributions are against policy" clause, `commit-checker`/`docs-checker`/`rebase-checker` CI, the strict commit-message grammar — describes *their* rules for *their* repository. Read it as background technical reference if useful (e.g. it explains why the code looks the way it does), but it is **not** this fork's process authority and must never be treated as a blocking gate here.

## Ground rules

- **No upstream interaction.** Never push to, or open issues/PRs against, `OpenTTD/OpenTTD`. Everything here — commits, branches, experiments — stays local to `korczis/OpenTTD`.
- **No commit or push without an explicit, current instruction.** A prior approval does not carry forward to later turns or later, different changes.
- **Protect the existing working tree.** Before any command that could discard uncommitted work (`checkout`/`restore`/`reset`/`clean`, `rm -rf` inside the repo), check `git status` first and preserve what's already there. Don't overwrite in-progress work you didn't create this session.
- **Prefer minimal, scoped changes over broad refactors.** This is a research substrate, not a product to polish.
- **Distinguish four kinds of change**, and be explicit about which one you're making:
  1. **Product change** — edits to OpenTTD's own game code/docs, under upstream's own conventions (`src/`, `docs/`, etc.).
  2. **Experimental infrastructure change** — this fork's own validation tooling (`tools/gate.sh`, `research/`).
  3. **Agentic tooling change** — the locally-excluded `.claude/`/`.aiad/` (see below).
  4. **Documentation change** — `README.md`, `AGENTS.md`, `CLAUDE.md`, or similar.
- **Experimental changes are allowed**, provided they are explicitly scoped, their effect is measurable, they're reversible, and they're validated by the matching gate below — not shipped on vibes.
- **Never report untested work as passing.** If a gate wasn't run, say "NOT RUN," not "should be fine." If a result is ambiguous, treat it as not-passing rather than rounding up — fail closed, not open.
- **Never invent your own bypass for a failing gate.** No silent `--no-verify`, no "I'll fix it in a follow-up commit" (a failed gate means nothing was committed — there's nothing to patch after the fact), no quietly narrowing scope just to dodge a failure. If a gate can't be satisfied, say so plainly and let the human decide; only add an explicit override at the human's own request, with their stated reason attached.
- **State which validation layers actually ran**, with the exact commands and their exit status, whenever reporting a change as done — see the taxonomy in `research/README.md`.
- **Don't blindly follow upstream's contribution workflow** (commit-message grammar, PR templates, "ask before large changes" discussion norms) unless it's actually relevant to a technical verification you're doing — it is not this fork's local process.

## Validation layers

One entry point, three build/test tiers of increasing cost, plus a reporting layer:

| Tier | Entry point | Cost | Purpose |
|---|---|---|---|
| Smoke | `tools/gate.sh smoke` | cheapest | fast feedback while actively iterating |
| Change | `tools/gate.sh change` | standard | before calling a task done |
| Full | `tools/gate.sh full` | expensive | significant changes, or an experimental baseline |
| Research/evaluation | `research/experiment-template.md` | n/a | records what was actually done and validated for an experiment |

Run `tools/gate.sh --help` for the exact commands each tier runs, and see `research/README.md` for the PASS/FAIL/PARTIAL/NOT RUN/NOT APPLICABLE taxonomy. `full` is not required for small or documentation-only changes — match the tier to the size of the change.

## Research-mode access to internals

This is a private academic research fork (see "What this repository actually is" above), not a project bound by upstream's public-API-purity or backward-compatibility goals as a *local* process rule. Controlled access to internal implementation details, non-public APIs, debug configurations, and research-only instrumentation is an explicitly allowed, expected part of working here — an agent does not need to ask for separate permission to use it beyond whatever scope the current task already has.

**"Internal exposure" in this repository means controlled observability and access for authorized local research tooling and agents.** It does not mean unauthenticated public exposure, secret disclosure, or weakening systems outside the experiment boundary — see Containment below.

### Allowed

Within an already-scoped task, an agent may:

- read and work directly with OpenTTD's internal implementation, non-public headers, and internal data structures (`src/`), including code with no documented stable interface;
- use non-public functions, classes, and symbols where relevant to the task;
- bypass a stable public abstraction if it stands in the way of a needed measurement — see the decision rule below;
- add a research-only adapter, test hook, or introspection point over an internal component;
- change a symbol's visibility, or add a `friend`/test-only accessor, when the change is minimal and documented at the point of change — this is not an absolute prohibition on touching visibility, just a bar of "smallest, most auditable change";
- create snapshots or serializations of internal state for experimental analysis, without expecting long-term format stability;
- enable and use debug builds, `OPTION_USE_ASSERTS` (on `cmake/Options.cmake`), additional invariant checks, the project's existing compiler warnings, profiling, tracing, verbose/subsystem debug logging (`-d <facility>=<level>`, facilities listed in `src/debug.h`, e.g. `net`, `sl`, `desync`, `script`, `yapf`), state dumps, deterministic seeds, and fault injection, as the task requires. Sanitizers (ASan/UBSan/TSan) have no dedicated CMake option in this codebase today — enabling one means passing the standard compiler flags directly (e.g. `-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined`), which is allowed but should be reported as exactly what was passed;
- build debug-only CLI commands, local diagnostic endpoints, instrumentation callbacks, event streams, experiment-control hooks, and validation probes, when they serve an explicitly scoped experiment.

**Prefer the smallest mechanism that makes the required state observable.** A direct internal hook is acceptable when a public abstraction would add complexity without improving the experiment. Do not redesign production architecture merely to avoid using an internal API in an experiment.

### What this does not mean

- publishing secrets, credentials, tokens, or private keys (including `.env` contents);
- exposing an internal or diagnostic interface on a public network interface without an explicit reason and protection;
- unauthorized access to external systems, or bypassing access controls outside this experiment's own scope;
- disclosing personal or sensitive data;
- claiming upstream OpenTTD supports or endorses a research-only interface;
- presenting a research-only interface as a stable, upstream-supported public API;
- treating a debug-build or instrumented result as evidence about release-build behavior without saying so.

### Labeling and containment

Label a research-only interface for what it is — `RESEARCH-ONLY`, `INTERNAL`, `UNSTABLE`, `DEBUG-ONLY`, `EXPERIMENTAL`, or whatever fits — and note, as relevant: why it exists, whether it's compile-time or runtime gated, whether it's off by default, what it exposes, whether it can affect behavior, and known observer effects. Not every hook needs every one of these; document what actually applies. Prefer containment that's already available (compile-time/runtime gating, default-off, local-only, easy to remove) over a mechanism with no off switch. **Research permission is not a license for unrelated refactoring, broad visibility changes, or permanent global debug output** — keep the experimental surface to what the task actually needs.

### Reporting

- State which internal APIs, internals, or instrumentation were used or changed.
- Label unstable/research-only interfaces as such — don't imply they're stable.
- Record the active debug/instrumentation configuration (build type, CMake options, compiler flags, runtime flags) alongside the validation-layer reporting already required above.
- Distinguish research instrumentation from an actual product behavior change.
- Report observer effects and known limitations rather than omitting them.

## `.claude/` and `.aiad/` (local agentic tooling)

These directories hold personal Claude Code tooling (commands, agents, skills, hooks) imported from another private project for local reference. They are excluded from git via `.git/info/exclude` — deliberately *not* `.gitignore`, since this exclusion is machine/person-local and not a rule every clone of this fork should inherit — and must stay uncommitted. Any generally-useful principle worth keeping from them belongs in *this* file, written once here, not copied into a second or third location.

## Where to look

- [CLAUDE.md](./CLAUDE.md) — Claude-specific workflow notes, plus OpenTTD's build/test/run commands, architecture, and code style (all still real technical reference).
- [README.md](./README.md) — short public-facing explanation of what this repository is.
- [research/README.md](./research/README.md) — the gating model and experiment-report taxonomy in full.
