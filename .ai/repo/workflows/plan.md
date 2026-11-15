# Taking work from the plan

Non-trivial work belongs to a milestone and to one issue with a place in a dependency
graph. Status is derived from what an issue records and from the state of its
dependencies; there is no status field to set, and writing one is an unknown key.

| When | Run |
|---|---|
| starting a session | `majordomus plan next` — the one issue that is ready |
| before you begin | `majordomus plan show <id>` — objective, scope, acceptance, evidence |
| you begin it | `majordomus plan start <id>` — refused unless it is READY |
| you have proof | `majordomus plan evidence <id> --covers <token> --type test --command "<cmd>" --result "<what it showed>"` |
| it is finished | `majordomus plan done <id>` — refused while a required token is uncovered |

Take something else only when it is a blocker, when it invalidates an assumption the
milestone rests on, or when a person reprioritised. Not because it looked more useful.
Trivial exceptions are a typo, a broken link, or a fix forced from outside the repository.

A repository with no `project/` section populated has no plan, and `plan` says so.
