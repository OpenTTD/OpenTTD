---
schema: context/v1
id: ai.repo.applications
kind: context
title: Applications
description: The contexts the tool suits, when each fits and when it does not, composed from use cases.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Applications

An application answers "in what context would someone use this": one Markdown file with
front matter (`share/schemas/majordomus/application/application.v1.schema.json`): when it fits, when it does not,
which use cases it composes, which rules and responsibilities it leans on; and a body
(`# Context`). A use case names the applications it belongs to and the application names
it back; `majordomus usecase validate` refuses a reference that resolves one way only.

`does_not_fit_when` is required: a catalogue that only lists fits is marketing.
