---
schema: adr/v1
id: adr-0030
kind: adr
title: Public servers are played with an unmodified official client; fork features run only on the owner's own servers, on a client built on the same release
status: proposed
date: 2026-11-16
tags:
  - network
  - security
provenance:
  origin: extracted
  derived_from:
    - file:src/network/core/network_game_info.cpp
    - file:cmake/scripts/FindVersion.cmake
---

# 30. Public servers are played with an unmodified official client; fork features run only on the owner's own servers, on a client built on the same release

## Context

The owner, 2026-11-16: "vzdy musim byt schopen se pripojit k realnym official vanilla servrerum,
projit check, no ban" (I must always be able to connect to real official vanilla servers, pass the
check, no ban), and "hide me".

`src/network/core/network_game_info.cpp:IsNetworkCompatibleVersion` admits a client only with the
server's revision string; a tagged build (an official release, `_openttd_revision_tagged`) requires an
exact match. `cmake/scripts/FindVersion.cmake` derives the revision from git: a build of this fork is
never exactly a release tag, so it cannot join an official server at all. Automated play (MCP input,
mcp-human) on a public server breaks its rules and risks a ban.

The owner was asked (AskUserQuestion, session 29543910-65b2-4bc7-937c-d650f6253179,
2026-11-16T03:45Z):

> Official OpenTTD servers run tagged releases (mostly 15.3, some 16.0-beta5) and admit only an
> exactly matching version. A client built from this fork never matches, so today it cannot join them
> at all. Automated play (MCP input, mcp-human) on public servers breaks their rules and risks a ban.
> How should 'always join official vanilla servers, pass the check, no ban' be met?

Options: "Separate vanilla client (Recommended)", "Fork client on the release tag", "Both". Answer:
**"Both"**: "A vanilla client for public servers, and a fork client on the release tag for your own
servers on the same version."

## Decision

1. **Public servers: an unmodified official client.** Built from the upstream release tag (15.3, and
   16.0-beta5 for beta servers) in a detached worktree with no fork commit, so its revision is the
   tag and it passes the version check by being exactly what the server expects. It runs with its own
   configuration directory: a player name that identifies nobody, the anonymous survey off, no
   automation of any kind. That is "hide me": the client discloses nothing of the owner or of this
   fork, because it contains nothing of this fork.
2. **Fork features (MCP, the toolbar menu, bots, the hub) run only on the owner's own servers**, on a
   client built on the same release tag, so the owner's servers speak the same version as the public
   ones.
3. **No disguise.** No build presents a modified client to a public server under an official
   version. A fork build stays recognisably a fork build.

## Alternatives rejected

- A fork client that reports the official release version on public servers: rejected by the owner's
  answer (the fork client on the tag is for the owner's own servers).
- Only a vanilla client (fork features never on the release version): the owner chose both.

## Consequences

- `tools/research/vanilla-client` builds and runs the official client from a tag and checks that it is
  the unmodified release (revision equals the tag, not modified, no fork commit).
- Fork features need a port to the release branch before (2) holds; until then they run on the
  fork's own version, on the owner's local servers only.
- Any fork feature that can act on a game (MCP mutations, armed input) must stay off when connected to
  a server that is not the owner's own.
