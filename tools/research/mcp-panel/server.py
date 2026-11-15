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
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
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
               "-G", str(self.args.seed), "-c", str(cfg), "-d", "net=1,script=1"]
        self.event("server", "starting " + " ".join(cmd))
        self.proc = subprocess.Popen(cmd, stdin=open(self.fifo, "r+b", buffering=0),
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.console_fd = os.open(self.fifo, os.O_WRONLY)
        threading.Thread(target=self._read_log, daemon=True).start()
        threading.Thread(target=self._bring_up, daemon=True).start()
        threading.Thread(target=self._auto_loop, daemon=True).start()

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
            return json.loads(r["result"]["content"][0]["text"])
        except (KeyError, IndexError, TypeError, ValueError):
            return {"error": r}

    # ------------------------------------------------------------------ approvals
    def set_pause(self, pause):
        out = self.tool("openttd.game.set_pause", {"pause": bool(pause)})
        if out.get("status") == "pending_approval":
            self.pending[int(out["approval_id"])] = {"pause": bool(pause), "created": time.time()}
            self.event("mcp", f"set_pause({pause}) -> pending approval #{out['approval_id']}")
        else:
            self.event("mcp", f"set_pause({pause}) -> {out.get('status')} {out.get('command_result', '')}")
        return out

    def approve(self, approval_id):
        info = self.pending.pop(int(approval_id), None)
        if info is None:
            return {"error": f"no pending approval #{approval_id} known to the panel"}
        self.console(f"prismatic_mcp approve {approval_id}")
        self.event("operator", f"approved #{approval_id} in the OpenTTD console")
        time.sleep(0.5)
        out = self.tool("openttd.game.set_pause", {"pause": info["pause"]})
        self.event("mcp", f"#{approval_id} resubmitted -> {out.get('status')} "
                          f"{out.get('command_result', '')} verdict={out.get('verdict', '')}")
        return out

    def _auto_loop(self):
        while True:
            time.sleep(1)
            if self.auto_approve and self.pending:
                for aid in list(self.pending):
                    self.approve(aid)

    def event(self, kind, text):
        self.events.appendleft({"t": time.strftime("%H:%M:%S"), "kind": kind, "text": text})


def make_handler(game):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def _send(self, code, body, ctype="application/json"):
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def _origin_ok(self):
            origin = self.headers.get("Origin")
            return origin is None or origin == f"http://127.0.0.1:{game.args.panel_port}"

        def do_GET(self):
            if self.path in ("/", "/index.html"):
                return self._send(200, (STATIC / "index.html").read_bytes(), "text/html; charset=utf-8")
            if self.path == "/vendor/flowbite.min.js":
                f = HERE / "node_modules" / "flowbite" / "dist" / "flowbite.min.js"
                if f.is_file():
                    return self._send(200, f.read_bytes(), "text/javascript")
                return self._send(404, {"error": "run npm ci in tools/research/mcp-panel"})
            if self.path.startswith("/static/"):
                f = (STATIC / self.path[len("/static/"):]).resolve()
                if STATIC in f.parents and f.is_file():
                    ctype = {"css": "text/css", "js": "text/javascript"}.get(f.suffix[1:], "application/octet-stream")
                    return self._send(200, f.read_bytes(), ctype)
                return self._send(404, {"error": "not found"})
            try:
                if self.path == "/api/status":
                    return self._send(200, {
                        "server_running": game.proc is not None and game.proc.poll() is None,
                        "mcp_up": game.mcp_up, "endpoint": game.endpoint(), "ai": game.args.ai,
                        "ai_started": game.ai_started, "auto_approve": game.auto_approve,
                        "pending": [{"id": k, **v} for k, v in game.pending.items()],
                        "game_port": game.args.game_port, "events": list(game.events)})
                if self.path == "/api/log":
                    with game.lock:
                        return self._send(200, {"lines": [l for _, l in list(game.log)[-150:]]})
                if not game.mcp_up:
                    return self._send(503, {"error": "MCP server not up yet"})
                if self.path == "/api/overview":
                    return self._send(200, {"state": game.resource("openttd://game/state"),
                                            "companies": game.resource("openttd://companies"),
                                            "decisions": game.resource("openttd://decisions")})
                if self.path == "/api/catalogue":
                    return self._send(200, {"resources": game.rpc("resources/list"),
                                            "tools": game.rpc("tools/list"),
                                            "initialize": game.rpc("initialize", {
                                                "protocolVersion": "2025-11-25", "capabilities": {},
                                                "clientInfo": {"name": "openttd-mcp-panel", "version": "1"}})})
                if self.path.startswith("/api/resource?uri="):
                    uri = urllib.request.unquote(self.path.split("=", 1)[1])
                    return self._send(200, game.resource(uri))
            except Exception as e:  # surface, never hide, a failing call
                return self._send(502, {"error": str(e)})
            return self._send(404, {"error": "not found"})

        def do_POST(self):
            if not self._origin_ok():
                return self._send(403, {"error": "foreign origin"})
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length") or 0)) or b"{}")
            try:
                if self.path == "/api/pause":
                    return self._send(200, game.set_pause(body.get("pause", True)))
                if self.path == "/api/approve":
                    return self._send(200, game.approve(body["id"]))
                if self.path == "/api/auto-approve":
                    game.auto_approve = bool(body.get("on"))
                    game.event("operator", f"auto-approve {'ON' if game.auto_approve else 'OFF'}")
                    return self._send(200, {"auto_approve": game.auto_approve})
                if self.path == "/api/start-ai":
                    game.console(f"start_ai {game.args.ai}")
                    game.ai_started = True
                    game.event("ai", f"start_ai {game.args.ai} sent")
                    return self._send(200, {"ok": True})
                if self.path == "/api/validate":
                    return self._send(200, game.tool("openttd.decision.validate", body))
                if self.path == "/api/console":
                    # The developer console: one line into OpenTTD's own console, as the operator.
                    cmd = str(body.get("command", "")).strip()
                    if not cmd or "\n" in cmd:
                        return self._send(400, {"error": "one non-empty line"})
                    with game.lock:
                        mark = game.seq
                    game.console(cmd)
                    game.event("console", cmd)
                    time.sleep(float(body.get("wait", 0.6)))
                    with game.lock:
                        lines = [l for n, l in game.log if n > mark]
                    return self._send(200, {"sent": cmd, "output": lines})
                if self.path == "/api/rhai":
                    # A Rhai script against this panel, run by tools/research/mcp-rhai.
                    rhai = HERE.parent / "mcp-rhai" / "target" / "release" / "mcp-rhai"
                    if not rhai.exists():
                        return self._send(503, {"error": "build it: cargo build --release in tools/research/mcp-rhai"})
                    r = subprocess.run([str(rhai), "--panel", f"http://127.0.0.1:{game.args.panel_port}",
                                        "-e", str(body.get("script", ""))], capture_output=True, text=True,
                                       timeout=float(body.get("timeout", 120)))
                    game.event("rhai", (body.get("script", "")[:80]) + f" -> exit {r.returncode}")
                    return self._send(200, {"exit": r.returncode, "output": (r.stdout + r.stderr).rstrip()})
                if self.path == "/api/rpc":
                    # Raw MCP JSON-RPC with the panel's token: anything an MCP client can call.
                    return self._send(200, game.rpc(body["method"], body.get("params")))
            except Exception as e:
                return self._send(502, {"error": str(e)})
            return self._send(404, {"error": "not found"})

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
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        game.stop()


if __name__ == "__main__":
    sys.exit(main())
