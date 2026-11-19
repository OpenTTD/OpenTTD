#!/usr/bin/env python3
"""RESEARCH-ONLY. Offline tests for tools/research/ai-battle/battle.py: no game, no SSH, no arena.

Run: python3 -B tools/research/test_ai_battle.py
(or python3 -B -m unittest discover -s tools/research -p 'test_*.py', as tools/gate.sh change does).

Covered: the referee line parser and company-id mapping, the match status contract (exit 0 and the
final-year report), scoring and its exclusions, the replay comparison, the console start script
(double quotes, settings), tick arithmetic, the record validator, the derived Markdown and README
region, and the tar export that strips a directory's parents.
"""
import importlib.util
import io
import json
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("battle", HERE / "ai-battle" / "battle.py")
battle = importlib.util.module_from_spec(spec)
sys.modules["battle"] = battle
spec.loader.exec_module(battle)

KEYS = ["A", "B C", "D"]
MC = {"years": 2, "start_year": 1960, "landscape": "temperate", "map_x": 8, "map_y": 8}


def ref(y, mo, cid, value, veh=1, name="X"):
    return (f"[2026-11-19 05:00:00] dbg: [script:1] [18] [E] REF Q date={y}-{mo} cid={cid} "
            f"value={value} money=5 income=7 perf=9 vehicles={veh} stations=2 name={name}")


def match(mid, host, seed, values, status="done", replica_of=None):
    lineup = [{"key": k, "ai": k} for k in KEYS]
    series = {k: [{"t": "1962-01", "value": v, "vehicles": 1, "stations": 1, "income": 0}]
              for k, v in zip(KEYS, values)}
    m = {"id": mid, "host": host, "host_version": "v1", "seed": seed, "years": 2,
         "ticks": battle.match_ticks(2), "lineup": lineup, "series": series, "errors": {},
         "status": status, "replica_of": replica_of, "match_cfg": MC}
    m["standings"] = battle.standings_of(m)
    return m


class ParseLog(unittest.TestCase):
    def test_reads_ref_lines_errors_and_keeps_last_duplicate(self):
        text = "\n".join([
            ref(1960, 1, 0, 0), ref(1960, 1, 1, 0, name="Two Words Co"),
            ref(1960, 4, 0, 10), ref(1960, 4, 0, 11),   # a repeated quarter: the last one wins
            "dbg: [script:1] [1] [S] Your script made an error: the index 'x' does not exist",
            "dbg: [script:1] [1] [I] chatter that is not a referee line",
        ])
        series, errors = battle.parse_log(text)
        self.assertEqual([s["t"] for s in series[0]], ["1960-01", "1960-04"])
        self.assertEqual(series[0][-1]["value"], 11)
        self.assertEqual(series[1][0]["name"], "Two Words Co")
        self.assertEqual(errors, {1: 1})


