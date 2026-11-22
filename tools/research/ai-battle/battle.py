#!/usr/bin/env python3
"""RESEARCH-ONLY: the LAN AI battle, a headless company-AI tournament on every reachable machine.

Every machine on the local network that answers SSH and has an OpenTTD build becomes an arena. It
runs free-for-all matches between the configured contestant AIs (PrismaticAI from this repository
and AIs from BaNaNaS) with the null video driver, as fast as its CPU allows. The read-only
BattleReferee Game Script logs every company's standing once a quarter. This process schedules
matches onto free arena slots without end, reads their logs over SSH, keeps the leaderboard,
replays an occasional match on a second arena to keep checking that results do not depend on the
machine, and serves a live dashboard. `report` turns the finished matches of the current arena
edition into a record under research/results/, and the leaderboard in research/ai-battle/README.md
is derived from those records (`check` fails when it is not). Full description:
research/ai-battle/README.md.

Commands:
  discover   find the SSH-reachable machines on the local /24 and write the local hosts file
  fetch      download the BaNaNaS contestants into the cache (throwaway loopback -D server)
  prepare    assemble the arena bundle: base set, AIs, PrismaticAI and referee at a clean commit
  probe      show which hosts are usable now (state, build, load, disk; paused and busy hosts)
  run        schedule matches forever and serve the dashboard (SIGTERM / Ctrl-C stops; STOP holds it)
  status     print the current edition's leaderboard
  report     write research/results/battle-<utc>-<edition>.{json,md} and the README leaderboard
  check      validate every battle record and that the README leaderboard is derived (the gate)
  fitness    one short local match of the working tree's PrismaticAI against a reference AI; fails
             on a Squirrel error, an unfinished match or a collapsing fleet (the gate, on change)

Guards (after an incident where `run` kept ~32 matches per host going for three days, drove targets
to load 86 on 18 cores and 35 of 37 GB swap, and nobody on a target could tell whose they were):
  PAUSED     an arena takes no new match while a file `<remote root>/PAUSED*` (default
             $HOME/ttd-battle/PAUSED) exists on it; its first line is shown as the reason. Anyone
             with a shell on the target can pause it: `echo "need the cores" > ~/ttd-battle/PAUSED`.
             Running matches finish on their own (or kill them by <root>/matches/*/*/pid).
  STOP       `run` starts no new match on any arena while `<local root>/STOP` (default
             ~/ttd-battle/STOP) exists on the orchestrator's machine; it logs an event, keeps
             reading the running matches and resumes when the file is gone.
  floor      every probe also reads load average, CPU count, swap and free disk of the remote root
             (one portable shell call). An arena whose load per core is above `max_load_per_core`
             (1.0), whose free disk is under `min_free_disk_gib` (35) or whose used swap is above
             `max_swap_used_gib` (16) is `busy` and takes no match; otherwise new matches are capped
             so the load stays under the ceiling. Defaults: battle.json `resource_floor`; per host:
             `resource_floor` in hosts.json. An unreadable figure never blocks; it is logged.
  ownership  before the first match on an arena, `run` writes `<remote root>/ORCHESTRATOR` (host,
             pid, start time, commit, how to stop it). Every match carries
             TTD_BATTLE_ORCHESTRATOR=<host>:<pid> in its environment (`ps eww`), the wrapper's argv
             names it (`ps` shows `ttd-battle[<host>:<pid>]`), and its match directory holds it in
             a file `orchestrator`.

Exit codes (Majordomus convention): 0 ok, 2 usage, 10 contract unmet, 11 drift, 12 missing.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import datetime as dt
import hashlib
import http.server
import io
import json
import math
import random
import os
import re
import signal
import socket
import sqlite3
import subprocess
import sys
import tarfile
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
DEFAULT_CONFIG = HERE / "battle.json"
RESULTS_DIR = REPO / "research" / "results"
README = REPO / "research" / "ai-battle" / "README.md"
BEGIN, END = "<!-- battle:begin -->", "<!-- battle:end -->"
RECORD_SCHEMA = "ai-battle-record/v1"
RECORD_GLOB = "battle-*.json"

# "REF Q date=<y>-<m> cid=<n> <key>=<int> ... name=<rest of line>" (referee v2 and v3 fields).
REF_LINE = re.compile(r"REF Q date=(\d+)-(\d+) cid=(\d+) (.*?) ?name=(.*)$")
REF_FIELD = re.compile(r"(\w+)=(-?\d+)")
MODES = ("rail", "road", "air", "water")
# An AI's Squirrel error: "dbg: [script:1] [<company>] [S] <text>".
SQ_ERROR = re.compile(r"\[script:\d\] \[(\d+)\] \[S\] ")

SSH_OPTS = ["-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
            "-o", "ControlMaster=auto", "-o", "ControlPersist=120",
            "-o", "ControlPath=/tmp/ttd-battle-cm-%r@%h:%p"]
DEFAULT_BINARY = "$HOME/dev/OpenTTD/build/openttd"


def log(msg: str) -> None:
    print(time.strftime("%H:%M:%S"), msg, flush=True)


def expand(p: str) -> Path:
    return Path(os.path.expandvars(os.path.expanduser(p)))


def load_config(path: Path) -> dict:
    cfg = json.loads(path.read_text())
    for key in ("cache", "results", "hosts_file", "baseset"):
        cfg[key] = expand(cfg[key])
    t = cfg.get("tuning")
    if t and t.get("enabled"):
        state = tuning_state(cfg)
        cfg["contestants"] = cfg["contestants"] + state["variants"]
    return cfg


# ------------------------------------------------------------------------------- self-tuning (GA)

def paired_vs(matches: list[dict], base: str) -> list[dict]:
    """Every configuration against `base` on the same seeds (common random numbers): pairs, mean
    value ratio, wins. A configuration is its settings, not its label: the GA's elite keeps its
    genes but takes a new label every generation, and only pooled by settings do its pairs add up.
    The paired comparison decides a default, not a GA score over a few matches."""
    by_seed: dict[int, dict[str, int]] = {}
    label_of: dict[str, str] = {}
    base_id = None
    for m in sorted(matches, key=lambda m: m.get("started_at", 0)):
        if m.get("status") != "done" or m.get("replica_of"):
            continue
        ident = {c["key"]: (c.get("ai", c["key"]) + "|" + c.get("settings", "")) for c in m.get("lineup", [])}
        for f in m.get("standings") or []:
            cid = ident.get(f["key"], f["key"])
            label_of[cid] = f["key"]                       # the newest label of that configuration
            if f["key"] == base:
                base_id = cid
            by_seed.setdefault((m.get("edition", ""), m["seed"]), {})[cid] = f["value"]
    stats: dict[str, list] = {}
    for vals in by_seed.values():
        if base_id not in vals:
            continue
        for k, v in vals.items():
            if k != base_id:
                stats.setdefault(k, []).append((v, vals[base_id]))
    out = []
    for k, ps in stats.items():
        ratios = [v / max(1, b) for v, b in ps]
        logs = [math.log(max(1, v) / max(1, b)) for v, b in ps]
        geo, ci_low = log_ratio_ci(logs, k)
        out.append({"contestant": label_of.get(k, k), "settings": k.split("|", 1)[1] if "|" in k else "",
                    "pairs": len(ps), "ratio": round(sum(ratios) / len(ratios), 3),
                    "wins": sum(v > b for v, b in ps), "geo": geo, "ci_low": ci_low})
    return sorted(out, key=lambda r: -r["ratio"])


def log_ratio_ci(logs: list[float], key: str, resamples: int = 2000) -> tuple[float, float]:
    """Geometric mean of the paired value ratios and the lower end of its 95% bootstrap interval
    (percentile, deterministic: the resampling is seeded by the configuration). The log ratio,
    not the win count, carries the size of each pair's difference: on 40 pairs the GA's elite
    won 25 (62%) yet its interval was 1.03-1.21."""
    if len(logs) < 2:
        return (round(math.exp(logs[0]), 3) if logs else 1.0), 0.0
    rng = random.Random(key)
    n = len(logs)
    means = sorted(sum(rng.choices(logs, k=n)) / n for _ in range(resamples))
    return round(math.exp(sum(logs) / n), 3), round(math.exp(means[int(0.025 * resamples)]), 3)


def promotions(matches: list[dict], rules: dict) -> list[dict]:
    """Candidates for the bot's next default: a PrismaticAI configuration paired with the default on
    common seeds, with a mean value ratio above `min_ratio`, and either
    - at least `min_pairs` pairs, winning at least `min_win_share` of them (a clear winner), or
    - at least `min_pairs_ci` pairs, with the lower end of the 95% bootstrap interval of its
      geometric mean ratio above `min_ci_low` (a steady gain in a noisy five-AI field, where an
      80% win share is out of reach for a +10-15% configuration).
    Proposed, never applied: a default changes through a commit."""
    base = rules.get("base", "PrismaticAI")
    out = []
    for r in paired_vs(matches, base):
        if not r["contestant"].startswith("PrismaticAI") or r["ratio"] <= rules.get("min_ratio", 1.05):
            continue
        clear = r["pairs"] >= rules.get("min_pairs", 16) and r["wins"] >= rules.get("min_win_share", 0.8) * r["pairs"]
        steady = ("min_pairs_ci" in rules and r["pairs"] >= rules["min_pairs_ci"]
                  and r["ci_low"] > rules.get("min_ci_low", 1.0))
        if clear or steady:
            out.append(dict(r, base=base, why="win share" if clear else "interval"))
    return out


def tuning_path(cfg: dict) -> Path:
    return cfg["cache"] / "tuning" / "state.json"


def settings_str(genes: dict) -> str:
    return ",".join(f"{k}={v}" for k, v in sorted(genes.items()))


def variant(t: dict, gen: int, i: int, genes: dict, parents: list) -> dict:
    # Every gene is clamped into the current space, so narrowing a range in battle.json (say
    # own_fail_pause 0 -> 1) also reaches the elite and children inherited from older generations.
    genes = {k: clamp_gene(t["space"], k, v) if k in t["space"] else v for k, v in genes.items()}
    return {"ai": t.get("ai", "PrismaticAI"), "label": f"{t['label_prefix']} g{gen}.{i}",
            "group": t["group"], "source": "repository", "genes": genes, "parents": parents,
            "settings": settings_str({**t.get("fixed", {}), **genes})}


def clamp_gene(space: dict, k: str, v: float) -> int:
    lo, hi, step = space[k]
    return int(min(hi, max(lo, lo + round((v - lo) / step) * step)))


def first_generation(t: dict) -> list[dict]:
    rng = random.Random(f"{t['seed']}:0")
    out = [variant(t, 0, 0, dict(t["start"]), [])]
    while len(out) < t["population"]:
        out.append(variant(t, 0, len(out), mutate(t, dict(t["start"]), rng, 1.0), []))
    return out


def mutate(t: dict, genes: dict, rng: random.Random, rate: float | None = None) -> dict:
    rate = t.get("mutation_rate", 0.35) if rate is None else rate
    out = dict(genes)
    for k, (lo, hi, step) in t["space"].items():
        if rng.random() < rate:
            span = max(1, (hi - lo) // step)
            out[k] = clamp_gene(t["space"], k, out.get(k, lo) + rng.choice([-1, 1]) *
                                step * rng.randint(1, max(1, span // 4)))
    return out


def brain_proposals(t: dict) -> list[dict]:
    """Up to `brain_offspring` proposals from brain.json (schema battle-brain/v1) younger than
    `brain_max_age_s`, as genes inside the declared space. Nothing when absent, stale or invalid."""
    n = int(t.get("brain_offspring", 0))
    path = expand(t.get("brain_file", "~/ttd-battle/results/brain.json"))
    if n <= 0 or not path.is_file() or time.time() - path.stat().st_mtime > t.get("brain_max_age_s", 7200):
        return []
    try:
        data = json.loads(path.read_text())
        if data.get("schema") != "battle-brain/v1":
            return []
        out = []
        for p in data.get("proposals", [])[:n]:
            kv = dict(x.split("=", 1) for x in str(p.get("settings", "")).split(",") if "=" in x)
            genes = {k: clamp_gene(t["space"], k, float(kv.get(k, t["start"].get(k, t["space"][k][0]))))
                     for k in t["space"]}
            out.append((genes, str(p.get("kind", "")).capitalize()))
        return out
    except (ValueError, KeyError, TypeError):
        return []


def tuning_state(cfg: dict) -> dict:
    p = tuning_path(cfg)
    if p.is_file():
        return json.loads(p.read_text())
    t = cfg["tuning"]
    state = {"generation": 0, "variants": first_generation(t), "history": []}
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(state, indent=1) + "\n")
    return state


def fitness_of(key: str, matches: list[dict], last_penalty: float = 0.0) -> dict:
    """Mean normalised points per match (1 = won, 0 = last), mean log10 of value / winner's value,
    and the share of matches it ended last, over the contestant's scored matches. The score is
    points + 0.1 x log gap - last_penalty x last share: an explorer that ends last is culled fast."""
    pts, logs, lasts = [], [], 0
    for m in matches:
        if m.get("status") != "done" or m.get("replica_of"):
            continue
        st = m["standings"]
        for rank, f in enumerate(st):
            if f["key"] == key:
                pts.append((len(st) - 1 - rank) / max(1, len(st) - 1))
                logs.append(math.log10(max(1, f["value"]) / max(1, st[0]["value"])))
                lasts += rank == len(st) - 1
    n = len(pts)
    return {"matches": n, "points": round(sum(pts) / n, 4) if n else 0.0,
            "log_gap": round(sum(logs) / n, 4) if n else 0.0,
            "last_share": round(lasts / n, 4) if n else 0.0,
            "score": round(sum(pts) / n + 0.1 * sum(logs) / n - last_penalty * lasts / n, 4) if n else -9.0}


def next_generation(t: dict, state: dict, scored: list[tuple[dict, dict]]) -> list[dict]:
    """Elitism (the best survives unchanged), then children of two tournament-chosen parents by
    uniform crossover and mutation. Deterministic per generation."""
    gen = state["generation"] + 1
    rng = random.Random(f"{t['seed']}:{gen}")
    ranked = [v for v, _ in sorted(scored, key=lambda x: -x[1]["score"])]
    out = [variant(t, gen, 0, dict(ranked[0]["genes"]), [ranked[0]["label"]])]

    def pick():
        a, b = rng.sample(ranked[:max(2, len(ranked))], 2) if len(ranked) > 1 else (ranked[0], ranked[0])
        return a if ranked.index(a) < ranked.index(b) else b
    # Model-guided offspring: the top proposals of tools/research/battle-brain (an Axon/EXLA ensemble
    # trained on every PrismaticAI result) join as children, clamped into the gene space.
    # A proposal's "kind" (safe: best lower confidence bound; explore: best upper) names it.
    used = set()
    for k, (genes, kind) in enumerate(brain_proposals(t)):
        if len(out) >= t["population"]:
            break
        tag = f"brain{kind}" if kind else f"brain{k}"
        if tag in used:  # two proposals of one kind: labels must stay unique, they are contestant keys
            tag = f"{tag}{k}"
        used.add(tag)
        out.append(variant(t, gen, len(out), genes, [tag]))
        out[-1]["label"] = f"{t['label_prefix']} g{gen}.{tag}"
    while len(out) < t["population"]:
        p1, p2 = pick(), pick()
        child = {k: (p1["genes"] if rng.random() < 0.5 else p2["genes"]).get(k, t["start"].get(k))
                 for k in t["space"]}
        out.append(variant(t, gen, len(out), mutate(t, child, rng), [p1["label"], p2["label"]]))
    return out


def contestant_key(c: dict) -> str:
    """The identity a contestant scores under: its label, else the AI name plus its settings."""
    if c.get("label"):
        return c["label"]
    return c["ai"] + (f"[{c['settings']}]" if c.get("settings") else "")


def git(*args: str, cwd: Path = REPO) -> str:
    return subprocess.run(["git", "-C", str(cwd), *args], capture_output=True, text=True,
                          check=True).stdout.strip()


# --------------------------------------------------------------------------------------- hosts

def load_hosts(cfg: dict) -> list[dict]:
    """Hosts live in a local, untracked file: names, addresses and users stay off the public repo."""
    if cfg["hosts_file"].is_file():
        return json.loads(cfg["hosts_file"].read_text())["hosts"]
    return [{"name": "local", "ssh": None, "slots": max(1, (os.cpu_count() or 2) // 3)}]


# What an arena must have left before it takes a match (battle.json `resource_floor`, per host
# `resource_floor` in hosts.json). The disk floor is the machine rule's 35 GiB.
FLOOR_DEFAULTS = {"max_load_per_core": 1.0, "min_free_disk_gib": 35, "max_swap_used_gib": 16}
GIB_KIB = 1024 * 1024


def floor_limits(defaults: dict | None, spec: dict) -> dict:
    """The resource floor of one host: built-in defaults < battle.json < the host's own entry."""
    return {**FLOOR_DEFAULTS, **(defaults or {}), **(spec.get("resource_floor") or {})}


