# Adding a decision that reads recorded history

The decision is ADR 0034; the store and the bus are ADR 0033 (issue I0080); the event kinds that
exist are in knowledge `the-panel-bus-carries-these-event-kinds`. This is the order of work for a
planning decision (a bot setting, a planner parameter, a kernel input, Prismatic's directive) that
should come from what earlier games recorded instead of a hand constant or one live snapshot. The
milestone that plans the known cases is `planning-is-generated-from-recorded-history`.

1. **Name the decision and its constant.** Say which choice changes (`own_lines`, `rail_after`, a
   calibration factor, ...), where it is decided (`tools/research/mcp-rhai/src/panel_bot.rs:settings_from_plan`,
   `planned_air`, `plan.rs:defaults`, `panel_decision.rs:pull`, ...) and what it is today: the
   constant or the snapshot, and the measurement it rests on, if any.
2. **Find the history.** Look for the rows in the store before adding a producer: `GET /api/db/tables`,
   then `events` by `kind` (`game.month`, `ai.log`, `plan.*`), `samples` by company and day, `runs` by
   seed. A producer is added only when nothing recorded carries the fact, and only with a decision
   (ADR 0034 point 5: `game.sample` is the one added so far).
3. **Write the query.** A read-only statement on `/api/db/query` of the instance (SELECT or WITH),
   in the decision's code, filtered to the runs it may learn from (same seed class, map size, bot
   version) and with a minimum row count. Never read a JSON file beside the store, and never parse
   `console.line`, `ai.log` text or `panel.*` sentences for a fact a structured kind carries.
4. **State the fallback.** For "store: off" (`/api/status`) and for too few rows: the current
   constant, the live snapshot, or a refusal. Write it next to the query and test both paths with a
   cargo test over fixed rows.
5. **Publish the decision as data.** One `plan.*` event per decision with `{decision, version,
   inputs, choice}`: `inputs` names the live resources and the query with its row count, or
   `fallback`; `version` is the revision and parameter digest. Add the kind to the declared table
   (I0081) and keep the `panel.*` sentence for the page. Re-read the event-kinds note and update it.
6. **Measure it on held-start pairs.** Run paired duels, derived against constant, with the held start
   on three seeds, one or two at a time:

   ```bash
   # arm 1: the constant (today's default); arm 2: the derived value, switched on for that run only
   PRISMATIC_START_PAUSED=2 tools/research/mcp-duel --months 36 --seed 4242 --label constant
   PRISMATIC_START_PAUSED=2 <the decision's switch> tools/research/mcp-duel --months 36 --seed 4242 --label derived
   # the same pair on --seed 31337 and --seed 20208
   ```

   A bot setting can also be forced per run without a code change (`--bot-settings own_lines=0`).

   Read each pair against its own rival's spread (the same bot in both arms). Before the held start
   two runs of one bot differed by up to 52%; with it by about 0.2-0.3%
   (`research/results/duel-20261117T234725-seed31337.md`). A difference inside the spread, one seed,
   or an unpaired run decides nothing. A long horizon matters: own lines cost value until about month
   50, so a 36-month duel shows their cost and not their return.
7. **Decide the default.** The derived value becomes the default only if all three pairs favour it
   beyond the spread (ADR 0034 point 6); otherwise the constant stays and the derived path stays
   behind its switch.
8. **Record it.** The duel records stay under `research/results/`; write or update a knowledge note
   under `.ai/repo/knowledge/curated/` with the seeds, the numbers and the verdict, record evidence
   on the issue (`majordomus plan evidence`), and run `tools/surfaces/surfaces sync` and
   `tools/surfaces/surfaces check` if a scoreboard or table moved.