class MatchContract(unittest.TestCase):
    def test_done_needs_exit_zero_and_final_year(self):
        m = match("m-0001", "h", 1, [3, 2, 1])
        self.assertEqual(battle.match_status(m, 0), "done")
        self.assertEqual(battle.match_status(m, 137), "incomplete")
        m["series"] = {k: [dict(s[0], t="1961-10")] for k, s in m["series"].items()}
        self.assertEqual(battle.match_status(m, 0), "incomplete")

    def test_ticks_cover_the_final_january_report(self):
        self.assertEqual(battle.match_ticks(20), int((20 * 365.25 + 6) * 74))
        self.assertGreater(battle.match_ticks(1), 365 * 74 + 2 * 74)

    def test_start_script_quotes_names_and_passes_settings(self):
        text = battle.start_script_text([{"ai": "LuDiAI AfterFix"},
                                         {"ai": "PrismaticAI", "settings": "a=1,b=2"}])
        self.assertEqual(text, 'start_ai "LuDiAI AfterFix"\nstart_ai "PrismaticAI" a=1,b=2\n')

    def test_launch_uses_an_edition_directory_and_refuses_a_leftover(self):
        host = battle.Host({"name": "h", "ssh": None}, "$HOME/ttd-battle")
        m = {"id": "m-0001", "edition": "abc", "lineup": [{"ai": "A"}], "ticks": 7, "seed": 3}
        text = battle.launch_script(host, m, MC)
        self.assertIn("test ! -e $HOME/ttd-battle/matches/abc/m-0001\n", text)
        self.assertIn("null:ticks=7", text)
        self.assertIn("-G 3", text)

    def test_grouped_contestants_take_turns_one_per_match(self):
        cs = [{"key": "P1", "group": "P"}, {"key": "P2", "group": "P"}, {"key": "A"}, {"key": "B"}]
        seen = set()
        for n in range(4):
            lu = [c["key"] for c in battle.lineup_for(cs, n)]
            self.assertEqual(len(lu), 3)
            self.assertEqual(sum(k.startswith("P") for k in lu), 1)
            self.assertTrue({"A", "B"} <= set(lu))
            seen |= {k for k in lu if k.startswith("P")}
        self.assertEqual(seen, {"P1", "P2"})

    def test_reap_keeps_the_current_edition_and_finished_matches(self):
        with tempfile.TemporaryDirectory() as tmp:
            for ed, mid, finished in (("cur", "m-0001", False), ("old", "m-0001", False), ("old", "m-0002", True)):
                d = Path(tmp) / "matches" / ed / mid
                d.mkdir(parents=True)
                proc = subprocess.Popen(["sleep", "30"])
                (d / "pid").write_text(str(proc.pid))
                if finished:
                    (d / "exit_code").write_text("0")
                procs = getattr(self, "_procs", [])
                procs.append((ed, mid, proc))
                self._procs = procs
            out = subprocess.run(["/bin/sh", "-c", battle.reap_script(tmp, "cur")], capture_output=True,
                                 text=True).stdout.strip()
            self.assertEqual(out, "reaped 1")
            alive = {(ed, mid): p.poll() is None for ed, mid, p in self._procs}
            self.assertEqual(alive, {("cur", "m-0001"): True, ("old", "m-0001"): False, ("old", "m-0002"): True})
            for _, _, p in self._procs:
                p.kill()

    def test_contestant_key_prefers_label(self):
        self.assertEqual(battle.contestant_key({"ai": "P", "settings": "x=1"}), "P[x=1]")
        self.assertEqual(battle.contestant_key({"ai": "P", "label": "P max"}), "P max")


class Scoring(unittest.TestCase):
    def test_points_wins_and_exclusions(self):
        ms = [match("m-0001", "h1", 1, [30, 20, 10]),
              match("m-0002", "h2", 2, [5, 50, 1]),
              match("m-0003", "h2", 1, [30, 20, 10], replica_of="m-0001"),  # replay: not scored
              match("m-0004", "h1", 3, [99, 0, 0], status="incomplete")]    # unfinished: not scored
        board = {r["key"]: r for r in battle.leaderboard(KEYS, ms)}
        self.assertEqual(board["A"]["points"], 2 + 1)
        self.assertEqual(board["B C"]["points"], 1 + 2)
        self.assertEqual(board["B C"]["wins"], 1)
        self.assertEqual(board["A"]["played"], 2)
        self.assertEqual(board["D"]["avg_rank"], 3.0)
        self.assertEqual(battle.leaderboard(KEYS, ms)[0]["key"], "B C")   # tie broken by value

    def test_replay_comparison(self):
        ms = [match("m-0001", "h1", 1, [30, 20, 10]),
              match("m-0003", "h2", 1, [30, 20, 10], replica_of="m-0001"),
              match("m-0002", "h1", 2, [3, 2, 1]),
              match("m-0004", "h2", 2, [3, 2, 9], replica_of="m-0002")]
        det = {d["replay"]: d["identical"] for d in battle.determinism(ms)}
        self.assertEqual(det, {"m-0003": True, "m-0004": False})


def record(**over):
    ms = [match("m-0001", "h1", 1, [30, 20, 10])]
    rec = {"schema": battle.RECORD_SCHEMA, "scoring": battle.SCORING, "id": "battle-20261119T000000-abcdef12",
           "generated_at": "2026-11-19T00:00:00+00:00", "edition": "abcdef1234567890",
           "provenance": {"prismatic_path": "p", "referee_path": "r", "rev": "a" * 40,
                          "dirty": False, "baseset": "opengfx.tar", "ais": []},
           "match_cfg": MC, "contestants": [{"key": k, "ai": k, "settings": ""} for k in KEYS],
           "hosts": [{"name": "h1", "versions": ["v1"]}],
           "matches": [{"id": m["id"], "host": m["host"], "host_version": "v1", "seed": 1,
                        "years": 2, "ticks": m["ticks"], "lineup": KEYS, "replica_of": None,
                        "status": "done", "exit_code": 0,
                        "standings": [{k: f[k] for k in ("key", "value", "vehicles", "stations",
                                                         "errors")} for f in m["standings"]]}
                       for m in ms],
           "determinism": []}
    rec["leaderboard"] = battle.leaderboard(KEYS, rec["matches"])
    rec.update(over)
    return rec


