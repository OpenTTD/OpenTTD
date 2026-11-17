# 19 — Continuation Runbook

Exact commands for the verified Decision-Contract vertical (Vertical B). Every line was run.

## Configure & build

```bash
cmake -S . -B build-mcp -DOPTION_RESEARCH_INSTRUMENTATION=ON -DOPTION_PRISMATIC_MCP_SERVER=ON
cmake --build build-mcp --target openttd -j$(sysctl -n hw.ncpu)
```

Status update (2026-11-17): the original run forced Homebrew LLVM
(`-DCMAKE_C_COMPILER=/usr/local/opt/llvm/bin/clang`, likewise for C++). That is no longer done: the
system compiler is kept when it supports `<source_location>`, and Homebrew LLVM is only the
fallback for an older one (ADR 0006, [`../tooling.md`](../tooling.md) "Troubleshooting"). For a
recorded experiment, build through the research CLI's `research-mcp` profile instead
([`../tooling.md`](../tooling.md), "Embedded MCP server checks").

## Run dedicated server + start MCP (console over a FIFO)

```bash
mkfifo /tmp/mcp_fifo
( sleep 900 > /tmp/mcp_fifo & )
./build-mcp/openttd -D -c <cfg> -g <save.sav> < /tmp/mcp_fifo > ottd.log 2>&1 &
echo "prismatic_mcp start" > /tmp/mcp_fifo     # -> http://127.0.0.1:8731/mcp
echo "prismatic_mcp token" > /tmp/mcp_fifo     # -> 64-hex token in ottd.log (operator only)
TOKEN=$(grep -oE '[0-9a-f]{64}' ottd.log | tail -1)
```

## Decision-contract vertical

```bash
H="Authorization: Bearer $TOKEN"; U=http://127.0.0.1:8731/mcp
# 1. validate (non-mutating)
curl -s $U -H "$H" -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"openttd.decision.validate","arguments":{"action":{"type":"game.set_pause","arguments":{"pause":true}}}}}'
# 2. submit -> pending_approval (note approval_id)
curl -s $U -H "$H" -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"openttd.decision.submit","arguments":{"action":{"type":"game.set_pause","arguments":{"pause":true}}}}}'
# 3. operator approves in the OpenTTD console
echo "prismatic_mcp approve 1" > /tmp/mcp_fifo
# 4. resubmit -> executed
curl -s $U -H "$H" -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"openttd.decision.submit","arguments":{"action":{"type":"game.set_pause","arguments":{"pause":true}}}}}'
# 5. observe the delta
curl -s $U -H "$H" -d '{"jsonrpc":"2.0","id":4,"method":"resources/read","params":{"uri":"openttd://game/state"}}'   # data.paused: true
# 6. provenance
curl -s $U -H "$H" -d '{"jsonrpc":"2.0","id":5,"method":"resources/read","params":{"uri":"openttd://decisions"}}'
```

Console parity: `prismatic_mcp approvals`, `prismatic_mcp approve <id>`, `prismatic_mcp deny <id>`,
`prismatic_mcp decisions [n]`.

## Stop

```bash
echo "prismatic_mcp stop" > /tmp/mcp_fifo
echo "quit" > /tmp/mcp_fifo
```

## Observed result (captured)

`paused` transitioned `false → true` via the approved, digest-bound `game.set_pause` through the normal
command path. Replay of the approved call → new `pending_approval` (denied). Changed-args submit
(`pause:false` under a `pause:true` approval) → `pending_approval` (digest-bound). Evidence:
`research/prismatic-mcp/evidence/decision-vertical.jsonl` (token redacted).

## Not covered here (staged)

Native toolbar/Control Center flow (Vertical A), the autonomous multiplayer lab (Vertical C), MCP
Inspector/conformance. See `continuation-gap-matrix.json`.