def probe_script(binary: str, root: str) -> str:
    """One shell call, POSIX sh on macOS and Linux, one tagged line per fact: the build, a pause
    marker, CPU count, load average, swap (macOS sysctl / Linux /proc/meminfo) and free disk of the
    remote root (its nearest existing parent before the first sync)."""
    return (f'b={binary}; r={root}\n'
            'if [ -x "$b" ]; then echo "VERSION $("$b" -h 2>&1 | head -1)"; else echo NO_BINARY; fi\n'
            'for f in "$r"/PAUSED*; do if [ -f "$f" ]; then echo "PAUSED $f"; '
            'echo "PAUSEDMSG $(head -1 "$f")"; break; fi; done\n'
            'echo "NCPU $(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || '
            'getconf _NPROCESSORS_ONLN 2>/dev/null)"\n'
            'echo "LOAD $(cat /proc/loadavg 2>/dev/null || sysctl -n vm.loadavg 2>/dev/null || uptime)"\n'
            'echo "SWAP $(sysctl -n vm.swapusage 2>/dev/null)"\n'
            "grep -E '^(MemTotal|MemAvailable|SwapTotal|SwapFree):' /proc/meminfo 2>/dev/null "
            "| sed 's/^/MEM /'\n"
            'd="$r"; while [ ! -d "$d" ]; do d=$(dirname "$d"); done\n'
            'echo "DISK $(df -Pk "$d" 2>/dev/null | tail -1)"\n'
            'exit 0\n')


UNIT_MIB = {"K": 1 / 1024, "M": 1, "G": 1024, "T": 1024 * 1024}
DF_LINE = re.compile(r"(\d+)\s+(\d+)\s+(\d+)\s+(\d+)%\s+(\S.*)$")


def parse_probe(text: str) -> dict:
    """The facts of `probe_script`'s output. Tolerant: a figure it cannot read stays None (and never
    blocks a host) and the line goes to `unparsed`, for the orchestrator to log."""
    res = {"version": None, "no_binary": False, "pause_file": None, "paused": None, "ncpu": None,
           "load": None, "disk_free_kib": None, "disk_mount": None, "swap_used_mib": None,
           "swap_total_mib": None, "mem_avail_kib": None, "unparsed": []}
    meminfo: dict[str, int] = {}
    for line in text.splitlines():
        tag, _, rest = line.partition(" ")
        rest = rest.strip()
        ok = True
        if tag == "VERSION":
            res["version"] = rest.replace("OpenTTD ", "")
        elif tag == "NO_BINARY":
            res["no_binary"] = True
        elif tag == "PAUSED":
            res["pause_file"] = rest
        elif tag == "PAUSEDMSG":
            res["paused"] = rest
        elif tag == "NCPU":
            ok = rest.isdigit() and int(rest) > 0
            res["ncpu"] = int(rest) if ok else None
        elif tag == "LOAD":
            # /proc/loadavg "0.52 0.58 0.59 1/389 12345"; sysctl "{ 1.2 1.1 1.0 }"; uptime "...
            # load average(s): 0.52, 0.58, 0.59" (some locales write 0,52).
            m = re.search(r"load averages?:\s*(.*)", rest)
            nums = re.findall(r"\d+(?:[.,]\d+)?", m.group(1) if m else rest)[:3]
            ok = bool(nums)
            res["load"] = [float(n.replace(",", ".")) for n in nums] if ok else None
        elif tag == "SWAP":
            if rest:   # macOS: "total = 2048.00M  used = 1024.50M  free = 1023.50M  (encrypted)"
                t = re.search(r"total = ([\d.]+)([KMGT])", rest)
                u = re.search(r"used = ([\d.]+)([KMGT])", rest)
                ok = bool(t and u)
                if ok:
                    res["swap_total_mib"] = float(t.group(1)) * UNIT_MIB[t.group(2)]
                    res["swap_used_mib"] = float(u.group(1)) * UNIT_MIB[u.group(2)]
        elif tag == "MEM":   # Linux: "MemAvailable:   1234 kB"
            m = re.match(r"(\w+):\s*(\d+)", rest)
            ok = bool(m)
            if ok:
                meminfo[m.group(1)] = int(m.group(2))
        elif tag == "DISK":
            m = DF_LINE.search(rest)
            ok = bool(m)
            if ok:
                res["disk_free_kib"], res["disk_mount"] = int(m.group(3)), m.group(5)
        elif line.strip():
            ok = False
        if not ok:
            res["unparsed"].append(line.strip()[:120])
    if "MemAvailable" in meminfo:
        res["mem_avail_kib"] = meminfo["MemAvailable"]
    if "SwapTotal" in meminfo and "SwapFree" in meminfo:
        res["swap_total_mib"] = meminfo["SwapTotal"] / 1024
        res["swap_used_mib"] = (meminfo["SwapTotal"] - meminfo["SwapFree"]) / 1024
    if res["pause_file"] and not res["paused"]:
        res["paused"] = f"{res['pause_file']} present"
    for key, tag in (("ncpu", "NCPU"), ("load", "LOAD"), ("disk_free_kib", "DISK")):
        if res[key] is None and not any(u.startswith(tag) for u in res["unparsed"]):
            res["unparsed"].append(f"no {tag} line")
    return res


def probe_state(res: dict, returncode: int, stderr: str, binary: str) -> tuple[str, str]:
    """(state, detail) of a host from its parsed probe. A pause marker wins over everything but an
    unreachable host: it is a human's order. Output without a build line is an error: a host whose
    probe is not understood is not handed matches."""
    if res["paused"]:
        return "paused", res["paused"]
    if res["no_binary"]:
        return "no-binary", f"{binary} not found"
    if returncode != 0:
        return "error", stderr.strip()[-200:]
    if res["version"] is None:
        return "error", "probe output not understood: " + "; ".join(res["unparsed"])[:200]
    return "online", resource_summary(res)


def resource_summary(res: dict) -> str:
    parts = []
    if res.get("load") and res.get("ncpu"):
        parts.append(f"load {max(res['load'][:2]):.1f}/{res['ncpu']} cores")
    if res.get("disk_free_kib") is not None:
        parts.append(f"disk {res['disk_free_kib'] / GIB_KIB:.1f} GiB free")
    if res.get("swap_used_mib") is not None:
        parts.append(f"swap {res['swap_used_mib'] / 1024:.1f} GiB used")
    if res.get("mem_avail_kib") is not None:
        parts.append(f"mem {res['mem_avail_kib'] / GIB_KIB:.1f} GiB available")
    return ", ".join(parts)


def floor_decision(load: float | None, ncpu: int | None, free_disk_kib: int | None, room: int,
                   limits: dict, swap_used_mib: float | None = None) -> tuple[int, str]:
    """How many new matches a host may take: (count, reason). `room` is its free slots. A figure
    that is None (unreadable) never blocks. Blocked by free disk under the floor, used swap over its
    ceiling, or load per core over its ceiling; otherwise capped so that one more match (one core,
    OpenTTD runs single-threaded) per new match keeps the load at or under the ceiling. The reason
    is empty when nothing but the slots limited the count."""
    room = max(0, room)
    floor_gib = limits.get("min_free_disk_gib")
    if free_disk_kib is not None and floor_gib is not None and free_disk_kib < floor_gib * GIB_KIB:
        return 0, f"free disk {free_disk_kib / GIB_KIB:.1f} GiB < floor {floor_gib} GiB"
    swap_cap = limits.get("max_swap_used_gib")
    if swap_used_mib is not None and swap_cap is not None and swap_used_mib > swap_cap * 1024:
        return 0, f"swap {swap_used_mib / 1024:.1f} GiB used > ceiling {swap_cap} GiB"
    ceiling = limits.get("max_load_per_core")
    if load is None or not ncpu or ceiling is None:
        return room, ""
    per_core = load / ncpu
    if per_core > ceiling:
        return 0, f"load {load:.1f} on {ncpu} cores = {per_core:.2f}/core > ceiling {ceiling}"
    headroom = max(0, math.floor(ceiling * ncpu - load + 1e-9))
    if headroom < room:
        return headroom, (f"load {load:.1f} on {ncpu} cores: room for {headroom} more under "
                          f"{ceiling}/core")
    return room, ""


def stop_requested(root: Path) -> str | None:
    """The orchestrator's STOP file: None when absent, else its first line (or a note that it exists)."""
    f = root / "STOP"
    if not f.is_file():
        return None
    try:
        first = (f.read_text(errors="replace").splitlines() or [""])[0].strip()
    except OSError:
        first = ""
    return first or f"{f} present"


def owner_marker(hostname: str, pid: int) -> str:
    """`<host>:<pid>` of the orchestrator, safe to put unquoted into a shell line and a process title."""
    return f"{re.sub(r'[^A-Za-z0-9._-]', '_', hostname) or 'unknown'}:{pid}"


def orchestrator_text(hostname: str, pid: int, started_at: str, commit: str, arena_rev: str,
                      edition: str, root: str, local_root: str) -> str:
    """The `<remote root>/ORCHESTRATOR` file: who runs the matches on this host and how to stop it."""
    marker = owner_marker(hostname, pid)
    return (f"# AIBattle (OpenTTD research fork, tools/research/ai-battle/battle.py run) schedules\n"
            f"# matches onto this host. Matches carry TTD_BATTLE_ORCHESTRATOR={marker}.\n"
            f"host={hostname}\npid={pid}\nstarted_at={started_at}\ncommit={commit}\n"
            f"arena_rev={arena_rev}\nedition={edition}\nmarker=TTD_BATTLE_ORCHESTRATOR={marker}\n"
            f"how_to_stop=echo '<reason>' > {root}/PAUSED on this host (no new match here; running "
            f"ones finish, or kill them by {root}/matches/*/*/pid), or SIGTERM pid {pid} on host "
            f"{hostname} (kill -TERM {pid}), or create {local_root}/STOP there (no new match "
            f"anywhere)\n")


