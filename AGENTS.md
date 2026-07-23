# AGENTS.md

Guidance for coding agents (Claude Code, or any other AI coding tool) working in this repository.

## Private fork — academic research project

This repository (`korczis/OpenTTD`) is a **private fork** belonging to korczis, used as an academic research project. It is not the official OpenTTD project.

- **Never push to, or open issues/PRs against, the upstream [OpenTTD/OpenTTD](https://github.com/OpenTTD/OpenTTD) repository.** All work here — commits, branches, experiments — stays local to this fork (`git@github.com:korczis/OpenTTD.git`).
- Upstream's contribution rules (see `CONTRIBUTING.md` and the "Project culture" section of [CLAUDE.md](./CLAUDE.md)) describe *their* norms for their own repository. They're kept in this fork's docs as background/context only — they are not constraints this fork's own workflow needs to satisfy.
- `.claude/` and `.aiad/` contain personal Claude Code tooling (commands, agents, skills, hooks) imported from another private project for use in this research context. They are git-ignored locally (via `.git/info/exclude`) and must never be committed or pushed.

## Where to look

- [CLAUDE.md](./CLAUDE.md) — build/test/run instructions, architecture overview, and code style notes for this codebase.
- [README.md](./README.md) — project overview (carries the same private-fork notice).
