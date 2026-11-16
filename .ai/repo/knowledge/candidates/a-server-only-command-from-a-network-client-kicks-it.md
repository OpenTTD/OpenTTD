---
schema: knowledge/v1
id: a-server-only-command-from-a-network-client-kicks-it
kind: knowledge
class: constraint
title: "A command flagged CommandFlag::Server, posted from a network client, gets that client kicked, and nothing on the client stops it being sent"
description: "Before an MCP tool, a bot or injected input posts a command from a GUI client's process."
status: candidate
epistemics: inferred
date: 2026-11-16
tags: [network, commands, mcp]
about: src/network/network_server.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/network/network_server.cpp
    - file:src/command.cpp
    - file:src/misc_cmd.h
    - file:src/prismatic_mcp/prismatic_mcp_server.cpp
    - commit:0cd5925bd2bc229e991f75c4cc876c250bdbc882
relations:
  - type: relates_to
    target: file:.ai/repo/adrs/0013-every-openttd-process-may-run-its-own-mcp-server-and-its-vie.md
  - type: supports
    target: file:.ai/repo/rules/project/game-state-mutates-only-through-commands.v1.md
---

# Statement

`ServerNetworkGameSocketHandler::ReceiveClientCommand` (`src/network/network_server.cpp`) answers
a client that sends a command whose flags include `CommandFlag::Server` with
`NetworkErrorCode::Kicked`. The client side does not filter: in `src/command.cpp`,
`InternalPostBefore` only marks the command `only_sending` when `_networking`, and no check of
`CommandFlag::Server` precedes the send.

`Commands::Pause` is such a command (`src/misc_cmd.h`, `DEF_CMD_TRAIT(Commands::Pause, CmdPause,
CommandFlags({CommandFlag::Server, CommandFlag::NoEst}), ...)`). Since commit 0cd5925bd2 a GUI
client can run its own MCP server, and `ExecuteAction` in
`src/prismatic_mcp/prismatic_mcp_server.cpp` posts `Command<Commands::Pause>::Post` with no
`_network_server` guard. An approved `openttd.game.set_pause` served by a network client's MCP
server is therefore expected to get the owner's client kicked rather than pause the game.

# Evidence status

Inferred from the source at f069e8f0f5; not reproduced in a running game. To verify: join a GUI
client with its MCP server to a dedicated server, approve a `set_pause` on the client's server, and
watch the server console for "Kicking client ... due to calling a server only command".

# What to do

A tool that posts a command from a process that may be a network client checks
`GetCommandFlags(cmd).Test(CommandFlag::Server)` against `_network_server` (or `!_networking`)
before posting, and refuses with an explanation instead.
