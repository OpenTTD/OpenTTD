#!/usr/bin/env python3
"""RESEARCH-ONLY. Tests for tools/research/mcp-duel's reads (issue I0083), offline: no game, no
panel, no stack.

Run: python3 -B tools/research/test_mcp_duel.py
(or python3 -B -m unittest discover -s tools/research -p 'test_*.py', as tools/gate.sh change does).

The duel's Python is loaded from the heredoc of tools/research/mcp-duel, so what is tested is what
runs. A fake panel on 127.0.0.1 answers the routes the duel calls. Its /api/db/query runs each
statement with the DuckDB CLI on a file made from the store's own schema (`TABLES` read from
tools/research/mcp-rhai/src/bus_sinks.rs, the sinks the panel's tap and the hub share since I0084)
and answers it as bus_sinks.rs does: {columns, rows, count, capped}, at most QUERY_CAP rows (read
from the same source) and `capped` beyond. Its
POST /api/bus/publish accepts only the kinds panel_events.rs declares external, with the four fields
refuse_external asks for. The store tests are skipped without a `duckdb` on PATH; the fallback and
the verdict's pure parts run without it.
"""
import contextlib
import http.server
import importlib.machinery
import importlib.util
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import types
import unittest
import urllib.parse
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SCRIPT = HERE / "mcp-duel"
STORE_RS = HERE / "mcp-rhai" / "src" / "bus_sinks.rs"  # the store's schema and query cap (I0084)
EVENTS_RS = HERE / "mcp-rhai" / "src" / "panel_events.rs"
NOTE = ROOT / ".ai" / "repo" / "knowledge" / "curated" / "the-panel-bus-carries-these-event-kinds.md"
DUCKDB = shutil.which("duckdb")


def load_duel():
    """The Python of mcp-duel's heredoc as a module (its run is under __main__, so nothing runs)."""
    m = re.search(r"<<'PY'\n(.*?)\nPY\n", SCRIPT.read_text(), re.S)
    mod = types.ModuleType("mcp_duel")
    exec(compile(m[1], f"{SCRIPT} (heredoc)", "exec"), mod.__dict__)
    return mod


duel = load_duel()
QUERY_CAP = int(re.search(r"pub const QUERY_CAP: usize = (\d+);", STORE_RS.read_text())[1])
TABLES = re.search(r'pub const TABLES: &str = "(.*?)";', STORE_RS.read_text(), re.S)[1]
EXTERNAL = set(re.findall(r'k\("([a-z._]+)", By::External', EVENTS_RS.read_text()))
READ_ONLY = ("SELECT", "WITH", "DESCRIBE", "SUMMARIZE", "SHOW")


def read_only(sql):
    """The first lock of bus_sinks.rs read_only_sql, enough for the statements the duel sends: a
    read-only first word and no second statement after a ';' outside quotes."""
    word = re.match(r"\s*([A-Za-z]+)", sql)
    if not word or word[1].upper() not in READ_ONLY:
        return False
    bare = re.sub(r"'(?:[^']|'')*'", "''", sql)
    return ";" not in bare.rstrip().rstrip(";")


def refuse_external(kind, data):
    """panel_events.rs refuse_external."""
    if kind not in EXTERNAL:
        return f"{kind} is not an external kind"
    missing = [f for f in ("decision", "version", "inputs", "choice") if f not in data]
    return f"missing: {', '.join(missing)}" if missing else None


class Db:
    """A store file with the panel's tables, read and written with the DuckDB CLI."""

    def __init__(self, path):
        self.path = str(path)
        self.exec(TABLES)

    def exec(self, sql):
        subprocess.run([DUCKDB, self.path, "-c", sql], check=True, capture_output=True, text=True)

    def query(self, sql):
        r = subprocess.run([DUCKDB, "-readonly", "-json", self.path, "-c", sql], capture_output=True, text=True)
        if r.returncode != 0:
            return 400, {"error": r.stderr.strip()}
        objs = json.loads(r.stdout or "[]")
        rows = [list(o.values()) for o in objs]
        return 200, {"columns": list(objs[0]) if objs else [], "count": min(len(rows), QUERY_CAP),
                     "rows": rows[:QUERY_CAP], "capped": len(rows) > QUERY_CAP}


