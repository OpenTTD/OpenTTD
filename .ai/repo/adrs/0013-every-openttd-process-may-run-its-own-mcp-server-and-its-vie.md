---
schema: adr/v1
id: adr-0013
kind: adr
title: Every OpenTTD process may run its own MCP server, and its view tools act on the local viewport only, with no command and no approval
status: proposed
date: 2026-11-16
tags:
  - mcp
  - gui
related:
  - file:.ai/repo/adrs/0008-an-agent-reaches-the-game-through-an-embedded-loopback-mcp-s.md
  - rule:project.game-state-mutates-only-through-commands
  - file:src/prismatic_mcp/prismatic_mcp_view.cpp
  - file:src/console_cmds.cpp
provenance:
  origin: extracted
  derived_from:
    - commit:0cd5925bd2bc229e991f75c4cc876c250bdbc882
    - issue:I0038
---

# 13. Every OpenTTD process may run its own MCP server, and its view tools act on the local viewport only, with no command and no approval

## Context

ADR 0008 embedded the MCP server and the deployments ran it in the dedicated server only. The
commit that changed this states the problem: "Only the dedicated server had an MCP server, so
nothing moved the window the owner watches" (commit 0cd5925bd2, issue I0038). The owner wanted
to watch the bot build from a GUI client, whose console reaches no standard output, so a token
printed to the console was unreachable to a launcher.

## Decision

1. **Any process starts its own server.** `prismatic_mcp start [port] [token-file]` (console
   command, `src/console_cmds.cpp`) starts the server in a GUI client as in a dedicated server,
   and writes `{"agent_token","endpoint"}` to the token file with owner-only permissions
   (`std::filesystem::permissions`, reported as "mode 0600").
2. **View tools are local.** `openttd.view.scroll_to`, `openttd.view.follow`,
   `openttd.view.zoom` and `openttd.view.state` (`src/prismatic_mcp/prismatic_mcp_view.cpp`) act
   on the main window's viewport of the process that serves them. They issue no command and need
   no approval; `src/prismatic_mcp/prismatic_mcp_view.cpp:MainWindow` returns no window in a
   dedicated server, where they answer an error.
3. **The IPv4 listener is required.** Start fails when `127.0.0.1` cannot be bound
   (`src/prismatic_mcp/prismatic_mcp_server.cpp`), because that is the advertised endpoint.

## Alternatives rejected

- **The dedicated server only** (the state ADR 0008 shipped): the owner's own window could not
  be moved by an agent.
- **Starting when only `::1` binds**: "a half bind sent clients to another service" (commit
  0cd5925bd2).

No other alternative is recorded in the history; none is invented here.

## Consequences

- A viewport is per-process presentation state, not simulation state, so the view tools cannot
  desync a network game. This rests on the viewport not being part of the command stream or the
  savegame; it is inferred from `ScrollMainWindowToTile` and `Viewport::follow_vehicle` being
  window state, not verified by a determinism run (I0026 has not run).
- Each process has its own token and port. A controller that drives several processes (the
  director, the cluster of I0039) holds one token per process.
- The approval gate of ADR 0008 is unchanged: a GUI client's server serves `openttd.decision.*`
  through the same code. Its one mutation, the pause, is `CommandFlag::Server`
  (`src/misc_cmd.h:CmdPause`), and
  `src/network/network_server.cpp:ServerNetworkGameSocketHandler::ReceiveClientCommand` kicks a client
  that sends one; nothing on the client filters it. An approved pause served by a network client's MCP
  server is therefore expected to get that client kicked, not to pause the game (traced in the source,
  confirmed by an independent model review, not run; knowledge candidate
  `a-server-only-command-from-a-network-client-kicks-it`).
