---
schema: adr/v1
id: adr-0032
kind: adr
title: The bridge's admin-protocol client is a native Rust service (axum + tokio), not the Python/FastAPI bridge
status: proposed
date: 2026-11-16
tags:
  - network
  - bridge
  - architecture
related:
  - file:research/prismatic-bridge/ARCHITECTURE.md
  - file:.ai/repo/adrs/0016-multiplayer-is-deterministic-lockstep-every-peer-runs-the-sa.md
provenance:
  origin: authored
  derived_from:
    - issue:I0012
    - file:research/prismatic-bridge/ARCHITECTURE.md
---

# 32. The bridge's admin-protocol client is a native Rust service (axum + tokio), not the Python/FastAPI bridge

## Context

`prismatic-platform` steers a company in a live game of this fork through a standalone bridge that
speaks OpenTTD's Admin Network protocol (length-prefixed binary TCP packets, fixed-width
integer/string fields, its own auth handshake) and exposes it over HTTP. The bridge already exists,
in the separate `openttd-prismatic-bridge` repository, as a working Python/FastAPI service: Phase 1
(control-path round trip, `start_ai`/`reload_ai` over HTTP) and Phase 2 (a real single-town bus
route driven by a settings string) are built and tested, 8/8 unit tests passing, and a live reload
took a company from `busses: 0` to `busses: 3, company_value: 11025`
(`research/prismatic-bridge/ARCHITECTURE.md` §5).

`research/prismatic-bridge/ARCHITECTURE.md` §3.1 deliberately left the client's language and home
open: an earlier instruction in this fork asked for Rust + Rustler + NIFs, a request that predates
knowing a working Python service already existed — and Rustler binds Rust to the BEAM, which a
standalone (non-Elixir) service has no host for. §3.1 laid out four options with their costs and
recorded that **no option had been chosen**, so the choice would be made explicitly rather than by a
silent rewrite. I0012 (`bridge-language-decision`) is that decision, which only a person can make:
the engineering default (keep the working, tested, un-bottlenecked Python) and the owner's earlier
stated preference (Rust) point different ways.

## Decision

The bridge's admin-protocol client is a **native Rust service** (`axum` + `tokio`), owning the
Admin Network TCP socket and the packet encode/decode directly and exposing the same HTTP surface
the Python bridge exposes today. FastAPI/uvicorn and the hand-written `bridge/protocol.py` codec are
retired; there is no PyO3 boundary and no second host language. This is the single-language,
no-host-binding form of the owner's original "Rust" intent: a standalone service has no BEAM to bind
a Rustler NIF to, so the native-service shape is what "Rust" means for this component.

Decided by the owner for I0012 on 2026-11-16 (session
`4ec5c536-e4e5-4d1e-b87e-e6d8df83d0fc`), via an explicit choice over the §3.1 options table; the
answer verbatim was "Full native Rust rewrite (axum)". Recorded in the decision log
(`majordomus decision`, title "The bridge's admin-protocol client is a full native Rust service").

This ADR records the decision and its rejected alternatives; it does not itself perform the rewrite.
The rewrite is implementation work in the `openttd-prismatic-bridge` repository, tracked by its own
follow-up issues, preserving the existing HTTP contract and port-for-port behaviour so Phases 3–5
(`research/prismatic-bridge/ARCHITECTURE.md` §5) are unaffected by the language change.

## Alternatives rejected

- **Keep Python/FastAPI as-is.** The minimal-change default (AGENTS.md prefers minimal changes),
  already working and 8/8 tested, with packet parsing not a measured bottleneck. Rejected in favour
  of a single native-Rust service: the owner's standing preference is Rust for this
  performance-/protocol-level component, and a native service removes the Python runtime and the
  FastAPI/uvicorn stack from the bridge's deployment entirely rather than carrying them indefinitely.
  Cost of the chosen path over this one: a full rewrite of a repo that already works — the largest
  of the options.
- **Accelerate only the codec with a Rust extension via PyO3/`maturin`.** Rewrite just
  `bridge/protocol.py`'s packet encode/decode in Rust and keep the FastAPI app around it — the
  closest analog to the original Rust-NIF idea, for Python instead of Elixir. Rejected: it adds a
  `maturin` build toolchain and a Python↔Rust FFI boundary for an unproven benefit (the codec is not
  a measured bottleneck) while still keeping the Python runtime and FastAPI. A native Rust service
  achieves the single-language goal without the FFI seam.
- **Move the admin client into `prismatic-platform` as an Elixir app with a Rustler NIF.** Elixir
  (`:gen_tcp`/a `GenServer`) owning the socket, a pure Rust NIF doing encode/decode only — the
  original §3.1 draft proposal. Rejected: it duplicates working functionality inside a different
  repo and language and is justified only if `prismatic-platform` must own the TCP connection itself
  rather than call an HTTP API; it calls an HTTP API, so the standalone service boundary stays.

## Consequences

- The `openttd-prismatic-bridge` repository is rewritten from Python/FastAPI to a native Rust
  (`axum` + `tokio`) service in follow-up implementation issues. Until that lands, the Python bridge
  remains the running implementation; this decision does not delete working code, it directs what
  replaces it. The HTTP contract (`/decision/pull`, `/ai/start`, `/ai/reload/{id}`, `/state`,
  `/companies`, `/ws/events`) and the localhost dev behaviour are preserved so the choice is
  invisible to `prismatic-platform` and to Phases 3–5.
- OpenTTD's `src/` is still untouched: the bridge routes through the existing Admin Network protocol
  and AI-script API, which the language of the client does not change
  (`research/prismatic-bridge/ARCHITECTURE.md` §6).
- The Admin Network protocol's binary framing and auth handshake must be reimplemented in Rust with
  parity to the Python `bridge/protocol.py`; the existing Python implementation and its 8/8 tests
  are the reference and the oracle for a byte-level conformance comparison. The insecure
  `AdminJoin` path only (not `AdminJoinSecure`) remains in scope, as in the Python bridge.
- `research/prismatic-bridge/ARCHITECTURE.md` §3.1 and §6 are updated to record the decision as made
  rather than open, pointing here.

## For this fork

- Supersedes the "no option has been chosen" default in
  `research/prismatic-bridge/ARCHITECTURE.md` §3.1.
- Maps: [network](../workflows/substrate/network/README.md).
- Related: [ADR 0016](0016-multiplayer-is-deterministic-lockstep-every-peer-runs-the-sa.md)
  (the admin/observation path is why `src/` never changes).
