---
id: majordomus.prompt-integrity
version: 1
kind: rule
title: Prompt integrity
description: Every repository-local prompt asset renders, and every token in it is one the renderer knows.
statement: Every reusable prompt asset renders, and every token in it is one the renderer knows.
status: active
class: blocking
depends_on: [majordomus.minimum-sufficient-context@1]
tags: [prompts]

x-majordomus:
  validator: prompts
  category: prompt
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [prompt-assets]
  tests: [test/cases/24_prompt_search.sh]
---

# Rationale

Every repository-local prompt asset renders, and every token in it is one the renderer knows.

# Required behaviour

Every reusable prompt asset renders, and every token in it is one the renderer knows.

# Failure behaviour

A violation is a `FAIL` finding under the category `prompt`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_prompts` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/24_prompt_search.sh` proves it, and CI runs that case.
