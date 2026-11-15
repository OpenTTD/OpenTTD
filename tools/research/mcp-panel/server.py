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
import os
import re
import shutil
import signal
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
TOKEN_RE = re.compile(r"Bearer token \(local operator only, not logged\): ([0-9a-fA-F]{32,})")


class Game:
    """One dedicated server, its console and its MCP endpoint."""

    def __init__(self, args):
        self.args = args
        self.tmp = Path(tempfile.mkdtemp(prefix="openttd-mcp-panel."))
        self.fifo = self.tmp / "console"
        self.proc = None
        self.console_fd = None
        self.token = None
        self.log = collections.deque(maxlen=600)  # (seq, line)
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

    def _read_log(self):
        for raw in self.proc.stdout:
            line = raw.decode("utf-8", "replace").rstrip()
            m = TOKEN_RE.search(line)
            if m:
                self.token = m.group(1)
                line = TOKEN_RE.sub("Bearer token (local operator only, not logged): <REDACTED>", line)
            if self.token:
                line = line.replace(self.token, "<REDACTED>")
            with self.lock:
                self.seq += 1
                self.log.append((self.seq, line))
            ai = AI_LOG_RE.search(line)
            if ai:
                self.publish("ai.log", {"company": int(ai.group(1)), "level": AI_LEVELS.get(ai.group(2), ai.group(2)),
                                        "text": ai.group(3)})
            else:
                self.publish("console.line", {"line": line})
        self.event("server", f"server process exited with {self.proc.wait()}")

    def console(self, command):
        os.write(self.console_fd, (command + "\n").encode())

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
        if self.args.ai:
            self.console(f"start_ai {self.args.ai}")
            self.ai_started = True
            self.event("ai", f"start_ai {self.args.ai} sent: the company plays on its own")

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
    def bots(self):
        """The AIs the owner can run as their representative, with their settings. From MCP when
        the game offers openttd.ai.list; until then derived from each AI's info.nut."""
        if "openttd.ai.list" in self.tool_names():
            return {"source": "mcp:openttd.ai.list", "data": self.tool("openttd.ai.list", {})}
        ais = []
        for info in sorted(AI_DIR.glob("*/info.nut")):
            text = info.read_text(errors="replace")
            m = re.search(r'GetName\(\)\s*\{\s*return\s*"([^"]+)"', text)
            settings = []
            for block in re.findall(r"AddSetting\(\{(.*?)\}\);", text, re.S):
                def field(k, cast=str):
                    v = re.search(rf"\b{k}\s*=\s*(\"[^\"]*\"|[^,\n]+)", block)
                    return cast(v.group(1).strip().strip('"')) if v else None
                settings.append({"name": field("name"), "description": field("description"),
                                 "min": field("min_value", int), "max": field("max_value", int),
                                 "step": field("step_size", int) or 1, "default": field("custom_value", int),
                                 "ingame": "CONFIG_INGAME" in (field("flags") or "")})
            ais.append({"ai": m.group(1) if m else info.parent.name, "dir": info.parent.name, "settings": settings})
        running = [c for c in self.resource("openttd://companies").get("data", {}).get("companies", []) if c.get("is_ai")]
        return {"source": "info.nut (openttd.ai.list not in tools/list yet)", "ais": ais, "running": running}

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
            return {"error": "the running game has no openttd.ai.set_setting tool yet; start a bot with new settings instead"}
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


def make_routes(game):
    """The panel's own HTTP surface, declared once: the router dispatches from this table and
    /openapi.json documents it. Routes derived from the live MCP catalogue are added at request
    time (derived_routes), from the same tools/list and resources/list every other surface reads."""

    def console(body):
        cmd = str(body.get("command", "")).strip()
        if not cmd or "\n" in cmd:
            return 400, {"error": "one non-empty line"}
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
        ("POST", "/api/bots/start", "botStart", "Start the owner's bot: an AI with a settings map", False,
         s({"ai": {"type": "string"}, "settings": OBJ}, ["ai"]), lambda b: (200, game.start_bot(b["ai"], b.get("settings", {})))),
        ("POST", "/api/bots/setting", "botSetting", "Change one setting of a running bot (openttd.ai.set_setting, approval-gated)", True,
         s({"company": {"type": "integer"}, "name": {"type": "string"}, "value": {"type": "integer"}}, ["company", "name", "value"]),
         lambda b: (200, game.set_bot_setting(b["company"], b["name"], b["value"]))),
        ("GET", "/api/graph", "graph", "The road network as a graph (nodes: junctions, ends, stations, town anchors; edges with length; components), derived from openttd://map, towns and stations by tools/research/mcp-rhai mcp-graph", True,
         None, lambda q: graph()),
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
    for r in resources:
        uri = r["uri"]
        path = uri.split("://", 1)[1] if "://" in uri else uri
        out.append(("GET", f"/api/resources/{path}", "resource_" + re.sub(r"\W", "_", path),
                    r.get("description") or r.get("name") or uri, None, r))
    return out


