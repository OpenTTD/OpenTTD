---
schema: knowledge/v1
id: fixed-loopback-ports-collide
kind: knowledge
class: lesson
title: "A fixed loopback port can belong to another local service; a listener that binds only IPv6 then sends clients elsewhere"
description: "When starting an MCP server in an OpenTTD process, or picking a port for any local research service."
status: verified
epistemics: observed
date: 2026-11-15
tags: [mcp, ports, loopback, macos]
about: src/prismatic_mcp/prismatic_mcp_server.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - issue:I0038
    - file:src/prismatic_mcp/prismatic_mcp_server.cpp
    - file:tools/research/mcp-e2e
---

# Lesson

The GUI client's MCP server was started on 8741, which `majordomus mcp` already held on IPv4. The
client bound `[::1]:8741` only, wrote `127.0.0.1:8741` into its token file, and the director's
calls reached Majordomus, which answered `session_required` (HTTP 400).

# How to apply

`Start()` now fails when the IPv4 listener cannot bind, because 127.0.0.1 is the advertised
endpoint. `mcp-e2e` asks the OS for a free loopback port for the client. Any new local service
picks a free port, or fails loudly, never half-binds.
