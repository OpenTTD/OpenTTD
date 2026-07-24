# 00 — Baseline (Phase 0)

Evidence-backed snapshot of the repository *before* any MCP implementation, per
the mission's Phase 0. Everything here is recovered from the live repo, not assumed.

## Repository state

| Field | Value |
|---|---|
| Repository | `/Users/korczis/dev/OpenTTD` (private research fork `korczis/OpenTTD`) |
| Base branch | `master` |
| Base revision | `0822f6b762ff19b470871a9a1dcee55b5124eadd` ("Add: Zola site scaffold for GitHub Pages") |
| Working branch (created this session) | `research/prismatic-mcp-server` |
| Pre-existing dirty file | `src/network/network_server.cpp` (26 insertions) — a **user-authored** research change adding `OTTD_RESEARCH_ALLOW_REVISION_MISMATCH` env gate to `ReceiveClientJoin`. Preserved untouched; carried onto the new branch. |
| Pre-existing build dirs | `build`, `build-research`, `build-research-asan`, `build-research-baseline-release`, `build-research-runs` |

## Toolchain

| Tool | Version |
|---|---|
| cmake | 4.4.0 |
| system compiler | Apple clang 14.0.3 — **too old** for `<source_location>` in `stdafx.h` |
| **build compiler** | Homebrew LLVM `/usr/local/opt/llvm/bin/clang++` (per `build-research/CMakeCache.txt`) |
| ctest, git, jq, python3, timeout | available (`gtimeout` missing — degraded fallback) |

The research-debug build **compiles and links cleanly** (verified this session: incremental
`cmake --build build-research --target openttd` → `[100%] Built target openttd` in ~49 s).
Only benign `ld` warnings about dylibs built for a newer macOS than the link target. This
confirms the in-game vertical is achievable on this machine.

## Existing research infrastructure (reused, not reinvented)

- **`cmake/Options.cmake:65`** declares `OPTION_RESEARCH_INSTRUMENTATION` (default OFF).
  **`:136-137`** turns it into the compile definition `-DOTTD_RESEARCH_INSTRUMENTATION`.
  The new `OPTION_PRISMATIC_MCP_SERVER` will follow this exact pattern and *depend* on it.
- **`build-research`** profile = Debug, asserts on, research instrumentation ON.
  **`build-research-baseline-release`** = RelWithDebInfo, asserts off, instrumentation OFF (comparison baseline).
  **`build-research-asan`** = Debug + ASan/UBSan (configured, not yet built).
- **`tools/gate.sh {smoke,change,full}`** — the validation entry point (never commits/pushes; fails fast).
- **`tools/research/research {doctor,init,configure,build,validate,report}`** — experiment manifest tooling.
- **`research/prismatic-bridge/ARCHITECTURE.md`** — the existing REST bridge design (the sibling
  `~/dev/openttd-prismatic-bridge` FastAPI service that talks the admin network). The MCP server is
  the *embedded, in-process* successor concept — it does not modify or depend on that bridge.

## Governing policy (from AGENTS.md / CLAUDE.md, both read in full)

- Private fork; **no upstream interaction**, no PR, never represent as official OpenTTD.
- **The command system is the core invariant** (`CLAUDE.md`): all synchronized game-state mutation
  must go through `src/command.cpp` two-phase test/execute, and in network games is propagated as a
  deterministic lockstep command stream. MCP mutations MUST use this path — never write pools directly.
- Research-only interfaces are *allowed and expected*, but must be labeled (`RESEARCH-ONLY`),
  compile-time/runtime gated, default-off, scoped, reversible, and honestly validated
  (**never mark a gate PASS that did not run** — fail closed).
- No commit/push without an explicit current instruction. No savegame-format change without documented need.

## Confirmed foundational facts (from parallel mapping, Phase 1)

- **No HTTP server exists** in OpenTTD — `network/core/http.h` is an outbound libcurl *client* only.
  The MCP transport implements the first inbound HTTP/1.1 server in the codebase.
- **Reusable listener**: `TCPListenHandler<...>` (`network/core/tcp_listen.h:28`) provides
  `Listen/AcceptClient/select/Receive`. The admin server (`network_admin.cpp`) is the closest analog
  ("a second TCP listener on its own port, polled from the game tick").
- **All network I/O runs on the main game thread**, driven from `NetworkGameLoop` /
  `NetworkBackgroundLoop` (`network.cpp:1086-1136`). Keeping MCP accept/parse/dispatch on the main
  thread is the idiomatic, lock-free choice — and mandatory for any code that touches pools.
- **CSPRNG**: `RandomBytesWithFallback(std::span<uint8_t>)` (`core/random_func.cpp:94`, uses
  `arc4random_buf` on macOS) — the same primitive the network crypto layer uses. 256-bit token =
  `std::array<uint8_t,32>`. Constant-time compare via Monocypher `crypto_verify32`.
- **JSON**: nlohmann json **3.11.3** vendored at `src/3rdparty/nlohmann/json.hpp`, already used by
  `network_survey.cpp`, `script/api/script_admin.cpp`, etc. **No new dependency needed.**

## Baseline result

`PASS` — repository state recovered, toolchain verified building, policy and reusable primitives
identified. Cleared to proceed to full system map and threat model.
