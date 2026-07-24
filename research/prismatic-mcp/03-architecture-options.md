# 03 — Architecture Options (Phase 3)

## Option A — Embedded Streamable-HTTP server (in-process)

A loopback HTTP/1.1 + JSON-RPC server living inside the OpenTTD process, serviced
on the main game thread from `GameLoop()`. Reuses `NetworkAddress::Listen` for the
socket, nlohmann/json for encoding, `RandomBytesWithFallback` for the token.

- **+** Native lifecycle; direct, lock-free access to live pool state on the main thread.
- **+** Attach-to-running-game workflow (the mission's primary use case).
- **+** No new runtime dependency; no new thread; no savegame change.
- **−** Must implement an inbound HTTP parser (none exists in the tree).

## Option B — Embedded stdio transport

OpenTTD launched by the MCP host, protocol over stdout.

- **−** stdout must carry only MCP framing — hostile to a GUI game that logs to stdout.
- **−** Cannot attach to an already-running game. Useful only as an optional later transport.

## Option C — Sidecar bridge (the existing `~/dev/openttd-prismatic-bridge`)

A separate process speaking the admin network, exposing REST.

- **−** Cannot expose native internals (UI windows, pools) — it only sees the admin protocol.
- **−** Fails the mission's explicit "embedded server" requirement. (It remains valuable as the
  external REST bridge; this feature is the in-process complement, not its replacement.)

## Option D — Community C++ MCP SDK

- **−** Adds a dependency requiring a license/maintenance/binary-size ADR; must be pinned to stable
  2025-11-25; integration surface against OpenTTD's build is non-trivial.
- **−** The protocol subset needed here (initialize, resources, tools, JSON-RPC over HTTP) is small
  enough to implement directly against nlohmann/json, avoiding the dependency entirely.

## Decision

**Option A.** It is the only option that satisfies "embedded server exposing native internals,"
reuses existing primitives (listener, JSON, CSPRNG), adds no dependency/thread/savegame change, and
keeps all game-state access on the main thread where it is safe by construction. The one cost — an
inbound HTTP parser — is bounded and self-contained. See `04-architecture-decision-record.md`.
