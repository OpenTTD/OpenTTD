# Determinism A/B (I0026) — the read-only MCP path does not perturb the simulation

**Status: PASS.** One map seed run three times, headless, through `tools/research/determinism-ab`:
an idle run, an idle control, and a run hammered with read-only MCP reads for its whole length.
At every shared game-day boundary the engine's own `_random.state[0..1]` is identical across all
three, and no run issued any command. So the MCP read path changes neither the random state nor
the command stream: it does not perturb the deterministic simulation. This is a demonstration, not
an assertion (I0026).

## How it is measured

A dedicated server (`-D -d desync=2`) logs, once per game day, the line the engine's own
desync/lockstep check compares (`src/network/network.cpp`, `NetworkGameLoop`):

```
sync: <economy-date>; <date-fract>; <_random.state[0]>; <_random.state[1]>
```

plus every `cmd:` line, into `<home>/save/autosave/commands-out.log`. `determinism-ab` runs three
arms — `idle`, `idle2` (control), `load` — each in its **own personal dir** (the config dir, via
`-c`, so each writes its own `commands-out.log`; no cross-arm or cross-session clash), then strips
the `[timestamp]` prefix and compares the `sync:` and `cmd:` streams:

- **control** `idle` vs `idle2` must be identical, or the harness aborts (the method is unsound);
- **result** `idle` vs `load` identical ⇒ the reads perturb nothing.

```
tools/research/determinism-ab            # build-research-mcp/openttd, seed 53744, OpenGFX, 16 days
  idle vs idle2: 16 shared day(s), 16 matching, 0 differing; cmd lines 0/0
  idle vs load:  16 shared day(s), 16 matching, 0 differing; cmd lines 0/0
  PASS
tools/research/research report --experiment-id determinism-ab-20261116   # leaves the manifest
```

The load exercises the read path a client actually uses: `mcp read openttd://...` across ~14
resources (state, map, map/terrain, map/ways, world/graph, vehicles, stations, companies, towns,
depots, prices, engines, railtypes, bridges), fed through the console seam (I0067), which calls the
same resource-read handlers in process as the HTTP transport's `resources/read`.

## Scope and honesty of the claim

- The comparison is **per game-day boundary** (the `sync:` line is logged when `date_fract == 0`,
  one per day), not per tick: a hypothetical divergence is bounded to a game day, not a tick. Over
  16 consecutive days with the read path saturated, zero divergence is strong evidence; a stricter
  tick-exact check would need a research-only Debug line logging `_random.state` every tick in
  `StateGameLoop` (added instrumentation, `src/`), which was not necessary for this result.
- `-G <seed>` sets the generation seed on the command line (the cfg `generation_seed` is not applied
  the same way); the dump/`commands-out.log` land under the config dir's personal home, not
  `~/Documents/OpenTTD`; `-v null` needs a baseset or it hangs — so the harness symlinks the baseset
  into each arm's home and pins OpenGFX with `-I`.

## Provenance

The measurement method — the `commands-out.log` `sync:` diff and the `-G`/personal-dir facts — came
from coordinating with peers `openttd-cd` and `openttd-a0` over the Majordomus mesh, after an initial
approach (comparing the desync `dmp_cmds` savegame dumps directly) proved fragile: the dumps are
written, but to the config dir's home (not where this session first looked), and two dumps of one
seed at one date can differ in non-canonical bytes, so a raw file diff is unreliable — the `sync:`
line comparison avoids both problems. The loopback-lockstep variant (server + client, one seed,
`network.sync_freq=1`, hammer the client's reads, let the engine disconnect on desync) remains a
stronger tick-exact follow-up if ever needed.

Experiment bundle: `build-research-runs/determinism-ab-20261116/` (manifest.json, summary.md,
determinism-ab-summary.txt). Harness: [`tools/research/determinism-ab`](../../tools/research/determinism-ab).