class Host:
    """One arena machine; `ssh` is None for the machine this orchestrator runs on."""

    def __init__(self, spec: dict, remote_root: str, floor: dict | None = None):
        self.name = spec["name"]
        self.ssh = spec.get("ssh")
        self.ssh_args = [os.path.expanduser(a) for a in spec.get("ssh_args", [])]
        self.binary = spec.get("binary", DEFAULT_BINARY)
        self.slots = int(spec.get("slots", 1))
        self.limits = floor_limits(floor, spec)
        self.root = remote_root
        self.state = "unknown"   # online | busy | paused | offline | no-binary | error
        self.detail = ""
        self.version = ""
        self.res: dict = {}
        self.synced_digest = None
        self.claimed = False     # <root>/ORCHESTRATOR written by this process
        self.next_probe = 0.0

    def sh(self, script: str, timeout: int = 60) -> subprocess.CompletedProcess:
        cmd = (["/bin/sh", "-c", script] if self.ssh is None
               else ["ssh", *SSH_OPTS, *self.ssh_args, self.ssh, script])
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)

    def try_sh(self, script: str, timeout: int = 60) -> subprocess.CompletedProcess:
        """Best-effort `sh`: a host that stops answering yields exit 124 instead of an exception,
        so one unreachable arena can never take the orchestrator down."""
        try:
            return self.sh(script, timeout)
        except subprocess.TimeoutExpired:
            return subprocess.CompletedProcess(script, 124, "", f"timed out after {timeout}s")

    def probe(self) -> None:
        try:
            r = self.sh(probe_script(self.binary, self.root), timeout=15)
        except subprocess.TimeoutExpired:
            self.state, self.detail = "offline", "timeout"
            return
        if r.returncode == 255:
            self.state, self.detail = "offline", (r.stderr.strip().splitlines() or [""])[-1]
            return
        self.res = parse_probe(r.stdout)
        self.state, self.detail = probe_state(self.res, r.returncode, r.stderr, self.binary)
        if self.res["version"] is not None:
            self.version = self.res["version"]

    def admit(self, running: int) -> int:
        """New matches this host may take now, from its last probe: its free slots, held to the
        resource floor. A host the floor blocks becomes `busy`, with the figures as its detail."""
        if self.state != "online":
            return 0
        load = self.res.get("load")
        n, why = floor_decision(max(load[:2]) if load else None, self.res.get("ncpu"),
                                self.res.get("disk_free_kib"), self.slots - running, self.limits,
                                self.res.get("swap_used_mib"))
        if why:
            self.state, self.detail = ("busy" if n == 0 else "online"), why
        return n

    def claim(self, text: str) -> bool:
        """Write <root>/ORCHESTRATOR (who schedules here, how to stop it). True when written."""
        r = self.try_sh(f"mkdir -p {self.root} && cat > {self.root}/ORCHESTRATOR <<'EOS'\n{text}EOS\n",
                        timeout=30)
        self.claimed = r.returncode == 0
        return self.claimed

    def sync_arena(self, cache: Path, digest: str) -> None:
        """Copy the arena bundle to <root>/arena on the host (skipped when it is the cache)."""
        if self.synced_digest == digest:
            return
        if self.ssh is None:
            dest = expand(self.root) / "arena"
            if dest.resolve() != (cache / "arena").resolve():
                dest.mkdir(parents=True, exist_ok=True)
                subprocess.run(["rsync", "-a", "--delete", f"{cache}/arena/", f"{dest}/"],
                               check=True, capture_output=True, timeout=300)
        else:
            self.sh(f"mkdir -p {self.root}")
            rsh = " ".join(["ssh", *SSH_OPTS, *self.ssh_args])
            r = subprocess.run(["rsync", "-a", "--delete", "-e", rsh,
                                f"{cache}/arena/", f"{self.ssh}:{self.root}/arena/"],
                               capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                raise RuntimeError(f"rsync to {self.name} failed: {r.stderr.strip()}")
        self.synced_digest = digest


def local_ipv4() -> str:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))   # TEST-NET-1: no packet is sent for a UDP connect
        return s.getsockname()[0]
    finally:
        s.close()


def ssh_aliases() -> dict[str, str]:
    """HostName -> alias from ~/.ssh/config (the first alias wins)."""
    out, alias = {}, None
    cfg = Path.home() / ".ssh" / "config"
    if not cfg.is_file():
        return out
    for line in cfg.read_text().splitlines():
        parts = line.strip().split()
        if len(parts) >= 2 and parts[0].lower() == "host" and "*" not in parts[1]:
            alias = parts[1]
        elif len(parts) >= 2 and parts[0].lower() == "hostname" and alias:
            out.setdefault(parts[1], alias)
    return out