class Health(unittest.TestCase):
    RULES = {"fleet_min_peak": 10, "fleet_keep": 0.7}

    @staticmethod
    def fleet(*counts):
        return [{"t": f"{1960 + i // 4}-{1 + 3 * (i % 4):02d}", "value": 1, "vehicles": v}
                for i, v in enumerate(counts)]

    def test_fleet_drop_and_errors_are_anomalies(self):
        self.assertEqual(battle.anomalies(self.fleet(5, 22, 20, 11), 0, self.RULES), ["fleet_drop:22->11"])
        self.assertEqual(battle.anomalies(self.fleet(5, 22, 20, 16), 0, self.RULES), [])
        self.assertEqual(battle.anomalies(self.fleet(2, 9, 3), 0, self.RULES), [])  # too small to judge
        self.assertEqual(battle.anomalies(self.fleet(1, 2), 3, self.RULES), ["squirrel_errors:3"])

    def test_fitness_fails_on_collapse_of_a_healthy_bot_and_on_any_bot_crash(self):
        fit = {"rules": self.RULES, "must_be_healthy": ["P steward"]}
        lineup = [{"ai": "PrismaticAI", "key": "P steward"}, {"ai": "PrismaticAI", "key": "P max"},
                  {"ai": "AAAHogEx", "key": "AAAHogEx"}]
        m = {"status": "done", "lineup": lineup, "errors": {},
             "series": {"P steward": self.fleet(5, 22, 21), "P max": self.fleet(5, 24, 9),
                        "AAAHogEx": self.fleet(1, 50, 2)}}
        self.assertEqual(battle.fitness_verdict(m, fit), [])          # only the healthy bot is judged
        m["series"]["P steward"] = self.fleet(5, 22, 8)
        self.assertEqual(battle.fitness_verdict(m, fit), ["P steward: fleet_drop:22->8"])
        m["series"]["P steward"] = self.fleet(5, 22, 21)
        m["errors"] = {"P max": 2}
        self.assertEqual(battle.fitness_verdict(m, fit), ["P max: squirrel_errors:2"])
        m["status"] = "incomplete"
        self.assertTrue(battle.fitness_verdict(m, fit)[0].startswith("match incomplete"))


class Telemetry(unittest.TestCase):
    def test_v3_fields_parse_and_v2_lines_still_do(self):
        v3 = ("dbg: [script:1] [18] [E] REF Q date=1961-4 cid=0 value=9 money=1 income=2 perf=3 "
              "vehicles=4 stations=5 rail=1 road=3 air=0 water=0 prail=-7 proad=12 pair=0 pwater=0 "
              "cargo=44 expenses=-6 name=Big Co Transport")
        series, _ = battle.parse_log(v3 + "\n" + ref(1961, 7, 0, 10))
        a, b = series[0]
        self.assertEqual((a["rail"], a["proad"], a["cargo"], a["name"]), (1, 12, 44, "Big Co Transport"))
        self.assertEqual((b["value"], b["vehicles"]), (10, 1))
        self.assertNotIn("rail", b)

    def test_postmortem_names_where_a_loser_falls_behind(self):
        lineup = [{"key": "W", "ai": "W"}, {"key": "P", "ai": "PrismaticAI"}]
        def s(t, value, veh, income, prail, proad):
            return {"t": t, "value": value, "vehicles": veh, "income": income, "rail": veh // 2,
                    "road": veh - veh // 2, "air": 0, "water": 0, "prail": prail, "proad": proad,
                    "pair": 0, "pwater": 0, "cargo": veh * 10}
        m = {"lineup": lineup, "errors": {}, "series": {
            "W": [s("1961-01", 100, 10, 50, 0, 0), s("1962-01", 1000, 100, 4000, 9000, 900)],
            "P": [s("1961-01", 80, 8, 40, 0, 0), s("1962-01", 200, 10, 100, 100, 50)]}}
        m["standings"] = battle.standings_of(m)
        pm = battle.postmortem(m)
        self.assertEqual(len(pm), 1)
        self.assertEqual((pm[0]["contestant"], pm[0]["winner"], pm[0]["gap"]), ("P", "W", 5.0))
        self.assertEqual(pm[0]["behind_since"], "1962-01")
        text = " | ".join(pm[0]["findings"])
        self.assertIn("fleet 10 vs 100", text)
        self.assertIn("rail profit/yr", text)

    def test_store_is_read_only_through_the_api(self):
        with tempfile.TemporaryDirectory() as tmp:
            st = battle.Store(Path(tmp) / "b.sqlite")
            m = match("m-0001", "h1", 1, [3, 2, 1])
            m.update(edition="e1", started_at=0.0, ticks=1, lineup=[{"key": k, "ai": k} for k in KEYS])
            st.match(m)
            r = st.query("SELECT contestant, rank FROM standings ORDER BY rank")
            self.assertEqual([row[0] for row in r["rows"]], KEYS)
            self.assertIn("error", st.query("DELETE FROM standings"))
            self.assertIn("error", st.query("SELECT 1; DROP TABLE matches"))
            lineup = json.loads(st.query("SELECT lineup FROM matches")["rows"][0][0])
            self.assertEqual(lineup[0], {"key": "A", "ai": "A", "settings": ""})

    def test_fitness_fails_when_a_never_last_bot_is_last(self):
        lineup = [{"ai": "PrismaticAI", "key": "PrismaticAI"}, {"ai": "X", "key": "X"}]
        fl = Health.fleet
        m = {"status": "done", "lineup": lineup, "errors": {},
             "series": {"PrismaticAI": [dict(p, value=5) for p in fl(1, 2)],
                        "X": [dict(p, value=9) for p in fl(1, 2)]}}
        fit = {"rules": Health.RULES, "never_last": ["PrismaticAI"]}
        self.assertEqual(battle.fitness_verdict(m, fit), ["PrismaticAI: last of 2 (value 5)"])
        m["series"]["X"][-1]["value"] = 1
        self.assertEqual(battle.fitness_verdict(m, fit), [])


