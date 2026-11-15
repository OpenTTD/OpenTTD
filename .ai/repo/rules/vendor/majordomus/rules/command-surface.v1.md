---
id: majordomus.command-surface
version: 1
kind: rule
title: The command surface is declared and reconciled
description: Every command the binary dispatches is described by the shipped registry, every command the registry declares public is dispatched, and a command is public exactly when the usage text lists it.
statement: The command registry is authority for what a command means and the dispatcher for what runs; neither derives from the other, and doctor refuses a command present in one and absent from the other, in either direction.
status: active
class: blocking
depends_on: [majordomus.define-done-first@1]
tags: [command, surface]

x-majordomus:
  validator: command_surface
  category: command
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [command-surface]
  tests: [test/cases/30_command_registry.sh]
---

# Rationale

A command is a product surface, not a shell function. Before the registry existed nothing
stated what a command was: its category, whether it mutates state, what it reads and
writes and which exit codes it can produce lived only as prose in three documents and as
behaviour in one shell file, and the website rendered its reference from a parse of the
help text. A surface nobody declares cannot be checked, measured or rendered honestly.

# Required behaviour

`share/commands.yaml` declares every command with its visibility, class, reads, writes and
exit codes. `bin/majordomus` remains the authority for what actually executes. A command
the dispatcher carries with no registry entry, an entry declared public that the dispatcher
does not carry, and a public entry the usage text does not list are each a failure. A
command is public exactly when `majordomus --help` lists it; there is no third state.

# Failure behaviour

A violation is a `FAIL` finding under the category `command`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_command_surface` decides it, dispatched from `doctor, watch`. The behavioural
case `test/cases/30_command_registry.sh` proves it, `test/cases/35_future_command.sh` proves
that a command added to the dispatcher alone is refused by every surface that owes it
something, and CI runs both.
