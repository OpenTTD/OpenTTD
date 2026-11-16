# Research / validation layer

This directory is part of this fork's own process scaffolding, not upstream OpenTTD material — see [AGENTS.md](../AGENTS.md) for the full context (this repo is a personal research fork used as a validation target for the Prismatic platform, not staged for upstream contribution).

## Gating model

Four layers, increasing in cost. Match the layer to the size of the change — don't run `full` for a one-line doc fix, and don't call a source change "done" on `smoke` alone.

| Layer | Entry point | Cost | When |
|---|---|---|---|
| Smoke | `tools/gate.sh smoke` | cheapest | fast iteration while actively editing |
| Change | `tools/gate.sh change` | standard | before marking a task/experiment done |
| Full | `tools/gate.sh full` | expensive | significant changes, or establishing an experimental baseline |
| Research | `tools/gate.sh research` | standard+ | a change to code behind `OPTION_RESEARCH_INSTRUMENTATION` (or the MCP server option), which the other tiers never compile |
| Research/evaluation | this directory | n/a | records what was actually done and validated for an experiment |
| Published surfaces | `tools/surfaces/surfaces check` | seconds | runs inside `change`, the pre-commit hook and CI: Markdown regions current, every relative link and anchor resolving |

`tools/gate.sh --help` documents the exact commands each layer runs. A tier that had to leave the `regression_*` cases out because no base graphics set is reachable says so, reports them as NOT RUN and exits 3 (PARTIAL), never PASS -- see [baseset-prerequisite-macos.md](./baseset-prerequisite-macos.md). All three are thin wrappers around this repo's own existing CMake/CTest toolchain (see `CLAUDE.md`) — there is no separate lint/format gate because no such tool (e.g. a `.clang-format` config) currently exists in this codebase; don't fake one.

## Internals, debugging, and instrumentation

Using internal APIs, debug builds, assertions, sanitizers, tracing, or research-only instrumentation is explicitly allowed in this fork — see [AGENTS.md](../AGENTS.md) §"Research-mode access to internals" for the full policy. What matters for an experiment report: state the exact build/runtime configuration used (e.g. `OPTION_USE_ASSERTS`, compiler flags, `-d <facility>=<level>`, any added instrumentation), and don't generalize a debug-build or instrumented result to release-build behavior without separate evidence. Instrumentation itself can alter timing, memory layout, or scheduling — note that as a known limitation when it applies.

## Recording an experiment

