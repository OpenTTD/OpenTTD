# Prismatic × OpenTTD Embedded MCP Server (research-only)

A **RESEARCH-ONLY**, compile-gated, loopback-only [Model Context Protocol](https://modelcontextprotocol.io)
server embedded inside this private OpenTTD research fork. It exposes live OpenTTD internal state to
MCP clients (Claude Code, MCP Inspector, the Prismatic platform) over HTTP+JSON-RPC, pinned to the
stable **2025-11-25** spec. It is **not** part of upstream OpenTTD and is never network-exposed.

> Status: a real, compiling, connectable **read-only** vertical. Typed mutation, the native GUI
> Control Center, SSE, and full conformance/determinism evidence are **staged** — see
> `05-implementation-plan.md` for the honest per-phase status. Nothing here is a mock: the resources
> return live pool state over real HTTP.

## Build

```bash
cmake -S . -B build-mcp \
  -DOPTION_RESEARCH_INSTRUMENTATION=ON \
  -DOPTION_PRISMATIC_MCP_SERVER=ON \
  -DCMAKE_C_COMPILER="$(brew --prefix llvm)/bin/clang" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build-mcp --target openttd -j$(sysctl -n hw.ncpu)
```

`OPTION_PRISMATIC_MCP_SERVER` defaults OFF and **requires** `OPTION_RESEARCH_INSTRUMENTATION=ON`
(configure fails loudly otherwise). When OFF, none of the module is compiled — zero footprint.

## Run

Start OpenTTD (GUI or dedicated), then in the in-game console (backtick `` ` ``):

```
prismatic_mcp start          # binds 127.0.0.1:8731, prints the bearer token
prismatic_mcp status         # running/endpoint/counters (never prints the token)
prismatic_mcp token          # prints the bearer token (local operator only)
prismatic_mcp rotate         # new token, invalidates the old one
prismatic_mcp stop
```

## Connect (any MCP client, or curl)

```bash
TOKEN=<token from 'prismatic_mcp token'>
curl -s http://127.0.0.1:8731/mcp \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'

curl -s http://127.0.0.1:8731/mcp -H "Authorization: Bearer $TOKEN" \
  -d '{"jsonrpc":"2.0","id":2,"method":"resources/read","params":{"uri":"openttd://game/state"}}'
```

Bearer token via a header only — never in the URL. The endpoint refuses any request carrying an
`Origin` header (anti-DNS-rebinding) and any non-loopback connection.

## Documents

`00-baseline.md` · `01-openttd-system-map.md` · `02-threat-model.md` · `03-architecture-options.md` ·
`04-architecture-decision-record.md` · `05-implementation-plan.md` · `07-resource-catalog.md` ·
`08-tool-catalog.md` · `09-testing.md` · `10-in-game-runbook.md` · `spec-compliance-matrix.md` ·
`exposure-coverage-matrix.md`.

## Boundaries

Not an official OpenTTD feature. No upstream PR/issue. No public bind, no unauthenticated access, no
token in logs/cfg/URL, no raw memory/pointer/filesystem exposure, no game-state mutation in this
build. See `02-threat-model.md` and the ADR.
