# Battle post-mortem

**RESEARCH-ONLY.** After each round of the [LAN AI battle](ai-battle/README.md), the post-mortem
answers where each contestant loses, and PrismaticAI above all. `battle.py` already writes a
post-mortem per match into its machine store. `battle-postmortem` adds the layer across
matches. It analyses every finished match of an edition in parallel and names the causes of each
loss against fixed thresholds. It counts how often each PrismaticAI contestant finishes last, and
it turns the frequent causes into settings proposals for the next edition. The output is a
derived record pair under `research/results/` plus DuckDB tables for ad-hoc questions.

The post-mortem reports and proposes. It never changes a contestant, a default or the bot. A
proposal becomes a contestant only when the battle's owner adds it to `battle.json`, which makes a
new edition. The rule that PrismaticAI is never last is `project.prismatic-ai-is-never-last`, and
`battle.py check` enforces it. The post-mortem shows why that rule fails when it does.

## Running it

```bash
cargo build --release --manifest-path tools/research/mcp-rhai/Cargo.toml --bin battle-postmortem
B=tools/research/mcp-rhai/target/release/battle-postmortem
$B                                  # newest edition in ~/ttd-battle/results/battle.sqlite
$B --edition 155d18ea50071e72       # one edition; from its JSON match records if the store lacks it
$B editions                         # the editions in the store, newest first
$B check                            # every postmortem-*.md is what its .json derives (exit 11 if not)
$B watch --every 60                 # the post-round loop (below)
```

With no finished match in the edition, nothing is written and the exit code is 12. A record of
nothing would pass for a round.

### The post-round loop

`watch` checks the store every `--every` seconds. When the newest edition has more finished
matches than at its last look, it analyses the edition again. It writes
`~/ttd-battle/results/postmortem-<edition>.json/.md` and the DuckDB tables, and prints one line:

```
postmortem 91a726628e57de21: 12 finished; best PrismaticAI: PrismaticAI avg_rank 2.1; PrismaticAI last: none
```

The same summary, each PrismaticAI contestant's top causes and the proposals are announced on the
Majordomus board as the named claims `battle.postmortem.summary`, `battle.postmortem.causes` and
`battle.postmortem.proposals`, under the peer `battle-postmortem` (`src/board.rs`; the server of this
checkout, or `$MAJORDOMUS_URL`). Every session on the mesh reads where PrismaticAI stands from the
board, and the board keeps the last posts after `watch` stops. `--no-board` turns this off, and
`MCP_BOARD_DRY=1` prints the announcements instead.

That line is what a monitor reads: an agent session's `Monitor`, or a shell loop. `watch` never
writes under `research/`. A committed record comes only from a deliberate
`battle-postmortem --edition <E>` once the round is over, together with the battle's own record.

If your `~/.cargo/config.toml` sets `-C lto` in a target's `rustflags`, prefix cargo with
`RUSTFLAGS="-C target-cpu=native"` (see [live-arena.md](live-arena.md)).

| Output | Content |
|---|---|
| `research/results/postmortem-<edition>.json` | schema `battle-postmortem/v1`: thresholds, per-contestant aggregates, every per-match analysis, `prismatic_last`, proposals |
| `research/results/postmortem-<edition>.md` | derived from the JSON only; `check` regenerates and compares it |
| `~/ttd-battle/results/analytics.duckdb` | tables `analyses`, `contestants`, `proposals`, all editions; an edition's rows are replaced when it is analysed again |

The DuckDB file can be joined with the battle's own store:

```sql
ATTACH '~/ttd-battle/results/battle.sqlite' AS battle (TYPE sqlite, READ_ONLY);
SELECT a.contestant, avg(a.idle_cash_share), avg(s.vehicles)
FROM analyses a JOIN battle.samples s ON s.match_id = a.match_id AND s.contestant = a.contestant
GROUP BY 1;
```

## Sources

- **The store.** `~/ttd-battle/results/battle.sqlite`, written by `battle.py`, with the tables
  `matches`, `samples`, `standings` and `postmortems`. It is opened read-only (`SQLITE_OPEN_READ_ONLY`),
  so the post-mortem cannot change what the orchestrator recorded.
- **JSON match records.** An edition from before the store existed is read from
  `~/ttd-battle/results/<edition>/matches/*.json` (`--matches DIR`), one parallel task per file.
- **Contestants.** The store's `matches.lineup` holds keys. Each contestant's `ai` and `settings`
  come from `tools/research/ai-battle/battle.json`, looked up by key. A lineup entry that already
  carries `ai`/`settings` is used as it is.

A replay (`replica_of` set) and an unfinished match (`status` other than `done`) are left out.

## How a loss is analysed

For each contestant of a finished match (`src/postmortem.rs`, function `contestant`), the
"last year" is the last four quarters of its series:

| Fact | Definition |
|---|---|
| rank, last | its place in the standings; `last` when it is the final place |
| value share of winner | final company value divided by the winner's |
| peak / final fleet | the largest and the last vehicle count in the series |
| idle cash share | money divided by company value, averaged over the last year (quarters without value left out) |
| income per vehicle | the last year's mean income divided by its mean fleet, also for the winner |
| stations per vehicle | final stations divided by the final fleet |
| gap opens | the first quarter from which its value stays below half of the winner's until the end |

Each cause is a test against a named constant. The record repeats the constants under
`thresholds`:

