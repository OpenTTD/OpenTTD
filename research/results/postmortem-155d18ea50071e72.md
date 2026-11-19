# Battle post-mortem: edition 155d18ea50071e72

**RESEARCH-ONLY.** Derived by `battle-postmortem` from the edition's match records; do not edit.
How it is computed: [research/battle-postmortem.md](../battle-postmortem.md).

Finished matches: 29. Contestants: 7.

**PrismaticAI finished last:**

- `PrismaticAI`: last in 22 of 29 matches

## Contestants

| Contestant | Matches | Avg rank | Last | Value / winner | Peak fleet | Final fleet | Idle cash | Gap opens (median) | Causes (matches) |
|---|---|---|---|---|---|---|---|---|---|
| AAAHogEx | 29 | 1.0 | 0 | 1.000 | 407.5 | 398.7 | 0.92 | - | - |
| PrismaticAI self-steward * | 29 | 2.621 | 0 | 0.056 | 29.1 | 22.4 | 0.93 | 1960-07 | fleet_decays (2), hoards_cash (29), idle_stations (29), low_yield (13) |
| AdmiralAI | 29 | 3.0 | 0 | 0.054 | 244.6 | 222.1 | 0.65 | 1962-04 | fleet_decays (2), hoards_cash (4), low_yield (29) |
| PrismaticAI solo-max * | 29 | 3.586 | 0 | 0.037 | 24.3 | 11.0 | 0.94 | 1960-07 | fleet_decays (27), hoards_cash (29), idle_stations (29), low_yield (9) |
| LuDiAI AfterFix | 29 | 4.966 | 0 | 0.017 | 126.8 | 118.4 | 0.34 | 1960-07 | fleet_decays (2), hoards_cash (3), low_yield (28) |
| CivilAI | 29 | 6.069 | 7 | 0.006 | 43.9 | 39.6 | 0.83 | 1960-07 | fleet_decays (1), hoards_cash (25), low_yield (28), stalls_small (3) |
| PrismaticAI * | 29 | 6.759 | 22 | 0.000 | 5.1 | 2.5 | 0.86 | 1960-07 | fleet_decays (17), hoards_cash (27), idle_stations (26), low_yield (29), stalls_small (29) |

`*` PrismaticAI. Causes and thresholds:

- `stalls_small`: peak fleet below 10.0
- `fleet_decays`: final fleet below 0.6 of its peak
- `hoards_cash`: cash above 0.6 of company value over the last 4 quarters, with a fleet below 0.25 of the winner's
- `low_yield`: income per vehicle below 0.5 of the winner's
- `idle_stations`: more than 2.0 stations per vehicle at the end
- gap opens: the first quarter from which value stays below 0.5 of the winner's

## Proposals for the next edition

Settings the causes ask for. These are inputs for the battle's owner, who decides whether they become contestants; nothing applies them.

| For | Settings | Because (matches) |
|---|---|---|
| PrismaticAI | `aggressiveness=100,cash_reserve=5000,intercity=1,max_routes=30,own_lines=8,self_steward=1` | fleet_decays (17), hoards_cash (27), idle_stations (26), low_yield (29), stalls_small (29) |
| PrismaticAI solo-max | `aggressiveness=100,cash_reserve=5000,intercity=1,max_routes=30,own_lines=8,self_steward=1` | fleet_decays (27), hoards_cash (29), idle_stations (29) |
