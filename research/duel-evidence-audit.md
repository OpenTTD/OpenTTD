# Duel evidence audit and a frozen duel set

RESEARCH-ONLY. What the recorded duels (`research/results/duel-*.json`) hold constant and what they
let vary, and one set of duels run with every input frozen and named. Tool: `tools/research/mcp-duel`;
scoreboard: `tools/research/readme-scoreboard`; rule `project.a-tuning-claim-needs-held-start-pairs`.

## What a duel measures

One game, one map, two companies. Company A is PrismaticAI started by the panel from the planner's
portfolio, which reaches the bot as settings (`plan.bot_settings`). Company B is the same AI started
with `start_ai <opponent>` and no settings, so it runs its own defaults. The winner has the higher
company value after N months.

**Company value** is `CalculateCompanyValue` (`src/economy.cpp`): money minus loan, plus every
train, road vehicle, aircraft and ship at 1.5 times its value, plus each station facility at a flat
price. Track, roads, depots and other infrastructure count for nothing, so a company that invested
in rail shows less value than it owns.

## What the 63 records let vary (audit of 2026-11-19, tree 55328bdb56)

| Input | Recorded? | What varied |
|---|---|---|
| Run length | yes (`months`) | 6, 8, 12, 24, 36 and 120 months |
| Held start | field `held_start` only in the 14 records since I0083 | earlier records that say "held-start" say it only in their label |
| Bot revision | free text in the label until 2026-11-18 (e.g. "bridge f598ede"); not at all for the I0087 pairs | the bot lived in `~/dev/openttd-prismatic-bridge` until `340209766d` (2026-11-18), and the records name the panel's revision, not the bridge checkout's |
| Panel revision | in the plan events of the 14 store-era records | the I0087 pair on seed 20208 ran at `ea11985ffc` and `c18b413802`, which differ by a results commit only (no change under `src/`, `tools/research/mcp-rhai/src`, the bot or `mcp-duel`) |
| A's extra settings | `bot_settings` in 39 records | `own_lines`, line targets and calibration variants |
| B's settings | not as a field; the bot's start line lists only part of them | B ran the bot's defaults of its day |
| B's behaviour | `final["2"].trains` | in 62 of 63 records B ran no train: the bot without a plan built buses |

So the scoreboard's tally of wins counts separate experiments, and its chart compares bot
revisions by design. Within one record the comparison is paired: both companies run the same AI
code in the same game from the same start, and only A gets the portfolio.

## What changed at df1e05e7d8

PrismaticAI's `autopilot` setting (default 1) makes a bot without a plan grow on its own: in-bot
steward, at least eight own freight lines, intercity roads, 30 routes. Since then a default duel's
company B is that autopilot, not the bus-only bot every earlier record measured. A default duel
now asks whether the external planner beats the in-bot autopilot. `--opponent 'PrismaticAI
autopilot=0'` starts the former rival (`start_ai [<AI>] [<settings>]`).

## A frozen duel set

Three duels on 2026-11-19, one per seed, every input fixed before the first one started and the same
for all three. Status: **PARTIAL**. The paired comparison ran on three seeds. It ran once per seed,
so it has no repeat to measure noise within a seed, and the rival's spread it prints is invalid
(see "Defects found").

**Inputs** (recorded at 2026-11-19T06:59:42Z, before the runs):

| Input | Value |
|---|---|
| Tree | `55328bdb56`, worktree `fix/evidence-consistency`. Uncommitted were only this audit's docs and tools; `git diff --quiet HEAD` held for `tools/research/prismatic-ai`, `src`, `tools/research/mcp-rhai/src` and `tools/research/mcp-e2e` |
| Bot | PrismaticAI from that tree, git tree `abc22ce80b`, loaded through a personal directory of its own whose `ai/PrismaticAI` links to the worktree, so commits elsewhere could not change it during a run |
| Game binary | `tools/gate.sh research` build of `42528010af` (PASS, 127/127), sha256 `8dc22031489ef782…`; `src/` is identical at `55328bdb56` |
| Panel | `mcp-panel` built at `55328bdb56` in the worktree's own target, sha256 `cb8ee06dd5c061a9…` |
| Planner | AlgoGlyph `1b8e009059` (`~/dev/algoglyph-wt/main`, clean), knapsack-dp |
| Settings | defaults on both sides; the records name the panel revision `55328bdb56` and `55328bdb56-dirty` |
| Start | held (`PRISMATIC_START_PAUSED=2`), resumed at ticks 1413, 1439 and 1451 |

Command, once per seed: `MCP_ALGOGLYPH_DIR=~/dev/algoglyph-wt/main tools/research/mcp-duel --months 36
--seed <seed> --label "frozen duel set at 55328bdb56: …"`, with `PRISMATIC_MCP_SUPERVISOR` set to the
task. All three exited 0 with no missing month and no fallback. Both companies' live settings were
read from `/api/bots` once both ran.

