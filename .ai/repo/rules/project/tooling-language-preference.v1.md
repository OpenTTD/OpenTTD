---
id: project.tooling-language-preference
version: 1
kind: rule
title: New research tooling is written in Rhai, then Rust, then C++, then C, in that order of preference
description: The owner's order of preference for this fork's own tooling. Extension and behaviour go in Rhai scripts over a Rust host; hosts, servers and tools are Rust; C++ only inside OpenTTD itself (src/), behind the research gates; C only where OpenTTD needs it. Python and shell remain for existing glue and are not where new capability goes.
statement: Write new extension logic in Rhai, new hosts and tools in Rust, change OpenTTD itself in C++, and reach for C only where OpenTTD requires it; do not grow new capability in Python or shell.
status: active
class: advisory
depends_on: []
tags: [tooling, language, rhai, rust]
---

# Rationale

Rhai keeps behaviour editable without a rebuild and is the extension surface the owner asked for
everywhere. Rust hosts it safely and is the owner's service language. C++ is the substrate's own
language and stays inside `src/`.

# Required behaviour

- Behaviour, bots, narration, demos and high-level commands: Rhai (`tools/research/mcp-panel/rhai/`).
- Hosts and servers: Rust (`tools/research/mcp-rhai`).
- Game-side capability: C++ in `src/prismatic_mcp`, compile-gated (rule `project.research-code-is-gated-and-labelled`).

# Known exception

None. The Python panel (server.py, removed in 60aef3caa5) predated this rule; I0035 ported it to the
Rust host (`tools/research/mcp-rhai`, binary `mcp-panel`) and removed it.

# Failure behaviour

Advisory: a reviewer reports new capability written in a less preferred language.
