# 18 — Continuation Testing

Uses the repository's existing Catch2 framework (no second framework), registered via
`add_test_files(... test_prismatic_mcp.cpp)` in `src/tests/CMakeLists.txt` and auto-discovered by
`catch_discover_tests(openttd_test)`. The whole file is `#ifdef OTTD_PRISMATIC_MCP_SERVER`, so it only
compiles into a research build.

## What is tested (pure, security-critical logic)

The digest/parse/auth helpers were factored into `src/prismatic_mcp/prismatic_mcp_detail.h` (pure,
no game-state dependency) so the exact production code paths are unit-tested. `test_prismatic_mcp.cpp`:

- **`ArgDigestHex`** — deterministic; different args → different digest (this is what defeats the
  "change arguments after approval" abuse case); different tool → different digest; 16 lowercase hex.
- **`IsKnownActionType`** — only `game.set_pause` is executable; unknown/empty/`rcon` rejected.
- **`ParseContentLength`** — case-insensitive; spaces; absent → nullopt; empty value → nullopt; zero.
- **`ExtractBearer`** — accepts `Bearer`/`bearer`, trims OWS; rejects `Basic`/no-scheme/empty.
- **`ConstantTimeEquals`** — matches `==` semantics incl. length mismatch and empty.

These are the exact functions the running server uses (the server now calls the shared helpers), so a
green test run is evidence about production behavior, not a parallel reimplementation.

## Run

```bash
cmake --build build-mcp --target openttd_test -j
cd build-mcp && ctest -R prismatic_mcp --output-on-failure
```

## Still NOT RUN (honest)

Catch2 tests for the stateful approval state machine and HTTP framing (partial reads) need a small
seam to drive the connection buffer without a socket — the free-function `HandleHttpRequest` is written
for that but not yet wired to a test harness. Fuzzing, determinism A/B, Inspector, conformance, and
native-UI tests remain staged (`continuation-gap-matrix.json`).
