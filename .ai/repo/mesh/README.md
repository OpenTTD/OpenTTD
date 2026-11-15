---
schema: context/v1
id: ai.repo.mesh
kind: context
title: Mesh
description: One canonical object that says whether this repository's servers take part in the mesh, over which transports, and which machines' keys they trust; a machine whose key is not listed keeps it closed.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Mesh

A mesh object states, once, everything the shared server needs to decide whether and how
to cooperate with the other running Majordomus servers of this repository — on this
machine, on the local network, on the tailnet: the master switch, the multicast group, the
rendezvous endpoints for segments multicast cannot cross, and the trust policy with the
public keys of the machines it trusts. The shared server reads it at startup; nothing else
interprets it, and no environment variable overrides it.

`majordomus init` ships `mesh.yaml` enabled, with `deny_unknown` and an empty allowlist. A
machine whose own key is not listed keeps the mesh **closed**: no multicast, no rendezvous,
no dial, and `majordomus mesh status` says which key to list. So nothing leaves a machine
until a person enrols it:

```sh
majordomus mesh enrol      # adds this machine's public key to trust.allow, once
git commit .ai/repo/mesh   # the next machine that pulls trusts this one
```

Each machine is enrolled once, by someone who works on it; worktrees of one machine share
its key. Turning the mesh off for the whole repository is `enabled: false`.

`trust.allow` holds Ed25519 *public* keys — nothing secret belongs in this directory, and a
machine's signing key never leaves the machine it was created on. The schema is `mesh/v1`
and the kind `mesh-declaration`; `majordomus mesh doctor` checks the declaration and this
machine's place in it.
