---
id: project.long-runs-are-owned-bounded-and-stoppable
version: 1
kind: rule
title: A long-running job names its owner on every machine it uses, stays within each machine's floors, and stops without its author
description: A research job that runs unattended for longer than a session turn, or on machines other than its own, writes an owner file on every host it uses, checks load and free disk before it adds work, and honours a pause file any session can create. Killing its processes is never the stop.
statement: Before a job runs unattended or on another machine, make it write who owns it and how to stop it on every host, refuse new work over the host's load ceiling or under its 35 GiB disk floor, and honour a PAUSED file on the host and a STOP file beside it.
status: active
class: blocking
depends_on: [project.no-commit-without-current-instruction@1]
tags: [ai-battle, compute, machines, load, disk, ownership, enforcement]
---

# Rationale

From 2026-11-19 to 11-22, the AI battle tournament held this Mac and the Linux build host at their
limits for three days: load 60 to 86 on 18 cores, swap 35 of 37 GB, disk under the floor. It ran from
another Mac over SSH. Its processes reparented to launchd and named nobody, so five sessions each
said it was not theirs, and all five were right. Killed matches came back on the next scheduling
pass. The owner's stop order, written as Markdown here, never reached it. The post-mortem is
[`research/incident-2026-11-19-ai-battle-overload.md`](../../../../research/incident-2026-11-19-ai-battle-overload.md).

A job that only its author can see or stop belongs to nobody as soon as its author's session ends
or is on another machine.

# Required behaviour

- **Owned:** on every host it uses, the job writes a file naming the machine it runs from, its pid,
  its commit, its start time, and how to stop it. Every process it starts carries that identity in
  its environment. For the AI battle these are `ORCHESTRATOR` in the arena root and
  `TTD_BATTLE_ORCHESTRATOR`.
- **Bounded:** before it adds work to a host, the job reads that host's load per core and free disk.
  It adds nothing over the configured load ceiling or under the 35 GiB floor. The thresholds are
  tracked configuration, not code, and a host can override them.
- **Stoppable by anyone:** a `PAUSED` file on a host takes that host out of the job, and a `STOP`
  file beside the job stops all new work. Any session may create either one, and the job reports it.
  Killing the job's processes by name is not a stop, because a scheduler refills what it lost.
- **Running a job is an action that needs a current instruction:** starting, restarting or preparing
  a new edition of an unattended job, like a commit, needs an instruction that is current.

# Failure behaviour

A job that cannot write its owner file, or that cannot read a host's load or disk, adds no work to
that host. It names the reason as the host's state.

# Verification

`python3 -m unittest tools/research/test_ai_battle.py` covers the pause and stop files, the floor
decision and the owner file, all offline. A reviewer checks any new long-running tool against this
rule before it first runs unattended.
