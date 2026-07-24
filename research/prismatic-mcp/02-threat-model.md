# 02 — Threat Model (Phase 2)

Scope: a RESEARCH-ONLY, compile-gated, **loopback-only** MCP server embedded in an
OpenTTD research fork. The endpoint is never bound to a public interface. The threat
model is nonetheless taken seriously because a local unauthenticated TCP port is a real
attack surface (other local processes, malicious web pages via DNS rebinding, an
over-eager model).

## Actors

Malicious local web page (DNS rebinding); malicious/curious local process; compromised or
buggy MCP host; the model itself invoking tools it shouldn't; a curious multiplayer client;
a hostile multiplayer server; malformed / slow / oversized HTTP clients; a local process
scraping the token; the developer accidentally enabling a remote bind.

## Design invariants that neutralize whole classes

1. **Loopback-only bind.** `Start()` constructs explicit `127.0.0.1` and `::1` listen sockets and
   never uses the all-interfaces default (`network.cpp:745`). There is no config/UI to bind publicly;
   a remote mode would need a separate threat model. → defeats remote-connect and most DNS-rebinding value.
2. **Bearer auth, 256-bit, per-start.** A fresh `RandomBytesWithFallback` token every `Start()`,
   compared in constant time, required on every request. Never in `openttd.cfg`, never in normal logs,
   never in a URL. → an unauthenticated local process cannot call the API.
3. **Origin rejection.** Any request carrying an `Origin` header is refused (403). Browsers always send
   `Origin`; a legitimate MCP host (Inspector CLI, Claude Code) does not. → defeats DNS-rebinding from a
   web page even before auth.
4. **Read-only in this build.** No mutation tool is registered. → privilege-escalation and
   double-execution abuse cases are structurally impossible in this vertical (they re-open only when
   Phase G/H land the approval workflow).
5. **Main-thread-only game access.** Handlers run inside `Poll()` on the game thread; no worker thread
   dereferences a pool. → no data race, no torn read of simulation state.
6. **Bounded everything.** Body ≤ 1 MiB, header section ≤ 32 KiB, ≤ 16 connections; `select` with a
   zero timeout so the game loop never blocks. → slow-client / oversized / connection-flood DoS is bounded.

## Abuse cases → response (this build)

| # | Abuse | Response |
|---|---|---|
| 1 | Remote host connects | Not bound to any non-loopback interface; OS refuses. |
| 2 | Browser DNS rebinding | `Origin` present → 403 before auth; also loopback-only. |
| 3 | Missing token | 401 `WWW-Authenticate: Bearer`. |
| 4/5 | Bad token / creds in URL | 401; token only read from `Authorization: Bearer`, never the path. |
| 6 | Duplicate/ambiguous headers | Case-insensitive single-value scan; last wins deterministically; no smuggling path (no `Transfer-Encoding` support). |
| 7 | Oversized `Content-Length` | Rejected when `> MAX_REQUEST_BODY` (1 MiB); connection closed. |
| 8 | Unsupported transfer encoding | Only `Content-Length` bodies are read; chunked is not accepted. |
| 9 | Deeply nested JSON | nlohmann parse is bounded by the 1 MiB body cap; parse errors → JSON-RPC `-32700`. |
| 10 | JSON-RPC batch | Explicitly rejected `-32600` (arrays refused). |
| 12 | Many incomplete connections | ≤ 16 connections; excess `accept`s are closed immediately. |
| 13/14 | Huge map / all-vehicle dump | Not exposed in this build; company/state resources are bounded and small. (Pagination lands with the high-cardinality resources — see plan.) |
| 15/16 | Mutation while read-only / after TTL | No mutation tool exists; every mutating method is "Method not found". |
| 20 | Private network-client data | `network/clients` resource not exposed yet; when added, IP/keys are redacted by default. |
| 21/22 | Arbitrary file / path traversal | No filesystem resource or export path is exposed in this build. |
| 26 | Shutdown during a write | `Shutdown()` closes all connection sockets and the listener; a partial send is simply dropped. |
| 27 | Malformed UTF-8 | nlohmann rejects invalid UTF-8 on parse/dump → `-32700`; no crash. |

## Residual risks (honestly stated)

- **Token visibility to the local operator.** `prismatic_mcp token` prints the token to the in-game
  console by design (the operator needs it to configure a client). Any process that can read that
  console output can read the token. Acceptable for a single-developer local research workflow;
  documented, not hidden.
- **No session table yet.** This build is effectively stateless per request (auth on every call). `DELETE`
  and token rotation are honored, but per-session scoping/expiry (mission §11.4) is staged for Phase G.
- **No rate limiting beyond connection/size caps.** A local authenticated client could issue many
  requests; each is cheap and bounded, but a formal rate limiter is a Phase J item.
- **Abuse cases 17-19, 23-25 (approval replay, mid-command save/load, token-rotation-mid-session)** are
  N/A until the approval/mutation workflow exists; they are pre-registered here so the regression tests
  land with that code.

Every item above maps to a test or an explicit "staged" note in `05-implementation-plan.md` and
`09-testing.md`; nothing is claimed as covered that is not.
