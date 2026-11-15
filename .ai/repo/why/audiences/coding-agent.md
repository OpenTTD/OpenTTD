---
schema: audience/v1
id: coding-agent
kind: audience
title: Coding agent working on an unfamiliar codebase
short_title: Coding agent
summary: 'A model session that arrives with no memory and must act correctly in a large C++ tree.'
workflow: 'Reads the layer, takes a scoped task, edits, validates at a tier, reports, hands over.'
status: stable
weight: 20
tags: [agent, session, provider]
---

# Coding agent

Claude, Codex, Gemini or any other worker that starts a session here. It inherits nothing
from the last session except what the repository records.
