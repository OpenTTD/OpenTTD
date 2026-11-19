# Battle post-mortem: edition 155d18ea50071e72

**RESEARCH-ONLY.** Derived by `battle-postmortem` from the edition's match records; do not edit.
How it is computed: [research/battle-postmortem.md](../battle-postmortem.md).

Finished matches: 37. Contestants: 7.

**PrismaticAI finished last:**

- `PrismaticAI`: last in 29 of 37 matches

## Contestants

| Contestant | Matches | Avg rank | Last | Value / winner | Peak fleet | Final fleet | Idle cash | Gap opens (median) | Causes (matches) |
|---|---|---|---|---|---|---|---|---|---|
| AAAHogEx | 37 | 1.0 | 0 | 1.000 | 406.7 | 395.7 | 0.92 | - | - |
| PrismaticAI self-steward * | 37 | 2.622 | 0 | 0.056 | 29.2 | 22.8 | 0.94 | 1960-07 | fleet_decays (2), hoards_cash (37), idle_stations (36), low_yield (16), slow_start (16) |
| AdmiralAI | 37 | 2.946 | 0 | 0.059 | 236.5 | 216.2 | 0.66 | 1962-01 | fleet_decays (2), hoards_cash (6), low_yield (36) |
| PrismaticAI solo-max * | 37 | 3.649 | 0 | 0.036 | 24.4 | 11.3 | 0.94 | 1960-07 | fleet_decays (34), hoards_cash (37), idle_stations (37), low_yield (14), slow_start (16) |
| LuDiAI AfterFix | 37 | 4.973 | 0 | 0.016 | 125.2 | 115.7 | 0.35 | 1960-07 | fleet_decays (3), hoards_cash (4), low_yield (36), slow_start (2) |
| CivilAI | 37 | 6.027 | 8 | 0.006 | 41.5 | 37.9 | 0.84 | 1960-07 | fleet_decays (1), hoards_cash (32), low_yield (36), slow_start (8), stalls_small (3) |
| PrismaticAI * | 37 | 6.784 | 29 | 0.000 | 5.0 | 2.5 | 0.86 | 1960-07 | fleet_decays (21), hoards_cash (35), idle_stations (33), low_yield (37), stalls_small (37) |

`*` PrismaticAI. Causes and thresholds:

- `stalls_small`: peak fleet below 10.0
- `fleet_decays`: final fleet below 0.6 of its peak
- `hoards_cash`: cash above 0.6 of company value over the last 4 quarters, with a fleet below 0.25 of the winner's
- `low_yield`: income per vehicle below 0.5 of the winner's
- `idle_stations`: more than 2.0 stations per vehicle at the end
- `slow_start`: first vehicle more than 1 quarter(s) after the start
- `mode_gap`: a mode that earns at least 0.3 of the winner's profit, with no vehicle of it (referee v3)
- gap opens: the first quarter from which value stays below 0.5 of the winner's

## Trajectory (median of the January samples over the matches)

Value / vehicles (of which aircraft and trains) / last year's profit from air and rail, the best contestant first. Without the parenthesis, the referee of that edition had no per-mode fields.

| Year | AAAHogEx | PrismaticAI self-steward | PrismaticAI solo-max | PrismaticAI |
|---|---|---|---|---|
| 1960 | 0 / 0 veh | 0 / 0 veh | 0 / 0 veh | 0 / 0 veh |
| 1961 | 229k / 9 veh | 15k / 9 veh | 3k / 7 veh | 22k / 4 veh |
| 1962 | 692k / 21 veh | 209k / 19 veh | 114k / 15 veh | 25k / 4 veh |
| 1965 | 2.9M / 122 veh | 817k / 23 veh | 632k / 18 veh | 22k / 3 veh |
| 1970 | 18.9M / 213 veh | 2.2M / 23 veh | 1.5M / 12 veh | 20k / 3 veh |
| 1975 | 49.3M / 277 veh | 3.7M / 22 veh | 2.5M / 11 veh | 30k / 3 veh |
| 1980 | 94.4M / 388 veh | 5.3M / 24 veh | 3.5M / 11 veh | 39k / 3 veh |

## Proposals for the next edition

Settings the causes ask for. These are inputs for the battle's owner, who decides whether they become contestants; nothing applies them.

| For | Settings | Because (matches) |
|---|---|---|
| PrismaticAI | `aggressiveness=100,cash_reserve=5000,intercity=1,max_routes=30,own_lines=8,self_steward=1` | fleet_decays (21), hoards_cash (35), idle_stations (33), low_yield (37), stalls_small (37) |
| PrismaticAI solo-max | `aggressiveness=100,cash_reserve=5000,intercity=1,max_routes=30,own_lines=8,self_steward=1` | fleet_decays (34), hoards_cash (37), idle_stations (37) |