Copy [`experiment-template.md`](./experiment-template.md), fill it in, and keep it wherever is convenient for you (this directory is a reasonable default, but filled-in reports are not required to be committed — they're working notes, not gated artifacts). The template is the versioned, reusable part; a specific experiment's report is not.

The status taxonomy is deliberately fail-closed: if you're unsure whether something counts as a full PASS, it doesn't.

- **PASS** — every validation command that was run completed successfully, and the stated goal was actually met.
- **FAIL** — at least one required validation command failed, or the goal was not met.
- **PARTIAL** — some validation ran and passed, but coverage was incomplete (e.g. `full` skipped for a change that would have warranted it), or the goal was only partly achieved.
- **NOT RUN** — validation was not executed at all for this experiment.
- **NOT APPLICABLE** — the change has no meaningful validation command (e.g. pure documentation with no behavioral claim attached).

Never round a result up to PASS to make it look more finished than it is.

## Documents in this directory

Generated from each document's title and first paragraph (`tools/surfaces/surfaces sync`).

<!-- surfaces:begin docs-index dir=research -->
| Document | What it is |
|---|---|
| [Experiment report: baseset-prerequisite-macos](baseset-prerequisite-macos.md) | The four regression ctest cases (regression_regression, regression_stationlist, regression_gs, regression_gs_compat) require a base graphics set to be installed, even though they run headless (-vnull -snull -mnull).... |
| [Experiment report:](experiment-template.md) | List the exact commands run and their outcome. Don't summarize away detail — "ran the tests" is not enough, "tools/gate.sh change -> exit 0, 98/98 tests" is. |
| [Driving OpenTTD from outside: MCP panel, developer console and Rhai](mcp-control.md) | RESEARCH-ONLY. The embedded MCP server (ADR 0008, research/prismatic-mcp/) is the channel; this is the tooling that drives a running game through it. Everything binds to loopback. |
| [The MCP hub: every instance, every operation, every event over one WebSocket](mcp-hub.md) | RESEARCH-ONLY, UNSTABLE. Issue I0058; rule project.mcp-surfaces-are-derived. |
| [A person-like player: the interface as data, the mouse as input](mcp-human.md) | RESEARCH-ONLY. The embedded MCP server (ADR 0008, research/prismatic-mcp/) runs in every OpenTTD process: a dedicated server and each GUI client have their own. This document covers the part that lets a bot play the... |
| [Prismatic ↔ OpenTTD bridge — architecture design](prismatic-bridge/ARCHITECTURE.md) | Status: Phase 1 (wire-protocol + basic control-path round trip) is real and built — as a standalone third repository, ~/dev/openttd-prismatic-bridge (Python/FastAPI), not inside ~/dev/prismatic-platform. This... |
| [00 — Baseline (Phase 0)](prismatic-mcp/00-baseline.md) | Evidence-backed snapshot of the repository before any MCP implementation, per the mission's Phase 0. Everything here is recovered from the live repo, not assumed. |
| [01 — OpenTTD System Map (Phase 1)](prismatic-mcp/01-openttd-system-map.md) | Actual call paths, ownership and thread context, recovered by reading the source (not inferred from filenames). Every claim is anchored file:line. This is the foundation the architecture is built on. |
| [02 — Threat Model (Phase 2)](prismatic-mcp/02-threat-model.md) | Scope: a RESEARCH-ONLY, compile-gated, loopback-only MCP server embedded in an OpenTTD research fork. The endpoint is never bound to a public interface. The threat model is nonetheless taken seriously because a local... |
| [03 — Architecture Options (Phase 3)](prismatic-mcp/03-architecture-options.md) | A loopback HTTP/1.1 + JSON-RPC server living inside the OpenTTD process, serviced on the main game thread from GameLoop(). Reuses NetworkAddress::Listen for the socket, nlohmann/json for encoding,... |
| [04 — Architecture Decision Record (Phase 4)](prismatic-mcp/04-architecture-decision-record.md) | Status: Accepted (foundational vertical implemented; broader phases staged). Decision: Embed a compile-gated, loopback-only Streamable-HTTP MCP server in the OpenTTD research fork, serviced entirely on the main game... |
| [05 — Implementation Plan & Phase Status (Phase 4)](prismatic-mcp/05-implementation-plan.md) | Honest status of every phase. DONE = implemented and compiled (and, where noted, run). PARTIAL = foundation in place, explicit gaps. STAGED = designed, not yet implemented. Per AGENTS.md, nothing is marked done that... |
| [07 — Resource Catalog](prismatic-mcp/07-resource-catalog.md) | Resources implemented in this build. All are read-only, wrapped in the common envelope (schema_version, source, source_revision, protocol_version, game_tick, game_date, game_mode, availability, redactions, truncated,... |
| [08 — Tool Catalog](prismatic-mcp/08-tool-catalog.md) | This build registers no tools. tools/list returns an empty array and tools/call is not implemented. This is deliberate (ADR decision 6): no tool — especially no mutating tool — is exposed until the... |
| [09 — Testing](prismatic-mcp/09-testing.md) | Honest status. Per AGENTS.md, a layer not run is reported NOT RUN, never "should pass." |
| [10 — In-Game Runbook & Verified Vertical](prismatic-mcp/10-in-game-runbook.md) | The read-only vertical, executed against a real running OpenTTD (MCP-enabled dedicated server, loaded from a fixture savegame) on 2026-07-24. Every line below is captured output, not illustration. |
| [11 — Continuation Baseline (Phase 2 Recovery)](prismatic-mcp/11-continuation-baseline.md) | Recovered actual state before writing continuation code, per the continuation mission §2-3. Source: direct inspection of the working tree this session (the Phase-1 implementation is this session's own prior work,... |
| [12 — Continuation Plan & Adversarial Review (Phase 2)](prismatic-mcp/12-continuation-plan.md) | One canonical MCP stack in src/prismatic_mcp/ (transport + JSON-RPC + resources), serviced on the main thread from GameLoop()->Poll(). Continuation adds — in the same module, no second stack — a typed... |
| [16 — Decision Contract & Approval Workflow](prismatic-mcp/16-decision-contract.md) | Implemented in src/prismatic_mcp/prismatic_mcp_server.cpp. Verified live (Vertical B). |
| [18 — Continuation Testing](prismatic-mcp/18-continuation-testing.md) | Uses the repository's existing Catch2 framework (no second framework), registered via add_test_files(... test_prismatic_mcp.cpp) in src/tests/CMakeLists.txt and auto-discovered by catch_discover_tests(openttd_test).... |
| [19 — Continuation Runbook](prismatic-mcp/19-continuation-runbook.md) | Exact commands for the verified Decision-Contract vertical (Vertical B). Every line was run. |
| [Prismatic × OpenTTD Embedded MCP Server (research-only)](prismatic-mcp/README.md) | A RESEARCH-ONLY, compile-gated, loopback-only Model Context Protocol server embedded inside this personal OpenTTD research fork. It exposes live OpenTTD internal state to MCP clients (Claude Code, MCP Inspector, the... |
| [Exposure Coverage Matrix](prismatic-mcp/exposure-coverage-matrix.md) | Honest coverage of internal domains using the mission's status vocabulary. This build exposes a deliberately small read-only slice; the rest is catalogued as designed-but-not-yet-adapted, not pretended-covered. |
| [MCP Spec Compliance Matrix (stable 2025-11-25)](prismatic-mcp/spec-compliance-matrix.md) | Pinned version: 2025-11-25 (PrismaticMCP::PROTOCOL_VERSION). Draft/RC behavior is not implemented. Status: DONE / PARTIAL / STAGED. |
| [A/B: the planner's air pair vs no planner (seed 22311, 12 months)](results/ab-20261116T014214-seed22311.md) | Generated by tools/research/mcp-ab (first planner-driven bot, air pair + buses). Both games use the same seed and map and start the same bot (PrismaticAI). A starts it with the planner's best air pair (mcp-plan... |
| [Research-mode tooling](tooling.md) | This document is the detailed reference for tools/research/research, the canonical entry point that turns the policy in AGENTS.md §"Research-mode access to internals" and this directory's README.md into runnable,... |
<!-- surfaces:end docs-index -->