class Tuning(unittest.TestCase):
    T = {"seed": "t", "group": "P", "label_prefix": "P GA", "population": 4, "mutation_rate": 0.5,
         "fixed": {"self_steward": 1}, "start": {"a": 50, "b": 4},
         "space": {"a": [0, 100, 10], "b": [2, 10, 2]}}

    def test_generations_are_deterministic_and_within_bounds(self):
        g0 = battle.first_generation(self.T)
        self.assertEqual(g0, battle.first_generation(self.T))
        self.assertEqual(len(g0), 4)
        self.assertEqual(g0[0]["genes"], {"a": 50, "b": 4})
        for v in g0:
            self.assertTrue(0 <= v["genes"]["a"] <= 100 and v["genes"]["a"] % 10 == 0)
            self.assertIn("self_steward=1", v["settings"])
            self.assertEqual(v["group"], "P")
        scored = [(v, {"score": i}) for i, v in enumerate(g0)]
        g1 = battle.next_generation(self.T, {"generation": 0}, scored)
        self.assertEqual(g1[0]["genes"], g0[3]["genes"])          # elitism: the best survives
        self.assertTrue(g1[0]["label"].endswith("g1.0"))
        self.assertEqual(g1, battle.next_generation(self.T, {"generation": 0}, scored))

    def test_brain_proposals_join_as_clamped_offspring(self):
        with tempfile.TemporaryDirectory() as tmp:
            f = Path(tmp) / "brain.json"
            f.write_text(json.dumps({"schema": "battle-brain/v1", "proposals": [
                {"settings": "a=37,b=99,zzz=1", "kind": "safe"}, {"settings": "a=200"}]}))
            t = dict(self.T, brain_offspring=2, brain_file=str(f))
            self.assertEqual(battle.brain_proposals(t), [({"a": 40, "b": 10}, "Safe"), ({"a": 100, "b": 4}, "")])
            g0 = battle.first_generation(t)
            g1 = battle.next_generation(t, {"generation": 0}, [(v, {"score": i}) for i, v in enumerate(g0)])
            self.assertEqual([v["label"].rsplit(" ", 1)[1] for v in g1[1:3]], ["g1.brainSafe", "g1.brain1"])
            f.write_text("{}")
            self.assertEqual(battle.brain_proposals(t), [])

    def test_fitness_normalises_points_and_gap(self):
        ms = [{"status": "done", "standings": [{"key": "W", "value": 100}, {"key": "X", "value": 10}]},
              {"status": "done", "standings": [{"key": "X", "value": 50}, {"key": "W", "value": 5}]},
              {"status": "done", "replica_of": "m", "standings": [{"key": "X", "value": 1}]}]
        f = battle.fitness_of("X", ms)
        self.assertEqual((f["matches"], f["points"], f["log_gap"], f["last_share"]), (2, 0.5, -0.5, 0.5))
        self.assertEqual(battle.fitness_of("X", ms, 0.5)["score"], round(0.5 - 0.05 - 0.25, 4))


