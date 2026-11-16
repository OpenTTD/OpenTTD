---
schema: knowledge/v1
id: rcon-and-admin-console-commands-pass-server-only-hooks
kind: knowledge
class: constraint
title: "A console command reached over rcon or the admin port passes server-only hooks, so a new command restricts remote callers itself"
description: "Before adding a console command (research_status, prismatic_mcp, ...) that must not be reachable remotely."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [gui-and-console, network]
about: src/network/network_server.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/network/network_server.cpp
    - file:src/network/network_admin.cpp
    - file:src/console_cmds.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/network/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/gui-and-console/README.md
---

# Statement

`src/network/network_server.cpp:ServerNetworkGameSocketHandler::ReceiveClientRemoteConsoleCommand`
and the admin equivalent execute the line in the server process, where hooks such as
`src/console_cmds.cpp:ConHookServerOnly` test only `_network_server`. `prismatic_mcp token` is
therefore readable by an authenticated rcon or admin caller, with the output redirected to them
(`src/console.cpp:IConsolePrint`).

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote rcon-and-admin-console-commands-pass-server-only-hooks` with evidence, or `majordomus knowledge reject rcon-and-admin-console-commands-pass-server-only-hooks --reason ...`.
