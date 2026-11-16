---
schema: adr/v1
id: adr-0029
kind: adr
title: A research-gated, local-only override accepts a mismatched peer revision for a loopback R&D game, without spoofing the build's own revision
status: proposed
date: 2026-11-16
tags:
  - research
  - network
  - determinism
related:
  - file:.ai/repo/adrs/0016-multiplayer-is-deterministic-lockstep-every-peer-runs-the-sa.md
  - file:.ai/repo/adrs/0004-research-access-to-internals-is-permitted-and-research-code-.md
  - rule:project.research-code-is-gated-and-labelled
  - rule:project.no-upstream-interaction
  - file:src/network/core/network_game_info.cpp
  - file:research/net-compat.md
provenance:
  origin: authored
  derived_from:
    - issue:I0070
    - file:src/network/core/network_game_info.cpp
    - file:src/network/network_server.cpp
---

# 29. A research-gated, local-only override accepts a mismatched peer revision for a loopback R&D game, without spoofing the build's own revision

## Context

The agent R&D of this fork runs the game and the bot together on one machine, over loopback: the
owner starts a server and a client of this fork and lets an agent play (ADR 0008, ADR 0013). The
server and the client are the same fork, behaviourally identical, but their revision strings often
differ — a dirty working tree reports `-m<hash>`, a clean build `-g<hash>`, and two commits report
two hashes. Deterministic lockstep (ADR 0016) refuses a client whose revision does not match the
server's: `src/network/network_server.cpp:ServerNetworkGameSocketHandler` rejects the join with
`NetworkErrorCode::WrongRevision`, and the client's own pre-check
(`src/network/core/network_game_info.cpp:CheckGameCompatibility`) marks the server incompatible.
Both go through one function, `src/network/core/network_game_info.cpp:IsNetworkCompatibleVersion`.

The owner is the author of this fork, the two endpoints are their own, and the traffic never leaves
127.0.0.1. Rebuilding both ends from one clean commit on every iteration (the honest fix recorded
in knowledge `a-build-from-a-modified-tree-cannot-join-a-clean-build`) is impractical for a tight
R&D loop against a dirty tree.

## Decision

Add a **research-gated, local-only, non-spoofing** acceptance policy over the revision check.

1. **It only widens acceptance, in four named policies.** `IsNetworkCompatibleVersion` is the one
   chokepoint both roles use, so one process acting as both the loopback server and the client is
   covered. The policy is `strict` (upstream behaviour, the default), `whitelist` (a genuine match
   plus revisions in a data-driven `allow` list), `blocklist` (any revision except those in a
   `block` list), or `open` (any peer). A genuine self-match always passes except under an explicit
   `block`; the policy never narrows below `strict`.
2. **It never changes what this build reports.** `GetNetworkRevisionString()` is untouched. This
   build always sends its own true (modified) revision. It does not, and the policy cannot be used
   to, impersonate another build. A stock, non-consenting remote server runs its own check and
   still rejects this client; the policy only lets a server **the owner runs** choose to accept a
   mismatched peer — the local operator's own decision on their own game.
3. **It is strict by default and compile-gated.** The code compiles only under
   `OPTION_RESEARCH_INSTRUMENTATION` (`OTTD_RESEARCH_INSTRUMENTATION`), and `_research_net_compat`
   defaults to `strict` even then. It is configured from the tilde console (`research_net_compat
   <policy>|allow <rev>|block <rev>|clear`), from `scripts/autoexec.scr` (this fork's native config
   script), and from the environment for headless, scripted CI/CD runs
   (`OTTD_RESEARCH_NET_REVISION_POLICY`, `..._ALLOW`, `..._BLOCK`, read once at startup; the
   boolean `OTTD_RESEARCH_IGNORE_NETWORK_REVISION=1` remains as an alias for `open`). The policy is
   address-agnostic: it is the revision comparison, not the socket, so it works with any bind or
   connect address (127.0.0.1, 0.0.0.0, a specific IP) through OpenTTD's existing options.

## Alternatives rejected

- **Rebuild both ends clean each iteration.** The honest baseline and still the right answer when
  it is cheap; the override exists for the dirty-tree R&D loop where it is not. The override does
  not remove this option, it adds one.
- **Spoof the revision string to match a target build.** Rejected. Making this build report a
  revision it is not would let a modified or bot-driven client pass a *non-consenting* server's
  check while concealing what it is. That deceives a third party and their players, serves no
  single-machine use (where the owner controls both ends), and is out of scope for this fork
  (`project.no-upstream-interaction`, and the honesty rule this ADR introduces).
- **A normal, always-compiled setting.** Rejected: the override must leave a default build
  byte-for-byte unaffected (`project.research-code-is-gated-and-labelled`).

## Consequences

- A mismatched client that is accepted is still subject to the desync check (ADR 0016): if the two
  builds are not in fact behaviourally identical, the game desyncs and the server drops the client.
  The override trades the revision guard for the desync guard; it does not make genuinely different
  code safe to join. This is acceptable only because the R&D game runs identical code on both ends.
- The override is local-only by construction: a stock server elsewhere rejects this client (it runs
  the check itself on the client's sent revision), so the override cannot be used to disrupt or
  sneak onto other people's games (knowledge `the-revision-check-is-one-function-both-roles-run`).
- The boundary is stated by `project.research-net-compat-is-local-and-non-spoofing` and pinned by
  the unit test, which asserts that with the override off this build still matches only its own
  revision, i.e. the override only ever widens acceptance and never alters the reported revision.
  The override code is small and self-contained so this stays reviewable in the diff.

## For this fork

- Documented in `research/net-compat.md` with the run recipe for a loopback game.
- Maps: [network](../workflows/substrate/network/README.md),
  [platform and files](../workflows/substrate/platform-and-files/README.md).
- Knowledge: `a-build-from-a-modified-tree-cannot-join-a-clean-build` (why the mismatch happens).