def cmd_discover(cfg: dict) -> None:
    d = cfg.get("discover", {})
    me = local_ipv4()
    prefix = me.rsplit(".", 1)[0]
    port = int(d.get("port", 22))

    def open22(i: int):
        ip = f"{prefix}.{i}"
        try:
            with socket.create_connection((ip, port), timeout=0.6):
                return ip
        except OSError:
            return None

    with concurrent.futures.ThreadPoolExecutor(64) as pool:
        candidates = [ip for ip in pool.map(open22, range(1, 255)) if ip and ip != me]
    aliases = ssh_aliases()
    ident = d.get("identity")
    # Names and slot counts the owner chose for a known host survive a re-discovery.
    known = {h.get("ssh"): h for h in (json.loads(cfg["hosts_file"].read_text())["hosts"]
                                        if cfg["hosts_file"].is_file() else [])}
    found = [known.get(None) or {"name": d.get("local_name", "local"), "ssh": None,
                                 "slots": max(1, (os.cpu_count() or 2) // 3)}]
    probe = ('hostname -s; uname -m; (sysctl -n hw.ncpu 2>/dev/null || nproc); '
             f'b={DEFAULT_BINARY}; [ -x "$b" ] && "$b" -h 2>&1 | head -1 || echo NO_BINARY')
    for ip in candidates:
        target = aliases.get(ip) or f"{d.get('user', os.environ.get('USER', ''))}@{ip}"
        args = [] if ip in aliases or not ident else ["-i", ident, "-o", "IdentitiesOnly=yes"]
        r = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
                            "-o", "StrictHostKeyChecking=accept-new", *[os.path.expanduser(a)
                                                                        for a in args],
                            target, probe], capture_output=True, text=True, timeout=30)
        lines = r.stdout.strip().splitlines()
        if r.returncode != 0 or len(lines) < 4:
            log(f"{ip:16} ssh refused ({(r.stderr.strip().splitlines() or ['?'])[-1][:60]})")
            continue
        name, arch, cpus, version = lines[0].lower(), lines[1], int(lines[2]), lines[3]
        log(f"{ip:16} {name:24} {arch:7} {cpus:3} cpu  {version}")
        if version == "NO_BINARY":
            continue
        prev = known.get(target, {})
        found.append({"name": prev.get("name", aliases.get(ip, name)), "ssh": target,
                      "ssh_args": args, "slots": prev.get("slots", max(1, cpus // 3)),
                      "arch": arch})
    cfg["hosts_file"].parent.mkdir(parents=True, exist_ok=True)
    cfg["hosts_file"].write_text(json.dumps({"hosts": found}, indent=2) + "\n")
    log(f"{len(found)} arena(s) written to {cfg['hosts_file']}")


# --------------------------------------------------------------------------------- arena bundle

def arena_digest(cache: Path) -> str:
    h = hashlib.sha256()
    for p in sorted((cache / "arena").rglob("*")):
        if p.is_file() and p.name != "PROVENANCE.json":
            h.update(str(p.relative_to(cache)).encode())
            h.update(p.read_bytes())
    return h.hexdigest()[:16]


def edition_of(cfg: dict) -> str:
    """The edition: the arena bundle plus everything else that decides a match (contestants with
    their settings, the match parameters). Changing any of them starts a new edition."""
    rules = json.dumps({"contestants": cfg["contestants"], "match": cfg["match"]}, sort_keys=True)
    return hashlib.sha256((arena_digest(cfg["cache"]) + rules).encode()).hexdigest()[:16]


def export_tree(path: str, rev: str, dest_tar: Path) -> None:
    """`git archive` a repository directory with its parents stripped: <basename>/... in the tar."""
    raw = subprocess.run(["git", "-C", str(REPO), "archive", "--format=tar", rev, path],
                         capture_output=True, check=True).stdout
    prefix = path.rstrip("/").rsplit("/", 1)[0] + "/" if "/" in path else ""
    with tarfile.open(fileobj=io.BytesIO(raw)) as src, tarfile.open(dest_tar, "w") as dst:
        for m in src.getmembers():
            if not m.name.startswith(prefix) or len(m.name) <= len(prefix):
                continue
            data = src.extractfile(m) if m.isfile() else None
            m.name = m.name[len(prefix):]
            dst.addfile(m, data)


def cmd_prepare(cfg: dict, allow_dirty: bool) -> None:
    """Assemble <cache>/arena/{baseset,ai,game} plus PROVENANCE.json."""
    arena = cfg["cache"] / "arena"
    subprocess.run(["rm", "-rf", str(arena)], check=True)
    for sub in ("baseset", "ai/library", "game"):
        (arena / sub).mkdir(parents=True, exist_ok=True)
    if not cfg["baseset"].is_file():
        sys.exit(f"base graphics set {cfg['baseset']} not found "
                 "(research/baseset-prerequisite-macos.md)")
    subprocess.run(["cp", str(cfg["baseset"]), str(arena / "baseset")], check=True)

    downloaded = cfg["cache"] / "content" / "content_download" / "ai"
    for tar in sorted(downloaded.rglob("*.tar")) if downloaded.is_dir() else []:
        dest = arena / "ai" / tar.relative_to(downloaded)
        dest.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["cp", str(tar), str(dest)], check=True)

    # PrismaticAI and the referee come from this repository at a commit, never from a work tree.
    p, ref = cfg["prismatic"], cfg["referee"]
    dirty = git("status", "--porcelain", "--", p["path"], ref["path"])
    if dirty and not allow_dirty:
        sys.exit(f"refusing: uncommitted changes under the exported paths:\n{dirty}\n"
                 "commit them, or pass --allow-dirty for a run whose records `check` refuses")
    rev = git("rev-parse", p.get("rev", "HEAD"))
    export_tree(p["path"], rev, arena / "ai" / "PrismaticAI.tar")
    if dirty:  # --allow-dirty: the referee as it is on disk, and the edition marked dirty
        subprocess.run(["cp", "-R", str(REPO / ref["path"]), str(arena / "game")], check=True)
    else:
        export_tree(ref["path"], rev, arena / "game" / "BattleReferee.tar")

    (arena / "PROVENANCE.json").write_text(json.dumps({
        "prismatic_path": p["path"], "referee_path": ref["path"], "rev": rev,
        "dirty": bool(dirty), "baseset": cfg["baseset"].name,
        "ais": sorted(str(t.relative_to(arena / "ai")) for t in (arena / "ai").rglob("*.tar")),
        "prepared_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
    }, indent=2) + "\n")
    log(f"arena prepared: rev {rev[:10]}{' (DIRTY)' if dirty else ''}, "
        f"edition {edition_of(cfg)}")


def cmd_fetch(cfg: dict) -> None:
    """Download the BaNaNaS contestants with a throwaway local dedicated server bound to loopback."""
    home = cfg["cache"] / "content"
    (home / "Documents" / "OpenTTD" / "baseset").mkdir(parents=True, exist_ok=True)
    # Even a dedicated server refuses to start without a base graphics set.
    subprocess.run(["cp", str(cfg["baseset"]), str(home / "Documents" / "OpenTTD" / "baseset")],
                   check=True)
    local = next((h for h in load_hosts(cfg) if h.get("ssh") is None), {})
    binary = str(expand(local.get("binary", DEFAULT_BINARY)))
    # With -c the config dir is the working directory, so content lands in <home>/content_download.
    (home / "fetch.cfg").write_text("[network]\nserver_advertise = false\n")
    proc = subprocess.Popen([binary, "-D", "127.0.0.1:39790", "-c", "fetch.cfg", "-x"],
                            cwd=home, env=dict(os.environ, HOME=str(home)),
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, bufsize=1)
    lines: list[str] = []
    threading.Thread(target=lambda: lines.extend(proc.stdout), daemon=True).start()

    def send(cmd: str, wait: float) -> None:
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()
        time.sleep(wait)

    time.sleep(5)
    send("content update", 10)
    wanted = [c["ai"] for c in cfg["contestants"] if c.get("source") == "bananas"]
    ids = {}
    for name in dict.fromkeys(wanted):
        mark = len(lines)
        send(f"content state {name}", 2)
        for line in lines[mark:]:
            parts = [x.strip() for x in line.split(",")]
            if len(parts) >= 4 and parts[1] == "AI" and parts[3] == name:
                ids[name] = parts[0]
    for cid in ids.values():
        send(f"content select {cid}", 1)
    send("content download", 30)
    send("quit", 3)
    try:
        proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        proc.kill()
    (home / "fetch.log").write_text("".join(lines))
    missing = sorted(set(wanted) - set(ids))
    done = sum(1 for line in lines if line.startswith("Completed download"))
    log(f"fetched {done} package(s) for {sorted(ids)}; missing: {missing or 'none'}")
    if missing:
        sys.exit(12)


# -------------------------------------------------------------------------------------- matches

def lineup_for(contestants: list[dict], n: int, block: int = 1) -> list[dict]:
    """Match n's lineup. Contestants sharing a `group` (the PrismaticAI configurations) take turns,
    one per match: two copies of one bot on one map share its best airport towns and industries
    and starve each other. Everyone else plays every match. The start order rotates once per
    `block` matches, the matches that share one seed. So every member of a group starts from the
    same company slot on a seed, and a paired comparison does not measure the slot. On seed
    371319927 the same genes scored 11.8M from slot 0 and 2.9M from slot 2."""
    groups: dict[str, list[dict]] = {}
    for c in contestants:
        if c.get("group"):
            groups.setdefault(c["group"], []).append(c)
    picked = {g: members[n % len(members)]["key"] for g, members in groups.items()}
    chosen = [c for c in contestants if not c.get("group") or picked[c["group"]] == c["key"]]
    rot = (n // max(1, block)) % len(chosen)
    return chosen[rot:] + chosen[:rot]


def match_ticks(years: int) -> int:
    """Start on Jan 1 and run past Jan 3 of the final year, so its Q4 report is printed."""
    return int((years * 365.25 + 6) * 74)


def battle_cfg_text(mc: dict) -> str:
    return ("[difficulty]\nmax_no_competitors = 0\nmax_loan = 300000\ndisasters = false\n"
            f"vehicle_breakdowns = {mc.get('breakdowns', 1)}\n"
            f"[game_creation]\nlandscape = {mc['landscape']}\nstarting_year = {mc['start_year']}\n"
            f"map_x = {mc['map_x']}\nmap_y = {mc['map_y']}\n"
            "[gui]\nautosave = off\n[graphicsset]\nname = OpenGFX\n"
            "[game_scripts]\nBattleReferee =\n")


def start_script_text(lineup: list[dict]) -> str:
    # The console tokenizer understands double quotes only; AI names may contain spaces.
    return "\n".join(f'start_ai "{c["ai"]}"' + (f" {c['settings']}" if c.get("settings") else "")
                     for c in lineup) + "\n"


def parse_start_script(text: str) -> tuple[list[str], int, str]:
    """Read a match's game_start.scr back into (opponents in start order, PrismaticAI's slot, its
    settings), so an ablate replays the arena's exact lineup: who starts first gets the first sites,
    and a replay against another field is a different game."""
    field, slot, settings = [], 0, ""
    for line in text.splitlines():
        m = re.match(r'\s*start_ai\s+"([^"]+)"\s*(\S*)', line)
        if not m:
            continue
        if m.group(1) == "PrismaticAI":
            slot, settings = len(field), m.group(2)
        else:
            field.append(m.group(1))
    return field, slot, settings


def match_replay_inputs(results: Path, ref: str) -> dict:
    """An arena match's replay inputs from its record (`results/<edition>/matches/<id>.json`):
    `ref` is "<edition prefix>/<m-NNNN>". Returns seed, years, field, slot, settings, the
    contestant's key and the host's build, so `ablate --match` replays the match as it was played."""
    edition, _, mid = ref.partition("/")
    found = sorted(results.glob(f"{edition}*/matches/{mid}.json"))
    if len(found) != 1:
        raise SystemExit(f"ablate: --match {ref}: {len(found)} records found under {results}")
    m = json.loads(found[0].read_text())
    field, slot, settings, key = [], 0, "", ""
    for c in m["lineup"]:
        if c["ai"] == "PrismaticAI":
            slot, settings, key = len(field), c.get("settings", ""), c.get("key", "")
        else:
            field.append(c["ai"])
    return {"seed": m["seed"], "years": m.get("years", 20), "field": field, "slot": slot,
            "settings": settings, "key": key, "host": m.get("host"),
            "host_version": m.get("host_version")}


def match_dir(root: str, m: dict) -> str:
    """Match ids restart in every edition, so the directory is per edition."""
    return f"{root}/matches/{m['edition']}/{m['id']}"


def reap_script(root: str, keep_edition: str) -> str:
    """Stop, through its pid file, every still-running match of another edition on a host: after a
    restart into a new edition (a new bot, config or GA generation) those would only hold slots."""
    return (f'n=0; for d in {root}/matches/*/m-*; do case "$d" in */{keep_edition}/*) continue;; esac; '
            '[ -f "$d/pid" ] || continue; [ -f "$d/exit_code" ] && continue; '
            'kill -0 "$(cat "$d/pid")" 2>/dev/null || continue; kill "$(cat "$d/pid")" && n=$((n+1)); '
            'done; echo "reaped $n"')


def launch_script(host: Host, m: dict, mc: dict, owner: str = "") -> str:
    """The shell that starts one match on its host. `owner` (`<host>:<pid>` of the orchestrator)
    goes into the match's environment (TTD_BATTLE_ORCHESTRATOR, for `ps eww`), the wrapper's argv
    (`ps` shows `ttd-battle[<owner>]`) and <match dir>/orchestrator: a match outlives its ssh session
    and is reparented to init/launchd, so this is how a target's user finds whose it is."""
    root, mdir = host.root, match_dir(host.root, m)
    personal = f"{mdir}/home/Documents/OpenTTD"
    owner = owner or "unknown"
    # A leftover directory would hand its old exit_code and log to this match: refuse it.
    return f"""set -e
export TTD_BATTLE_ORCHESTRATOR={owner}
test ! -e {mdir}
mkdir -p {personal}/scripts
cd {mdir}
echo "$TTD_BATTLE_ORCHESTRATOR" > orchestrator
for d in baseset ai game; do ln -sfn {root}/arena/$d {personal}/$d; done
# Linux (XDG) reads the personal directory from ~/.local/share/openttd: point it at the same one.
mkdir -p {mdir}/home/.local/share && ln -sfn {personal} {mdir}/home/.local/share/openttd
test -f {root}/arena/baseset/*.tar
cat > {personal}/scripts/game_start.scr <<'EOS'
{start_script_text(m['lineup'])}EOS
cat > battle.cfg <<'EOS'
{battle_cfg_text(mc)}EOS
nohup sh -c 'HOME={mdir}/home {host.binary} -c battle.cfg -x -v null:ticks={m['ticks']} \\
  -s null -m null -g -G {m['seed']} -d script=1 > match.log 2>&1 < /dev/null &
  echo $! > pid; wait $!; echo $? > exit_code' 'ttd-battle[{owner}]' > /dev/null 2>&1 < /dev/null &
echo launched
"""


def parse_log(text: str) -> tuple[dict, dict]:
    """({company id: [quarterly samples]}, {company id: Squirrel error count}) from a match log."""
    by_key: dict[tuple, dict] = {}
    errors: dict[int, int] = {}
    for line in text.splitlines():
        m = REF_LINE.search(line)
        if m:
            y, mo, cid, fields, name = m.groups()
            sample = {"t": f"{y}-{int(mo):02d}", "name": name.strip()}
            sample.update({k: int(v) for k, v in REF_FIELD.findall(fields)})
            for k in ("value", "money", "income", "perf", "vehicles", "stations"):
                sample.setdefault(k, 0)
            by_key[(int(cid), int(y), int(mo))] = sample
            continue
        e = SQ_ERROR.search(line)
        if e:
            errors[int(e.group(1))] = errors.get(int(e.group(1)), 0) + 1
    series: dict[int, list] = {}
    for (cid, _y, _mo), s in sorted(by_key.items()):
        series.setdefault(cid, []).append(s)
    return series, errors


def standings_of(m: dict, rules: dict | None = None) -> list[dict]:
    final = []
    for c in m["lineup"]:
        s = m["series"].get(c["key"], [])
        last = s[-1] if s else {}
        errors = m["errors"].get(c["key"], 0)
        final.append({"key": c["key"], "value": last.get("value", 0),
                      "vehicles": last.get("vehicles", 0), "stations": last.get("stations", 0),
                      "income": last.get("income", 0), "errors": errors,
                      "anomalies": anomalies(s, errors, rules or {})})
    final.sort(key=lambda f: (-f["value"], f["key"]))
    return final


def match_status(m: dict, exit_code: int) -> str:
    """done only when the binary exited 0 and the final year's January report exists."""
    last_t = max((s[-1]["t"] for s in m["series"].values() if s), default="")
    expected = f"{m['match_cfg']['start_year'] + m['years']}-01"
    return "done" if exit_code == 0 and last_t >= expected else "incomplete"


SCORING = "points-per-match/v2"


def leaderboard(contestants: list[str], matches: list[dict], scoring: str = SCORING) -> list[dict]:
    """Points per match: last place 0, each place above +1. Replays and unfinished matches don't count."""
    rows = {k: {"key": k, "played": 0, "wins": 0, "points": 0, "rank_sum": 0, "value_sum": 0,
                "errors": 0, "anomalies": 0} for k in contestants}
    for m in matches:
        if m["status"] != "done" or m.get("replica_of"):
            continue
        n = len(m["standings"])
        for rank, f in enumerate(m["standings"]):
            r = rows.setdefault(f["key"], {"key": f["key"], "played": 0, "wins": 0, "points": 0,
                                           "rank_sum": 0, "value_sum": 0, "errors": 0,
                                           "anomalies": 0})
            r["played"] += 1
            r["wins"] += rank == 0
            r["points"] += n - 1 - rank
            r["rank_sum"] += rank + 1
            r["value_sum"] += f["value"]
            r["errors"] += f["errors"]
            r["anomalies"] += bool(f.get("anomalies"))
    out = list(rows.values())
    for r in out:
        r["avg_rank"] = round(r["rank_sum"] / r["played"], 2) if r["played"] else None
        r["avg_value"] = r["value_sum"] // r["played"] if r["played"] else 0
        del r["rank_sum"], r["value_sum"]
        if scoring == SCORING:
            # Grouped contestants play one match in N: rank per match played, not by total.
            r["avg_points"] = round(r["points"] / r["played"], 3) if r["played"] else 0
    if scoring == SCORING:
        out.sort(key=lambda r: (-r["avg_points"], -r["avg_value"], r["key"]))
    else:  # v1 (records without "scoring"): total points
        out.sort(key=lambda r: (-r["points"], -r["avg_value"], r["key"]))
    return out


def determinism(matches: list[dict]) -> list[dict]:
    """Every replay against its original: same seed, lineup and length on another arena."""
    by_id = {m["id"]: m for m in matches}
    out = []
    for m in matches:
        o = by_id.get(m.get("replica_of") or "")
        if m["status"] != "done" or o is None or o["status"] != "done":
            continue
        a = [(f["key"], f["value"]) for f in o["standings"]]
        b = [(f["key"], f["value"]) for f in m["standings"]]
        out.append({"seed": m["seed"], "original": o["id"], "replay": m["id"],
                    "hosts": [o["host"], m["host"]],
                    "versions": [o.get("host_version", ""), m.get("host_version", "")],
                    "identical": a == b})
    return out


def postmortem(m: dict) -> list[dict]:
    """Where each contestant lost to the winner of a finished match: the final gap, the year the
    winner pulled away (twice the value), and the largest deficits by transport mode, fleet size,
    income per vehicle and cargo delivered. Derived from the referee's quarterly samples only."""
    st = m.get("standings") or []
    if not st:
        return []
    win = st[0]["key"]
    ws = m["series"].get(win, [])
    wlast = ws[-1] if ws else {}
    wby_t = {s["t"]: s for s in ws}
    out = []
    for f in st[1:]:
        s = m["series"].get(f["key"], [])
        if not s:
            continue
        last = s[-1]
        behind = next((x["t"] for x in s if x["t"] in wby_t and x["value"] > 0
                       and wby_t[x["t"]]["value"] > 2 * x["value"]), None)
        findings = []
        if wlast.get("vehicles", 0) > 1.5 * max(1, last.get("vehicles", 0)):
            findings.append(f"fleet {last.get('vehicles', 0)} vs {wlast['vehicles']}")
        ipv, wipv = (last.get("income", 0) / max(1, last.get("vehicles", 0)),
                     wlast.get("income", 0) / max(1, wlast.get("vehicles", 0)))
        if wipv > 1.5 * max(1, ipv):
            findings.append(f"income per vehicle {ipv:,.0f} vs {wipv:,.0f}")
        if "cargo" in last and wlast.get("cargo", 0) > 1.5 * max(1, last["cargo"]):
            findings.append(f"cargo delivered/q {last['cargo']:,} vs {wlast['cargo']:,}")
        for mode in MODES:
            mine, theirs = last.get("p" + mode), wlast.get("p" + mode)
            if mine is None or theirs is None:
                continue
            if theirs - mine > 0.2 * max(1, wlast.get("income", 0)) * 4:
                findings.append(f"{mode} profit/yr {mine:,} vs {theirs:,} "
                                f"({last.get(mode, 0)} vs {wlast.get(mode, 0)} vehicles)")
        findings += [f"anomaly {a}" for a in f.get("anomalies", [])]
        out.append({"contestant": f["key"], "winner": win,
                    "gap": round(wlast.get("value", 0) / max(1, last.get("value", 1)), 2),
                    "behind_since": behind,
                    "mix": {mode: last.get(mode) for mode in MODES if mode in last},
                    "winner_mix": {mode: wlast.get(mode) for mode in MODES if mode in wlast},
                    "findings": findings})
    return out


SCHEMA = """
CREATE TABLE IF NOT EXISTS matches (id TEXT, edition TEXT, host TEXT, host_version TEXT,
  seed INTEGER, years INTEGER, ticks INTEGER, status TEXT, exit_code INTEGER, started_at REAL,
  finished_at REAL, replica_of TEXT, lineup TEXT, PRIMARY KEY (edition, id));
CREATE TABLE IF NOT EXISTS samples (edition TEXT, match_id TEXT, contestant TEXT, t TEXT,
  value INTEGER, money INTEGER, income INTEGER, perf INTEGER, vehicles INTEGER, stations INTEGER,
  rail INTEGER, road INTEGER, air INTEGER, water INTEGER, prail INTEGER, proad INTEGER,
  pair INTEGER, pwater INTEGER, cargo INTEGER, expenses INTEGER,
  PRIMARY KEY (edition, match_id, contestant, t));
CREATE TABLE IF NOT EXISTS standings (edition TEXT, match_id TEXT, rank INTEGER, contestant TEXT,
  value INTEGER, vehicles INTEGER, stations INTEGER, errors INTEGER, anomalies TEXT,
  PRIMARY KEY (edition, match_id, contestant));
CREATE TABLE IF NOT EXISTS postmortems (edition TEXT, match_id TEXT, contestant TEXT, winner TEXT,
  gap REAL, behind_since TEXT, findings TEXT, mix TEXT, winner_mix TEXT,
  PRIMARY KEY (edition, match_id, contestant));
CREATE TABLE IF NOT EXISTS host_perf (edition TEXT, match_id TEXT, host TEXT, host_version TEXT,
  game_years REAL, wall_s REAL, years_per_min REAL, PRIMARY KEY (edition, match_id));
CREATE TABLE IF NOT EXISTS events (at REAL, edition TEXT, msg TEXT);
"""
SAMPLE_COLS = ("value", "money", "income", "perf", "vehicles", "stations", "rail", "road", "air",
               "water", "prail", "proad", "pair", "pwater", "cargo", "expenses")


class Store:
    """Every sample, standing, post-mortem, throughput figure and event, in SQLite (machine-friendly;
    DuckDB reads it with ATTACH '<file>' (TYPE sqlite)). One writer: the orchestrator's loop."""

    def __init__(self, path: Path):
        self.path = path
        # WAL: readers (the API, battle-postmortem, a notebook) never block the writer or each other,
        # and a busy timeout instead of "database is locked" when they meet a checkpoint.
        self.db = sqlite3.connect(path, check_same_thread=False, timeout=30)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA busy_timeout=30000")
        self.db.executescript(SCHEMA)
        self.lock = threading.Lock()

    def match(self, m: dict) -> None:
        with self.lock, self.db:
            self.db.execute("INSERT OR REPLACE INTO matches VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)", (
                m["id"], m["edition"], m["host"], m.get("host_version", ""), m["seed"], m["years"],
                m["ticks"], m["status"], m.get("exit_code"), m["started_at"], m.get("finished_at"),
                m.get("replica_of"), json.dumps([{"key": c["key"], "ai": c["ai"],
                                                  "settings": c.get("settings", "")}
                                                 for c in m["lineup"]])))
            rows = [(m["edition"], m["id"], key, s["t"], *[s.get(c) for c in SAMPLE_COLS])
                    for key, series in m["series"].items() for s in series]
            self.db.executemany("INSERT OR REPLACE INTO samples VALUES (" + ",".join("?" * 20) + ")",
                                rows)
            for rank, f in enumerate(m.get("standings") or [], 1):
                self.db.execute("INSERT OR REPLACE INTO standings VALUES (?,?,?,?,?,?,?,?,?)", (
                    m["edition"], m["id"], rank, f["key"], f["value"], f["vehicles"],
                    f["stations"], f["errors"], json.dumps(f.get("anomalies", []))))
            for pm in m.get("postmortem") or []:
                self.db.execute("INSERT OR REPLACE INTO postmortems VALUES (?,?,?,?,?,?,?,?,?)", (
                    m["edition"], m["id"], pm["contestant"], pm["winner"], pm["gap"],
                    pm["behind_since"], json.dumps(pm["findings"]), json.dumps(pm["mix"]),
                    json.dumps(pm["winner_mix"])))
            if m.get("perf"):
                p = m["perf"]
                self.db.execute("INSERT OR REPLACE INTO host_perf VALUES (?,?,?,?,?,?,?)", (
                    m["edition"], m["id"], m["host"], m.get("host_version", ""), p["game_years"],
                    p["wall_s"], p["years_per_min"]))

    def event(self, edition: str, msg: str) -> None:
        with self.lock, self.db:
            self.db.execute("INSERT INTO events VALUES (?,?,?)", (time.time(), edition, msg))

    def query(self, sql: str, cap: int = 5000) -> dict:
        """Read-only SQL for the API: one SELECT/WITH statement on a read-only connection."""
        if not re.match(r"\s*(select|with)\b", sql, re.I) or ";" in sql.strip().rstrip(";"):
            return {"error": "one read-only SELECT or WITH statement only"}
        ro = sqlite3.connect(f"file:{self.path}?mode=ro", uri=True, timeout=30)
        try:
            cur = ro.execute(sql)
            cols = [d[0] for d in cur.description or []]
            rows = cur.fetchmany(cap + 1)
            return {"columns": cols, "rows": rows[:cap], "count": min(len(rows), cap),
                    "capped": len(rows) > cap}
        except sqlite3.Error as exc:
            return {"error": str(exc)}
        finally:
            ro.close()


OPENAPI = {
    "openapi": "3.0.3",
    "info": {"title": "LAN AI battle", "version": "1",
             "description": "RESEARCH-ONLY. Read-only view of the running tournament "
                            "(tools/research/ai-battle/battle.py run). See research/ai-battle/README.md."},
    "paths": {
        "/api/state": {"get": {"summary": "Everything the dashboard shows: edition, arenas, live "
                                          "matches with quarterly series, leaderboard, replays, events",
                               "responses": {"200": {"description": "state"}}}},
        "/api/stream": {"get": {"summary": "The same state as server-sent events (event: state), "
                                           "pushed every stream_every_s",
                                "responses": {"200": {"description": "text/event-stream"}}}},
        "/api/leaderboard": {"get": {"summary": "Leaderboard and replay comparison of the edition",
                                     "responses": {"200": {"description": "leaderboard"}}}},
        "/api/hosts": {"get": {"summary": "Arenas: state, build, slots, running and done matches",
                               "responses": {"200": {"description": "hosts"}}}},
        "/api/matches": {"get": {"summary": "Every match of the edition: status, anomalies, standings",
                                 "responses": {"200": {"description": "matches"}}}},
        "/api/postmortems": {"get": {"summary": "Where each contestant lost to the winner, per "
                                                 "finished match of the edition (newest first)",
                                      "responses": {"200": {"description": "post-mortems"}}}},
        "/api/tuning": {"get": {"summary": "Self-tuning: the generation, every variant's settings, "
                                           "parents and fitness, the anchors, and the history",
                                "responses": {"200": {"description": "tuning state"}}}},
        "/api/promotions": {"get": {"summary": "Every contestant paired with the default on common seeds, "
                                               "and the configurations that qualify as its successor",
                                    "responses": {"200": {"description": "paired, promotions"}}}},
        "/api/perf": {"get": {"summary": "Throughput per arena: game years per wall-clock minute",
                              "responses": {"200": {"description": "per-host throughput"}}}},
        "/api/query": {"get": {"summary": "Read-only SQL over the store (tables matches, samples, "
                                          "standings, postmortems, host_perf, events)",
                               "parameters": [{"name": "sql", "in": "query", "required": True,
                                               "schema": {"type": "string"}}],
                               "responses": {"200": {"description": "{columns, rows, count, capped}"}}}},
        "/api/matches/{id}": {"get": {"summary": "One match with its full quarterly series",
                                      "parameters": [{"name": "id", "in": "path", "required": True,
                                                      "schema": {"type": "string"}}],
                                      "responses": {"200": {"description": "match"},
                                                    "404": {"description": "no such match"}}}},
    },
}

SWAGGER_PAGE = """<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>LAN AI Battle API</title>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/5.17.14/swagger-ui.min.css">
</head><body><div id="ui"></div>
<script src="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/5.17.14/swagger-ui-bundle.min.js"></script>
<script>SwaggerUIBundle({url: '/api/openapi.json', dom_id: '#ui'});</script></body></html>"""


class Orchestrator:
    def __init__(self, cfg: dict, edition: str | None = None):
        self.cfg = cfg
        self.digest = edition or edition_of(cfg)
        self.provenance = json.loads((cfg["cache"] / "arena" / "PROVENANCE.json").read_text())
        self.mdir = cfg["results"] / self.digest / "matches"
        self.mdir.mkdir(parents=True, exist_ok=True)
        # Which code this edition runs: GA generations are new editions of the same bot, and pairing
        # for promotion pools every edition of the same arena revision.
        rev = json.loads((cfg["cache"] / "arena" / "PROVENANCE.json").read_text())["rev"]
        (self.mdir.parent / "edition.json").write_text(json.dumps({"edition": self.digest, "rev": rev}) + "\n")
        self.rev = rev
        self.store = Store(cfg["results"] / "battle.sqlite")
        self.hosts = [Host(h, cfg.get("remote_root", "$HOME/ttd-battle"), cfg.get("resource_floor"))
                      for h in load_hosts(cfg)]
        self.contestants = [dict(c, key=contestant_key(c)) for c in cfg["contestants"]]
        self.matches: dict[str, dict] = {}
        for f in sorted(self.mdir.glob("*.json")):
            m = json.loads(f.read_text())
            self.matches[m["id"]] = m
        if edition and self.matches:
            # A past edition: its contestants are every one its matches were played with (with groups,
            # one match holds only one member of a group), in first-seen order.
            seen: dict[str, dict] = {}
            for m in sorted(self.matches.values(), key=lambda m: m["id"]):
                for c in m["lineup"]:
                    seen.setdefault(c["key"], dict(c))
            self.contestants = list(seen.values())
        self.counter = max([int(m["id"].split("-")[1]) for m in self.matches.values()] + [0])
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.events: list[dict] = []
        self.promoted: set[str] = set()
        # Who this orchestrator is, on every host it touches (ORCHESTRATOR file, match environment).
        self.hostname, self.pid = socket.gethostname(), os.getpid()
        self.owner = owner_marker(self.hostname, self.pid)
        self.started_at = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
        try:
            self.commit = git("rev-parse", "HEAD")
        except (subprocess.CalledProcessError, OSError):
            self.commit = "unknown"
        self.held_by_stop: str | None = None

    def save(self, m: dict) -> None:
        tmp = self.mdir / f"{m['id']}.json.tmp"
        tmp.write_text(json.dumps(m))
        tmp.replace(self.mdir / f"{m['id']}.json")
        self.store.match(m)

    def event(self, msg: str) -> None:
        log(msg)
        with self.lock:
            self.events.append({"at": time.strftime("%H:%M:%S"), "msg": msg})
            del self.events[:-80]
        self.store.event(self.digest, msg)

    # ---- scheduling
    def replay_candidate(self, host: Host) -> dict | None:
        """A finished, never-replayed match from another arena, to replay here."""
        replayed = {m.get("replica_of") for m in self.matches.values()}
        for m in sorted(self.matches.values(), key=lambda m: m["id"]):
            if (m["status"] == "done" and not m.get("replica_of") and m["host"] != host.name
                    and m["id"] not in replayed):
                return m
        return None

    def new_match(self, host: Host) -> dict:
        self.counter += 1
        mc, every = self.cfg["match"], int(self.cfg.get("replay_every", 0))
        original = self.replay_candidate(host) if every and self.counter % every == 0 else None
        if original:
            seed, lineup = original["seed"], original["lineup"]
        else:
            # Common random numbers: the members of a group take turns on the SAME seed, so every
            # variant meets the same maps (one seed per round of the group), from the same slot.
            sizes = {}
            for c in self.contestants:
                if c.get("group"):
                    sizes[c["group"]] = sizes.get(c["group"], 0) + 1
            block = max(sizes.values(), default=1)
            lineup = lineup_for(self.contestants, self.counter, block)
            # The edition is part of the seed: every GA generation meets new maps instead of being
            # judged (and tuned) on the same handful of seeds again and again.
            seed = int(hashlib.sha256(f"{self.cfg['seed']}:{self.digest}:{self.counter // block}".encode())
                       .hexdigest()[:8], 16) % 2_000_000_000
        return {"id": f"m-{self.counter:04d}", "host": host.name, "host_version": host.version,
                "seed": seed, "years": mc["years"], "ticks": match_ticks(mc["years"]),
                "lineup": lineup, "replica_of": original["id"] if original else None,
                "status": "starting", "started_at": time.time(), "finished_at": None,
                "series": {}, "errors": {}, "standings": None, "edition": self.digest,
                "match_cfg": mc}

    def running_on(self, host: Host) -> list[dict]:
        return [m for m in self.matches.values()
                if m["host"] == host.name and m["status"] in ("starting", "running")]

    def reap(self) -> None:
        for host in self.hosts:
            try:
                r = host.sh(reap_script(host.root, self.digest), timeout=30)
            except subprocess.TimeoutExpired:
                continue
            n = r.stdout.strip().replace("reaped ", "")
            if r.returncode == 0 and n not in ("", "0"):
                self.event(f"host {host.name}: stopped {n} match(es) of an older edition")

    def reload_hosts(self) -> None:
        """Pick up hosts added to the local hosts file (e.g. by `discover`), and slot counts changed
        in it (e.g. a host handed back after another experiment), without a restart."""
        known = {h.name: h for h in self.hosts}
        for spec in load_hosts(self.cfg):
            if spec["name"] in known:
                h, slots = known[spec["name"]], int(spec.get("slots", 1))
                if h.slots != slots:
                    self.event(f"host {h.name}: slots {h.slots} -> {slots} from the hosts file")
                    h.slots = slots
                limits = floor_limits(self.cfg.get("resource_floor"), spec)
                if h.limits != limits:
                    self.event(f"host {h.name}: resource floor {limits} from the hosts file")
                    h.limits = limits
            else:
                self.hosts.append(Host(spec, self.cfg.get("remote_root", "$HOME/ttd-battle"),
                                       self.cfg.get("resource_floor")))
                self.event(f"host {spec['name']}: added from the hosts file")

    def tuning_step(self) -> bool:
        """Self-tuning: when every variant of the generation has played its matches, publish the
        round's record, breed the next generation and restart into its edition. True: restarting."""
        t = self.cfg.get("tuning")
        if not t or not t.get("enabled"):
            return False
        state = tuning_state(self.cfg)
        variants = state["variants"]
        ms = list(self.matches.values())
        scored = [(v, fitness_of(v["label"], ms, t.get("last_penalty", 0.0))) for v in variants]
        if any(f["matches"] < t["matches_per_variant"] for _, f in scored):
            return False
        anchors = {k: fitness_of(k, ms) for k in t.get("anchors", [])}
        self.event(f"TUNING generation {state['generation']} done: " + "; ".join(
            f"{v['label']} {f['score']}" for v, f in sorted(scored, key=lambda x: -x[1]["score"])))
        try:
            cmd_report(self.cfg, False, self.digest)
        except Exception as exc:  # the record is a by-product; tuning goes on
            self.event(f"TUNING report failed: {exc}")
        state["history"].append({"generation": state["generation"], "edition": self.digest,
                                 "at": time.time(), "anchors": anchors,
                                 "variants": [{"label": v["label"], "genes": v["genes"],
                                               "parents": v["parents"], "fitness": f}
                                              for v, f in scored]})
        state["variants"] = next_generation(t, state, scored)
        state["generation"] += 1
        tuning_path(self.cfg).write_text(json.dumps(state, indent=1) + "\n")
        # Matches of the finished generation that still run are stopped through their pid files.
        for m in ms:
            if m["status"] == "running":
                host = next((h for h in self.hosts if h.name == m["host"]), None)
                if host:
                    host.try_sh(f"kill $(cat {match_dir(host.root, m)}/pid) 2>/dev/null; true")
                m["status"] = "abandoned"
                self.save(m)
        self.event(f"TUNING generation {state['generation']}: " + ", ".join(
            f"{v['label']} [{v['settings']}]" for v in state["variants"]) + "; restarting")
        return True

    def stop_file_holds(self) -> bool:
        """True while <local root>/STOP exists: no new match anywhere. Logs when it appears and goes."""
        why = stop_requested(self.cfg["cache"])
        if why is not None and self.held_by_stop is None:
            self.event(f"STOP: {self.cfg['cache'] / 'STOP'} exists ({why}): no new match on any host; "
                       "running matches finish and are still read")
        elif why is None and self.held_by_stop is not None:
            self.event(f"STOP: {self.cfg['cache'] / 'STOP'} removed: scheduling resumes")
        self.held_by_stop = why
        return why is not None

    def orchestrator_file(self, host: Host) -> str:
        return orchestrator_text(self.hostname, self.pid, self.started_at, self.commit, self.rev,
                                 self.digest, host.root, str(self.cfg["cache"]))

    def schedule_pass(self) -> None:
        self.reload_hosts()
        if self.stop_file_holds():
            return
        for host in self.hosts:
            if time.time() < host.next_probe:
                continue
            prev = host.state
            host.probe()
            n = host.admit(len(self.running_on(host)))
            if host.state != prev:
                self.event(f"host {host.name}: {prev} -> {host.state} {host.detail}".strip())
            unparsed = tuple(host.res.get("unparsed", [])) if host.state != "offline" else ()
            if unparsed and unparsed != getattr(host, "unparsed_logged", ()):
                self.event(f"host {host.name}: probe figures not understood, not blocking on them: "
                           + "; ".join(unparsed)[:300])
            host.unparsed_logged = unparsed
            if host.state in ("offline", "no-binary", "error"):
                host.next_probe = time.time() + self.cfg.get("offline_probe_every_s", 300)
                continue
            if n <= 0:   # paused, busy, or every slot taken: probed again next pass
                continue
            try:
                host.sync_arena(self.cfg["cache"], self.digest)
            except Exception as exc:  # one broken host must not stop the others
                host.state, host.detail = "error", str(exc)[:200]
                self.event(f"host {host.name}: arena sync failed: {exc}")
                continue
            if not host.claimed:
                # A match nobody on the target can trace back to this process is not started.
                if not host.claim(self.orchestrator_file(host)):
                    host.state, host.detail = "error", f"cannot write {host.root}/ORCHESTRATOR"
                    self.event(f"host {host.name}: {host.detail}; no match started there")
                    continue
                self.event(f"host {host.name}: wrote {host.root}/ORCHESTRATOR ({self.owner})")
            for _ in range(n):
                if self.stop.is_set() or self.stop_file_holds():
                    break
                m = self.new_match(host)
                r = host.try_sh(launch_script(host, m, self.cfg["match"], self.owner))
                m["status"] = "running" if "launched" in r.stdout else "failed"
                tag = f" (replay of {m['replica_of']})" if m["replica_of"] else ""
                self.event(f"{m['id']} started on {host.name}{tag}, seed {m['seed']}"
                           if m["status"] == "running" else
                           f"{m['id']} failed to launch on {host.name}: {r.stderr.strip()[:200]}")
                with self.lock:
                    self.matches[m["id"]] = m
                self.save(m)
                if m["status"] == "failed":
                    break

    # ---- observation
    def poll_host(self, host: Host) -> None:
        running = self.running_on(host)
        # A paused or busy host takes no new match, but the ones it runs are still read to the end.
        if not running or host.state not in ("online", "busy", "paused"):
            return
        parts = []
        for m in running:
            d = match_dir(host.root, m)
            parts.append(f"echo '@@@ {m['id']}'; cat {d}/exit_code 2>/dev/null || echo RUNNING; "
                         f"grep -a -E 'REF Q|\\] \\[S\\] ' {d}/match.log 2>/dev/null | tail -n 4000")
        try:
            r = host.sh("; ".join(parts), timeout=30)
        except subprocess.TimeoutExpired:
            return
        if r.returncode == 255:
            return
        for chunk in r.stdout.split("@@@ ")[1:]:
            mid, _, rest = chunk.partition("\n")
            status_line, _, body = rest.partition("\n")
            m = self.matches.get(mid.strip())
            if m is None:
                continue
            series, errors = parse_log(body)
            samples = sum(len(v) for v in series.values())
            if samples > m.get("samples", 0):
                m["samples"], m["progress_at"] = samples, time.time()
            lineup = m["lineup"]
            # Company ids follow the start order: a `-g` null-driver game has no human company.
            m["series"] = {lineup[c]["key"]: s for c, s in series.items() if c < len(lineup)}
            m["errors"] = {lineup[c]["key"]: n for c, n in errors.items() if c < len(lineup)}
            self.watch(m)
            if status_line.strip() != "RUNNING":
                self.finish(m, int(status_line.strip() or -1))
            elif time.time() - m.get("progress_at", m["started_at"]) > self.cfg.get("stall_s", 600):
                # A hung binary (e.g. a blocking OS error dialog) never writes exit_code.
                d = match_dir(host.root, m)
                host.try_sh(f"kill $(cat {d}/pid) 2>/dev/null; echo 137 > {d}/exit_code")
                self.event(f"{m['id']} on {host.name}: no referee output for "
                           f"{self.cfg.get('stall_s', 600)}s, killed")
            else:
                self.save(m)

    def watch(self, m: dict) -> None:
        """Self-monitoring: every contestant's anomalies, live; a new one goes to the event log."""
        rules = self.cfg.get("anomaly_rules", {})
        live = {c["key"]: anomalies(m["series"].get(c["key"], []), m["errors"].get(c["key"], 0), rules)
                for c in m["lineup"]}
        seen = m.setdefault("alerted", {})
        for key, found in live.items():
            for a in found:
                kind = a.split(":")[0]
                if kind not in seen.get(key, []):
                    seen.setdefault(key, []).append(kind)
                    self.event(f"ALERT {m['id']} on {m['host']}: {key} {a}")
        m["anomalies"] = {k: v for k, v in live.items() if v}

    def sibling_matches(self) -> list[dict]:
        """Finished matches of every edition that runs the same arena revision (GA generations)."""
        out = [m for m in self.matches.values() if m.get("status") == "done"]
        for ed in self.cfg["results"].glob("*/edition.json"):
            try:
                info = json.loads(ed.read_text())
            except ValueError:
                continue
            if info.get("rev") != self.rev or info.get("edition") == self.digest:
                continue
            for f in (ed.parent / "matches").glob("*.json"):
                m = json.loads(f.read_text())
                if m.get("status") == "done":
                    out.append(m)
        return out

    def announce_promotions(self) -> None:
        for p in promotions(self.sibling_matches(), self.cfg.get("promotion", {})):
            key = p["contestant"]
            if key not in self.promoted:
                self.promoted.add(key)
                self.event(f"PROMOTE {key}: beat {p['base']} on {p['wins']}/{p['pairs']} common seeds, "
                           f"mean ratio {p['ratio']}; candidate for the next default")

    def finish(self, m: dict, code: int) -> None:
        self.watch(m)
        m["standings"] = standings_of(m, self.cfg.get("anomaly_rules", {}))
        m["exit_code"], m["finished_at"] = code, time.time()
        m["status"] = match_status(m, code)
        m["postmortem"] = postmortem(m)
        wall = m["finished_at"] - m["started_at"]
        last_t = max((s[-1]["t"] for s in m["series"].values() if s), default="")
        years = (int(last_t[:4]) + (int(last_t[5:]) - 1) / 12 - m["match_cfg"]["start_year"]) if last_t else 0
        m["perf"] = {"game_years": round(years, 2), "wall_s": round(wall, 1),
                     "years_per_min": round(years / max(wall, 1) * 60, 2)}
        self.save(m)
        self.announce_promotions()
        for pm in m["postmortem"]:
            if pm["contestant"].startswith("PrismaticAI") and pm["findings"]:
                self.event(f"POSTMORTEM {m['id']}: {pm['contestant']} {pm['gap']}x behind "
                           f"{pm['winner']}" + (f" since {pm['behind_since']}" if pm["behind_since"] else "")
                           + ": " + "; ".join(pm["findings"][:3]))
        w = m["standings"][0] if m["standings"] else None
        self.event(f"{m['id']} on {m['host']} {m['status']}"
                   + (f": winner {w['key']} ({w['value']:,})" if w else ""))

    # ---- views
    def tuning_view(self) -> dict | None:
        t = self.cfg.get("tuning")
        if not t or not t.get("enabled"):
            return None
        state = tuning_state(self.cfg)
        ms = list(self.matches.values())
        return {"generation": state["generation"], "matches_per_variant": t["matches_per_variant"],
                "space": t["space"],
                "variants": [{"label": v["label"], "settings": v["settings"], "parents": v["parents"],
                              "fitness": fitness_of(v["label"], ms, t.get("last_penalty", 0.0))}
                             for v in state["variants"]],
                "anchors": {k: fitness_of(k, ms) for k in t.get("anchors", [])},
                "history": [{"generation": h["generation"], "best": max(
                    (v["fitness"]["score"] for v in h["variants"]), default=None),
                    "anchors": {k: a["score"] for k, a in h["anchors"].items()}}
                    for h in state["history"]]}

    def perf(self) -> dict:
        """Game years per wall-clock minute per arena, over its finished matches of the edition."""
        out = {}
        for m in self.matches.values():
            p = m.get("perf")
            if p and m["status"] == "done":
                h = out.setdefault(m["host"], {"matches": 0, "game_years": 0.0, "wall_s": 0.0})
                h["matches"] += 1
                h["game_years"] += p["game_years"]
                h["wall_s"] += p["wall_s"]
        for h in out.values():
            h["years_per_min"] = round(h["game_years"] / max(h["wall_s"], 1) * 60, 2)
        return out

    def state(self) -> dict:
        now = time.time()
        if now - getattr(self, "_pair_at", 0) > 30:  # pooled over sibling editions: cached 30 s
            pool = self.sibling_matches()
            self._paired = paired_vs(pool, self.cfg.get("promotion", {}).get("base", "PrismaticAI"))
            self._promos = promotions(pool, self.cfg.get("promotion", {}))
            self._pair_at = now
        paired, promos = self._paired, self._promos
        with self.lock:
            matches = sorted(self.matches.values(), key=lambda m: m["id"], reverse=True)
            keys = [c["key"] for c in self.contestants]
            return {
                "now": time.time(), "edition": self.digest, "provenance": self.provenance,
                "match_cfg": self.cfg["match"], "contestants": keys,
                "hosts": [{"name": h.name, "state": h.state, "detail": h.detail,
                           "version": h.version, "slots": h.slots,
                           "running": len(self.running_on(h)),
                           "done": sum(1 for m in matches
                                       if m["host"] == h.name and m["status"] == "done")}
                          for h in self.hosts],
                "live": [m for m in matches if m["status"] == "running"],
                "recent": [{k: m.get(k) for k in ("id", "host", "seed", "status", "standings",
                                                  "replica_of", "years")}
                           for m in matches if m["status"] not in ("running", "starting")][:30],
                "leaderboard": leaderboard(keys, matches),
                "determinism": determinism(matches),
                "perf": self.perf(),
                "tuning": self.tuning_view(),
                "paired": paired, "promotions": promos,
                "postmortems": [{"match": m["id"], "postmortem": m["postmortem"]} for m in matches
                                if m.get("postmortem")][:12],
                "events": list(self.events),
            }

    def run(self, port: int) -> None:
        orch = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def send_body(self, body: bytes, ctype: str, code: int = 200):
                self.send_response(code)
                self.send_header("Content-Type", ctype)
                self.send_header("Cache-Control", "no-store")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass  # the client (a poller or a closed tab) left before the reply

            def json(self, data, code: int = 200):
                self.send_body(json.dumps(data).encode(), "application/json", code)

            def do_GET(self):  # noqa: N802 (http.server's naming)
                path = self.path.split("?", 1)[0]
                if path in ("/", "/index.html"):
                    self.send_body((HERE / "dashboard" / "index.html").read_bytes(),
                                   "text/html; charset=utf-8")
                elif path == "/docs":
                    self.send_body(SWAGGER_PAGE.encode(), "text/html; charset=utf-8")
                elif path == "/api/openapi.json":
                    self.json(OPENAPI)
                elif path == "/api/state":
                    self.json(orch.state())
                elif path == "/api/leaderboard":
                    st = orch.state()
                    self.json({"edition": st["edition"], "leaderboard": st["leaderboard"],
                               "determinism": st["determinism"]})
                elif path == "/api/hosts":
                    self.json(orch.state()["hosts"])
                elif path == "/api/tuning":
                    self.json(orch.tuning_view() or {"enabled": False})
                elif path == "/api/promotions":
                    st = orch.state()
                    self.json({"paired": st["paired"], "promotions": st["promotions"]})
                elif path == "/api/perf":
                    self.json(orch.perf())
                elif path == "/api/postmortems":
                    self.json([{"match": m["id"], "host": m["host"], "seed": m["seed"],
                                "postmortem": m.get("postmortem", [])}
                               for m in sorted(orch.matches.values(), key=lambda m: m["id"],
                                               reverse=True) if m.get("postmortem")])
                elif path == "/api/query":
                    from urllib.parse import parse_qs, urlsplit
                    sql = (parse_qs(urlsplit(self.path).query).get("sql") or [""])[0]
                    res = orch.store.query(sql)
                    self.json(res, 400 if "error" in res else 200)
                elif path.startswith("/api/matches/"):
                    m = orch.matches.get(path.rsplit("/", 1)[1])
                    self.json(m if m else {"error": "no such match"}, 200 if m else 404)
                elif path == "/api/matches":
                    self.json([{k: m.get(k) for k in ("id", "host", "seed", "status", "replica_of",
                                                      "anomalies", "standings")}
                               for m in sorted(orch.matches.values(), key=lambda m: m["id"])])
                elif path == "/api/stream":
                    # Server-sent events: the whole state every stream_every_s, until the client goes.
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    self.send_header("Cache-Control", "no-store")
                    self.send_header("Access-Control-Allow-Origin", "*")
                    self.end_headers()
                    try:
                        while not orch.stop.is_set():
                            self.wfile.write(b"event: state\ndata: " + json.dumps(orch.state()).encode()
                                             + b"\n\n")
                            self.wfile.flush()
                            orch.stop.wait(orch.cfg.get("stream_every_s", 2))
                    except (BrokenPipeError, ConnectionResetError):
                        pass
                else:
                    self.json({"error": "not found", "see": "/api/openapi.json"}, 404)

            def log_message(self, *args):
                pass

        server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.event(f"dashboard on http://127.0.0.1:{port}/ (edition {self.digest}, "
                   f"rev {self.provenance['rev'][:10]})")
        signal.signal(signal.SIGTERM, lambda *_: self.stop.set())
        signal.signal(signal.SIGINT, lambda *_: self.stop.set())
        self.reap()
        last = 0.0
        restart = False
        while not self.stop.is_set():
            if time.time() - last > self.cfg.get("schedule_every_s", 30):
                if self.tuning_step():
                    restart = True
                    break
                self.schedule_pass()
                last = time.time()
            for host in self.hosts:
                self.poll_host(host)
            self.stop.wait(self.cfg.get("poll_every_s", 4))
        server.shutdown()
        server.server_close()
        if restart:
            # The next generation is a new edition: start over in a fresh process.
            os.execv(sys.executable, [sys.executable, *sys.argv])
        self.event("stopped; running matches keep going and are re-attached by the next `run`")


# ---------------------------------------------------------------------------- records and docs

def build_record(cfg: dict, orch: Orchestrator) -> dict:
    matches = sorted((m for m in orch.matches.values() if m["status"] in ("done", "incomplete")),
                     key=lambda m: m["id"])
    keys = [c["key"] for c in orch.contestants]
    now = dt.datetime.now(dt.timezone.utc)
    hosts = {}
    for m in matches:
        hosts.setdefault(m["host"], set()).add(m.get("host_version", ""))
    return {
        "schema": RECORD_SCHEMA,
        "scoring": SCORING,
        "id": f"battle-{now.strftime('%Y%m%dT%H%M%S')}-{orch.digest[:8]}",
        "generated_at": now.isoformat(timespec="seconds"),
        "edition": orch.digest,
        "provenance": {k: orch.provenance[k] for k in
                       ("prismatic_path", "referee_path", "rev", "dirty", "baseset", "ais")},
        "match_cfg": cfg["match"],
        "contestants": [{"key": c["key"], "ai": c["ai"], "settings": c.get("settings", "")}
                        for c in orch.contestants],
        "hosts": [{"name": h, "versions": sorted(v)} for h, v in sorted(hosts.items())],
        "matches": [{"id": m["id"], "host": m["host"], "host_version": m.get("host_version", ""),
                     "seed": m["seed"], "years": m["years"], "ticks": m["ticks"],
                     "lineup": [c["key"] for c in m["lineup"]], "replica_of": m.get("replica_of"),
                     "status": m["status"], "exit_code": m.get("exit_code"),
                     "standings": [{k: f.get(k, []) for k in ("key", "value", "vehicles",
                                                              "stations", "errors", "anomalies")}
                                   for f in m["standings"] or []]}
                    for m in matches],
        "leaderboard": leaderboard(keys, matches),
        "determinism": determinism(matches),
    }


def money(v: int) -> str:
    for div, unit in ((1e9, " B"), (1e6, " M"), (1e3, " k")):
        if abs(v) >= div:
            return f"{v / div:.1f}{unit}"
    return str(v)


def render_md(rec: dict) -> str:
    p, mc = rec["provenance"], rec["match_cfg"]
    done = [m for m in rec["matches"] if m["status"] == "done" and not m["replica_of"]]
    det = rec["determinism"]
    same = sum(d["identical"] for d in det)
    lines = [
        f"# {rec['id']}", "",
        f"Derived from `{rec['id']}.json` by `tools/research/ai-battle/battle.py report`; "
        "do not edit.", "",
        f"- Edition `{rec['edition']}`: PrismaticAI and referee at `{p['rev']}`"
        f"{' (DIRTY)' if p['dirty'] else ''}, base set `{p['baseset']}`",
        f"- {mc['years']}-year free-for-all from {mc['start_year']}, map "
        f"{2 ** mc['map_x']}x{2 ** mc['map_y']} {mc['landscape']}, breakdowns "
        f"{mc.get('breakdowns', 1)}",
        f"- {len(done)} scored match(es) on "
        + ", ".join(f"`{h['name']}` ({', '.join(h['versions'])})" for h in rec["hosts"]),
        f"- Replays: {same} of {len(det)} identical to their original on another arena", "",
    ]
    v2 = rec.get("scoring") == SCORING
    if v2:
        lines += ["| # | Contestant | Played | Wins | Points | Points/match | Avg rank | Avg final value "
                  "| Squirrel errors | Matches with anomalies |",
                  "|---|---|---|---|---|---|---|---|---|---|"]
    else:
        lines += ["| # | Contestant | Played | Wins | Points | Avg rank | Avg final value | Squirrel errors "
                  "| Matches with anomalies |", "|---|---|---|---|---|---|---|---|---|"]
    for i, r in enumerate(rec["leaderboard"], 1):
        lines.append(f"| {i} | {r['key']} | {r['played']} | {r['wins']} | {r['points']} | "
                     + (f"{r.get('avg_points', '-')} | " if v2 else "")
                     + f"{r['avg_rank'] if r['avg_rank'] is not None else '-'} | "
                     f"{money(r['avg_value'])} | {r['errors']} | {r.get('anomalies', 0)} |")
    if det:
        lines += ["", "| Seed | Original | Replay | Arenas | Identical |", "|---|---|---|---|---|"]
        for d in det:
            lines.append(f"| {d['seed']} | {d['original']} | {d['replay']} | "
                         f"{' / '.join(d['hosts'])} | {'yes' if d['identical'] else 'NO'} |")
    lines += ["", "| Match | Arena | Seed | Winner | Value | Last | Anomalies |",
              "|---|---|---|---|---|---|---|"]
    for m in rec["matches"]:
        s = m["standings"]
        flags = "; ".join(f"{f['key']}: {', '.join(f['anomalies'])}" for f in s if f.get("anomalies"))
        lines.append(f"| {m['id']}{' (replay)' if m['replica_of'] else ''} | {m['host']} | "
                     f"{m['seed']} | {s[0]['key'] if s else '-'} | "
                     f"{money(s[0]['value']) if s else '-'} | {s[-1]['key'] if s else '-'} | "
                     f"{flags or '-'} |")
    return "\n".join(lines) + "\n"


def render_region(rec: dict | None) -> str:
    if rec is None:
        return f"{BEGIN}\nNo battle record yet: run `battle.py report`.\n{END}"
    det = rec["determinism"]
    body = render_md(rec).split("\n| Match |")[0].split("\n", 4)[4].rstrip()
    return (f"{BEGIN}\n<!-- derived from research/results/{rec['id']}.json by "
            f"`battle.py report`; `battle.py check` fails when it is stale -->\n"
            f"Latest record: [`{rec['id']}`](../results/{rec['id']}.md) "
            f"({sum(d['identical'] for d in det)}/{len(det)} replays identical).\n\n"
            f"{body}\n{END}")


def tracked_records(pattern: str = RECORD_GLOB, results: Path | None = None,
                    root: Path | None = None) -> list[Path]:
    """The records the next commit can hold, counted like tools/research/readme-scoreboard
    records(): what git tracks plus new files it would not ignore, so a rewrite and `check` agree
    before and after a `git add`; inside a git hook (git exports GIT_INDEX_FILE to pre-commit) only
    the index, because a record written but never added would pass the hook, become the latest in
    the README region and break CI, whose checkout does not have it. Outside a git checkout of the
    repository every record on disk counts."""
    results, root = results or RESULTS_DIR, root or REPO
    env = dict(os.environ)
    if env.get("GIT_INDEX_FILE"):  # pre-commit may export it relative to the hook's directory
        env["GIT_INDEX_FILE"] = os.path.abspath(env["GIT_INDEX_FILE"])
    sources = ["--cached"]
    if not env.get("GIT_INDEX_FILE"):
        sources += ["--others", "--exclude-standard"]

    def run_git(*args: str) -> str:
        return subprocess.run(["git", *args], cwd=results, env=env, check=True,
                              capture_output=True, text=True).stdout
    try:
        if Path(run_git("rev-parse", "--show-toplevel").strip()).resolve() != root.resolve():
            raise ValueError("not a checkout of the repository")
        listed = run_git("ls-files", "-z", *sources, "--", ".").split("\0")
    except (OSError, ValueError, subprocess.CalledProcessError):
        return sorted(results.glob(pattern))
    return sorted(results / f for f in listed
                  if f and "/" not in f and Path(f).match(pattern) and (results / f).exists())


def latest_record() -> dict | None:
    files = tracked_records()
    return json.loads(files[-1].read_text()) if files else None


def replace_region(text: str, region: str) -> str:
    a, b = text.index(BEGIN), text.index(END) + len(END)
    return text[:a] + region + text[b:]


REQUIRED = {"schema": str, "id": str, "generated_at": str, "edition": str, "provenance": dict,
            "match_cfg": dict, "contestants": list, "hosts": list, "matches": list,
            "leaderboard": list, "determinism": list}
REQUIRED_MATCH = ("id", "host", "host_version", "seed", "years", "ticks", "lineup", "status",
                  "standings")


def validate_record(rec: dict, name: str) -> list[str]:
    """What makes a record reproducible: every match names its arena build, seed, start order and
    length, and the edition names a clean commit for PrismaticAI and the referee."""
    errs = [f"{name}: '{k}' missing or not {t.__name__}" for k, t in REQUIRED.items()
            if not isinstance(rec.get(k), t)]
    if errs:
        return errs
    if rec["schema"] != RECORD_SCHEMA:
        errs.append(f"{name}: schema is {rec['schema']!r}, expected {RECORD_SCHEMA!r}")
    if name != f"{rec['id']}.json":
        errs.append(f"{name}: file name does not match id {rec['id']!r}")
    p = rec["provenance"]
    if p.get("dirty") is not False:
        errs.append(f"{name}: edition built from uncommitted PrismaticAI/referee (prepare --allow-dirty)")
    if not re.fullmatch(r"[0-9a-f]{40}", str(p.get("rev", ""))):
        errs.append(f"{name}: provenance.rev is not a full commit hash")
    keys = {c["key"] for c in rec["contestants"]}
    for m in rec["matches"]:
        missing = [k for k in REQUIRED_MATCH if k not in m]
        if missing:
            errs.append(f"{name}: match {m.get('id')} lacks {missing}")
            continue
        if not m["host_version"]:
            errs.append(f"{name}: match {m['id']} does not name its arena build")
        if set(m["lineup"]) - keys:
            errs.append(f"{name}: match {m['id']} lineup names an unknown contestant")
        if m["ticks"] != match_ticks(m["years"]):
            errs.append(f"{name}: match {m['id']} ticks do not match its length")
    if rec["leaderboard"] != leaderboard([c["key"] for c in rec["contestants"]],
                                         [dict(m, standings=m["standings"]) for m in rec["matches"]],
                                         rec.get("scoring", "points/v1")):
        errs.append(f"{name}: leaderboard is not derived from its matches")
    return errs


def cmd_report(cfg: dict, check_only: bool, edition: str | None = None) -> int:
    if check_only:
        return cmd_check()
    if edition and json.loads((cfg["cache"] / "arena" / "PROVENANCE.json").read_text())["rev"] \
            and not (cfg["results"] / edition / "matches").is_dir():
        log(f"no matches of edition {edition}")
        return 12
    orch = Orchestrator(cfg, edition)
    rec = build_record(cfg, orch)
    if not [m for m in rec["matches"] if m["status"] == "done"]:
        log("no finished match in this edition yet; nothing written")
        return 12
    errs = validate_record(rec, f"{rec['id']}.json")
    if errs:
        print("\n".join(errs), file=sys.stderr)
        return 10
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    (RESULTS_DIR / f"{rec['id']}.json").write_text(json.dumps(rec, indent=1) + "\n")
    (RESULTS_DIR / f"{rec['id']}.md").write_text(render_md(rec))
    README.write_text(replace_region(README.read_text(), render_region(rec)))
    log(f"wrote research/results/{rec['id']}.{{json,md}} and the README leaderboard")
    return 0


def cmd_check() -> int:
    errs, drift = [], []
    files = tracked_records()
    for f in files:
        rec = json.loads(f.read_text())
        errs += validate_record(rec, f.name)
        md = f.with_suffix(".md")
        if not md.is_file() or md.read_text() != render_md(rec):
            drift.append(f"{md.name}: not derived from {f.name} (run battle.py report)")
    latest = latest_record()
    cfg_never = json.loads(DEFAULT_CONFIG.read_text()).get("never_last", [])
    if latest and latest["leaderboard"]:
        board = [r for r in latest["leaderboard"] if r["played"]]
        if board and board[-1]["key"] in cfg_never:
            errs.append(f"{latest['id']}: {board[-1]['key']} is last on the leaderboard "
                        "(battle.json never_last; rule project.prismatic-ai-passes-the-headless-fitness-check)")
    text = README.read_text()
    if BEGIN not in text or END not in text:
        errs.append(f"{README.relative_to(REPO)}: battle region markers missing")
    elif replace_region(text, render_region(latest_record())) != text:
        drift.append(f"{README.relative_to(REPO)}: leaderboard is not derived from the latest record")
    for line in errs + drift:
        print(line, file=sys.stderr)
    if errs:
        return 10
    if drift:
        return 11
    n = len(files)
    print(f"battle check: {n} record(s) valid, README leaderboard derived")
    return 0


def fleet_peak_final(series: list[dict]) -> tuple[int, int]:
    """(peak, final) vehicle count over a contestant's quarterly samples."""
    counts = [s["vehicles"] for s in series] or [0]
    return max(counts), counts[-1]


def anomalies(series: list[dict], errors: int, rules: dict) -> list[str]:
    """What looks wrong with one contestant in one match. The orchestrator attaches these to every
    match and the fitness check fails on them. `rules`: fleet_min_peak, fleet_keep (fraction)."""
    out = []
    if errors:
        out.append(f"squirrel_errors:{errors}")
    peak, final = fleet_peak_final(series)
    if peak >= rules.get("fleet_min_peak", 10) and final < rules.get("fleet_keep", 0.7) * peak:
        out.append(f"fleet_drop:{peak}->{final}")
    if series and len(series) >= 8 and series[-1]["vehicles"] == 0:
        out.append("no_vehicles")
    return out


def fitness_verdict(m: dict, fit: dict) -> list[str]:
    """The fitness check's failures for one finished local match (empty: PASS)."""
    fails = []
    if m["status"] != "done":
        fails.append(f"match {m['status']} (exit {m.get('exit_code')}): the bot or the referee broke")
    for c in m["lineup"]:
        if not c["ai"].startswith("PrismaticAI"):
            continue
        bad = anomalies(m["series"].get(c["key"], []), m["errors"].get(c["key"], 0), fit["rules"])
        # Every PrismaticAI must not crash; only the ones named healthy must also keep their fleet.
        if c["key"] not in fit.get("must_be_healthy", []):
            bad = [b for b in bad if b.startswith("squirrel")]
        fails += [f"{c['key']}: {b}" for b in bad]
    # The owner's order (2026-11-19): PrismaticAI is never last. By final value on the fitness seed.
    finals = sorted(((s[-1]["value"] if s else 0), k) for k, s in
                    ((c["key"], m["series"].get(c["key"], [])) for c in m["lineup"]))
    if finals and finals[0][1] in fit.get("never_last", []):
        fails.append(f"{finals[0][1]}: last of {len(finals)} (value {finals[0][0]:,})")
    return fails


def cmd_fitness(cfg: dict) -> int:
    """A short local match of the working tree's PrismaticAI (not an exported commit: this is the
    check that runs before a commit) against a reference AI, on a fixed seed. Deterministic: the null
    driver runs on ticks, so the same tree and binary give the same verdict."""
    fit = cfg["fitness"]
    arena = cfg["cache"] / "arena"
    binary = REPO / "build" / "openttd"
    ref_tars = [t for t in (arena / "ai").glob("*.tar") if t.name != "PrismaticAI.tar"] if (
        arena / "ai").is_dir() else []
    if not binary.is_file() or not (arena / "baseset").is_dir() or not ref_tars:
        print(f"fitness: NOT RUN: needs {binary.relative_to(REPO)} and an arena cache with a base set "
              f"and reference AIs at {arena} (battle.py fetch && battle.py prepare)")
        return 0
    import tempfile
    with tempfile.TemporaryDirectory(prefix="battle-fitness-") as tmp:
        home = Path(tmp) / "home"
        personal = home / "Documents" / "OpenTTD"
        (personal / "scripts").mkdir(parents=True)
        (personal / "ai").mkdir()
        (personal / "game").mkdir()
        os.symlink(arena / "baseset", personal / "baseset")
        (home / ".local" / "share").mkdir(parents=True)
        os.symlink(personal, home / ".local" / "share" / "openttd")   # Linux (XDG)
        subprocess.run(["cp", "-R", str(REPO / cfg["prismatic"]["path"]), str(personal / "ai")],
                       check=True)
        subprocess.run(["cp", "-R", str(arena / "ai" / "library"), *map(str, ref_tars),
                        str(personal / "ai")], check=True)
        subprocess.run(["cp", "-R", str(REPO / cfg["referee"]["path"]), str(personal / "game")],
                       check=True)
        mc = dict(cfg["match"], years=fit["years"])
        (Path(tmp) / "battle.cfg").write_text(battle_cfg_text(mc))
        t0 = time.time()
        fails: list[str] = []
        # One match per lineup, run in parallel (one single-threaded OpenTTD process each).
        lineups = [[dict(c, key=contestant_key(c)) for c in lu] for lu in fit["lineups"]]
        procs = []
        for i, lineup in enumerate(lineups):
            mhome = Path(tmp) / f"m{i}"
            mper = mhome / "Documents" / "OpenTTD"
            (mper / "scripts").mkdir(parents=True)
            for d in ("baseset", "ai", "game"):
                os.symlink(personal / d, mper / d)
            (mhome / ".local" / "share").mkdir(parents=True)
            os.symlink(mper, mhome / ".local" / "share" / "openttd")
            (mper / "scripts" / "game_start.scr").write_text(start_script_text(lineup))
            procs.append(subprocess.Popen(
                [str(binary), "-c", "battle.cfg", "-x", "-v", f"null:ticks={match_ticks(fit['years'])}",
                 "-s", "null", "-m", "null", "-g", "-G", str(fit["seed"]), "-d", "script=1"],
                cwd=tmp, env=dict(os.environ, HOME=str(mhome)), stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True))
        for i, (lineup, proc) in enumerate(zip(lineups, procs)):
            out, _ = proc.communicate(timeout=fit.get("timeout_s", 600))
            series, errors = parse_log(out)
            m = {"lineup": lineup, "years": fit["years"], "match_cfg": mc,
                 "exit_code": proc.returncode,
                 "series": {lineup[c]["key"]: s for c, s in series.items() if c < len(lineup)},
                 "errors": {lineup[c]["key"]: n for c, n in errors.items() if c < len(lineup)}}
            m["status"] = match_status(m, proc.returncode)
            for c in lineup:
                s = m["series"].get(c["key"], [])
                peak, final = fleet_peak_final(s)
                print(f"fitness[{i}]: {c['key']:26} value {s[-1]['value'] if s else 0:>12,}  vehicles "
                      f"peak {peak:>4} final {final:>4}  "
                      f"{' '.join(anomalies(s, m['errors'].get(c['key'], 0), fit['rules'])) or 'ok'}")
            fails += [f"[{i}] {f}" for f in fitness_verdict(m, fit)]
    for f in fails:
        print(f"fitness: FAIL {f}", file=sys.stderr)
    print(f"fitness: {'FAIL' if fails else 'PASS'} ({len(lineups)} match(es), {fit['years']} years, "
          f"seed {fit['seed']}, {time.time() - t0:.0f} s)")
    return 10 if fails else 0


def cmd_ablate(cfg: dict, seed: int, years: int, variants: list[str], field: list[str],
               write: bool, log_dir: str | None = None, slot: int = 0) -> int:
    """Run one local match per variant of the working tree's PrismaticAI (settings "name=a=1,b=2"),
    each against the same field on the same seed, in parallel, and print (and with --write record
    under research/results/) every variant's final standing. Deterministic: same tree, binary and
    seed, same table."""
    arena = cfg["cache"] / "arena"
    binary = REPO / "build" / "openttd"
    if not binary.is_file() or not (arena / "baseset").is_dir():
        print("ablate: NOT RUN: needs build/openttd and a prepared arena cache")
        return 12
    import tempfile
    mc = dict(cfg["match"], years=years)
    rows = []
    with tempfile.TemporaryDirectory(prefix="battle-ablate-") as tmp:
        ai = Path(tmp) / "ai"
        ai.mkdir()
        subprocess.run(["cp", "-R", str(REPO / cfg["prismatic"]["path"]), str(ai)], check=True)
        subprocess.run(["cp", "-R", str(arena / "ai" / "library"), *[str(t) for t in (arena / "ai").glob("*.tar")
                        if t.name != "PrismaticAI.tar"], str(ai)], check=True)
        (Path(tmp) / "battle.cfg").write_text(battle_cfg_text(mc))
        procs = []
        for v in variants:
            name, _, settings = v.partition("=")
            home = Path(tmp) / f"h-{name}"
            per = home / "Documents" / "OpenTTD"
            (per / "scripts").mkdir(parents=True)
            os.symlink(ai, per / "ai")
            os.symlink(arena / "baseset", per / "baseset")
            os.symlink(arena / "game", per / "game")
            (home / ".local" / "share").mkdir(parents=True)
            os.symlink(per, home / ".local" / "share" / "openttd")
            # `slot`: PrismaticAI's place in the start order (company id), as in the arena's match;
            # who starts first gets the first airport sites, so a replay must keep it.
            lineup = [{"ai": f, "key": f} for f in field]
            lineup.insert(min(slot, len(lineup)), {"ai": "PrismaticAI", "key": name, "settings": settings})
            (per / "scripts" / "game_start.scr").write_text(start_script_text(lineup))
            procs.append((name, settings, lineup, subprocess.Popen(
                [str(binary), "-c", "battle.cfg", "-x", "-v", f"null:ticks={match_ticks(years)}", "-s", "null",
                 "-m", "null", "-g", "-G", str(seed), "-d", f"script={4 if log_dir else 1}"], cwd=tmp,
                env=dict(os.environ, HOME=str(home)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)))
        for name, settings, lineup, proc in procs:
            out, _ = proc.communicate(timeout=1800)
            if log_dir:
                Path(log_dir).mkdir(parents=True, exist_ok=True)
                (Path(log_dir) / f"{name}.log").write_text(out)
            series, errors = parse_log(out)
            me = min(slot, len(field))
            mine = series.get(me, [])
            last = mine[-1] if mine else {}
            field_best = max((s[-1]["value"] for c, s in series.items() if c != me and s), default=0)
            rows.append({"variant": name, "settings": settings, "exit_code": proc.returncode,
                         "value": last.get("value", 0), "money": last.get("money", 0),
                         "vehicles": last.get("vehicles", 0), "rail": last.get("rail"), "road": last.get("road"),
                         "air": last.get("air"), "errors": errors.get(0, 0), "field_best": field_best,
                         "t": last.get("t", "")})
    base = rows[0]["value"] or 1
    for r in rows:
        r["vs_first"] = round(r["value"] / base - 1, 4)
        print(f"ablate: {r['variant']:12} value {r['value']:>12,} ({r['vs_first']:+.1%})  veh {r['vehicles']:>4} "
              f"rail {r['rail']} road {r['road']} air {r['air']}  err {r['errors']}  [{r['settings']}]")
    if write:
        now = dt.datetime.now(dt.timezone.utc)
        rid = f"ablation-{now.strftime('%Y%m%dT%H%M%S')}-seed{seed}"
        rec = {"schema": "ai-battle-ablation/v1", "id": rid, "generated_at": now.isoformat(timespec="seconds"),
               "seed": seed, "years": years, "field": field, "slot": slot, "match_cfg": mc,
               "rev": git("rev-parse", "HEAD"), "tree_dirty": bool(git("status", "--porcelain", "--",
                                                                        cfg["prismatic"]["path"])),
               "rows": rows}
        (RESULTS_DIR / f"{rid}.json").write_text(json.dumps(rec, indent=1) + "\n")
        md = [f"# {rid}", "", "Derived from its .json by `tools/research/ai-battle/battle.py ablate --write`; do not edit.", "",
              f"One local match per variant of PrismaticAI against {', '.join(field)}, seed {seed}, {years} years "
              f"from {mc['start_year']}, at `{rec['rev'][:10]}`{' (tree dirty)' if rec['tree_dirty'] else ''}. "
              "Each value is the variant's last quarterly referee sample. One seed: a direction, not a verdict.", "",
              "| Variant | Value | vs first | Vehicles | Rail | Road | Air | Squirrel errors | Settings |",
              "|---|---|---|---|---|---|---|---|---|"]
        md += [f"| {r['variant']} | {r['value']:,} | {r['vs_first']:+.1%} | {r['vehicles']} | {r['rail']} | {r['road']} | "
               f"{r['air']} | {r['errors']} | `{r['settings']}` |" for r in rows]
        (RESULTS_DIR / f"{rid}.md").write_text("\n".join(md) + "\n")
        log(f"wrote research/results/{rid}.{{json,md}}")
    return 0


def cmd_status(cfg: dict) -> None:
    orch = Orchestrator(cfg)
    print(f"edition {orch.digest} (rev {orch.provenance['rev'][:10]})")
    print(f"{'contestant':40} {'played':>6} {'wins':>5} {'pts':>5} {'avg rank':>8} "
          f"{'avg value':>14} {'sq errs':>7}")
    for r in leaderboard([c["key"] for c in orch.contestants], list(orch.matches.values())):
        print(f"{r['key']:40} {r['played']:6} {r['wins']:5} {r['points']:5} "
              f"{r['avg_rank'] if r['avg_rank'] is not None else '-':>8} "
              f"{r['avg_value']:>14,} {r['errors']:7}")
    for d in determinism(list(orch.matches.values())):
        print(f"replay {d['replay']} of {d['original']} ({' / '.join(d['hosts'])}): "
              f"{'identical' if d['identical'] else 'DIFFERENT'}")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("command", choices=["discover", "fetch", "prepare", "probe", "run", "status",
                                        "report", "check", "fitness", "ablate", "reap"])
    ap.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--allow-dirty", action="store_true",
                    help="prepare: export uncommitted PrismaticAI/referee (records are refused)")
    ap.add_argument("--check", action="store_true", help="report: only check, write nothing")
    ap.add_argument("--seed", type=int, default=930967890, help="ablate: the map seed")
    ap.add_argument("--years", type=int, default=6, help="ablate: game years")
    ap.add_argument("--variant", action="append", default=[],
                    help='ablate: "name=setting=v,setting=v" (the first is the baseline), repeatable')
    ap.add_argument("--field", default="AAAHogEx,AdmiralAI", help="ablate: comma-separated opponents")
    ap.add_argument("--write", action="store_true", help="ablate: record under research/results/")
    ap.add_argument("--slot", type=int, default=0,
                    help="ablate: PrismaticAI's place in the start order (its company id), as in the arena")
    ap.add_argument("--lineup", type=Path,
                    help="ablate: a match's game_start.scr; sets --field and --slot from it and adds "
                         "its PrismaticAI settings as variant 'arena'")
    ap.add_argument("--match", help="ablate: an arena match as <edition prefix>/<m-NNNN>; takes "
                                    "seed, years, field, slot and settings (variant 'arena') from "
                                    "its record")
    ap.add_argument("--log-dir", help="ablate: keep each variant's full log (script debug level 4) here")
    ap.add_argument("--edition", help="report: a past edition (its arena bundle must still be the "
                                      "prepared one, for its provenance)")
    a = ap.parse_args(argv)
    if a.command == "check":
        return cmd_check()
    cfg = load_config(a.config)
    if a.command == "discover":
        cmd_discover(cfg)
    elif a.command == "fetch":
        cmd_fetch(cfg)
    elif a.command == "prepare":
        cmd_prepare(cfg, a.allow_dirty)
    elif a.command == "probe":
        for spec in load_hosts(cfg):
            h = Host(spec, cfg.get("remote_root", "$HOME/ttd-battle"), cfg.get("resource_floor"))
            h.probe()
            n = h.admit(0)
            print(f"{h.name:10} {h.state:10} slots={h.slots} admits={n} {h.version} {h.detail}".rstrip())
            if h.res.get("unparsed"):
                print(f"{'':10} probe figures not understood: {'; '.join(h.res['unparsed'])}")
    elif a.command == "run":
        Orchestrator(cfg).run(a.port)
    elif a.command == "status":
        cmd_status(cfg)
    elif a.command == "report":
        return cmd_report(cfg, a.check, a.edition)
    elif a.command == "fitness":
        return cmd_fitness(cfg)
    elif a.command == "reap":
        Orchestrator(cfg).reap()
    elif a.command == "ablate":
        field, slot, variants = a.field.split(","), a.slot, list(a.variant)
        seed, years = a.seed, a.years
        if a.match:
            r = match_replay_inputs(cfg["results"], a.match)
            seed, years, field, slot = r["seed"], r["years"], r["field"], r["slot"]
            if not any(v.partition("=")[0] == "arena" for v in variants):
                variants.append(f"arena={r['settings']}")
            print(f"ablate: {a.match} ({r['key']}) seed {seed}, slot {slot}, field {','.join(field)}; "
                  f"played on {r['host']} with {r['host_version']}")
        elif a.lineup:
            field, slot, settings = parse_start_script(a.lineup.read_text())
            if not any(v.partition("=")[0] == "arena" for v in variants):
                variants.append(f"arena={settings}")
        return cmd_ablate(cfg, seed, years, variants, field, a.write, a.log_dir, slot)
    return 0


if __name__ == "__main__":
    sys.exit(main())