def openapi(game, routes):
    paths = {}

    def op(method, path, op_id, summary, schema, tag):
        o = {"operationId": op_id, "summary": summary, "tags": [tag],
             "responses": {"200": {"description": "OK", "content": {"application/json": {"schema": OBJ}}}}}
        if schema is not None:
            o["requestBody"] = {"required": True, "content": {"application/json": {"schema": schema}}}
        if path in ("/api/events", "/api/resource"):
            o["parameters"] = [{"name": n, "in": "query", "schema": {"type": "string"}} for n in
                               (("since", "kinds", "limit") if path == "/api/events" else ("uri",))]
        paths.setdefault(path, {})[method.lower()] = o

    for method, path, op_id, summary, _mcp, schema, _h in routes:
        op(method, path, op_id, summary, schema, "panel")
    paths["/api/rhai/stream"] = {"post": {"operationId": "rhaiStream", "tags": ["panel"],
                                          "summary": "Run a Rhai script and stream its output line by line (SSE: out, err, exit)",
                                          "requestBody": {"required": True, "content": {"application/json": {"schema": {
                                              "type": "object", "properties": {"script": {"type": "string"}}, "required": ["script"]}}}},
                                          "responses": {"200": {"description": "text/event-stream"}}}}
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
        op(method, path, op_id, summary, schema, "mcp tools" if method == "POST" else "mcp resources")
    return {"openapi": "3.1.0",
            "info": {"title": "OpenTTD MCP panel (RESEARCH-ONLY)", "version": "1",
                     "description": "Declared once in tools/research/mcp-panel/server.py and derived from the live "
                                    "MCP catalogue: every MCP tool and resource of the running game is a route here."},
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


def make_handler(game):
    routes = make_routes(game)
    table = {(m, p): (needs, h) for m, p, _i, _s, needs, _sc, h in routes}

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
                if path == "/world":
                    return self._send(200, (STATIC / "world.html").read_bytes(), "text/html; charset=utf-8")
                if path == "/docs":
                    return self._send(200, SWAGGER_HTML.encode(), "text/html; charset=utf-8")
                if path == "/openapi.json":
                    return self._send(200, openapi(game, routes))
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
            try:
                if hit:
                    needs, h = hit
                    if needs and not game.mcp_up:
                        return self._send(503, {"error": "MCP server not up yet"})
                    code, out = h(body if method == "POST" else q)
                    return self._send(code, out)
                if not game.mcp_up:
                    return self._send(503, {"error": "MCP server not up yet"})
                if method == "POST" and path.startswith("/api/tools/"):
                    return self._send(200, game.tool(path[len("/api/tools/"):], body or {}))
                if method == "GET" and path.startswith("/api/resources/"):
                    return self._send(200, game.resource("openttd://" + path[len("/api/resources/"):]))
            except Exception as e:  # surface, never hide, a failing call
                return self._send(502, {"error": str(e)})
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
    args = p.parse_args()

    game = Game(args)
    game.start()
    httpd = ThreadingHTTPServer(("127.0.0.1", args.panel_port), make_handler(game))
    print(f"mcp-panel: http://127.0.0.1:{args.panel_port}/  (game server 127.0.0.1:{args.game_port}, "
          f"MCP {game.endpoint()})", flush=True)
    # A process started in the background of a non-interactive shell inherits SIGINT as ignored;
    # take both signals explicitly so the dedicated server is always stopped with the panel.
    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        game.stop()


if __name__ == "__main__":
    sys.exit(main())