def console_lines(month):
    """The console's `companies` answer in its own format (console_cmds.cpp ConCompanies)."""
    return [f"#:1(Dark Blue) Company Name: 'Planned Transport'  Year Founded: 1950  Money: 100000  Loan: 300000  "
            f"Value: {50000 + month}  (T:{month}, R:0, P:0, S:0)",
            f"#:2(Red) Company Name: 'Rival Transport'  Year Founded: 1950  Money: 9000  Loan: 50000  "
            f"Value: {9000 + month}  (T:0, R:{month + 1}, P:0, S:0)"]


class FakePanel(http.server.HTTPServer):
    """The panel's routes the duel calls. The game clock moves on by one month every second read of
    /api/resources/game/state, from 1950-01-04."""

    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.port = self.server_address[1]
        self.db = None
        self.bus = None            # GET /api/bus answer; None: the route does not exist (404)
        self.status_events = []    # GET /api/status events, newest first
        self.bus_events = []       # GET /api/events, the in-memory bus
        self.clock_reads = 0
        self.break_store_at = None  # store answers 503 from this clock read on
        self.sql = []
        self.published = []
        self.bots = None           # GET /api/bots answer; None: the route does not exist (404)

    def date(self):
        m = self.clock_reads // 2
        return f"{1950 + m // 12}-{m % 12 + 1:02d}-04", m


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def answer(self, code, body):
        data = json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        s, url = self.server, urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(url.query)
        if url.path == "/api/bus":
            return self.answer(404, {"error": "no route"}) if s.bus is None else self.answer(200, s.bus)
        if url.path == "/api/status":
            return self.answer(200, {"ai_started": True, "events": s.status_events})
        if url.path == "/api/resources/game/state":
            date, _ = s.date()
            s.clock_reads += 1
            return self.answer(200, {"game_date": date})
        if url.path == "/api/bots":
            return self.answer(404, {"error": "no route"}) if s.bots is None else self.answer(200, s.bots)
        if url.path == "/api/events":
            kinds = q.get("kinds", [""])[0].split(",")
            limit = int(q.get("limit", ["5000"])[0])
            return self.answer(200, {"events": [e for e in s.bus_events if e["kind"] in kinds][-limit:]})
        self.answer(404, {"error": url.path})

    def do_POST(self):
        s = self.server
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        if self.path == "/api/console":
            _, m = s.date()
            return self.answer(200, {"output": console_lines(m)})
        if self.path == "/api/db/query":
            sql = body["sql"]
            s.sql.append(sql)
            if not read_only(sql):
                return self.answer(400, {"error": "read-only"})
            if s.db is None or (s.break_store_at is not None and s.clock_reads >= s.break_store_at):
                return self.answer(503, {"error": "store off: the store is not open"})
            return self.answer(*s.db.query(sql))
        if self.path == "/api/bus/publish":
            why = refuse_external(body.get("kind"), body.get("data") or {})
            if why:
                return self.answer(400, {"error": why})
            seq = 90001 + len(s.published)
            s.published.append(body)
            if s.db is not None:
                data = json.dumps(body["data"]).replace("'", "''")
                s.db.exec(f"INSERT INTO events VALUES ('panel-t', {seq}, 99999, '{body['kind']}', NULL, NULL, '{data}')")
            return self.answer(200, {"published": body["kind"], "seq": seq})
        self.answer(404, {"error": self.path})


AI_LOG_ROWS = 12000  # past two pages of QUERY_CAP


