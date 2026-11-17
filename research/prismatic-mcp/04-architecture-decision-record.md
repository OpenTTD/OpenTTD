# 04 — Architecture Decision Record (Phase 4)

**Status:** Accepted (foundational vertical implemented; broader phases staged).
**Decision:** Embed a compile-gated, loopback-only Streamable-HTTP MCP server in the OpenTTD
research fork, serviced entirely on the main game thread, exposing read-only live state now and
staging typed mutation behind an approval workflow.

## Context

The mission requires an MCP server *inside* OpenTTD that exposes broad typed internal state and,
eventually, approved typed actions, without breaking multiplayer determinism, savegame
compatibility, game-loop performance, or local security. Phase-1 mapping established: no inbound HTTP
server exists; a reusable non-blocking listener + main-thread network pump does; all mutation must go
through the `Command<>::Post` deterministic path; pools are unsynchronized main-thread state; a
vendored JSON library and a CSPRNG already exist.

## Decision detail

1. **Compile gate.** `OPTION_PRISMATIC_MCP_SERVER` (default OFF) requires
   `OPTION_RESEARCH_INSTRUMENTATION=ON` (configure `FATAL_ERROR` otherwise) and defines
   `OTTD_PRISMATIC_MCP_SERVER`. When OFF: no sources compiled, no listener, no token, no console
   command, no symbols — zero footprint. Verified by the baseline build.
2. **Transport.** Own HTTP/1.1 parser over raw `recv`/`send` on sockets from
   `NetworkAddress("127.0.0.1"/"::1").Listen()`. Bound to loopback only, never the all-interfaces
   default. Serviced from `PrismaticMCP::Poll()` in `GameLoop()` with a zero-timeout `select` so the
   game loop never blocks.
3. **Threading.** Single rule: *network-side code never dereferences a pool off the main thread.*
   Because `Poll()` runs on the game thread, handlers read pools directly with no locking — the same
   safety property the admin network relies on. No worker thread is introduced.
4. **Protocol.** JSON-RPC 2.0 pinned to MCP stable **2025-11-25**: `initialize`,
   `notifications/initialized`, `ping`, `resources/list`, `resources/read`, `tools/list`,
   `prompts/list`. Batches rejected; parse/validation errors mapped to JSON-RPC codes.
5. **Security.** 256-bit bearer token per `Start()` (CSPRNG, constant-time compare, never in
   cfg/logs/URL); any `Origin` header rejected (anti-DNS-rebinding); loopback-only bind; bounded
   body/header/connection limits.
6. **Read-only first.** This build exposes only read-only resources over live state. **No mutation
   is registered.** Mutation is deferred to the approval workflow (Phase G/H) so that the
   two-phase-approval + digest-binding + replay-prevention guarantees exist *before* any command can
   be issued — never a mutation path without them.
7. **No savegame change, no new MP command.** MCP runtime state is process-local and ephemeral;
   future mutations reuse existing `Commands`, never a new command "for MCP convenience."

## Adversarial design review (mission §8 — 20 questions)

| # | Question | Response in this design |
|---|---|---|
| 1 | Network callback accesses a pool directly? | Only from `Poll()` on the main thread; no other thread exists. |
| 2 | MCP mutates synchronized state without a normal command? | No mutation exists yet; when it lands it must go through `Command<>::Post`. |
| 3 | Polling changes simulation? | `Poll()` only reads pools + services sockets; issues no command. |
| 4 | Serialization alters iteration/RNG? | `Pool::Iterate()` is const traversal; no RNG touched; token uses the OS CSPRNG, not the game RNG. |
| 5 | Slow client stalls the loop? | `select` timeout 0; per-request work bounded; oversized/slow connections closed. |
| 6 | Huge query exceeds frame budget? | Only small bounded resources exposed; high-cardinality ones ship with pagination (staged). |
| 7 | Request survives a savegame transition wrongly? | `OnGameModeChanged()` hook present; this build rebuilds snapshots per request so none persist. |
| 8 | Session retains access after disarm? | No arming/mutation yet; token rotation invalidates the bearer immediately. |
| 9 | Token leaks to logs/cfg? | Never written to cfg or normal logs; only printed on explicit `prismatic_mcp token`. |
| 10 | Untrusted browser reaches the listener? | Loopback-only + `Origin` rejection. |
| 11 | Client invokes a tool for another company? | No company-scoped tool exists yet; will require ownership validation + approval. |
| 12 | Malformed IDs hit freed pool objects? | Resource reads use `Iterate()`/`GetIfValid`; invalid IDs → typed error, never `Get()` on a bad id. |
| 13 | Widget insertion breaks toolbar indices? | Native toolbar deferred; console surface has no index-sync hazard. |
| 14 | Dedicated build compiles without GUI symbols? | Module references no GUI symbols; `#ifdef DEDICATED` handled in `meta/build`. (Dedicated build test staged.) |
| 15 | Feature-off build omits everything? | Gate compiles nothing when OFF; verified by baseline build. |
| 16 | Responses expose private MP info? | Company data is server-visible; network-client IP/keys resource not exposed yet, redacted when added. |
| 17 | Resource ordering nondeterministic? | Pools iterate ascending-index; catalogue is a fixed `std::array`. |
| 18 | Cancellation leaves half-applied mutation? | No mutation/long-op yet; N/A until Phase H. |
| 19 | Retries execute a command twice? | No command path yet; idempotency + digest binding are Phase G requirements. |
| 20 | Server claims success before observing result? | No mutation yet; the async-callback caveat (§5.4) is documented so future mutation observes real state. |

Every "staged/N/A" answer is tracked in `05-implementation-plan.md` with the phase that closes it.

Status update (2026-11-17): this record describes the first vertical and is kept as written.
Since then:

- **Decision 6 (read-only first)** has been followed through, not reversed: mutations were
  registered only after the approval workflow existed. `openttd.game.set_pause`,
  `openttd.cmd.<Name>` (every game command whose parameters convert from JSON), `openttd.ai.*` and
  `openttd.api.*` with `execute: true` all go through the two-phase approval bound to the exact
  canonical arguments, with replay refused, and post through `Command<>::Post`
  (`16-decision-contract.md`).
- **Q2, Q18, Q19, Q20:** every mutation goes through `Command<>::Post`; one approval executes once
  (`detail::ClassifySubmit`, unit-tested); the verdict is taken after the command has run and read
  with `openttd.decision.get`.
- **Q6:** high-cardinality resources (`openttd://vehicles`, `openttd://map`, ...) are exposed whole,
  without pagination (`05-implementation-plan.md` Phase E).
- **Q8:** interface input from an agent is armed per process by the owner
  ([ADR 0014](../../.ai/repo/adrs/0014-interface-input-from-an-agent-is-armed-per-process-by-the-ow.md),
  proposed); token rotation still invalidates the bearer.
- **Q11:** a command tool takes a `company` actor; company-scoped authorization per client is
  still staged.
- **Q13:** the fork has its own toolbar button and menu (I0072, `prismatic_mcp_menu.cpp`); a
  dedicated server draws no toolbar.

## Consequences

- A genuinely connectable MCP server exists in-process with strong local-security defaults and zero
  release-build footprint.
- The riskiest surfaces (mutation, native GUI toolbar) are deliberately *not* shipped in the first
  vertical, so no unsafe path exists ahead of its guarantees. This is a smaller-but-honest slice, not
  a mock: the resources return live pool state over real HTTP+JSON-RPC.