**The treatment.** 131 settings each. A differs from B only in what the panel's planned start sets:
the portfolio's freight lines (`fr*_from`, `fr*_to`, `fr*_cargo`: 9 lines on 20208, 13 on 31337, 10 on
4242), the passenger rail pair (`rail_a`, `rail_b`), `rail_after` from the plan's scores, and
`own_lines=8`. Both have `autopilot=1`, which acts only in a bot without planner lines, so B grew on
autopilot.

| Seed | A planned: value | money | loan | trains / road / planes | B autopilot: value | money | loan | trains / road / planes | A over B |
|---|---:|---:|---:|---|---:|---:|---:|---|---:|
| 20208 | 879,332 | 298,682 | 0 | 16 / 1 / 0 | 453,776 | 25,107 | 190,000 | 7 / 12 / 6 | +93.8% |
| 31337 | 1,095,101 | 495,789 | 0 | 17 / 3 / 0 | 698,884 | 62,573 | 300,000 | 8 / 13 / 11 | +56.7% |
| 4242 | 715,486 | 89,344 | 0 | 17 / 1 / 0 | 441,921 | 18,848 | 270,000 | 9 / 22 / 4 | +61.9% |

Records: [`duel-20261119T085457-seed20208`](results/duel-20261119T085457-seed20208.md),
[`duel-20261119T085551-seed31337`](results/duel-20261119T085551-seed31337.md),
[`duel-20261119T085602-seed4242`](results/duel-20261119T085602-seed4242.md).

Refused or failed builds in the bot logs: A 1, 0 and 5 (rail on a tile owned by B or not clear);
B 20, 17 and 4 (airports on unflat land, bus stops without cash). The records hold no figure for
construction spending, so investment is not compared here.

**What it shows.** At this revision, on three maps and 36 months from a held start, the bot given
the planner's portfolio ended with more company value than the same bot growing on autopilot, by
57% to 94%. A also ended without a loan and B with one.

**Limits.**
- One run per seed, so no estimate of within-seed noise. Earlier held repeats drifted by -7.6% to
  +11.4% over 120 months (knowledge `duels-run-side-by-side-are-noisy`).
- The three ran side by side on a machine with a load near 55.
- Company value leaves out track, roads and depots. A built rail and B built roads, rail and
  airports (airports count only as station facilities), so how much each side's uncounted
  infrastructure is worth, and in which direction it biases the gap, is not known.
- 36 months only.
- The control is the autopilot of `df1e05e7d8`, not the bus-only bot of the earlier records.
- No stronger opponent was tested: that is a separate external-validity question, which the LAN AI
  battle (`research/ai-battle/README.md`) addresses with its own lineup.

**Not run:** repeats per seed of this set and 120-month runs.

## The same set against the former rival

Six more duels, two per seed, with the same frozen inputs (tree, bot tree `abc22ce80b`, game
binary, panel, planner, held start, 36 months). Only B differs: `--opponent 'PrismaticAI
autopilot=0'`, the rival every record before `df1e05e7d8` measured. Status: **PARTIAL**, for the same
reasons as above.

- **How they ran.** One after another on 2026-11-20, not side by side like the autopilot set: the
  first run 00:39Z to 02:53Z, the repeat 03:14Z to 05:25Z (load 42 to 70 throughout). Two earlier attempts on 2026-11-19 did not run. At 10:53Z, three side
  by side at load 65 to 86 with swap nearly full, the planner for A ended by a signal on every seed
  ("planner failed for the bot: exit -1") before the planned bot started, and `mcp-duel` gave up
  after its 8-minute wait (exit 1, no record). From 11:08Z to 15:08Z a wait for load under 25 and
  6 GB of free swap never got there (the 1-minute load never fell below 33).
- **The rival.** Each record holds B's live settings (`rival`, read by `mcp-duel` before the first
  month): PrismaticAI, 131 settings, `autopilot=0`, `own_lines=0`. A had `autopilot=1` and
  `own_lines=8`, as in the first set.

| Seed | A planned: value | money | loan | trains / road / planes | B former rival: value | money | loan | trains / road / planes |
|---|---:|---:|---:|---|---:|---:|---:|---|
| 20208 | 1,130,884 | 523,482 | 0 | 17 / 2 / 0 | 1 | 16,346 | 50,000 | 0 / 3 / 0 |
| 31337 | 1,468,025 | 735,302 | 0 | 20 / 3 / 0 | 47,923 | 41,457 | 40,000 | 0 / 7 / 0 |
| 4242 | 900,900 | 226,594 | 0 | 18 / 1 / 0 | 42,956 | 40,952 | 40,000 | 0 / 6 / 0 |
| 20208, repeat | 1,147,048 | 540,425 | 0 | 17 / 2 / 0 | 1 | 15,823 | 50,000 | 0 / 3 / 0 |
| 31337, repeat | 1,441,319 | 758,848 | 0 | 19 / 3 / 0 | 49,526 | 43,060 | 40,000 | 0 / 7 / 0 |
| 4242, repeat | 782,373 | 61,322 | 0 | 19 / 1 / 0 | 46,371 | 44,367 | 40,000 | 0 / 6 / 0 |

