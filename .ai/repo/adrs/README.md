---
schema: context/v1
id: ai.repo.adrs
kind: context
title: Architecture decisions
description: Accepted, durable decisions about how this repository is built.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Architecture decisions

Accepted, durable decisions about how this repository is built, one file each, with the
context, the decision, the alternatives rejected and the consequences. Nothing here is
generated and nothing is promoted here automatically: a decision recorded while working
(`majordomus decision add`) stays in the checkout's local state until a person judges it
durable enough to write down here.