class Promotion(unittest.TestCase):
    @staticmethod
    def m(seed, vals, **kw):
        return dict({"status": "done", "seed": seed, "edition": "e",
                     "standings": [{"key": k, "value": v} for k, v in vals],
                     "lineup": [{"key": k, "ai": k, "settings": ""} for k, _ in vals]}, **kw)

    def test_pairs_on_common_seeds_and_promotes_only_clear_winners(self):
        ms = [self.m(s, [("PrismaticAI", 100), ("PrismaticAI X", 130), ("AAAHogEx", 900)]) for s in range(18)]
        ms += [self.m(s, [("PrismaticAI", 100), ("PrismaticAI Y", 104)]) for s in range(18)]
        ms.append(self.m(99, [("PrismaticAI X", 1)]))                      # no default on that seed
        ms.append(self.m(1, [("PrismaticAI", 1), ("PrismaticAI X", 999)], replica_of="m-1"))  # replay
        pv = {r["contestant"]: r for r in battle.paired_vs(ms, "PrismaticAI")}
        self.assertEqual((pv["PrismaticAI X"]["pairs"], pv["PrismaticAI X"]["wins"], pv["PrismaticAI X"]["ratio"]), (18, 18, 1.3))
        promo = [p["contestant"] for p in battle.promotions(ms, {"min_pairs": 16, "min_win_share": 0.8, "min_ratio": 1.05})]
        self.assertEqual(promo, ["PrismaticAI X"])                         # Y: +4% is below min_ratio
        self.assertEqual(battle.promotions(ms, {"min_pairs": 20}), [])

    def test_a_configuration_is_pooled_across_its_labels(self):
        def g(seed, label, value, settings):
            return {"status": "done", "seed": seed, "started_at": seed, "edition": f"e{seed // 6}",
                    "lineup": [{"key": "PrismaticAI", "ai": "PrismaticAI", "settings": ""},
                               {"key": label, "ai": "PrismaticAI", "settings": settings}],
                    "standings": [{"key": label, "value": value}, {"key": "PrismaticAI", "value": 100}]}
        ms = [g(s, f"PrismaticAI GA g{s // 6}.0", 150, "a=1") for s in range(18)]
        pv = battle.paired_vs(ms, "PrismaticAI")
        self.assertEqual(len(pv), 1)
        self.assertEqual((pv[0]["pairs"], pv[0]["contestant"], pv[0]["settings"]), (18, "PrismaticAI GA g2.0", "a=1"))


class Records(unittest.TestCase):
    NAME = "battle-20261119T000000-abcdef12.json"

    def test_valid_record_passes(self):
        self.assertEqual(battle.validate_record(record(), self.NAME), [])

    def test_dirty_edition_unnamed_build_and_forged_board_are_refused(self):
        rec = record()
        rec["provenance"]["dirty"] = True
        rec["matches"][0]["host_version"] = ""
        rec["leaderboard"][0]["points"] += 1
        errs = "\n".join(battle.validate_record(rec, self.NAME))
        self.assertIn("uncommitted", errs)
        self.assertIn("arena build", errs)
        self.assertIn("not derived", errs)

    def test_missing_field_and_wrong_name(self):
        rec = record()
        del rec["matches"]
        self.assertTrue(battle.validate_record(rec, self.NAME))
        self.assertTrue(battle.validate_record(record(), "battle-other.json"))

    def test_region_is_derived_and_replaceable(self):
        rec = record()
        region = battle.render_region(rec)
        self.assertTrue(region.startswith(battle.BEGIN) and region.endswith(battle.END))
        self.assertIn("| 1 | A |", region)
        self.assertNotIn("| Match |", region)        # the per-match table stays in the record
        text = f"intro\n{battle.BEGIN}\nold\n{battle.END}\noutro\n"
        out = battle.replace_region(text, region)
        self.assertTrue(out.startswith("intro\n") and out.endswith("\noutro\n"))
        self.assertEqual(battle.replace_region(out, region), out)
        self.assertIn("| m-0001 | h1 | 1 | A |", battle.render_md(rec))


class Export(unittest.TestCase):
    def test_export_strips_parent_directories(self):
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp) / "ref.tar"
            path = "tools/research/ai-battle/referee/BattleReferee"
            battle.export_tree(path, "HEAD", dest) if self._tracked(path) else self.skipTest(
                "referee not committed yet")
            with tarfile.open(dest) as t:
                names = t.getnames()
            self.assertIn("BattleReferee/main.nut", names)
            self.assertFalse(any(n.startswith("tools/") for n in names))

    @staticmethod
    def _tracked(path):
        r = subprocess.run(["git", "-C", str(battle.REPO), "ls-tree", "-d", "HEAD", path],
                           capture_output=True, text=True)
        return bool(r.stdout.strip())


if __name__ == "__main__":
    unittest.main()
