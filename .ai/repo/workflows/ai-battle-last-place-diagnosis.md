# Diagnosing a PrismaticAI last place in the AI battle

The battle is `tools/research/ai-battle/battle.py` (described in `research/ai-battle/README.md`).
A PrismaticAI configuration that ends last, goes bankrupt or stalls is a lead for a bot fix. A
battle record cannot show why it happened, but a replay can. This is the order of work.

1. **Pick the case.** Use a match where one configuration did much worse than the others on the
   same seed and slot. Find candidates in the post-mortems
   (`~/ttd-battle/results/postmortem-<edition>.json`: `last`, `losses`, `mode_mix`,
   `idle_cash_share`, `stations_per_vehicle`) or in the run log's `POSTMORTEM` and `ALERT` lines.
   A contestant that is last only against stronger configurations of the same genes is noise.
2. **Recover the exact inputs.**
   - The seed is in the run log (`m-NNNN started on <host>, seed N`).
   - The start slot is the line number of PrismaticAI in the match's
     `home/Documents/OpenTTD/scripts/game_start.scr`, counting from 0. On a remote arena, read it
     with `battle.Host(spec, remote_root).try_sh(...)`; hosts come from `battle.load_hosts`.
   - The genes are the contestant's `settings` in `~/ttd-battle/tuning/state.json`.
   - The slot matters: one gene set has scored 4x more from one slot than from another.
3. **Replay with the bot's log.** Run `battle.py ablate --seed S --years 20 --slot N
   --log-dir DIR --variant name=<settings> --allow-dirty`. It uses the working-tree bot. Check
   that the value matches the arena roughly before trusting the log.
4. **Read the log for the leak, not the symptom.**
   - Tally the bot's messages (`grep PrismaticAI | sed 's/[0-9]+/N/g' | sort | uniq -c`).
   - Follow the company's `REF Q` lines for money, vehicles and stations per year.
   - Two cases where the symptom misled: stations up while vehicles fell were early bus stops,
     and the leak was own lines dropped for want of a flat station site. Planes piling up came
     from the invest loop re-adding to a route whose profit per vehicle counted only year-old
     vehicles.
5. **Fix in the bot, bounded.** A guard should stop the failure whatever the genes say. Narrowing
   a gene range in `battle.json` is the weaker fix, because `variant()` only clamps future
   generations into the space. Cite the seed, slot and configuration in the code comment.
6. **A/B against the committed bot.**
   - Export HEAD's bot with `git archive HEAD tools/research/prismatic-ai/PrismaticAI | tar -x
     -C <dir>`.
   - Write a copy of `battle.json` whose `prismatic.path` is that absolute directory, and pass
     it to `ablate --config <copy>`. In zsh, write the flag and path literally: a `$var` holding
     both is passed as one word.
   - Run the default and the failing genes on the failing seed and on at least two reference
     seeds, each in its own slot.
   - Compare the geometric mean of the default's value per seed. Report each seed, including
     the ones that got worse.
7. **Gate.** `battle.py fitness --allow-dirty` must pass, which is the rule for changes to
   `lines.nut`, `fleet.nut`, `steward.nut` and the own-lines loop.
8. **Ship.**
   - Commit with explicit paths, including the doc updates: the
     `research/ai-battle/README.md` bullet and the curated knowledge note
     `prismatic-ai-without-a-steward-keeps-one-train-per-line.md`. Then push to master.
   - `battle.py prepare` exports the new rev into the arena.
   - Stop the orchestrator by its explicit pid, wait for it to exit, and restart `battle.py run`.
9. **Tell the brain's owner.**
   - Send the first edition id and the A/B figures.
   - If the default moved materially, the model is cut at that edition and not rebased, because
     the effect depends on the genes.

A replay is deterministic for one tree, binary, seed and slot. A local replay can still differ
from the arena match (another host build), so a failure that does not reproduce locally cannot
validate its fix. Say so, and do not claim the fix.
