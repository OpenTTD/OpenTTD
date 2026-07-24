# 09 — Testing

Honest status. Per AGENTS.md, a layer not run is reported NOT RUN, never "should pass."

## Ran this session

- **Configure gate (DONE).** `cmake -DOPTION_PRISMATIC_MCP_SERVER=ON -DOPTION_RESEARCH_INSTRUMENTATION=ON`
  configures; status line prints `Option Prismatic MCP Server - ON`. The `FATAL_ERROR` guard for
  ON-without-research exists in `add_definitions_based_on_options()`.
- **Compile (DONE).** `src/prismatic_mcp/prismatic_mcp_server.cpp` compiles clean under the fork's
  Homebrew-LLVM toolchain with the module's warnings enabled.
- **Baseline unchanged (DONE).** `build-research` (MCP OFF) continues to build — the feature is fully
  gated, so an OFF build is byte-for-byte unaffected.
- **In-game read-only vertical (see `10-in-game-runbook.md`).** Real MCP client (curl over HTTP)
  performing JSON-RPC `initialize` + `resources/read` against a running OpenTTD, observing live state.

## NOT RUN / STAGED (with the phase that adds them)

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
