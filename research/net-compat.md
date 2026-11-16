# A loopback R&D game across a revision mismatch

RESEARCH-ONLY, UNSTABLE. Issue I0070; ADR 0029; rule
`project.research-net-compat-is-local-and-non-spoofing`.

## The problem

OpenTTD multiplayer is deterministic lockstep (ADR 0016): a client whose code differs from the
server's would desync, so the server refuses a client whose revision string does not match its own
("The revision of this client does not match the server's revision"). The agent R&D of this fork
runs the game and the bot together on one machine, over loopback, from the same fork — but a dirty
working tree reports `-m<hash>` and a clean build reports `-g<hash>`, and two commits report two
hashes, so the two ends refuse each other even though the code is behaviourally identical
(knowledge `a-build-from-a-modified-tree-cannot-join-a-clean-build`).

## The honest baseline

Build the server and the client from the **same clean commit**. Their revisions match and no
override is needed. This is always correct and is the right answer whenever it is cheap.

## The override, for the dirty-tree R&D loop

A research-gated, local-only, non-spoofing policy lets a game **you run** choose which peer
revisions it accepts. It is `strict` (upstream behaviour) by default, compiles only in a research
build, and **never changes the revision this build reports** — so it does not impersonate another
build, and a stock remote server still rejects this client (it runs its own check). Accepting a
mismatched peer is only ever your own choice on your own game.

### Policies

| Policy | Accepts | Use |
|---|---|---|
| `strict` | only a genuine/compatible match (default) | normal play |
| `whitelist` | a genuine match **plus** revisions you list in `allow` | a known set of R&D builds |
| `blocklist` | **any** revision **except** those you list in `block` | open R&D, a few builds excluded |
| `open` (alias `full`) | any peer revision | a throwaway single-machine loop |

The policy only ever *widens* acceptance beyond `strict`; a genuine self-match always passes (except
one you explicitly `block`). The allow/block lists are data — comma-separated revision strings.

### From the console (tilde)

On both the server and the client process:

```text
research_net_compat                 # show the policy, lists and the revision this build reports
research_net_compat open            # accept any peer (alias: full; 'on' = open, 'off' = strict)
research_net_compat whitelist       # then add the revisions you trust:
research_net_compat allow 20261116-master-m812defb0ce
research_net_compat blocklist       # accept all but:
research_net_compat block 20261116-master-mdeadbeef00
research_net_compat clear           # empty both lists
research_net_compat strict          # back to the normal check
```

### Config sources (precedence: env var wins, then the runtime flag)

The override can be set three ways, so it fits an interactive session, a persistent dev machine
and an automated pipeline alike:

1. **Environment variables** — for CI/CD, GitHub/GitLab pipelines and perf harnesses, which all set
   env. Read once at startup:
   ```bash
   OTTD_RESEARCH_NET_REVISION_POLICY=open  ./build-gate-research/openttd -D   # dedicated server
   OTTD_RESEARCH_NET_REVISION_POLICY=whitelist \
   OTTD_RESEARCH_NET_REVISION_ALLOW=20261116-master-m812defb0ce,20261116-master-g0a1b2c3d4e \
     ./build-gate-research/openttd                                            # the client that joins
   # OTTD_RESEARCH_NET_REVISION_BLOCK=<rev,rev>  for the blocklist policy
   # OTTD_RESEARCH_IGNORE_NETWORK_REVISION=1     back-compatible alias for policy=open
   ```
2. **Config file** — put the console commands in `scripts/autoexec.scr`, which the game executes at
   startup (`src/openttd.cpp` runs `exec scripts/autoexec.scr`). It persists across runs on a dev
   machine, and it is this fork's native script config:
   ```text
   research_net_compat whitelist
   research_net_compat allow 20261116-master-m812defb0ce
   ```
3. **Console** — `research_net_compat ...` at the tilde prompt, any time.

### Addresses

The override is address-agnostic: it is the revision comparison, not the socket. Bind and connect
on any address with OpenTTD's own options — `127.0.0.1` (the default loopback for R&D), `0.0.0.0`
(all interfaces, e.g. a server reached from a container in CI), or a specific IP — through `-D`,
the server's `network.server_bind_ip`/bind addresses, and the client's connect address. None of
that is changed here.

Then join as usual. The dialog is gone because the game you run chose to accept your build's true
revision, not because your build pretended to be another.

## What it does not do

- It does not change `GetNetworkRevisionString()`: this build always sends its own real revision.
- It cannot make a non-consenting remote server accept this client: that server runs its own check.
- It does not make genuinely different code safe to join. A truly different client still desyncs
  and is dropped by the desync check (ADR 0016). The override is sound only because the loopback
  game runs identical code on both ends.

## Validation

- `build-gate-research/openttd_test '[research]'` — with the override on, any peer revision is
  accepted; with it off, the check is unchanged and this build still matches only itself.
- `tools/gate.sh research` — the research build compiles and its tests pass.
- The rule `project.research-net-compat-is-local-and-non-spoofing` states the non-spoofing
  boundary; the override is small and self-contained, so a diff that made it touch the reported
  revision would be visible in review, and the unit test's "matches only itself with the override
  off" assertion pins that it only widens acceptance.

## Build

The override needs `OPTION_RESEARCH_INSTRUMENTATION=ON`:

```bash
cmake -S . -B build-gate-research -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPTION_RESEARCH_INSTRUMENTATION=ON
cmake --build build-gate-research -j"$(sysctl -n hw.ncpu)" --target openttd openttd_test
```

A default build (`OPTION_RESEARCH_INSTRUMENTATION=OFF`) does not contain any of this.
