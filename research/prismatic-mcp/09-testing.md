# 09 — Testing

Honest status. Per AGENTS.md, a layer not run is reported NOT RUN, never "should pass."

## Current layers

Counts are not written here, because they change with every test added; each row names the
command that measures them. Counted from the source on 2026-11-17 (commit `fce2a08d53`):
27 `TEST_CASE`s in `src/tests/test_prismatic_mcp.cpp` and 28 `check` calls in
`tools/research/mcp-live-check`.

| Layer | Command | What it checks | Count it with |
|---|---|---|---|
| Unit tests | `tools/gate.sh research` (Catch2, `src/tests/test_prismatic_mcp.cpp`, tag `[prismatic_mcp]`) | security helpers, the action-type table, command parameter conversion, the reset rules, the approval state machine, the toolbar menu and requests, the console command family, prices, terrain, the base map, ways, empty building and vehicle lists, tool annotations | `grep -c '^TEST_CASE' src/tests/test_prismatic_mcp.cpp`; assertions: the Catch2 summary line of the run |
| Live security and approval | `tools/research/mcp-live-check` | on a headless dedicated game: token file (mode 600), 401 without or with a wrong token, 403 with an Origin, protocol 2025-11-25, live reads, pending approval, changed arguments refused, approved call executed with its effect observed and its verdict read with `openttd.decision.get`, replay refused, a submitted decision denied changes nothing and asks again, `openttd.cmd.Pause` with wrong and right parameters, a command with a text parameter and a news command run once approved, the listener closed on stop | `grep -c '^check ' tools/research/mcp-live-check`; the run prints one line per check |
| Coverage | `tools/research/mcp-coverage-check --probe <token-file>` | on a dedicated game: every resource read and every read-only tool called; the approval-gated tools are the live check's | the script's own report: published entries against those called (`openttd://mcp/coverage`) |
| Client coverage | `tools/research/mcp-coverage-check --probe --exercise <token-file>` on a headless client (`openttd -v null:ticks=2147483647 -s null -m null -g`, I0077) | every view, UI, input, toolbar-menu and decision tool called once with schema-derived arguments, every answer well-formed, the game still running after it; no tool without readOnlyHint | the script's own report |

The token comes from the token file `prismatic_mcp start <port> <file>` writes, not from console
text (knowledge `console-lines-the-panel-parses-are-an-interface`): the live check read the console
line and broke when its wording became accurate.

## History: the first session

- **Configure gate (DONE).** `cmake -DOPTION_PRISMATIC_MCP_SERVER=ON -DOPTION_RESEARCH_INSTRUMENTATION=ON`
  configures; status line prints `Option Prismatic MCP Server - ON`. The `FATAL_ERROR` guard for
  ON-without-research exists in `add_definitions_based_on_options()`.
- **Compile (DONE).** `src/prismatic_mcp/prismatic_mcp_server.cpp` compiles clean under the fork's
  Homebrew-LLVM toolchain with the module's warnings enabled.
- **Baseline unchanged (DONE).** `build-research` (MCP OFF) continues to build — the feature is fully
  gated, so an OFF build is byte-for-byte unaffected.
- **In-game read-only vertical (see `10-in-game-runbook.md`).** Real MCP client (curl over HTTP)
  performing JSON-RPC `initialize` + `resources/read` against a running OpenTTD, observing live state.

## NOT RUN / STAGED at the first session (most of it has since run: see Current status)

- **Catch2 unit tests** — JSON-RPC parsing, HTTP partial-read, auth, Origin, registry determinism,
  resource serialization, redaction. Files go in `src/tests/` via `add_test_files(... CONDITION
  OPTION_PRISMATIC_MCP_SERVER)`; the HTTP parser and JSON-RPC dispatch are written as free functions
  precisely so they can be unit-tested without a socket. (Phase J)
- **Fuzz targets** — HTTP parser, JSON-RPC, resource-uri, Authorization header, with ASan/UBSan
  (`build-research-asan` profile exists). (Phase J)
- **Determinism experiment** — A/B: identical seed, one run idle, one hammered with read-only MCP
  reads; compare game-state/command-log. The read path issues no command and touches no RNG, so the
  expectation is bit-identical — but this must be *demonstrated*, not asserted. (Phase J)
- **Multiplayer mutation test**, **MCP Inspector session**, **official conformance suite**,
  **native-GUI resolution matrix** — all STAGED with their feature phases.

## Abuse-case coverage

`02-threat-model.md` enumerates each abuse case with its response; the ones exercisable without the
mutation/session code (loopback-only, Origin reject, missing/bad token, oversized body, batch reject,
malformed JSON) are structurally enforced in `HandleHttpRequest`/`DispatchOne` and are the first
Catch2 targets. Approval-replay / mid-save / token-rotation-mid-session are N/A until Phase G.
