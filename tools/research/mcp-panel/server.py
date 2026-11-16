#!/usr/bin/env python3
"""RESEARCH-ONLY control panel for OpenTTD's embedded MCP server (ADR 0008).

Starts a headless dedicated server from an MCP-enabled build, owns its console (a FIFO), starts
the MCP server and, optionally, a company AI, and serves a local control window at
http://127.0.0.1:<panel-port>/ that drives the game through MCP: live state, companies, every
resource, tools, and the approval-gated pause with an operator Approve button or auto-approve.

The bearer token lives in this process only. It is redacted from the console log the window
shows and is never written to disk. Everything binds to loopback.

Usage: server.py --bin <openttd> [--panel-port 8800] [--mcp-port 8731] [--game-port 3979]
                 [--seed 1] [--ai PrismaticAI] [--auto-approve]
"""
import argparse
import collections
import json
import queue
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
AI_DIR = Path.home() / "Documents" / "OpenTTD" / "ai"
STATIC = HERE / "static"
AI_LOG_RE = re.compile(r"\[script:\d+\] \[(\d+)\] \[([SEPWI])\] (.*)$")
AI_LEVELS = {"S": "squirrel_error", "E": "error", "P": "squirrel_info", "W": "warning", "I": "info"}
# The console's token line; its wording changed upstream of the panel (5a26a6189e), so match any
# "Bearer token (...): <hex>" line rather than one phrasing.
# The ~ console's plan verb (openttd-59): "[prismatic-plan] <n> <text>"; answered with echo lines
# carrying the same prefix, so each request is handled once by its number.
# The console's `companies` line: "#:1(Blue) Company Name: '...'  Year Founded: 1950  Money: 5694  Loan: 100000  ..."
COMPANY_ROW = re.compile(r"#:(\d+)\(.*?\)\s+Company Name: '[^']*'\s+Year Founded: \d+\s+Money: (-?\d+)\s+Loan: (\d+)")
DENIED_RE = re.compile(r"^denied #(\d+)\b")
PLAN_RE = re.compile(r"\[prismatic-plan\] (\d+) (.+)$")
TOKEN_RE = re.compile(r"Bearer token \([^)]*\): ([0-9a-fA-F]{32,})")


