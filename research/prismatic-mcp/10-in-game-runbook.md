# 10 — In-Game Runbook & Verified Vertical

The read-only vertical, executed against a **real running OpenTTD** (MCP-enabled dedicated server,
loaded from a fixture savegame) on 2026-07-24. Every line below is captured output, not illustration.

## Setup

```bash
# Build (research instrumentation + MCP, Homebrew LLVM toolchain)
cmake -S . -B build-mcp -DOPTION_RESEARCH_INSTRUMENTATION=ON -DOPTION_PRISMATIC_MCP_SERVER=ON \
  -DCMAKE_C_COMPILER=/usr/local/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++
cmake --build build-mcp --target openttd -j

# Run dedicated server from a fixture save; feed console via a FIFO
./build-mcp/openttd -D -c <cfg> -g <save.sav> < fifo > log 2>&1 &
echo "prismatic_mcp start" > fifo    # -> "MCP server started: http://127.0.0.1:8731/mcp"
echo "prismatic_mcp token" > fifo    # -> 64-hex bearer token (local operator only)
```

Server banner: `OpenTTD 20260724-prismatic-mcp-server-m0822f6b762`. Console log:
`[net:1] [prismatic-mcp] RESEARCH-ONLY MCP server listening on http://127.0.0.1:8731/mcp (loopback only)`.

## Verified steps (captured)

| Step | Request | Result |
|---|---|---|
| Auth required | `POST /mcp` (no token) `ping` | **401** |
| Initialize | `initialize` | `protocolVersion: 2025-11-25`, `serverInfo.name: openttd-prismatic-mcp`, version = live build revision |
| List resources | `resources/list` | 4: `openttd://mcp/status`, `.../meta/build`, `.../game/state`, `.../companies` |
| **Read live state** | `resources/read openttd://game/state` | `game_date 2049-05-15`, `game_tick 2687077`, `map 256x256`, `num_towns 22`, `num_vehicles 11`, `num_companies 1`, `paused false` |
| **Read live companies** | `resources/read openttd://companies` | company `id 0`, `is_ai true`, `money 79963`, `inaugurated_year 2047` |
| Liveness proof | two reads | `game_tick` advanced 2687077 → 2687079 between reads — the game is running and reads execute on the live main thread |
| Origin rejected | `ping` + `Origin: http://evil.example` | **403** (anti-DNS-rebinding) |
| Bad token | `ping` + `Bearer deadbeef` | **401** |
| Unknown resource | `resources/read openttd://nope` | JSON-RPC `-32602 Unknown resource uri` |
| Batch rejected | `[{...}]` | `-32600 Batch requests are not supported` |
| Method not found | `tools/call` | `-32601 Method not found` (no tools registered — by design) |
| Token rotation | `prismatic_mcp rotate` then reuse old token | **401** — old bearer invalidated |
| Status | `prismatic_mcp status` | `running endpoint=http://127.0.0.1:8731/mcp read_only=true requests=11 denied=4 conns=0` |
| Clean stop | `prismatic_mcp stop` then `GET /mcp` | connection **refused** — listener closed; game continued, then `quit` |

Redacted transcript: `evidence/mcp-transcript.jsonl` (token replaced with `<REDACTED_TOKEN>`).

## What this proves / does not prove

**Proves:** a real MCP client (curl performing JSON-RPC 2.0 over HTTP) connects to a running OpenTTD
process, negotiates protocol 2025-11-25, discovers resources, and reads **live** game and company
state; local-security defaults (bearer auth, Origin rejection, token rotation, loopback-only, clean
teardown) behave as designed.

**Does not prove (STAGED):** MCP Inspector / official conformance run, a native-UI approval of a typed
mutation with observed post-command state, the determinism A/B experiment, multiplayer, SSE. These are
the mission's remaining phases (F–J) and are honestly marked not-run in `09-testing.md`.