def fill(db, port):
    """Two runs of instance panel-t on this port: an older one (ts 1000..9999) that every read must
    leave out, and the duel's (from ts 10000): its plan sentences and events, the held start,
    12000 ai.log rows and 7 game months of daily samples for two companies."""
    portfolio = {"decision": "bot.portfolio", "version": {}, "inputs": {}, "choice": {"chosen": [{"cargo": "Coal"}]}}
    settings = {"decision": "bot.settings", "version": {}, "inputs": {}, "choice": {"rail_after": 2, "own_lines": 8}}
    j = lambda v: json.dumps(v).replace("'", "''")
    db.exec(f"""
        INSERT INTO runs VALUES ('panel-t', 's0', 31337, 1000, {port}, 1, 2), ('panel-t', 's1', 31337, 10000, {port}, 3, 4);
        INSERT INTO events VALUES
          ('panel-t', 1, 2000, 'plan.portfolio', 5, '1949-12-01', '{j(portfolio)}'),
          ('panel-t', 2, 2001, 'ai.log', 5, '1949-12-01', '{j({"company": 0, "level": "info", "text": "stale run"})}'),
          ('panel-t', 1, 10001, 'panel.ai', NULL, NULL, '{j({"text": "planner: game paused for a repeatable start; it runs once 2 AI company(ies) exist"})}'),
          ('panel-t', 2, 10002, 'game.paused', 1400, '1950-01-04', '{j({"date": "1950-01-04", "tick": 1400})}'),
          ('panel-t', 3, 10003, 'plan.portfolio', 1400, '1950-01-04', '{j(portfolio)}'),
          ('panel-t', 4, 10004, 'plan.bot_settings', 1400, '1950-01-04', '{j(settings)}'),
          ('panel-t', 5, 10005, 'panel.ai', 1400, '1950-01-04', '{j({"text": "planner: Coal by rail Coal Mine -> Power Station, 212.0 tiles"})}'),
          ('panel-t', 6, 10006, 'game.resumed', 1437, '1950-01-04', '{j({"date": "1950-01-04", "tick": 1437})}'),
          ('panel-t', 7, 10007, 'panel.ai', 1437, '1950-01-04', '{j({"text": "planner: game unpaused at tick 1400 with 2 AI company(ies) after 100 s"})}'),
          ('panel-t', 8, 10008, 'panel.server', 1437, '1950-01-04', '{j({"text": "planner-like but not panel.ai"})}');
        INSERT INTO events SELECT 'panel-t', 100 + i, 10100 + i, 'ai.log', 1500 + i, '1950-02-01',
          json_object('company', i % 2, 'level', 'info', 'text', 'line ' || i) FROM range({AI_LOG_ROWS}) t(i);
        INSERT INTO samples VALUES ('panel-t', 3000, 7, '1950-02-01', 0, 'Stale', 1.0, 1.0, 1.0, 0.0, 1);
        INSERT INTO samples SELECT 'panel-t', 20000 + i * 10 + c, 1000 + i * 74, printf('1950-%02d-%02d', i // 28 + 1, i % 28 + 1),
          c, CASE c WHEN 0 THEN 'Planned Transport' ELSE 'Rival Transport' END, 100000.0 + i, 300000.0 - c,
          (c + 1) * 1000.0 * (i + 1), -200000.0 + i, c + 3 FROM range(7 * 28) t(i), range(2) u(c);
    """)