Records: [`duel-20261120T022428-seed20208`](results/duel-20261120T022428-seed20208.md),
[`duel-20261120T030925-seed31337`](results/duel-20261120T030925-seed31337.md),
[`duel-20261120T035343-seed4242`](results/duel-20261120T035343-seed4242.md), and the repeats
[`duel-20261120T045851-seed20208`](results/duel-20261120T045851-seed20208.md),
[`duel-20261120T054204-seed31337`](results/duel-20261120T054204-seed31337.md),
[`duel-20261120T062525-seed4242`](results/duel-20261120T062525-seed4242.md).

**What it shows.** Against the bus-only rival the planned bot wins on every seed by far more than
against the autopilot. No percentage is given: OpenTTD reports a company value of at least 1
(`src/economy.cpp`, `std::max<Money>(value, 1)`), and on seed 20208 B's money minus loan was
-33,654, so its value of 1 is that floor, not a measurement.

**A depends on its rival.** The planned bot itself ended higher here than in the autopilot set on
every seed: 1,130,884 against 879,332 on 20208, 1,468,025 against 1,095,101 on 31337, and 900,900
against 715,486 on 4242. The two sets differ in the rival and in running one after another instead
of side by side, so this is not one effect measured. But it means a figure for A is a figure against
a particular rival: two duels compare only when their rivals' settings match, which is what the
rival's recorded settings now make checkable.

**The repeats.** Each repeat's record reads the rival's spread over exactly the two runs of its
seed with the same rival settings, as the fixed grouping intends: 3.3% on 31337 and 8.0% on 4242.
On 20208 it is 0.0, which measures nothing: B's value was the floor of 1 in both runs (money minus
loan -33,654 and -34,177). A moved between the two runs by +1.4% on 20208, -1.8% on 31337 and
-13.2% on 4242. On 4242 the repeat ended with 61,322 in cash instead of 226,594 and one more train:
money spent on track no longer counts, so building more shows as a lower company value. The gap
between A and B on every seed is at least six times the larger run-to-run difference of A (on 4242: about 740,000 against 118,527).

**Limits.** Two runs per seed, so each spread is a range of two values, not a distribution. Company
value leaves out track, roads and depots, and the records hold no construction spending, so the
-13.2% on 4242 cannot be split into play and investment.

## Defects found

- **The rival's spread mixed rival configurations.** `mcp-duel` computed `rival_spread` over B's final
  value in every record of the same seed, months, opponent string and start mode. Since
  `df1e05e7d8` the opponent string `PrismaticAI` names a different rival, so the three records of the
  autopilot set print spreads of 453775.0, 30.18 and 19.16. Those numbers are meaningless, not measurements, and
  stay in the records as written.
- **B's configuration was not in the record.** The bot's start line lists only part of its settings
  (no `autopilot`, no `own_lines`). The autopilot set read both companies from `/api/bots` by hand.
- **Which earlier verdicts read a mixed spread** (checked 2026-11-22 against each verdict's records).
  The I0087 pairs (`4b43f4873f`, `71cd3f1ed9`, `6615cb7f60`) and the rail-geometry pairs
  (`3eebf84a32`) read spreads over records that all predate `df1e05e7d8`, with a rival that ran no
  train: not mixed, and they stand. The 8 records of `d048525e57` (air_add_waiting and
  catchment_stops, 120 months, 2026-11-19) all ran the autopilot rival, but each one's stored
  `rival_spread` also takes in 4 earlier bus-only records: 73.75 to 393.32, invalid. That verdict did
  not use them: it measured the rival between identical arms within its own pairs (21.6%, 6.3%,
  20.0%), so "inconclusive" stands. Only those 8 stored fields mislead; the records are kept as
  written.
- **A worktree's first duel can stall before the panel starts.** `mcp-e2e` asks `majordomus context
  --json` for its supervision record. In this worktree that call did not return in 9 minutes, and
  `mcp-duel` gave up (exit 1). Setting `PRISMATIC_MCP_SUPERVISOR` beforehand skips it.

The first two are fixed in `mcp-duel` (uncommitted with this audit): it reads B's live settings
from `GET /api/bots` before the first month, keeps them in the record as `rival`, and groups the
rival's spread by them; a record without them groups only with others without them
(`tools/research/test_mcp_duel.py`, 22 tests). The three duels against the former rival ran with the
fix and recorded the rival's settings. The panel's own reading of stored duels (`duel_outcomes.rs`,
rule `own-lines-by-handed-v1`) reads each group against a rival spread too and does not separate
rival configurations; it is not changed here.
