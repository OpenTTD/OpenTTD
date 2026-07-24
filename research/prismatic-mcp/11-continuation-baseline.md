# 11 — Continuation Baseline (Phase 2 Recovery)

Recovered actual state before writing continuation code, per the continuation mission §2-3.
Source: direct inspection of the working tree this session (the Phase-1 implementation is this
session's own prior work, committed as `0a49a3dbb8`).

## Repository state

- Branch `research/prismatic-mcp-server`, HEAD `0a49a3dbb8` ("Add: Embedded loopback MCP server").
- Pre-existing uncommitted change: `src/network/network_server.cpp` (user's revision-mismatch research
  gate) — **preserved, not touched, not committed** (treated as another operation's work).
- Untracked build dirs (`build-mcp`, `build-research*`) and `.cache/` (clangd) — ignored.

## Minimum-baseline verification (mission §3)

| Baseline item | State | Evidence |
|---|---|---|
| Feature gate exists, OFF by default | PRESENT_VERIFIED | `OPTION_PRISMATIC_MCP_SERVER` default OFF; requires research instrumentation (FATAL_ERROR verified) |
| GUI build, feature ON, compiles | PRESENT_VERIFIED | `build-mcp` GUI build linked `openttd` (76 MB) |
| Baseline build, feature OFF, compiles | PRESENT_VERIFIED | `build-research` (no `OTTD_PRISMATIC_MCP_SERVER`) built 100% |
| Dedicated build, feature ON | PARTIAL | ran a dedicated server from the `build-mcp` binary (GUI build, `-D`); a true `OPTION_DEDICATED=ON` MCP build is NOT RUN |
| Loopback listener starts | PRESENT_VERIFIED | console `prismatic_mcp start` → listening on 127.0.0.1:8731 |
| Authentication works | PRESENT_VERIFIED | no token → 401; bad token → 401; live run |
| initialize / ping | PRESENT_VERIFIED | live JSON-RPC handshake, protocol 2025-11-25 |
| resources/list + real resources/read | PRESENT_VERIFIED | 4 resources; game/state + companies returned live data (tick advanced between reads) |
| Native status visible | PARTIAL | via **console** (`prismatic_mcp status`); no native toolbar/window yet |
| Server stops cleanly | PRESENT_VERIFIED | `stop` → connection refused; game continued |
| No pool access off main thread | PRESENT_VERIFIED | handlers run inside `Poll()` in `GameLoop()`; no worker thread exists |
| No mutation off main thread | PRESENT_VERIFIED (vacuous) | no mutation exists yet |
| No secret in logs | PRESENT_VERIFIED | token only via explicit `prismatic_mcp token`; never in normal logs/cfg |

**Conclusion:** the Phase-1 baseline is real and usable; no repair needed. There is exactly **one**
MCP stack (no duplicate to consolidate). Continuation builds on it in place — same module, same
console command, same architecture. The two known baseline gaps (native UI; dedicated-ON MCP build)
are carried into the gap matrix, not silently skipped.
