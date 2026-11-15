---
schema: context/v1
id: ai.repo.prompts
kind: context
title: Prompt assets
description: The provider-neutral prompts the tool renders, with the tokens it fills in.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Prompt assets

One Markdown file per prompt: front matter with `name` (equal to the file name) and a
one-line `description`, over a body that is the prompt itself. `majordomus prompt` renders
one by substituting `{{TOKEN}}` placeholders from the active task and the checkout. A
token the renderer does not know is an error, because a prompt that ships an
unsubstituted placeholder is worse than one that never rendered.

These are assets, not policy. A prompt asks a worker to do something the rules already
require; it never introduces a rule of its own, and it names commands and files rather
than a provider's tools.