class Served(unittest.TestCase):
    """A fake panel on its own port for every test."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "research" / "results").mkdir(parents=True)
        self.panel = FakePanel()
        self.thread = threading.Thread(target=self.panel.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.panel.shutdown()
        self.panel.server_close()
        self.tmp.cleanup()

    def cfg(self, months=6, start_paused=2):
        return {"months": months, "seed": 31337, "label": "test", "root": self.root, "opponent": "PrismaticAI",
                "bot_settings": "own_lines=8", "start_paused": start_paused}

    def run_duel(self, months=6, start_paused=2):
        p = duel.Panel(self.panel.port, sleep=lambda s: None)
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return duel.duel(p, self.cfg(months, start_paused), poll=0, sleep=lambda s: None)


@unittest.skipUnless(DUCKDB, "the store tests run each query with the duckdb CLI")
class StoreReads(Served):
    def setUp(self):
        super().setUp()
        self.panel.db = Db(self.root / "panel-t.duckdb")
        self.panel.bus = {"instance": "panel-t", "store": str(self.root / "panel-t.duckdb"), "schema": "prismatic-bus/v1"}
        fill(self.panel.db, self.panel.port)
        # The same planner sentence on /api/status, which a store read must not need.
        self.panel.status_events = [{"kind": "ai", "text": "planner: from /api/status"}]
        self.out = self.run_duel()
        self.rec = self.out["record"]

    def test_the_record_names_the_store_file_instance_and_run(self):
        st = self.rec["store"]
        self.assertEqual(st["file"], str(self.root / "panel-t.duckdb"))
        self.assertEqual(st["instance"], "panel-t")
        self.assertEqual(st["run"], {"instance": "panel-t", "session": "s1", "seed": 31337, "started_ms": 10000,
                                     "panel_port": self.panel.port, "mcp_port": 3, "game_port": 4}, "the newest run on this port")
        self.assertEqual(st["window_ms"], [10000, 10100 + AI_LOG_ROWS - 1])
        self.assertIsNone(self.rec["fallback"])
        self.assertEqual(set(st["queries"]), {"plan", "plan_lines", "ai_log", "held_start", "samples"})

    def test_the_plan_comes_from_the_runs_plan_events(self):
        kinds = [(e["seq"], e["kind"]) for e in self.rec["plan_events"]]
        self.assertEqual(kinds, [(3, "plan.portfolio"), (4, "plan.bot_settings")], "the older run's plan.portfolio is left out")
        self.assertEqual(self.rec["plan_events"][1]["data"]["choice"], {"rail_after": 2, "own_lines": 8})
        # The sentences for reading come from the store too (panel.ai only), newest first as before.
        self.assertEqual(self.rec["plan"], ["planner: game unpaused at tick 1400 with 2 AI company(ies) after 100 s",
                                            "planner: Coal by rail Coal Mine -> Power Station, 212.0 tiles",
                                            "planner: game paused for a repeatable start; it runs once 2 AI company(ies) exist"])

    def test_the_ai_log_is_complete_past_the_query_cap(self):
        log = self.rec["bot_log"]
        self.assertEqual(sum(len(v) for v in log.values()), AI_LOG_ROWS)
        self.assertEqual(log["0"][:2], ["line 0", "line 2"])
        self.assertEqual(log["1"][-1], f"line {AI_LOG_ROWS - 1}")
        self.assertNotIn("stale run", log["0"])
        q = self.rec["store"]["queries"]["ai_log"]
        self.assertEqual((q["rows"], q["pages"]), (AI_LOG_ROWS, AI_LOG_ROWS // QUERY_CAP + 1))
        self.assertEqual(duel.PAGE, QUERY_CAP, "the duel pages by the store's own cap")
        self.assertFalse(self.rec["bot_log_capped"])
        self.assertTrue(all(read_only(s) for s in self.panel.sql), "every statement passes the read-only guard")

    def test_the_months_are_the_samples_first_days_with_matching_dates(self):
        series = self.rec["series"]
        self.assertEqual([s["month"] for s in series], [1, 2, 3, 4, 5, 6])
        self.assertEqual([s["date"] for s in series], [f"1950-0{m}-01" for m in range(2, 8)])
        start = duel.month_index("1950-01-04")
        self.assertTrue(all(duel.month_index(s["date"]) - start == s["month"] for s in series))
        m3 = series[2]["companies"]  # 1950-04-01: sample i = 3 * 28
        self.assertEqual(m3["1"]["value"], 1000 * (3 * 28 + 1))
        self.assertEqual(m3["2"]["value"], 2000 * (3 * 28 + 1))
        self.assertEqual((m3["1"]["name"], m3["2"]["vehicles"]), ("Planned Transport", 4))
        # The fleet split is the console's reading of the same month.
        self.assertEqual((m3["1"]["trains"], m3["2"]["road"]), (3, 4))
        self.assertEqual(series[2]["console_date"], "1950-04-04")
        self.assertEqual(self.rec["final"], series[-1]["companies"])
        self.assertEqual(self.rec["missing_months"], [])
        self.assertEqual(self.rec["winner"], "B (unplanned)")

    def test_the_held_start_is_the_paused_tick(self):
        self.assertEqual(self.rec["held_start_tick"], 1400)
        self.assertEqual(self.rec["held_start"], {"tick": 1400, "paused_seq": 2, "resumed_seq": 6, "resumed_tick": 1437})

    def test_the_verdict_is_published_as_plan_duel_and_stored(self):
        self.assertEqual(len(self.panel.published), 1)
        body = self.panel.published[0]
        self.assertEqual(body["kind"], "plan.duel")
        data = body["data"]
        self.assertIsNone(refuse_external("plan.duel", data))
        self.assertEqual(data["decision"], "duel.verdict")
        choice = data["choice"]
        self.assertEqual(set(choice), {"seed", "months", "held_start_tick", "a", "b", "rival_spread", "winner"})
        self.assertEqual((choice["seed"], choice["months"], choice["held_start_tick"]), (31337, 6, 1400))
        self.assertEqual(choice["a"], {"company": 1, "name": "Planned Transport", "value": 1000 * (6 * 28 + 1),
                                       "money": 100000 + 6 * 28, "loan": 300000})
        self.assertEqual(choice["b"]["value"], 2000 * (6 * 28 + 1))
        self.assertEqual(choice["rival_spread"]["runs"], 1)
        stored = data["inputs"]["stored"]
        self.assertEqual(stored["by_query"]["ai_log"], AI_LOG_ROWS)
        self.assertEqual(stored["instance"], "panel-t")
        self.assertIn("kind = 'ai.log'", stored["query"]["ai_log"])
        params = data["version"]["params"]
        self.assertEqual(data["version"]["digest"], duel.fnv1a64(params))
        self.assertEqual(self.rec["verdict_event"]["seq"], 90001)
        self.assertTrue(self.rec["verdict_event"]["stored"], "found in the store after the publish")

    def test_the_recorded_queries_read_the_same_rows_from_the_file(self):
        # After the panel stopped, the file itself is read with the record's statements.
        for name, q in self.rec["store"]["queries"].items():
            code, a = self.panel.db.query(f"SELECT count(*) AS n FROM ({q['sql']}) AS q")
            self.assertEqual((code, a["rows"][0][0]), (200, q["rows"]), name)

    def test_the_reading_names_the_store(self):
        md = duel.markdown(self.rec, self.out["b_name"])
        self.assertIn(f"Read from the store `{self.root / 'panel-t.duckdb'}` (instance `panel-t`", md)
        self.assertIn(f"{AI_LOG_ROWS} AI log line(s)", md)
        self.assertIn("plan.duel seq 90001, on the bus and in the store", md)
        self.assertIn("| 6 | 1950-07-01 |", md)


@unittest.skipUnless(DUCKDB, "the store tests run each query with the duckdb CLI")
class StoreLostAfterTheStart(Served):
    def test_a_store_that_stops_answering_falls_back_and_says_so(self):
        self.panel.db = Db(self.root / "panel-t.duckdb")
        self.panel.bus = {"instance": "panel-t", "store": str(self.root / "panel-t.duckdb")}
        fill(self.panel.db, self.panel.port)
        self.panel.break_store_at = 4
        rec = self.run_duel(months=3)["record"]
        self.assertIsNone(rec["store"])
        self.assertTrue(rec["fallback"].startswith("the store could not be read after the duel: POST /api/db/query answered 503"))
        self.assertEqual([s["date"] for s in rec["series"]], ["1950-02-04", "1950-03-04", "1950-04-04"], "the console's months")


class Fallback(Served):
    """A panel without a store: the reads of before I0083, and the record says why."""

    def setUp(self):
        super().setUp()
        self.panel.status_events = [{"kind": "ai", "text": "planner: game unpaused at tick 1400 with 2 AI company(ies) after 9 s"},
                                    {"kind": "ai", "text": "planner: Coal by rail"}, {"kind": "server", "text": "planner? no"}]
        self.panel.bus_events = ([{"seq": i, "kind": "ai.log", "data": {"company": i % 2, "text": f"line {i}"}} for i in range(6000)]
                                 + [{"seq": 7000, "kind": "game.resumed", "data": {"tick": 1437}}])

    def test_without_the_bus_route_it_reads_as_before_and_records_it(self):
        out = self.run_duel(months=3)
        rec = out["record"]
        self.assertIsNone(rec["store"])
        self.assertTrue(rec["fallback"].startswith("GET /api/bus answered 404"), rec["fallback"])
        self.assertEqual(rec["plan"], ["planner: game unpaused at tick 1400 with 2 AI company(ies) after 9 s", "planner: Coal by rail"])
        self.assertEqual(rec["plan_events"], [])
        self.assertEqual(sum(len(v) for v in rec["bot_log"].values()), 5000)
        self.assertTrue(rec["bot_log_capped"])
        self.assertEqual([s["date"] for s in rec["series"]], ["1950-02-04", "1950-03-04", "1950-04-04"])
        self.assertEqual(rec["final"]["1"]["value"], 50003)
        # No game.paused in the buffer: the resumed tick, read from the structured event.
        self.assertEqual(rec["held_start_tick"], 1437)
        body = self.panel.published[0]
        self.assertEqual(body["data"]["inputs"]["stored"], {"fallback": rec["fallback"]})
        self.assertIsNone(refuse_external("plan.duel", body["data"]))
        self.assertEqual(rec["verdict_event"]["seq"], 90001)
        self.assertFalse(rec["verdict_event"]["stored"])
        md = duel.markdown(rec, out["b_name"])
        self.assertIn("No store: GET /api/bus answered 404", md)
        self.assertIn("(the newest 5000 bus events, cut there)", md)

    def test_the_record_holds_the_rivals_live_settings(self):
        setting = lambda n, v: {"name": n, "value": v, "default": v}
        self.panel.bots = {"running": [
            {"ai": "PrismaticAI", "id": 0, "settings": [setting("autopilot", 1), setting("own_lines", 8)]},
            {"ai": "PrismaticAI", "id": 1, "settings": [setting("autopilot", 0), setting("max_routes", 4)]}]}
        out = self.run_duel(months=1)
        rec = out["record"]
        self.assertEqual(rec["rival"]["settings"], {"autopilot": 0, "max_routes": 4}, "company id 1, not the planned bot")
        self.assertEqual(rec["rival"]["ai"], "PrismaticAI")
        self.assertEqual(rec["rival_spread"]["rival_settings"], "recorded")
        md = duel.markdown(rec, out["b_name"])
        self.assertIn("start and rival settings", md)
        self.assertIn("B ran PrismaticAI with 2 settings, autopilot=0", md)

    def test_without_the_bots_route_the_rival_is_not_recorded(self):
        out = self.run_duel(months=1)
        self.assertIsNone(out["record"]["rival"])
        self.assertEqual(out["record"]["rival_spread"]["rival_settings"], "not recorded")
        self.assertIn("(B's settings not recorded)", duel.markdown(out["record"], out["b_name"]))

    def test_a_store_that_is_off_is_named(self):
        self.panel.bus = {"instance": "panel-t", "store": "off"}
        rec = self.run_duel(months=1)["record"]
        self.assertIn("store: off", rec["fallback"])
        self.assertEqual(self.panel.sql, [], "no query is sent to a store that is off")

    def test_an_old_panel_that_refuses_plan_duel_is_recorded(self):
        global EXTERNAL
        saved, EXTERNAL = EXTERNAL, {"plan.kernel"}
        try:
            rec = self.run_duel(months=1)["record"]
        finally:
            EXTERNAL = saved
        self.assertIsNone(rec["verdict_event"]["seq"])
        self.assertIn("POST /api/bus/publish answered 400", rec["verdict_event"]["error"])

    def test_the_held_tick_falls_back_to_the_sentence_and_an_unheld_start_has_none(self):
        self.panel.bus_events = []
        self.assertEqual(self.run_duel(months=1)["record"]["held_start_tick"], 1400)
        self.panel.clock_reads = 0
        self.assertIsNone(self.run_duel(months=1, start_paused=0)["record"]["held_start_tick"])


class Verdict(unittest.TestCase):
    def test_the_digest_is_the_panels(self):
        # The same pin is in panel_events.rs a_plan_event_carries_decision_version_inputs_and_choice.
        self.assertEqual(duel.fnv1a64({"own_lines": 8, "b": 1}), "fnv1a64:846dbf03f70f43fa")
        self.assertEqual(duel.fnv1a64({"b": 1, "own_lines": 8}), duel.fnv1a64({"own_lines": 8, "b": 1}))

    def test_two_duels_ending_in_the_same_second_get_two_names(self):
        with tempfile.TemporaryDirectory() as d:
            results = Path(d) / "results"
            ticks = iter(["20261118T102030", "20261118T102030", "20261118T102031"])
            waited = []
            first = duel.claim_name(results, 31337, clock=lambda: "20261118T102030", sleep=waited.append)
            second = duel.claim_name(results, 31337, clock=lambda: next(ticks), sleep=waited.append)
            self.assertEqual((first, second), ("duel-20261118T102030-seed31337", "duel-20261118T102031-seed31337"))
            self.assertEqual(waited, [1, 1], "the taken second is waited out")
            self.assertTrue((results / f"{second}.json").exists(), "the name is claimed by its file")

    def test_plan_duel_is_declared_external_in_the_table_and_the_note(self):
        self.assertIn("plan.duel", EXTERNAL)
        self.assertIn("| `plan.duel` | external (`POST /api/bus/publish`) | `tools/research/mcp-duel:publish_verdict` |", NOTE.read_text())
        self.assertEqual(duel.VERDICT, "plan.duel")

    def test_held_start_takes_the_pause_before_the_resume_after_the_settings(self):
        rows = [{"seq": 1, "kind": "game.paused", "game_tick": 10}, {"seq": 2, "kind": "game.resumed", "game_tick": 12},
                {"seq": 3, "kind": "game.paused", "game_tick": 50}, {"seq": 4, "kind": "plan.bot_settings", "game_tick": 50},
                {"seq": 5, "kind": "game.resumed", "game_tick": 80}, {"seq": 6, "kind": "game.paused", "game_tick": 900},
                {"seq": 7, "kind": "game.resumed", "game_tick": 905}]
        self.assertEqual(duel.held_start(rows, 2), {"tick": 50, "paused_seq": 3, "resumed_seq": 5, "resumed_tick": 80})
        self.assertIsNone(duel.held_start(rows, 0))
        self.assertIsNone(duel.held_start(rows[:4], 2))

    def test_rival_spread_reads_the_same_seed_length_opponent_and_start(self):
        with tempfile.TemporaryDirectory() as d:
            results = Path(d)
            rec = lambda b, **k: {"seed": 31337, "months": 6, "opponent": "PrismaticAI", "final": {"1": {"value": 1}, "2": {"value": b}}, **k}
            files = {"duel-1-seed31337": rec(9830, held_start_tick=1400),
                     "duel-2-seed31337": rec(9000, plan=["planner: game unpaused at tick 1447 with 2 AI company(ies)"]),
                     "duel-3-seed31337": rec(20000),                       # not held
                     "duel-4-seed31337": rec(5000, months=36, held_start_tick=1),
                     "duel-5-seed4242": rec(5000, seed=4242, held_start_tick=1)}
            for n, r in files.items():
                (results / f"{n}.json").write_text(json.dumps(r))
            (results / "duel-6-seed31337.json").write_text("not json")
            out = duel.rival_spread(results, 31337, 6, "PrismaticAI", True, 9852, "duel-now")
            self.assertEqual(out["records"], ["duel-1-seed31337", "duel-2-seed31337", "duel-now"])
            self.assertEqual((out["runs"], out["min"], out["max"], out["spread"]), (3, 9000, 9852, round(852 / 9000, 4)))
            alone = duel.rival_spread(results, 31337, 6, "AdmiralAI", True, 100, "duel-now")
            self.assertEqual((alone["runs"], alone["spread"]), (1, None))

    def test_rival_spread_groups_by_the_rivals_settings(self):
        with tempfile.TemporaryDirectory() as d:
            results = Path(d)
            rec = lambda b, **k: {"seed": 31337, "months": 6, "opponent": "PrismaticAI", "held_start_tick": 1,
                                  "final": {"1": {"value": 1}, "2": {"value": b}}, **k}
            auto, buses = {"autopilot": 1, "max_routes": 4}, {"autopilot": 0, "max_routes": 4}
            files = {"duel-1-seed31337": rec(9000),                                    # B's settings not recorded
                     "duel-2-seed31337": rec(450000, rival={"ai": "PrismaticAI", "settings": auto}),
                     "duel-3-seed31337": rec(9500, rival={"ai": "PrismaticAI", "settings": buses})}
            for n, r in files.items():
                (results / f"{n}.json").write_text(json.dumps(r))
            out = duel.rival_spread(results, 31337, 6, "PrismaticAI", True, 9852, "duel-now", buses)
            self.assertEqual(out["records"], ["duel-3-seed31337", "duel-now"])
            self.assertEqual(out["rival_settings"], "recorded")
            out = duel.rival_spread(results, 31337, 6, "PrismaticAI", True, 440000, "duel-now", auto)
            self.assertEqual(out["records"], ["duel-2-seed31337", "duel-now"])
            legacy = duel.rival_spread(results, 31337, 6, "PrismaticAI", True, 9852, "duel-now")
            self.assertEqual((legacy["records"], legacy["rival_settings"]), (["duel-1-seed31337", "duel-now"], "not recorded"))

    def test_readme_scoreboard_reads_the_new_record(self):
        loader = importlib.machinery.SourceFileLoader("readme_scoreboard", str(HERE / "readme-scoreboard"))
        board = importlib.util.module_from_spec(importlib.util.spec_from_loader("readme_scoreboard", loader))
        loader.exec_module(board)
        with tempfile.TemporaryDirectory() as d:
            board.RESULTS = Path(d)
            new = {"seed": 31337, "months": 36, "label": "store", "winner": "A (planned)", "store": {"file": "x"},
                   "plan_events": [], "held_start_tick": 1400, "final": {"1": {"value": 950946}, "2": {"value": 9852}}}
            (Path(d) / "duel-20261118T120000-seed31337.json").write_text(json.dumps(new))
            runs = board.load()
            self.assertEqual([(r["a"], r["b"], r["won"]) for r in runs], [(950946, 9852, True)])


if __name__ == "__main__":
    unittest.main()