class Game:
    """One dedicated server, its console and its MCP endpoint."""

    def __init__(self, args):
        self.args = args
        self.tmp = Path(tempfile.mkdtemp(prefix="openttd-mcp-panel."))
        self.fifo = self.tmp / "console"
        self.proc = None
        self.console_fd = None
        self.console_lock = threading.Lock()
        self.console_last = 0.0
        self.token = None
        self.log = collections.deque(maxlen=5000)  # (seq, line); the whole story of a game session
        self.seq = 0
        self.lock = threading.Lock()
        self.events = collections.deque(maxlen=200)
        # The event bus every surface reads: REST (/api/events), SSE (/api/events/stream), Rhai.
        self.bus = collections.deque(maxlen=5000)
        self.bus_seq = 0
        self.bus_cv = threading.Condition()
        self.pending = {}  # approval id -> {"args": ..., "created": ts}
        self.auto_approve = args.auto_approve
        self.mcp_up = False
        self.ai_started = False
        self.rpc_id = 0
        self._client_tools, self._client_tools_at = [], 0.0
        self._server_tools, self._server_tools_at = [], 0.0
        self.owner_console_q = queue.Queue()
        self.plans_seen = set()  # ~ console plan request numbers already answered
        self.asks_reported = set()  # ids of answered questions already published as ask.answered
        self.view_token = None  # token file of the GUI client whose view the panel shows and drives

    # ------------------------------------------------------------------ process
    def start(self):
        os.mkfifo(self.fifo)
        cfg = self.tmp / "openttd.cfg"
        cfg.write_text(
            "[network]\nserver_name = OpenTTD MCP research server\nserver_game_type = local\n"
            f"server_port = {self.args.game_port}\nmax_companies = 15\n"
            "[difficulty]\nmax_no_competitors = 1\n"
        )
        cmd = [self.args.bin, "-D", f"127.0.0.1:{self.args.game_port}", "-x", "-g",
               "-G", str(self.args.seed), "-c", str(cfg), "-d", "net=1,script=4"]
        self.event("server", "starting " + " ".join(cmd))
        self.proc = subprocess.Popen(cmd, stdin=open(self.fifo, "r+b", buffering=0),
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.console_fd = os.open(self.fifo, os.O_WRONLY)
        threading.Thread(target=self._read_log, daemon=True).start()
        threading.Thread(target=self._bring_up, daemon=True).start()
        threading.Thread(target=self._auto_loop, daemon=True).start()
        threading.Thread(target=self._watch, daemon=True).start()
        threading.Thread(target=self._watch_news, daemon=True).start()
        threading.Thread(target=self._owner_console_loop, daemon=True).start()

    def _read_log(self):
        for raw in self.proc.stdout:
            line = raw.decode("utf-8", "replace").rstrip()
            m = TOKEN_RE.search(line)
            if m:
                self.token = m.group(1)
                line = TOKEN_RE.sub("Bearer token (held by the panel): <REDACTED>", line)
            if self.token:
                line = line.replace(self.token, "<REDACTED>")
            with self.lock:
                self.seq += 1
                self.log.append((self.seq, line))
            denied = DENIED_RE.search(line)
            if denied:
                # Withdrawn or refused in the game: no longer pending here either.
                with self.lock:
                    self.pending.pop(int(denied.group(1)), None)
            plan = PLAN_RE.search(line)
            if plan and int(plan.group(1)) not in self.plans_seen:
                self.plans_seen.add(int(plan.group(1)))
                threading.Thread(target=self._answer_plan, args=(int(plan.group(1)), plan.group(2)), daemon=True).start()
            ai = AI_LOG_RE.search(line)
            if ai:
                self.publish("ai.log", {"company": int(ai.group(1)), "level": AI_LEVELS.get(ai.group(2), ai.group(2)),
                                        "text": ai.group(3)})
            else:
                self.publish("console.line", {"line": line})
        self.event("server", f"server process exited with {self.proc.wait()}")

    def console(self, command):
        # One line at a time, 0.25 s apart: the dedicated server tests stdin with select() and reads it
        # with std::getline (src/video/dedicated_v.cpp), so lines that arrive together sit in the
        # stream buffer, invisible to select(), until the next write (seen: a plan answer's lines
        # appeared minutes late). A gap longer than one game-loop pass gives each line its own read.
        with self.console_lock:
            wait = self.console_last + 0.25 - time.time()
            if wait > 0:
                time.sleep(wait)
            os.write(self.console_fd, (command + "\n").encode())
            self.console_last = time.time()

    def to_owner_console(self, text, level="plain"):
        """Print a line in the owner's ~ console (the GUI client), coloured by level, through
        openttd.view.console.print; queued so lines keep their order and a missing client costs nothing."""
        if not self.view_token:
            return
        self.owner_console_q.put((text, level))

    def _owner_console_loop(self):
        while True:
            text, level = self.owner_console_q.get()
            try:
                self.view_call("openttd.view.console.print", {"text": text, "level": level})
            except Exception:  # noqa: BLE001 - no client, or an older one without the tool
                pass

    def _answer_plan(self, n, text):
        """Answer a sentence typed into the ~ console (`plan <text>`) through the intent resolver, into
        the same console with echo: the reading, the first candidates, and for a build its settings."""
        def say(msg):
            line = "[prismatic-plan] {} {}".format(n, msg.replace('"', "'")[:240])
            self.console('echo "{}"'.format(line))
            self.to_owner_console(line, "plan")
        exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-intent"
        if not exe.exists():
            return say("the intent resolver is not built (cargo build --release in tools/research/mcp-rhai)")
        ml = self.max_loan()
        extra = ["--params", json.dumps({"budget": ml})] if ml else []
        r = subprocess.run([str(exe), "--panel", f"http://127.0.0.1:{self.args.panel_port}", *extra, text],
                           capture_output=True, text=True, timeout=180)
        if r.returncode != 0:
            return say(f"not understood: {r.stderr.strip()[:200]}")
        out = json.loads(r.stdout)
        i, a = out["intent"], out["answer"]
        say(f"{i['action']} {i.get('ordering') or ''} {', '.join(x['name'] for x in i['subjects']) or 'anything'} ({i['language']}): {a['how']}")
        for k, c in enumerate(a["candidates"][:3], 1):
            if "from" in c:
                say(f"{k}. {c['from']['name']} ({c['from']['production']:.0f}/month) -> {c['to']['name']}, {c['cargo']}, {c['distance']:.0f} tiles")
            elif "production" in c:
                say(f"{k}. {c['name']} at ({c['x']},{c['y']}), {c['production']:.0f}/month")
            else:
                f, t = (c.get("a") or c.get("producer") or {}), (c.get("b") or c.get("acceptor") or {})
                say(f"{k}. {c.get('cargo') or 'passengers'} by {c.get('mode')}: {f.get('name')} -> {t.get('name')}, score {c.get('score')}")
        if a.get("build"):
            say(f"build: settings {json.dumps(a['build']['settings'])}; {a['build']['execution']}")
        self.event("intent", f"~ plan #{n} {text!r}: {len(a['candidates'])} candidate(s)")

    def _log_has(self, needle):
        with self.lock:
            return any(needle in line for _, line in self.log)

    def _bring_up(self):
        # The console reads stdin only once the game loop runs; repeat until it answers.
        for _ in range(90):
            if self.proc.poll() is not None:
                return
            self.console(f"prismatic_mcp start {self.args.mcp_port}")
            time.sleep(2)
            if self.token or self._log_has("already running"):
                break
        if not self.token:
            self.event("mcp", "the MCP server did not start; see the console log")
            return
        self.mcp_up = True
        self.event("mcp", f"MCP server up at {self.endpoint()} (token held by the panel)")
        # The bot plans paths in Squirrel; at the default budget (10000 opcodes a tick) a 240-tile rail
        # search ate months of game time. 250000 is the game's own maximum for this server setting;
        # scripts run on the server only, so it changes nothing in what clients compute.
        self.console("setting script.script_max_opcode_till_suspend 250000")
        if self.args.ai:
            # The bot starts with what the planner's portfolio chose (freight rail, passenger rail, air),
            # so its routes come from the one profitability model; without a plan it plays as before.
            settings = self.planned_air()
            flat = ",".join(f"{k}={v}" for k, v in settings.items())
            self.console(f"start_ai {self.args.ai} {flat}".strip())
            self.ai_started = True
            self.event("ai", f"start_ai {self.args.ai} {flat} sent: the company plays on its own".replace("  ", " "))

    def planned_air(self):
        """The bot's routes from planner v4's portfolio within the budget (difficulty.max_loan): the
        first chosen freight rail pair (fr_from, fr_to, fr_cargo), passenger rail pair (rail_a,
        rail_b) and air pair (air_a, air_b, air_planes). Buses the bot plans itself. {} without a
        planner or a portfolio."""
        exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-plan"
        if not self.args.plan_bot or not exe.exists():
            return {}
        try:
            budget = self.max_loan()
            params = {"budget": budget} if budget else {}
            # With AlgoGlyph present the best rail lines are priced on the real map (track-planning)
            # before the portfolio is chosen, so the bot builds what pays on this terrain.
            # Six lines of each list, so the bot has priced lines to fall back on.
            track = ["--track", "6"] if os.environ.get("MCP_ALGOGLYPH_DIR") else []
            r = subprocess.run([str(exe), "--panel", f"http://127.0.0.1:{self.args.panel_port}", "--limit", "10",
                                "--params", json.dumps(params), *track], capture_output=True, text=True, timeout=240)
            out = json.loads(r.stdout) if r.returncode == 0 else {}
        except Exception as e:  # noqa: BLE001 - the bot still starts
            self.event("ai", f"planner not reachable for the bot: {e}")
            return {}
        pf = out.get("portfolio") or {}
        chosen = pf.get("chosen") or []
        self.event("ai", f"planner portfolio by {pf.get('solver', 'no solver')}: {len(chosen)} investment(s), "
                         f"total score {pf.get('total_score')}, {pf.get('investment_k')}k of {pf.get('budget_k')}k")
        priced = (out.get("line_costs") or {}).get("priced") or []
        if priced:
            found = [p for p in priced if p.get("status") == "found"]
            self.event("ai", f"planner: {len(priced)} rail line(s) priced on the map by track-planning, {len(found)} with a route: "
                             + ", ".join(f"{p.get('pieces')} pieces {round(p.get('money') or 0)}" for p in found)
                             + ("" if len(found) == len(priced) else f"; {len(priced) - len(found)} without a route dropped their rail option"))
        num = lambda v: int(str(v).split(":")[-1])  # world-graph ids are "industry:12"
        settings = {}

        priced_any = bool((out.get("line_costs") or {}).get("priced"))

        def bot_can_build(opt):
            """PrismaticAI lays rail over buildable land, level crossings and bridges (rail.nut
            FindRailPath), with no tunnel. A line track-planning priced with a tunnel is not for the bot. When
            lines were priced, an unpriced one is not handed over blind either: on seed 20208 an
            unpriced passenger line needed a bridge over a river and failed. Without AlgoGlyph
            (nothing priced) every line is tried as before."""
            t = opt.get("track")
            if not t:
                return not priced_any
            n = t.get("counts") or {}
            return t.get("status") == "found" and not n.get("tunnels")

        # The portfolio first; a rail line the bot cannot build is replaced by the next priced or
        # unpriced line of its kind that it can, from the planner's lists (best score first).
        spare = sorted([c for c in (out.get("freight") or []) + ((out.get("by_mode") or {}).get("rail") or [])
                        if c not in chosen and (c.get("score") or 0) > 0], key=lambda c: -(c.get("score") or 0))
        freight_lines, freight_producers = [], set()
        for c in chosen + spare:
            mode = c.get("mode")
            opt = next((o for o in c.get("options", []) if o.get("mode") == mode), {})
            if mode in ("rail", "freight_rail") and not bot_can_build(opt):
                n = (opt.get("track") or {}).get("counts") or {}
                if c in chosen:
                    line = f"{c.get('producer', c.get('a', {})).get('name')} - {c.get('acceptor', c.get('b', {})).get('name')}"
                    why = (f"needs {n.get('tunnels', 0)} tunnel(s); the bot builds no tunnel"
                           if opt.get("track") else "was not priced on the map, so it is not handed to the bot blind")
                    self.event("ai", f"planner: the {mode} line {line} {why}; the next line it can build goes instead")
                continue
            if c not in chosen and mode not in ("rail", "freight_rail"):
                continue
            if c.get("kind") == "freight" and mode == "freight_rail" and len(freight_lines) < 4:
                # Up to four freight lines in the planner's order, one per producer (a producer's
                # cargo is counted once): fr_* is built at once, fr2_* to fr4_* when money allows.
                producer = num(c["producer"]["id"])
                if producer in freight_producers:
                    continue
                freight_producers.add(producer)
                prefix = "fr_" if not freight_lines else f"fr{len(freight_lines) + 1}_"
                freight_lines.append(prefix)
                settings.update({prefix + "from": producer, prefix + "to": num(c["acceptor"]["id"]), prefix + "cargo": int(c["cargo_id"])})
                self.event("ai", f"planner: {c['cargo']} by rail {c['producer']['name']} -> {c['acceptor']['name']}, "
                                 f"{c['distance']} tiles, profit {opt.get('monthly_profit')}/month, score {c['score']}"
                                 + ("" if prefix == "fr_" else f" (line {len(freight_lines)}, built when the money allows)"))
            elif c.get("kind") != "freight" and mode == "rail" and "rail_a" not in settings:
                settings.update(rail_a=c["a"]["id"], rail_b=c["b"]["id"])
                self.event("ai", f"planner: passengers by rail {c['a']['name']} - {c['b']['name']}, score {c['score']}")
            elif c.get("kind") != "freight" and mode == "air" and "air_a" not in settings:
                count = int(((opt.get("vehicles") or {}).get("count")) or 2)
                settings.update(air_a=c["a"]["id"], air_b=c["b"]["id"], air_planes=max(1, min(8, count)))
                self.event("ai", f"planner: air {c['a']['name']} - {c['b']['name']}, {count} plane(s), score {c['score']}")
        return settings

    def max_loan(self):
        """The most a new company can borrow (difficulty.max_loan), read from the console; None if unknown."""
        mark = self.seq
        self.console("setting difficulty.max_loan")
        time.sleep(0.6)
        with self.lock:
            lines = [line for seq, line in self.log if seq > mark]
        for line in lines:
            m = re.search(r"max_loan'? is:? '?(\d+)", line)
            if m:
                return int(m.group(1))
        return None

    def stop(self):
        try:
            if self.proc and self.proc.poll() is None:
                self.console("quit")
                self.proc.wait(timeout=5)
        except Exception:
            if self.proc:
                self.proc.kill()
        shutil.rmtree(self.tmp, ignore_errors=True)

    # ------------------------------------------------------------------ MCP
    def endpoint(self):
        return f"http://127.0.0.1:{self.args.mcp_port}/mcp"

    def rpc(self, method, params=None):
        if not self.token:
            raise RuntimeError("MCP server not up yet")
        self.rpc_id += 1
        body = {"jsonrpc": "2.0", "id": self.rpc_id, "method": method}
        if params is not None:
            body["params"] = params
        req = urllib.request.Request(self.endpoint(), data=json.dumps(body).encode(), method="POST", headers={
            "Content-Type": "application/json", "Accept": "application/json, text/event-stream",
            "Authorization": f"Bearer {self.token}"})
        try:
            with urllib.request.urlopen(req, timeout=5) as r:
                return json.loads(r.read() or b"{}")
        except urllib.error.HTTPError as e:
            return {"http_error": e.code, "body": e.read().decode("utf-8", "replace")}

    def resource(self, uri):
        r = self.rpc("resources/read", {"uri": uri})
        try:
            return json.loads(r["result"]["contents"][0]["text"])
        except (KeyError, IndexError, TypeError, ValueError):
            return {"error": r}

    def tool(self, name, arguments):
        r = self.rpc("tools/call", {"name": name, "arguments": arguments})
        try:
            out = json.loads(r["result"]["content"][0]["text"])
        except (KeyError, IndexError, TypeError, ValueError):
            return {"error": r}
        # Any tool that answers pending_approval is tracked, so the operator sees it with an Approve
        # button and the panel can resubmit the same call once it is approved.
        if isinstance(out, dict) and out.get("status") == "pending_approval" and out.get("approval_id") is not None:
            aid = int(out["approval_id"])
            if aid not in self.pending:
                self.pending[aid] = {"tool": name, "args": arguments, "created": time.time()}
                self.event("mcp", f"{name}({json.dumps(arguments)}) -> pending approval #{aid}")
        return out

    def tool_names(self):
        try:
            return {t["name"] for t in self.rpc("tools/list").get("result", {}).get("tools", [])}
        except Exception:
            return set()

    # ------------------------------------------------------------------ the owner's view (GUI client)
    def view_call(self, tool, arguments=None):
        """A tool call on the GUI client's own MCP server, from its token file (reread each call, so
        a restarted client is picked up)."""
        if not self.view_token:
            raise RuntimeError("no GUI client attached (POST /api/view/attach {token_file})")
        cfg = json.loads(Path(self.view_token).read_text())
        body = {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": tool, "arguments": arguments or {}}}
        req = urllib.request.Request(cfg["endpoint"], data=json.dumps(body).encode(), method="POST", headers={
            "Content-Type": "application/json", "Accept": "application/json, text/event-stream",
            "Authorization": f"Bearer {cfg['agent_token']}"})
        with urllib.request.urlopen(req, timeout=10) as r:
            out = json.loads(r.read())
        try:
            return json.loads(out["result"]["content"][0]["text"])
        except (KeyError, IndexError, TypeError, ValueError):
            return out

    def server_tools(self):
        """The game server's tools/list, cached for a few seconds."""
        now = time.time()
        if now - self._server_tools_at > 5 and self.mcp_up:
            try:
                self._server_tools = self.rpc("tools/list").get("result", {}).get("tools", [])
                self._server_tools_at = now
            except Exception:  # noqa: BLE001
                pass
        return self._server_tools

    def client_tools(self):
        """The attached GUI client's tools/list, cached for a few seconds; [] without a client."""
        if not self.view_token:
            return []
        now = time.time()
        if now - self._client_tools_at > 5:
            try:
                cfg = json.loads(Path(self.view_token).read_text())
                self._client_tools = mcp_post(cfg["endpoint"], cfg["agent_token"], "tools/list").get("result", {}).get("tools", [])
                self._client_tools_at = now
            except Exception:  # noqa: BLE001 - the client restarts; keep the last list
                pass
        return self._client_tools

    # ------------------------------------------------------------------ approvals
    def set_pause(self, pause):
        out = self.tool("openttd.game.set_pause", {"pause": bool(pause)})
        if out.get("status") != "pending_approval":
            self.event("mcp", f"set_pause({pause}) -> {out.get('status')} {out.get('command_result', '')}")
        return out

    def approve(self, approval_id):
        info = self.pending.pop(int(approval_id), None)
        if info is None:
            return {"error": f"no pending approval #{approval_id} known to the panel"}
        self.console(f"prismatic_mcp approve {approval_id}")
        self.event("operator", f"approved #{approval_id} in the OpenTTD console")
        time.sleep(0.5)
        out = self.tool(info["tool"], info["args"])
        self.event("mcp", f"#{approval_id} {info['tool']} resubmitted -> {out.get('status')} "
                          f"{out.get('command_result', '')} verdict={out.get('verdict', '')}")
        return out

    # ------------------------------------------------------------------ the owner's bot
    @staticmethod
    def _info_settings(text):
        """Settings as an AI's info.nut declares them, in openttd.ai.list's shape (flags, labels,
        value = the default, editable), so the panel draws one kind of control for either source."""
        labels = {}
        for name, body in re.findall(r'AddLabels\(\s*"([^"]+)"\s*,\s*\{(.*?)\}\s*\)', text, re.S):
            labels[name] = {k: v for k, v in re.findall(r'_(-?\d+)\s*=\s*"([^"]*)"', body)}
        out = []
        for block in re.findall(r"AddSetting\(\{(.*?)\}\);", text, re.S):
            def field(k, cast=str):
                v = re.search(rf"\b{k}\s*=\s*(\"[^\"]*\"|[^,\n]+)", block)
                return cast(v.group(1).strip().strip('"')) if v else None
            flags_text = field("flags") or ""
            flags = [f for f, token in (("boolean", "CONFIG_BOOLEAN"), ("in_game", "CONFIG_INGAME"), ("developer", "CONFIG_DEVELOPER")) if token in flags_text]
            name = field("name")
            default = field("custom_value", int)
            mn, mx = field("min_value", int), field("max_value", int)
            if "boolean" in flags:
                mn, mx = 0, 1
            out.append({"name": name, "description": field("description"), "min": mn, "max": mx,
                        "step": field("step_size", int) or 1, "default": default, "flags": flags,
                        "labels": labels.get(name, {}), "value": default, "editable": True,
                        "ingame": "in_game" in flags})
        return out

    def bots(self):
        """The AIs the owner can run as their representative, with their settings, and the running AI
        companies with the value each setting has now. One shape whatever the source: from
        openttd.ai.list when the game offers it (values read from the running scripts), else from
        each AI's info.nut (defaults only, and no live change possible)."""
        tools = self.tool_names()
        ais = []
        for info in sorted(AI_DIR.glob("*/info.nut")):
            text = info.read_text(errors="replace")
            m = re.search(r'GetName\(\)\s*\{\s*return\s*"([^"]+)"', text)
            ais.append({"ai": m.group(1) if m else info.parent.name, "dir": info.parent.name, "settings": self._info_settings(text)})
        companies = {c.get("id"): c for c in self.resource("openttd://companies").get("data", {}).get("companies", []) if c.get("is_ai")}
        running = []
        source = "info.nut (openttd.ai.list not in tools/list yet)"
        if "openttd.ai.list" in tools:
            data = self.tool("openttd.ai.list", {})
            data = data.get("data", data) if isinstance(data, dict) else {}
            source = "mcp:openttd.ai.list"
            # an AI installed but without an info.nut under AI_DIR still appears, with no settings to set
            known = {a["ai"] for a in ais}
            for a in data.get("available", []):
                if a.get("name") not in known:
                    ais.append({"ai": a.get("name"), "dir": None, "settings": []})
            for c in data.get("companies", []):
                base = companies.get(c.get("company"), {})
                for st in c.get("settings", []):
                    st["ingame"] = "in_game" in st.get("flags", [])
                running.append({"id": c.get("company"), "name": base.get("name", ""), "money": base.get("money"),
                                "ai": c.get("ai"), "running": c.get("running"), "settings": c.get("settings", [])})
        else:
            for cid, c in companies.items():
                running.append({"id": cid, "name": c.get("name", ""), "money": c.get("money"), "ai": None, "running": True, "settings": []})
        return {"source": source, "ais": ais, "running": running,
                "live": {"list": "openttd.ai.list" in tools, "set_setting": "openttd.ai.set_setting" in tools}}

    def bot_choices(self):
        """What a town, industry or cargo setting may name: towns from openttd://towns (by
        population), and from openttd://world/graph where the game serves it, industries with what
        they produce and accept and every cargo they name. A kind the game cannot list is empty,
        and the panel then shows a number box for it instead of a picker with nothing in it."""
        towns, industries, cargos = [], [], {}
        t = self.resource("openttd://towns")
        for n in (t.get("data", t) if isinstance(t, dict) else {}).get("towns", []) or []:
            towns.append({"id": n.get("id"), "name": n.get("name", f"town {n.get('id')}"), "population": n.get("population", 0)})
        g = self.resource("openttd://world/graph")
        g = g.get("data", g) if isinstance(g, dict) else {}
        for n in g.get("nodes", []) or []:
            kind, _, idx = str(n.get("id", "")).partition(":")
            if kind == "town" and not towns:
                towns.append({"id": int(idx), "name": n.get("name", f"town {idx}"), "population": n.get("population", 0)})
            elif kind == "industry":
                industries.append({"id": int(idx), "name": n.get("name", f"industry {idx}"),
                                   "produces": [p.get("name") for p in n.get("produces", []) or []],
                                   "accepts": [a.get("name") for a in n.get("accepts", []) or []]})
                for c in (n.get("produces", []) or []) + (n.get("accepts", []) or []):
                    if c.get("cargo") is not None:
                        cargos[int(c["cargo"])] = c.get("name", f"cargo {c['cargo']}")
        towns.sort(key=lambda x: -(x["population"] or 0))
        industries.sort(key=lambda x: x["name"])
        return {"town": towns, "industry": industries, "cargo": [{"id": k, "name": v} for k, v in sorted(cargos.items(), key=lambda kv: kv[1])]}

    def start_bot(self, ai, settings):
        flat = ",".join(f"{k}={int(v)}" for k, v in settings.items())
        if "openttd.ai.start" in self.tool_names():
            return self.tool("openttd.ai.start", {"name": ai, "settings": settings})
        self.console(f"start_ai {ai} {flat}".strip())
        self.ai_started = True
        self.event("ai", f"bot started: start_ai {ai} {flat}")
        return {"status": "started", "via": "console", "command": f"start_ai {ai} {flat}".strip()}

    def set_bot_setting(self, company, name, value):
        if "openttd.ai.set_setting" not in self.tool_names():
            return {"error": "this game binary has no openttd.ai.set_setting: a running bot's settings cannot change; start a bot with the new settings instead",
                    "fallback": "start"}
        return self.tool("openttd.ai.set_setting", {"company": int(company), "name": name, "value": int(value)})

    def _auto_loop(self):
        while True:
            time.sleep(1)
            if self.auto_approve and self.pending:
                for aid in list(self.pending):
                    self.approve(aid)

    def event(self, kind, text):
        self.events.appendleft({"t": time.strftime("%H:%M:%S"), "kind": kind, "text": text})
        self.publish(f"panel.{kind}", {"text": text})

    def publish(self, kind, data):
        with self.bus_cv:
            self.bus_seq += 1
            self.bus.append({"seq": self.bus_seq, "time": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                             "kind": kind, "data": data})
            self.bus_cv.notify_all()

    def events_since(self, since, kinds=None, limit=500):
        with self.bus_cv:
            out = [e for e in self.bus if e["seq"] > since
                   and (not kinds or any(e["kind"] == k or e["kind"].startswith(k + ".") for k in kinds))]
        return out[:limit]

    def ask_answers(self):
        """The game window's questions and answers. Every answer not yet reported is published as
        ask.answered here, so a reader of GET /api/ask and the watcher see the same events."""
        out = self.view_call("openttd.view.ask.answers")
        with self.lock:
            fresh = [a for a in out.get("asks", []) if a.get("answer") is not None and a["id"] not in self.asks_reported]
            self.asks_reported.update(a["id"] for a in fresh)
        for a in fresh:
            self.publish("ask.answered", a)
        return out

    def _watch_news(self):
        """The news the owner's window shows, as events news.shown: what the game reports to the player
        (new companies, first arrivals, subsidies, ...) and what any MCP caller posted to them."""
        seen = None
        while True:
            time.sleep(2)
            if not self.view_token:
                continue
            try:
                news = self.view_call("openttd.view.news.list", {"limit": 30}).get("news", [])
            except Exception:  # noqa: BLE001 - the client restarts; try again next round
                continue
            try:
                self.ask_answers()
            except Exception:  # noqa: BLE001
                pass
            keys = [(n["date"], n["type"], n["text"]) for n in news]
            if seen is None:
                seen = set(keys)  # what was there before the panel attached is history, not news
                continue
            for n, k in zip(reversed(news), reversed(keys)):
                if k not in seen:
                    seen.add(k)
                    self.publish("news.shown", n)

    def _watch(self):
        """Derive game events from what MCP reports, by difference between two reads."""
        prev = None
        while True:
            time.sleep(1)
            if not self.mcp_up:
                continue
            try:
                st = self.resource("openttd://game/state")
                cs = self.resource("openttd://companies")
                ds = self.resource("openttd://decisions")
            except Exception:
                continue
            if "data" not in st:
                continue
            cur = {
                "date": st.get("game_date"), "paused": st["data"].get("paused"),
                "vehicles": st["data"].get("num_vehicles"), "towns": st["data"].get("num_towns"),
                "companies": {c["id"]: c for c in cs.get("data", {}).get("companies", [])},
                "decisions": {d["decision_id"]: d for d in ds.get("data", {}).get("decisions", [])},
                "tick": st.get("game_tick"),
            }
            if prev is None:
                self.publish("game.observed", {"date": cur["date"], "tick": cur["tick"], "vehicles": cur["vehicles"],
                                               "companies": len(cur["companies"]), "towns": cur["towns"]})
                for c in cur["companies"].values():
                    self.publish("company.observed", c)
            else:
                if cur["tick"] is not None and prev["tick"] is not None and cur["tick"] < prev["tick"]:
                    self.publish("game.new_world", {"date": cur["date"], "towns": cur["towns"]})
                if (cur["date"] or "")[:7] != (prev["date"] or "")[:7]:
                    self.publish("game.month", {"date": cur["date"], "vehicles": cur["vehicles"],
                                                "companies": {i: {"money": c.get("money"), "value": c.get("company_value")}
                                                              for i, c in cur["companies"].items()}})
                if cur["paused"] != prev["paused"]:
                    self.publish("game.paused" if cur["paused"] else "game.resumed",
                                 {"date": cur["date"], "tick": cur["tick"]})
                if cur["vehicles"] != prev["vehicles"]:
                    self.publish("vehicles.count", {"from": prev["vehicles"], "to": cur["vehicles"], "date": cur["date"]})
                for i in cur["companies"].keys() - prev["companies"].keys():
                    self.publish("company.founded", cur["companies"][i])
                for i in prev["companies"].keys() - cur["companies"].keys():
                    self.publish("company.removed", prev["companies"][i])
                for i in cur["companies"].keys() & prev["companies"].keys():
                    a, b = prev["companies"][i], cur["companies"][i]
                    if a.get("name") != b.get("name") and b.get("name"):
                        self.publish("company.renamed", {"id": i, "from": a.get("name"), "to": b.get("name")})
                for i in cur["decisions"].keys() - prev["decisions"].keys():
                    self.publish("decision.recorded", cur["decisions"][i])
                for i in cur["decisions"].keys() & prev["decisions"].keys():
                    if cur["decisions"][i] != prev["decisions"][i]:
                        self.publish("decision.updated", cur["decisions"][i])
            prev = cur


OBJ = {"type": "object"}


# ---------------------------------------------------------------------- discovery (cluster)
# Every panel and every GUI client it attaches registers here; any panel lists the whole cluster
# and checks each entry with a live probe. One JSON file per instance, written atomically, 0600,
# removed when its panel stops; an entry whose owning process is gone is dropped on discovery.
REGISTRY = Path(os.environ.get("PRISMATIC_MCP_REGISTRY") or Path.home() / ".local/state/prismatic-mcp/instances")


def registry_write(name, entry):
    REGISTRY.mkdir(parents=True, exist_ok=True)
    entry = {"name": name, "owner_pid": os.getpid(), "registered": time.time(), **entry}
    tmp = REGISTRY / f".{name}.tmp"
    tmp.write_text(json.dumps(entry))
    os.chmod(tmp, 0o600)
    os.replace(tmp, REGISTRY / f"{name}.json")


def registry_remove(name):
    try:
        (REGISTRY / f"{name}.json").unlink()
    except FileNotFoundError:
        pass


def mcp_post(endpoint, token, method, params=None, timeout=3):
    body = {"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}
    req = urllib.request.Request(endpoint, data=json.dumps(body).encode(), method="POST", headers={
        "Content-Type": "application/json", "Accept": "application/json, text/event-stream",
        "Authorization": f"Bearer {token}"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())


def discover():
    out = []
    for f in sorted(REGISTRY.glob("*.json")) if REGISTRY.is_dir() else []:
        try:
            e = json.loads(f.read_text())
        except (OSError, ValueError):
            continue
        try:
            os.kill(int(e.get("owner_pid", 0)), 0)
        except (OSError, ValueError):
            f.unlink(missing_ok=True)  # its panel is gone
            continue
        e["alive"] = False
        try:
            if e.get("kind") == "panel":
                with urllib.request.urlopen(e["url"] + "/api/status", timeout=2) as r:
                    st = json.loads(r.read())
                e.update(alive=True, mcp_up=st.get("mcp_up"), ai=st.get("ai"), game_port=st.get("game_port"))
            else:
                cfg = json.loads(Path(e["token_file"]).read_text())
                e["endpoint"] = cfg["endpoint"]
                e["alive"] = "result" in mcp_post(cfg["endpoint"], cfg["agent_token"], "ping", timeout=2)
        except Exception as ex:  # noqa: BLE001 - a probe failure is the answer
            e["error"] = str(ex)
        out.append(e)
    return out


def instance_call(name, method, params):
    e = next((x for x in discover() if x["name"] == name), None)
    if e is None:
        return 404, {"error": f"no instance {name}"}
    if e.get("kind") == "panel":
        req = urllib.request.Request(e["url"] + "/api/rpc", method="POST", headers={"Content-Type": "application/json"},
                                     data=json.dumps({"method": method, "params": params or {}}).encode())
        with urllib.request.urlopen(req, timeout=10) as r:
            return 200, json.loads(r.read())
    cfg = json.loads(Path(e["token_file"]).read_text())
    return 200, mcp_post(cfg["endpoint"], cfg["agent_token"], method, params, timeout=10)


def window_classes():
    """WindowClass numbers to names, derived from src/window_type.h (aliases kept as extra names), so
    the windows openttd.ui.windows reports by number get the names the source gives them."""
    src = HERE.parents[2] / "src" / "window_type.h"
    body = src.read_text().split("enum class WindowClass", 1)[1].split("\n};", 1)[0]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    names, value, out = {}, -1, {}
    for m in re.finditer(r"\b([A-Z][A-Za-z0-9]*)\s*(?:=\s*([^,]+))?,", body):
        name, expr = m.group(1), (m.group(2) or "").strip()
        if expr.startswith("WindowClass::"):
            v = names[expr.split("::", 1)[1]]
        elif expr:
            v = int(expr, 0)
        else:
            v = value + 1
        value = v
        names[name] = v
        out.setdefault(v, []).append(name)
    return {str(v): {"name": ns[-1] if len(ns) > 1 else ns[0], "aliases": ns} for v, ns in sorted(out.items())}


APPROVAL_LINE = re.compile(r"^\s*(prismatic_mcp|mcp)\s+(approve|auto_approve|auto-approve)\b", re.I)


def make_routes(game):
    """The panel's own HTTP surface, declared once: the router dispatches from this table and
    /openapi.json documents it. Routes derived from the live MCP catalogue are added at request
    time (derived_routes), from the same tools/list and resources/list every other surface reads."""

    def console(body):
        cmd = str(body.get("command", "")).strip()
        if not cmd or "\n" in cmd:
            return 400, {"error": "one non-empty line"}
        if APPROVAL_LINE.match(cmd):
            # Approval is the owner's act: through /api/approve (the panel's Approve button), never a
            # console line any caller (an agent included) could send. Withdrawing (deny) stays open.
            game.event("operator", f"refused console line: {cmd}")
            return 403, {"error": "approvals go through POST /api/approve (the owner's button), not the console"}
        with game.lock:
            mark = game.seq
        game.console(cmd)
        game.event("console", cmd)
        time.sleep(float(body.get("wait", 0.6)))
        with game.lock:
            lines = [l for n, l in game.log if n > mark]
        return 200, {"sent": cmd, "output": lines}

    def rhai(body):
        exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-rhai"
        if not exe.exists():
            return 503, {"error": "build it: cargo build --release in tools/research/mcp-rhai"}
        r = subprocess.run([str(exe), "--panel", f"http://127.0.0.1:{game.args.panel_port}",
                            "-e", str(body.get("script", ""))], capture_output=True, text=True,
                           timeout=float(body.get("timeout", 120)))
        game.event("rhai", (body.get("script", "")[:80]) + f" -> exit {r.returncode}")
        for line in r.stdout.splitlines():
            game.to_owner_console(f"[rhai] {line}", "rhai")
        for line in r.stderr.splitlines():
            game.to_owner_console(f"[rhai] {line}", "error")
        return 200, {"exit": r.returncode, "output": (r.stdout + r.stderr).rstrip()}

    def auto(body):
        game.auto_approve = bool(body.get("on"))
        game.event("operator", f"auto-approve {'ON' if game.auto_approve else 'OFF'}")
        return 200, {"auto_approve": game.auto_approve}

    def start_ai(body):
        name = str(body.get("ai") or game.args.ai)
        settings = str(body.get("settings", "")).strip()
        game.console(f"start_ai {name} {settings}".strip())
        game.ai_started = True
        game.event("ai", f"start_ai {name} {settings}".strip())
        return 200, {"ok": True}

    def view_attach(body):
        f = Path(str(body.get("token_file", "")))
        if not f.is_file():
            return 400, {"error": f"no token file at {f}"}
        game.view_token = str(f)
        registry_write(f"client-{game.args.panel_port}", {"kind": "client", "role": "GUI client (owner view)",
                                                          "token_file": str(f), "panel": f"http://127.0.0.1:{game.args.panel_port}"})
        game.event("view", f"GUI client attached ({f.name})")
        return 200, {"attached": True, "state": game.view_call("openttd.view.state")}

    def action_plan(body):
        d = os.environ.get("MCP_ALGOGLYPH_DIR")
        if not d:
            return 503, {"error": "no AlgoGlyph runner: set MCP_ALGOGLYPH_DIR (mcp-e2e finds one)"}
        inp = dict(body)
        if "cash" not in inp or "loan" not in inp:
            # Money and loan from the console's `companies` line (openttd://companies has no loan on
            # every build): the requested company, else the first AI company.
            mark = game.seq
            game.console("companies")
            time.sleep(0.8)
            with game.lock:
                lines = [ln for sq, ln in game.log if sq > mark]
            rows = [m for ln in lines for m in COMPANY_ROW.findall(ln)]
            want = inp.get("company")
            row = next((m for m in rows if want is not None and int(m[0]) - 1 == want), None) or (rows[0] if rows else None)
            if row:
                inp.setdefault("cash", int(row[1]))
                inp.setdefault("loan", int(row[2]))
        inp.setdefault("max_loan", game.max_loan() or 300000)
        inp.setdefault("loan_step", 10000)
        inp.pop("company", None)
        # The answer goes to a file, not a pipe: the flow CLI ends with process.exit(), which drops
        # what a pipe has not taken yet (an answer above 64 KiB arrived cut off, 2026-11-16).
        import tempfile
        with tempfile.TemporaryFile() as answer:
            r = subprocess.run(["npm", "run", "--silent", "flow", "--", "kernel", "action-plan", "-"], cwd=d,
                               input=json.dumps(inp).encode(), stdout=answer, stderr=subprocess.PIPE, timeout=120)
            answer.seek(0)
            text = answer.read().decode()
        try:
            out = json.loads(text)
        except ValueError:
            return 502, {"error": (r.stderr.decode() or text).strip()[:400]}
        out["input"] = inp
        out["runner"] = d
        game.event("plan", f"action plan: {len(inp.get('actions', []))} action(s) -> "
                           + (f"{len(out.get('result', {}).get('steps', []))} step(s)" if out.get("ok") else f"refused: {out.get('errors')}"))
        return (200 if out.get("ok") else 422), out

    def intent(text, params=None):
        exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-intent"
        if not exe.exists():
            return 503, {"error": "build it: cargo build --release in tools/research/mcp-rhai"}
        # The planner's portfolio needs a budget to solve: the caller's, else what a company may borrow.
        params = dict(params or {})
        if "budget" not in params:
            ml = game.max_loan()
            if ml:
                params["budget"] = ml
        extra = ["--params", json.dumps(params)] if params else []
        r = subprocess.run([str(exe), "--panel", f"http://127.0.0.1:{game.args.panel_port}", *extra, text],
                           capture_output=True, text=True, timeout=180)
        if r.returncode != 0:
            return (400 if r.returncode == 2 else 502), {"error": r.stderr.strip()}
        out = json.loads(r.stdout)
        game.event("intent", f"{text!r} -> {out['intent']['action']} {out['intent'].get('ordering')} "
                             f"{[x['name'] for x in out['intent']['subjects']]}: {len(out['answer']['candidates'])} candidate(s)")
        return 200, out

    def plan(limit, params="", track=0):
        exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-plan"
        if not exe.exists():
            return 503, {"error": "build it: cargo build --release in tools/research/mcp-rhai"}
        extra = ["--params", params] if params else []
        if track > 0:
            if not os.environ.get("MCP_ALGOGLYPH_DIR"):
                return 503, {"error": "track=N needs AlgoGlyph track-planning: MCP_ALGOGLYPH_DIR is not set"}
            extra += ["--track", str(min(track, 10))]
        r = subprocess.run([str(exe), "--panel", f"http://127.0.0.1:{game.args.panel_port}", "--limit", str(max(1, min(limit, 500))), *extra],
                           capture_output=True, text=True, timeout=60 + 60 * min(track, 10))
        if r.returncode != 0:
            return (400 if r.returncode == 2 else 502), {"error": r.stderr.strip()}
        return 200, json.loads(r.stdout)

    def investment(params=""):
        """What to build, decided by AlgoGlyph's transport-investment kernel over the planner's own
        candidates (one kernel run per cargo, then the portfolio knapsack): mcp-plan --invest."""
        exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-plan"
        if not exe.exists():
            return 503, {"error": "build it: cargo build --release in tools/research/mcp-rhai"}
        if not os.environ.get("MCP_ALGOGLYPH_DIR"):
            return 503, {"error": "MCP_ALGOGLYPH_DIR is not set (mcp-e2e sets it when it finds an AlgoGlyph checkout)"}
        try:
            p = json.loads(params) if params else {}
        except ValueError as e:
            return 400, {"error": f"params is not JSON: {e}"}
        if not isinstance(p, dict):
            return 400, {"error": "params must be a JSON object"}
        if "budget" not in p and game.max_loan():
            p["budget"] = game.max_loan()
        r = subprocess.run([str(exe), "--panel", f"http://127.0.0.1:{game.args.panel_port}", "--invest", "--params", json.dumps(p)],
                           capture_output=True, text=True, timeout=240)
        if r.returncode != 0:
            return (400 if r.returncode == 2 else 502), {"error": r.stderr.strip()}
        out = json.loads(r.stdout)
        inv = out.get("investment", {})
        chosen = inv.get("portfolio", {}).get("chosen", [])
        game.event("plan", f"investment: {len(chosen)} chosen of {inv.get('candidates')} by {inv.get('portfolio', {}).get('solver')}: "
                           + ", ".join(f"{c.get('cargo')} {c.get('mode')} roi {c.get('roi') or 0:.2f}" for c in chosen))
        return 200, {"investment": inv, "planner_portfolio": out.get("portfolio"), "params": out.get("params")}

    def graph():
        exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-graph"
        if not exe.exists():
            return 503, {"error": "build it: cargo build --release in tools/research/mcp-rhai"}
        r = subprocess.run([str(exe), "--panel", f"http://127.0.0.1:{game.args.panel_port}"],
                           capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            return 502, {"error": r.stderr.strip()}
        return 200, json.loads(r.stdout)

    def status(q):
        return 200, {
            "server_running": game.proc is not None and game.proc.poll() is None,
            "mcp_up": game.mcp_up, "endpoint": game.endpoint(), "ai": game.args.ai,
            "ai_started": game.ai_started, "auto_approve": game.auto_approve,
            "pending": [{"id": k, **v} for k, v in game.pending.items()],
            "game_port": game.args.game_port, "events": list(game.events), "event_seq": game.bus_seq}

    def events(q):
        since = int(q.get("since", ["0"])[0])
        kinds = [k for k in q.get("kinds", [""])[0].split(",") if k]
        return 200, {"events": game.events_since(since, kinds, int(q.get("limit", ["500"])[0])),
                     "last_seq": game.bus_seq}

    def resource_by_uri(q):
        return 200, game.resource(q.get("uri", [""])[0])

    def log(q):
        with game.lock:
            return 200, {"lines": [l for _, l in list(game.log)[-150:]]}

    def overview(q):
        return 200, {"state": game.resource("openttd://game/state"),
                     "companies": game.resource("openttd://companies"),
                     "decisions": game.resource("openttd://decisions")}

    def catalogue(q):
        return 200, {"resources": game.rpc("resources/list"), "tools": game.rpc("tools/list"),
                     "initialize": game.rpc("initialize", {
                         "protocolVersion": "2025-11-25", "capabilities": {},
                         "clientInfo": {"name": "openttd-mcp-panel", "version": "1"}})}

    s = lambda props, req=(): {"type": "object", "properties": props, "required": list(req)}
    # (method, path, operationId, summary, needs MCP, request schema, handler)
    return [
        ("GET", "/api/status", "status", "Panel, server and MCP status; pending approvals; recent events", False, None, status),
        ("GET", "/api/log", "log", "The server console log (token redacted)", False, None, log),
        ("GET", "/api/events", "events", "Events after ?since=<seq>, optionally ?kinds=game,company,... ; SSE: /api/events/stream", False, None, events),
        ("GET", "/api/overview", "overview", "Game state, companies and decisions in one read", True, None, overview),
        ("GET", "/api/catalogue", "catalogue", "MCP initialize, resources/list and tools/list", True, None, catalogue),
        ("GET", "/api/resource", "readResource", "Read one MCP resource by ?uri=", True, None, resource_by_uri),
        ("POST", "/api/rpc", "rpc", "Raw MCP JSON-RPC with the panel's token", True,
         s({"method": {"type": "string"}, "params": OBJ}, ["method"]), lambda b: (200, game.rpc(b["method"], b.get("params")))),
        ("POST", "/api/console", "console", "One OpenTTD console line as the operator; returns its output", False,
         s({"command": {"type": "string"}, "wait": {"type": "number"}}, ["command"]), console),
        ("POST", "/api/pause", "pause", "Pause or resume through the approval-gated MCP tool", True,
         s({"pause": {"type": "boolean"}}, ["pause"]), lambda b: (200, game.set_pause(b.get("pause", True)))),
        ("POST", "/api/approve", "approve", "The operator's approval of a pending decision, then its resubmission", True,
         s({"id": {"type": "integer"}}, ["id"]), lambda b: (200, game.approve(b["id"]))),
        ("POST", "/api/auto-approve", "autoApprove", "The owner's full-auto switch", False,
         s({"on": {"type": "boolean"}}, ["on"]), auto),
        ("POST", "/api/start-ai", "startAi", "Start a company AI with an optional settings string", False,
         s({"ai": {"type": "string"}, "settings": {"type": "string"}}), start_ai),
        ("GET", "/api/bots", "bots", "The AIs the owner can run as their bot, their settings (from openttd.ai.list, else info.nut) and the running AI companies", True,
         None, lambda q: (200, game.bots())),
        ("GET", "/api/bots/choices", "botChoices", "What a bot setting naming a town, industry or cargo may name, from openttd://world/graph (towns by population, industries with their cargos, every cargo)", True,
         None, lambda q: (200, game.bot_choices())),
        ("POST", "/api/bots/start", "botStart", "Start the owner's bot: an AI with a settings map", False,
         s({"ai": {"type": "string"}, "settings": OBJ}, ["ai"]), lambda b: (200, game.start_bot(b["ai"], b.get("settings", {})))),
        ("POST", "/api/bots/setting", "botSetting", "Change one setting of a running bot (openttd.ai.set_setting, approval-gated)", True,
         s({"company": {"type": "integer"}, "name": {"type": "string"}, "value": {"type": "integer"}}, ["company", "name", "value"]),
         lambda b: (200, game.set_bot_setting(b["company"], b["name"], b["value"]))),
        ("GET", "/api/plan", "plan", "A route plan: town pairs ranked by model v2 from the game's payment formula, vehicle speeds and base prices (tools/research/mcp-rhai src/plan.rs), each with its mode of transport (bus, rail, air), plus the best pairs of each mode; ?limit=N&params=JSON overrides any assumption (the answer echoes the effective params); &track=N prices the N best rail lines of each list on the real map with AlgoGlyph track-planning (line_costs) before the portfolio is chosen", True,
         None, lambda q: plan(int(q.get("limit", ["20"])[0]), q.get("params", [""])[0], int(q.get("track", ["0"])[0] or 0))),
        ("GET", "/api/investment", "investment", "What to build, decided by AlgoGlyph's transport-investment kernel (routes of one or two legs through hubs, ranked by return on investment) over the planner's candidates from the live game, one run per cargo with that cargo's own vehicles, then one portfolio within the budget (default: the loan limit); ?params=JSON as /api/plan. The planner's own portfolio comes alongside for comparison", True,
         None, lambda q: investment(q.get("params", [""])[0])),
        ("POST", "/api/actionplan", "actionPlan", "Plan before acting: a set of actions with dependencies ({id, kind, label?, cost?, income?, amount?+step?, repeat?, after?}) turned by AlgoGlyph's action-plan kernel into a validated DAG and executable presses (repay 50k = 5 x repay 10k as one plan) with the cash and loan after each, refused if it would overdraw; cash, loan and loan limits default to the bot company's and the game's", True,
         s({"actions": {"type": "array"}, "cash": {"type": "number"}, "loan": {"type": "number"}, "max_loan": {"type": "number"},
            "loan_step": {"type": "number"}, "company": {"type": "integer"}}, ["actions"]), lambda b: action_plan(b)),
        ("POST", "/api/intent", "intent", "A sentence from the owner (\"najdi nejbližší les + pilu\", \"vybuduj nejlukrativnější trať\", \"find the most productive coal mine\") answered over the live game: nouns from the game's own language files and world graph, orderings nearest / most productive / most profitable, candidates in order, and for build the bot settings that carry out the first (tools/research/mcp-rhai src/intent.rs)", True,
         s({"text": {"type": "string"}, "params": OBJ}, ["text"]), lambda b: intent(b["text"], b.get("params"))),
        ("GET", "/api/graph", "graph", "The road network as a graph (nodes: junctions, ends, stations, town anchors; edges with length; components), derived from openttd://map, towns and stations by tools/research/mcp-rhai mcp-graph", True,
         None, lambda q: graph()),
        ("GET", "/api/instances", "instances", "Every OpenTTD MCP instance in the cluster (panels and GUI clients) from the shared registry, each checked with a live probe", True,
         None, lambda q: (200, {"registry": str(REGISTRY), "instances": discover()})),
        ("POST", "/api/instances/call", "instancesCall", "One MCP JSON-RPC call on a named instance of the cluster (a panel's server or a GUI client)", True,
         s({"name": {"type": "string"}, "method": {"type": "string"}, "params": OBJ}, ["name", "method"]),
         lambda b: instance_call(b["name"], b["method"], b.get("params"))),
        ("GET", "/api/news", "news", "The news the owner's game window has shown, newest first (openttd.view.news.list on the GUI client); ?limit=N", True,
         None, lambda q: (200, game.view_call("openttd.view.news.list", {"limit": int(q.get("limit", ["20"])[0])}))),
        ("POST", "/api/news", "newsShow", "Show a news message in the owner's game window, from the bot, a script or another AI (openttd.view.news.show): text, optional style normal|thin|small|company, a tile, x/y, town or vehicle for its viewport, or a company (and title) for the manager's face", True,
         s({"text": {"type": "string"}, "style": {"type": "string", "enum": ["normal", "thin", "small", "company"]}, "company": {"type": "integer"},
            "title": {"type": "string", "enum": ["none", "launch", "trouble", "merger", "bankrupt"]}, "tile": {"type": "integer"},
            "x": {"type": "integer"}, "y": {"type": "integer"}, "town": {"type": "integer"}, "vehicle": {"type": "integer"}}, ["text"]),
         lambda b: (200, game.view_call("openttd.view.news.show", b))),
        ("GET", "/api/ui/classes", "uiClasses", "Window class numbers to names, derived from src/window_type.h (for openttd.ui.windows)", False,
         None, lambda q: (200, window_classes())),
        ("POST", "/api/ask", "ask", "Ask the player a yes/no question in the game window (openttd.view.ask); the answer arrives as event ask.answered and in GET /api/ask: yes, no (also a closed dialog), or replaced by a newer question", True,
         s({"question": {"type": "string"}, "caption": {"type": "string"}}, ["question"]), lambda b: (200, game.view_call("openttd.view.ask", b))),
        ("GET", "/api/ask", "askAnswers", "Every question asked in the game window, newest first, with its answer (yes, no, replaced, or null while open)", True,
         None, lambda q: (200, game.ask_answers())),
        ("POST", "/api/news/close", "newsClose", "Close the news window in the owner's game", True,
         None, lambda b: (200, game.view_call("openttd.view.news.close"))),
        ("POST", "/api/view/attach", "viewAttach", "Attach the GUI client the owner watches, by its MCP token file", False,
         s({"token_file": {"type": "string"}}, ["token_file"]), view_attach),
        ("POST", "/api/view/tool", "viewTool", "Call a tool on the attached GUI client's own MCP server (openttd.view.* and the rest)", False,
         s({"name": {"type": "string"}, "arguments": OBJ}, ["name"]), lambda b: (200, game.view_call(b["name"], b.get("arguments")))),
        ("POST", "/api/rhai", "rhai", "Run a Rhai script against this panel (tools/research/mcp-rhai)", False,
         s({"script": {"type": "string"}, "timeout": {"type": "number"}}, ["script"]), rhai),
    ]


def derived_routes(game):
    """One REST route per MCP tool (POST /api/tools/<name>, the tool's inputSchema as the body) and per
    MCP resource (GET /api/resources/<path of openttd://...>), from the live catalogue."""
    tools = game.rpc("tools/list").get("result", {}).get("tools", [])
    resources = game.rpc("resources/list").get("result", {}).get("resources", [])
    out = []
    for t in tools:
        out.append(("POST", f"/api/tools/{t['name']}", "tool_" + re.sub(r"\W", "_", t["name"]),
                    t.get("description", t["name"]), t.get("inputSchema") or OBJ, t))
    # The attached GUI client's own tools (openttd.ui.*, openttd.input.*, openttd.view.* and whatever it
    # gains), one route each, from its live tools/list: the owner's window is a surface like the server.
    for t in game.client_tools():
        out.append(("POST", f"/api/client/{t['name']}", "client_" + re.sub(r"\W", "_", t["name"]),
                    "GUI client: " + t.get("description", t["name"]), t.get("inputSchema") or OBJ, t))
    for r in resources:
        uri = r["uri"]
        path = uri.split("://", 1)[1] if "://" in uri else uri
        out.append(("GET", f"/api/resources/{path}", "resource_" + re.sub(r"\W", "_", path),
                    r.get("description") or r.get("name") or uri, None, r))
    return out


def rpc_status(out):
    """The HTTP status of an MCP answer the panel passes on: 200 for content, 404 when the server does
    not know the resource or tool (JSON-RPC -32602, e.g. a resource this build does not have), 502 for
    any other JSON-RPC error. A tool's own refusal inside its content stays 200 with its body."""
    err = out.get("error") if isinstance(out, dict) else None
    rpc = err.get("error") if isinstance(err, dict) and "jsonrpc" in err else None
    if not isinstance(rpc, dict):
        return 200
    return 404 if rpc.get("code") == -32602 else 502


def openapi(game, routes):
    paths = {}

    def op(method, path, op_id, summary, schema, tag):
        o = {"operationId": op_id, "summary": summary, "tags": [tag],
             "responses": {"200": {"description": "OK", "content": {"application/json": {"schema": OBJ}}}}}
        if schema is not None:
            o["requestBody"] = {"required": True, "content": {"application/json": {"schema": schema}}}
        query = {"/api/events": ("since", "kinds", "limit"), "/api/resource": ("uri",), "/api/news": ("limit",),
                 "/api/plan": ("limit", "params", "track"), "/api/investment": ("params",)}.get(path) if method == "GET" else None
        if query:
            o["parameters"] = [{"name": n, "in": "query", "schema": {"type": "string"}} for n in query]
        paths.setdefault(path, {})[method.lower()] = o

    for method, path, op_id, summary, _mcp, schema, _h in routes:
        op(method, path, op_id, summary, schema, "panel")
    paths["/api/rhai/stream"] = {"post": {"operationId": "rhaiStream", "tags": ["panel"],
                                          "summary": "Run a Rhai script and stream its output line by line (SSE: out, err, exit)",
                                          "requestBody": {"required": True, "content": {"application/json": {"schema": {
                                              "type": "object", "properties": {"script": {"type": "string"}}, "required": ["script"]}}}},
                                          "responses": {"200": {"description": "text/event-stream"}}}}
    paths["/api/view/screenshot"] = {"get": {"operationId": "viewScreenshot", "tags": ["view"],
                                             "summary": "What the attached GUI client shows right now, as PNG",
                                             "responses": {"200": {"description": "image/png"}}}}
    paths["/api/events/stream"] = {"get": {"operationId": "eventStream", "tags": ["events"],
                                           "summary": "Server-sent events: every event as it is published (?since=, ?kinds=)",
                                           "responses": {"200": {"description": "text/event-stream"}}}}
    derived = []
    if game.mcp_up:
        try:
            derived = derived_routes(game)
        except Exception:
            derived = []
    for method, path, op_id, summary, schema, item in derived:
        tag = "gui client tools" if path.startswith("/api/client/") else ("mcp tools" if method == "POST" else "mcp resources")
        op(method, path, op_id, summary, schema, tag)
        if isinstance(item, dict) and item.get("annotations"):
            paths[path][method.lower()]["x-mcp-annotations"] = item["annotations"]
    return {"openapi": "3.1.0",
            "info": {"title": "OpenTTD MCP panel (RESEARCH-ONLY)", "version": "1",
                     "description": "Declared once in tools/research/mcp-panel/server.py and derived from the live "
                                    "MCP catalogues: every MCP tool and resource of the running game, and every tool "
                                    "of the attached GUI client (/api/client/...), is a route here."},
            "servers": [{"url": f"http://127.0.0.1:{game.args.panel_port}"}], "paths": paths}


SWAGGER_HTML = """<!doctype html><html lang="en"><head><meta charset="utf-8"><title>OpenTTD MCP API</title>
<link rel="stylesheet" href="/vendor/swagger-ui/swagger-ui.css"></head><body>
<div id="swagger"></div><script src="/vendor/swagger-ui/swagger-ui-bundle.js"></script>
<script>SwaggerUIBundle({url: '/openapi.json', dom_id: '#swagger', deepLinking: true});</script></body></html>"""

VENDOR = {
    "/vendor/flowbite.min.js": ("flowbite/dist/flowbite.min.js", "text/javascript"),
    "/vendor/echarts.min.js": ("echarts/dist/echarts.min.js", "text/javascript"),
    "/vendor/swagger-ui/swagger-ui.css": ("swagger-ui-dist/swagger-ui.css", "text/css"),
    "/vendor/swagger-ui/swagger-ui-bundle.js": ("swagger-ui-dist/swagger-ui-bundle.js", "text/javascript"),
}


JSON_TYPES = {"string": str, "integer": int, "number": (int, float), "boolean": bool, "object": dict, "array": list}


def body_errors(schema, body):
    """What is wrong with a request body against its declared JSON schema: not an object, a required
    field missing, a field of the wrong type, a value outside its enum. [] when it fits. One check for
    every POST route, declared or derived, so a bad call answers 400 and never reaches a handler."""
    if not schema:
        return []
    if not isinstance(body, dict):
        return ["the body must be a JSON object"]
    errs = [f"missing {k}" for k in schema.get("required", []) if k not in body]
    for k, spec in (schema.get("properties") or {}).items():
        if k not in body or not isinstance(spec, dict):
            continue
        want = JSON_TYPES.get(spec.get("type"))
        v = body[k]
        if want and (not isinstance(v, want) or (spec.get("type") in ("integer", "number") and isinstance(v, bool))):
            errs.append(f"{k} must be {spec['type']}")
        elif "enum" in spec and v not in spec["enum"]:
            errs.append(f"{k} must be one of {spec['enum']}")
    return errs


def make_handler(game):
    routes = make_routes(game)
    table = {(m, p): (needs, h) for m, p, _i, _s, needs, _sc, h in routes}
    schemas = {(m, p): sc for m, p, _i, _s, _n, sc, _h in routes}

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *a):
            pass

        def _send(self, code, body, ctype="application/json"):
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def _origin_ok(self):
            origin = self.headers.get("Origin")
            return origin is None or origin == f"http://127.0.0.1:{game.args.panel_port}"

        def _stream(self, q):
            since = int(q.get("since", [str(game.bus_seq)])[0])
            kinds = [k for k in q.get("kinds", [""])[0].split(",") if k]
            self.close_connection = True
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Connection", "close")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            try:
                while True:
                    with game.bus_cv:
                        game.bus_cv.wait_for(lambda: game.bus_seq > since, timeout=15)
                    batch = game.events_since(since, kinds)
                    since = max([since] + [e["seq"] for e in game.events_since(since)])
                    if not batch:
                        self.wfile.write(b": keep-alive\n\n")
                    for e in batch:
                        self.wfile.write(f"id: {e['seq']}\nevent: {e['kind']}\ndata: {json.dumps(e)}\n\n".encode())
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                return

        def _rhai_stream(self, body):
            """Run a Rhai script and stream every line it prints as it is printed (SSE: event "out",
            "err", then "exit" with the status)."""
            exe = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-rhai"
            if not exe.exists():
                return self._send(503, {"error": "build it: cargo build --release in tools/research/mcp-rhai"})
            script = str(body.get("script", ""))
            self.close_connection = True  # the stream ends with the script; nothing follows on this connection
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Connection", "close")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            proc = subprocess.Popen([str(exe), "--panel", f"http://127.0.0.1:{game.args.panel_port}", "-e", script],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
            game.event("rhai", script[:80] + " (streamed)")
            try:
                for line in proc.stdout:
                    kind = "err" if line.startswith("mcp-rhai:") else "out"
                    game.to_owner_console(f"[rhai] {line.rstrip()}", "error" if kind == "err" else "rhai")
                    self.wfile.write(f"event: {kind}\ndata: {json.dumps(line.rstrip())}\n\n".encode())
                    self.wfile.flush()
                rc = proc.wait()
                self.wfile.write(f"event: exit\ndata: {rc}\n\n".encode())
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                proc.kill()

        def _dispatch(self, method, body=None):
            url = urllib.parse.urlsplit(self.path)
            path, q = url.path, urllib.parse.parse_qs(url.query)
            if method == "GET":
                if path in ("/", "/index.html"):
                    return self._send(200, (STATIC / "index.html").read_bytes(), "text/html; charset=utf-8")
                if path == "/ui":
                    return self._send(200, (STATIC / "ui.html").read_bytes(), "text/html; charset=utf-8")
                if path == "/world":
                    return self._send(200, (STATIC / "world.html").read_bytes(), "text/html; charset=utf-8")
                if path == "/docs":
                    return self._send(200, SWAGGER_HTML.encode(), "text/html; charset=utf-8")
                if path == "/openapi.json":
                    return self._send(200, openapi(game, routes))
                if path == "/api/view/screenshot":
                    # What the owner's window shows right now, as PNG (openttd.view.screenshot).
                    try:
                        shot = game.view_call("openttd.view.screenshot")
                        f = Path(shot["path"])
                        before = f.stat().st_mtime if f.exists() else 0
                        # The client captures on its drawing thread; the file appears within a frame.
                        for _ in range(40):
                            if f.exists() and f.stat().st_mtime > before:
                                break
                            time.sleep(0.05)
                        return self._send(200, f.read_bytes(), "image/png")
                    except Exception as e:
                        return self._send(503, {"error": str(e)})
                if path == "/api/events/stream":
                    return self._stream(q)
                if path in VENDOR:
                    rel, ctype = VENDOR[path]
                    f = HERE / "node_modules" / rel
                    return self._send(200, f.read_bytes(), ctype) if f.is_file() else \
                        self._send(404, {"error": "run npm ci in tools/research/mcp-panel"})
                if path.startswith("/static/"):
                    f = (STATIC / path[len("/static/"):]).resolve()
                    if STATIC in f.parents and f.is_file():
                        ctype = {"css": "text/css", "js": "text/javascript"}.get(f.suffix[1:], "application/octet-stream")
                        return self._send(200, f.read_bytes(), ctype)
                    return self._send(404, {"error": "not found"})
            hit = table.get((method, path))
            if method == "POST":
                schema = schemas.get((method, path))
                if schema is None and path.startswith(("/api/tools/", "/api/client/")):
                    name = path.split("/", 3)[3]
                    cat = game.client_tools() if path.startswith("/api/client/") else game.server_tools()
                    schema = next((t.get("inputSchema") for t in cat if t["name"] == name), None)
                errs = body_errors(schema, body if body is not None else {})
                if errs:
                    return self._send(400, {"error": "invalid body: " + "; ".join(errs)})
            try:
                if hit:
                    needs, h = hit
                    if needs and not game.mcp_up:
                        return self._send(503, {"error": "MCP server not up yet"})
                    code, out = h(body if method == "POST" else q)
                    return self._send(code, out)
                if not game.mcp_up:
                    return self._send(503, {"error": "MCP server not up yet"})
                if method == "POST" and path.startswith("/api/client/"):
                    try:
                        return self._send(200, game.view_call(path[len("/api/client/"):], body or {}))
                    except Exception as e:
                        return self._send(503, {"error": str(e)})
                if method == "POST" and path.startswith("/api/tools/"):
                    out = game.tool(path[len("/api/tools/"):], body or {})
                    return self._send(rpc_status(out), out)
                if method == "GET" and path.startswith("/api/resources/"):
                    out = game.resource("openttd://" + path[len("/api/resources/"):])
                    return self._send(rpc_status(out), out)
            except Exception as e:  # surface, never hide, a failing call
                # A route that needs the owner's window answers 503 on a stack without one (headless,
                # or the client restarting): the state is temporary, not a broken route.
                code = 503 if "no GUI client attached" in str(e) else 502
                return self._send(code, {"error": str(e)})
            return self._send(404, {"error": "not found"})

        def do_GET(self):
            self._dispatch("GET")

        def do_POST(self):
            if not self._origin_ok():
                return self._send(403, {"error": "foreign origin"})
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length") or 0)) or b"{}")
            except ValueError:
                return self._send(400, {"error": "body is not JSON"})
            if urllib.parse.urlsplit(self.path).path == "/api/rhai/stream":
                return self._rhai_stream(body)
            self._dispatch("POST", body)

    return Handler


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--bin", required=True)
    p.add_argument("--panel-port", type=int, default=8800)
    p.add_argument("--mcp-port", type=int, default=8731)
    p.add_argument("--game-port", type=int, default=3979)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--ai", default="PrismaticAI")
    p.add_argument("--auto-approve", action="store_true")
    p.add_argument("--no-plan-bot", dest="plan_bot", action="store_false",
                   help="start the bot without the planner's air pair")
    args = p.parse_args()
    # A port another process already listens on is replaced by a free one: on macOS a second listener
    # can bind the same loopback port, and calls then reach either server at random (seen 2026-11-16).
    for attr in ("mcp_port", "game_port"):
        port = getattr(args, attr)
        with socket.socket() as probe:
            busy = probe.connect_ex(("127.0.0.1", port)) == 0
        if busy:
            with socket.socket() as free:
                free.bind(("127.0.0.1", 0))
                setattr(args, attr, free.getsockname()[1])
            print(f"mcp-panel: port {port} is in use; --{attr.replace('_', '-')} {getattr(args, attr)} instead", flush=True)

    game = Game(args)
    game.start()
    httpd = ThreadingHTTPServer(("127.0.0.1", args.panel_port), make_handler(game))
    print(f"mcp-panel: http://127.0.0.1:{args.panel_port}/  (game server 127.0.0.1:{args.game_port}, "
          f"MCP {game.endpoint()})", flush=True)
    # A process started in the background of a non-interactive shell inherits SIGINT as ignored;
    # take both signals explicitly so the dedicated server is always stopped with the panel.
    me = f"panel-{args.panel_port}"
    registry_write(me, {"kind": "panel", "role": "dedicated server + panel", "url": f"http://127.0.0.1:{args.panel_port}",
                        "mcp": game.endpoint(), "game_port": args.game_port})

    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        registry_remove(me)
        registry_remove(f"client-{args.panel_port}")
        game.stop()


if __name__ == "__main__":
    sys.exit(main())