| Cause | Test | What it means |
|---|---|---|
| `stalls_small` | peak fleet < 10 | it never grew past its first routes |
| `fleet_decays` | final fleet < 0.6 × peak | vehicles were sold and not replaced |
| `hoards_cash` | not the winner, idle cash > 0.6 of value and fleet < 0.25 × the winner's | money is not working |
| `low_yield` | not the winner, income per vehicle < 0.5 × the winner's | its vehicles earn too little |
| `idle_stations` | more than 2 stations per vehicle at the end | stations built and left unserved |
| `slow_start` | first vehicle later than the first quarter after the start | it idled while the winner was already earning |
| `mode_gap:<mode>` | not the winner; a mode earns at least 0.3 of the winner's last-year profit, and it runs no vehicle of that mode | it is absent from the winner's source of money (referee v3 fields `air`/`pair` etc.; absent in older editions) |

`hoards_cash` is relative on purpose. The first version tested idle cash alone and flagged the
winner in every match: late in a 20-year game a winner's value is mostly cash, because vehicles
depreciate. With the fleet condition added, AAAHogEx has no cause, and AdmiralAI has `hoards_cash`
in 6 of 37 matches, against 35 to 37 of 37 for every PrismaticAI contestant (edition `155d18ea`).

## Trajectory

For each contestant and each year, the record keeps the median over finished matches of the January
sample's `value`, `vehicles`, `air`, `rail`, `pair`, `prail` and `proad` (`trajectories`). The
Markdown shows the best contestant next to every PrismaticAI one, for the first three years, every
fifth year after that, and the last year. That table answers when and in what the gap opens.

## From causes to proposals

Per edition (`edition`), every PrismaticAI contestant's causes are counted over its matches. Each
cause seen in at least half of them applies its remedy to the contestant's settings string. A
remedy names the knobs of `info.nut` that address the cause:

| Cause | Remedy |
|---|---|
| `stalls_small` | `aggressiveness=100, max_routes=30, cash_reserve=5000, intercity=1` |
| `fleet_decays`, `hoards_cash`, `idle_stations` | `self_steward=1, cash_reserve=5000` |
| `low_yield` | `own_lines=8, intercity=1` |
| `slow_start` | `aggressiveness=100, cash_reserve=5000` |
| `mode_gap:<mode>` | none: no setting adds a mode; it is a bot change |

If the result differs from the contestant's settings, it is a proposal. A proposal that equals a
contestant already in the battle tells you that the settings alone cannot fix the cause and the
bot's behaviour must change. That is what edition `155d18ea` shows. Both proposals equal the
`self-steward` contestant's settings, and that contestant still has `idle_stations` in 36 and
`hoards_cash` in 37 of 37 matches.

## What the first post-mortem found

From [`postmortem-155d18ea50071e72.md`](results/postmortem-155d18ea50071e72.md) (37 matches, PrismaticAI
at `ba51ba6022`, 20 years from 1960, an 8×8 temperate map):

- Default `PrismaticAI` was last in 29 of 37 matches: a peak fleet of 5 and a final fleet of 2.5,
  with `stalls_small` and `low_yield` in all of them.
- `solo-max` lost its fleet (peak 24, final 11, `fleet_decays` in 34) and kept 55 stations for it.
- `self-steward` was second on average with 23 vehicles against the winner's 396. Its stations stay
  idle and its cash keeps piling up.
- The gap to the winner opens, in the median, in the second quarter (1960-07). The deficit starts
  with early fleet growth, not with late reinvestment.

These findings hold for that edition and its match parameters only.

The next edition, `91a72662` (PrismaticAI at `df1e05e7d8`, with autopilot but without air), read from
the store after 10 finished matches, showed the two causes that explain the rest:

- `mode_gap:air` in 10 of 10 matches for every PrismaticAI contestant. In 1961 the winner already
  ran 10 aircraft that earned 338k a year, and no PrismaticAI company ever owned an aircraft.
- Scaling, not quality. In 1962 PrismaticAI's trains earned about 29k each and the winner's about
  4k. By 1965 the winner ran 24 trains and 26 aircraft, and by 1980 84 trains, 50 aircraft and
  280 road vehicles. PrismaticAI's whole fleet peaked at about 24 vehicles and then shrank.

## Performance

Parsing and analysis run on rayon's pool (all cores). One task per match record, and the
per-contestant analysis is a single pass over its series. The 37 matches of `155d18ea` (7
contestants, 81 quarters each) took 8 ms to read and 16 ms to analyse on 12 threads. The binary
prints both timings on every run. DuckDB writes go in one transaction.

## Where it lives

| File | What it holds |
|---|---|
| [`tools/research/mcp-rhai/src/postmortem.rs`](../tools/research/mcp-rhai/src/postmortem.rs) | the analysis, thresholds, remedies and Markdown; pure, with unit tests |
| [`tools/research/mcp-rhai/src/bin/battle-postmortem.rs`](../tools/research/mcp-rhai/src/bin/battle-postmortem.rs) | reading the store or JSON, rayon, DuckDB output, `check` |
| [`research/results/postmortem-*.json`](results/) / `.md` | the records |

Tested by `cargo test --bin battle-postmortem`: a stalled bot, a hoarding bot, a winner with cash
but no cause, an unfinished match, a replay, and an edition that is deterministic in any input
order. `tools/gate.sh change` runs `battle-postmortem check`.

## Related

- [LAN AI battle](ai-battle/README.md), [Live arena](live-arena.md)
- Rule `project.prismatic-ai-is-never-last` and `project.battle-records-are-reproducible`
