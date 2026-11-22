#!/usr/bin/env python3
"""Derive the town-authority model from the OpenTTD sources (RESEARCH-ONLY).

What a town's local authority rewards, what it punishes, what it then allows or refuses, and what a
company can do about it, read from src/ rather than written by hand: every number in the output
comes from a line of the game's source, and every rule carries that file and line. Re-run it after
the engine changes; `--check` fails when the committed outputs no longer match the sources.

Outputs (all generated, do not edit):
  tools/research/town-model/town_model.json            the model, the one source for the others
  tools/research/prismatic-ai/PrismaticAI/town_model.nut  the model as a Squirrel table (bot)
  tools/research/mcp-panel/rhai/town_model.rhai        the model as a Rhai function (scripts, panel)

The model's parts:
  constants  every RATING_* constant of src/town_type.h with its comment, aliases resolved
  bands      the rating bands an AI sees (ScriptTown::GetRating), lowest and highest value of each
  effects    every place a company's rating changes: ChangeTownRating calls and direct writes to
             Town::ratings, with the enclosing function's brief as the trigger, the delta and the
             limit the change stops at
  gates      every comparison of a rating with a threshold: what the authority refuses below it
  actions    the town actions (TownAction), their cost factor and what each one sets
  links      other rules that tie a town to a company's stations (statue bonus, exclusivity, noise)
  settings   the game settings these rules read, with their defaults
  relations  the graph the parts imply: trigger -> rating -> gate, action -> effect
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
SRC = REPO / "src"
OUT_JSON = REPO / "tools/research/town-model/town_model.json"
OUT_NUT = REPO / "tools/research/prismatic-ai/PrismaticAI/town_model.nut"
OUT_RHAI = REPO / "tools/research/mcp-panel/rhai/town_model.rhai"
OUT_DOC = REPO / "research/mechanisms/town-authority.md"  # only between its town-model markers
DOC_BEGIN, DOC_END = "<!-- town-model:begin -->", "<!-- town-model:end -->"


def rel(path):
    return str(path.relative_to(REPO))


def tracked(base, pattern):
    """Files under `base` matching `pattern` that the next commit can hold.

    Counted like tools/surfaces/surfaces.py load_repo: what git tracks plus new files it would not
    ignore, and inside a git hook (git exports GIT_INDEX_FILE) only the index, so a source file
    written but never added cannot shape the derived model that CI then cannot reproduce. Outside
    a git checkout of REPO every file on disk counts.
    """
    env = dict(os.environ)
    if env.get("GIT_INDEX_FILE"):  # a hook may export it relative to the work tree
        env["GIT_INDEX_FILE"] = os.path.abspath(env["GIT_INDEX_FILE"])
    sources = ["--cached"] + ([] if env.get("GIT_INDEX_FILE") else ["--others", "--exclude-standard"])
    try:
        top = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=base, env=env, check=True,
                             capture_output=True, text=True).stdout.strip()
        if Path(top).resolve() != REPO.resolve():
            raise ValueError("not a checkout of REPO")
        out = subprocess.run(["git", "ls-files", "-z", *sources, "--", "."], cwd=base, env=env,
                             check=True, capture_output=True, text=True).stdout
    except (OSError, ValueError, subprocess.CalledProcessError):
        return sorted(base.rglob(pattern))
    return sorted(base / n for n in out.split("\0")
                  if n and Path(n).match(pattern) and (base / n).is_file())


def read_lines(path):
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def git_blob(path):
    """The blob id of a source file as committed, so a reader can tell which source a rule came from."""
    try:
        out = subprocess.run(["git", "hash-object", str(path)], cwd=REPO, capture_output=True, text=True, check=True)
        return out.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


# --- constants ---------------------------------------------------------------------------------

CONST_RE = re.compile(r"static constexpr int (RATING_\w+)\s*=\s*([^;]+);\s*(?:///<\s*(.*))?")


def derive_constants():
    path = SRC / "town_type.h"
    lines = read_lines(path)
    raw = {}
    for i, line in enumerate(lines):
        m = CONST_RE.search(line)
        if not m:
            continue
        name, expr, doc = m.group(1), m.group(2).strip(), (m.group(3) or "").strip()
        if not doc:
            # A trailing // comment, or the block comment just above, documents it.
            tail = re.search(r";\s*//\s*(.*)", line)
            if tail:
                doc = tail.group(1).strip()
        raw[name] = {"expr": expr, "doc": doc, "source": f"{rel(path)}:{i + 1}"}
    values = {}

    def value(name, seen=()):
        if name in values:
            return values[name]
        if name in seen:
            raise ValueError(f"cyclic constant {name}")
        expr = raw[name]["expr"]
        for ref in re.findall(r"RATING_\w+", expr):
            expr = expr.replace(ref, str(value(ref, seen + (name,))))
        values[name] = int(expr)  # every RATING_* is a literal or another constant
        return values[name]

    out = {}
    for name, c in raw.items():
        out[name] = {"value": value(name), "expr": c["expr"], "doc": c["doc"], "source": c["source"]}
    return out


def resolve(expr, constants):
    """An expression's value when it is a constant, a literal or its negation; None otherwise."""
    e = expr.strip()
    neg = e.startswith("-")
    if neg:
        e = e[1:].strip()
    if e in constants:
        v = constants[e]["value"]
    elif re.fullmatch(r"\d+", e):
        v = int(e)
    else:
        return None
    return -v if neg else v


# --- functions and their briefs ----------------------------------------------------------------

FUNC_RE = re.compile(r"^(?:static\s+|/\* static \*/\s*)?(?:[\w:<>,\s\*&]+?)\b([A-Za-z_]\w*)\s*\(([^;]*)$")


def enclosing_function(lines, idx):
    """Name, line and Doxygen brief of the function whose body contains line idx."""
    for j in range(idx, -1, -1):
        line = lines[j]
        if line.startswith("{") and j > 0:
            k = j - 1
            header = lines[k]
            while k > 0 and not FUNC_RE.match(header) and not header.strip() == "":
                k -= 1
                header = lines[k] + header
            m = FUNC_RE.match(lines[k])
            if not m:
                continue
            brief = doc_brief(lines, k)
            ct = re.fullmatch(r"ClearTile_(\w+)", m.group(1))
            if ct and brief.startswith("(as "):
                brief = f"A {ct.group(1).lower()} tile is cleared (ClearTileProc)."
            return {"name": m.group(1), "line": k + 1, "brief": brief}
    return {"name": None, "line": None, "brief": ""}


def doc_brief(lines, k):
    """The first sentence of the /** ... */ block ending just above line k."""
    j = k - 1
    while j >= 0 and lines[j].strip() == "":
        j -= 1
    if j < 0 or not lines[j].strip().endswith("*/"):
        return ""
    block = []
    while j >= 0:
        block.insert(0, lines[j])
        if "/**" in lines[j]:
            break
        j -= 1
    text = " ".join(re.sub(r"^\s*(/\*\*|\*/|\*)\s?", "", l).replace("*/", "").strip() for l in block)
    cd = re.search(r"@copydoc\s+(\w+)", text)
    if cd and not text.split("@", 1)[0].strip():
        return f"(as {cd.group(1)})"
    text = text.split("@", 1)[0].strip()
    m = re.match(r"(.+?\.)(\s|$)", text)
    return (m.group(1) if m else text).strip()


def comment_near(lines, idx):
    """The comment on line idx or in the two lines above it, as a hint of intent."""
    for j in (idx, idx - 1, idx - 2):
        if j < 0:
            continue
        m = re.search(r"(?://|/\*)\s*(.+?)\s*(?:\*/)?$", lines[j])
        if m and not lines[j].strip().startswith("#"):
            return m.group(1)
    return ""


def source_files():
    skip = ("src/script/", "src/prismatic_mcp/", "src/saveload/", "src/3rdparty/", "src/tests/")
    for p in tracked(SRC, "*.cpp"):
        if not rel(p).startswith(skip) and "_gui" not in p.name:
            yield p


# --- effects -------------------------------------------------------------------------------------

CHANGE_RE = re.compile(r"ChangeTownRating\(\s*\w+\s*,\s*([^,]+?)\s*,\s*([^,]+?)\s*,")
ADD_RE = re.compile(r"ratings\[[^\]]+\]\s*\+\s*(RATING_\w+)")
SET_RE = re.compile(r"ratings\[[^\]]+\]\s*=\s*(RATING_\w+)\s*;")
FILL_RE = re.compile(r"ratings\.fill\((RATING_\w+)\)")


def derive_effects(constants):
    effects = []
    for path in source_files():
        lines = read_lines(path)
        for i, line in enumerate(lines):
            for kind, rx in (("change", CHANGE_RE), ("add", ADD_RE), ("set", SET_RE), ("initial", FILL_RE)):
                m = rx.search(line)
                if not m:
                    continue
                if kind == "change" and "void ChangeTownRating" in line:
                    continue
                fn = enclosing_function(lines, i)
                delta_expr = m.group(1)
                limit_expr = m.group(2) if kind == "change" else None
                if kind == "add":
                    # The limit is the min/max the sum is clamped to on the same line, if any.
                    lim = re.search(r"std::(min|max)\(\s*\(int\)\s*(RATING_\w+)", line)
                    limit_expr = lim.group(2) if lim else None
                eff = {
                    "kind": kind,
                    "function": fn["name"],
                    "trigger": fn["brief"],
                    "hint": comment_near(lines, i),
                    "delta_expr": delta_expr,
                    "delta": resolve(delta_expr, constants) if kind in ("change", "add") else None,
                    "to": resolve(delta_expr, constants) if kind in ("set", "initial") else None,
                    "limit_expr": limit_expr,
                    "limit": resolve(limit_expr, constants) if limit_expr else None,
                    "source": f"{rel(path)}:{i + 1}",
                }
                effects.append(eff)
    # A delta held in a local (road removal) is resolved from the assignments in the same function.
    for eff in effects:
        if eff["delta"] is None and eff["kind"] == "change":
            path, line = eff["source"].rsplit(":", 1)
            lines = read_lines(REPO / path)
            name = eff["delta_expr"].lstrip("-").strip()
            cands = []
            for j in range(max(0, int(line) - 40), int(line)):
                m = re.search(rf"\b{re.escape(name)}\s*=\s*(RATING_\w+|hs->\w+)", lines[j])
                if m:
                    cands.append(m.group(1))
            if cands:
                eff["delta_candidates"] = [
                    {"expr": c, "value": (-1 if eff["delta_expr"].startswith("-") else 1) * constants[c]["value"]}
                    if c in constants else {"expr": c, "value": None, "note": "per house type (NewGRF property)"}
                    for c in cands
                ]
    return effects


# --- gates ---------------------------------------------------------------------------------------

CMP_RE = re.compile(r"(?:ratings\[[^\]]+\]|GetRating\(t\))\s*(<=|>=|<|>)\s*(RATING_\w+)")
TABLE_ROW_RE = re.compile(r"\{\s*(RATING_\w+)\s*,\s*(RATING_\w+)\s*\}\s*,\s*//\s*(\w+)")
TOLERANCE_RE = re.compile(r"(TOWN_COUNCIL_\w+)\s*=\s*(\d+)")


def derive_gates(constants):
    gates = []
    for path in source_files():
        lines = read_lines(path)
        for i, line in enumerate(lines):
            m = CMP_RE.search(line)
            if not m:
                continue
            fn = enclosing_function(lines, i)
            gates.append({
                "function": fn["name"],
                "guards": fn["brief"],
                "hint": comment_near(lines, i),
                "op": m.group(1),
                "threshold_expr": m.group(2),
                "threshold": constants[m.group(2)]["value"],
                "source": f"{rel(path)}:{i + 1}",
            })
    # The per-tolerance table of CheckforTownRating: what a company needs to remove town roads,
    # tunnels and bridges, by the difficulty setting town_council_tolerance.
    path = SRC / "town_cmd.cpp"
    lines = read_lines(path)
    cols = []
    for i, line in enumerate(lines):
        h = re.search(r"/\*\s*((?:\w+,\s*)+\w+)\s*\*/", line)
        if h and "Remove" in h.group(1):
            cols = [c.strip() for c in h.group(1).split(",")]
        m = TABLE_ROW_RE.search(line)
        if m and cols:
            fn = enclosing_function(lines, i)
            for col, expr in zip(cols, (m.group(1), m.group(2))):
                gates.append({
                    "function": fn["name"],
                    "guards": fn["brief"],
                    "hint": f"{col} at tolerance {m.group(3)}",
                    "op": ">=",
                    "threshold_expr": expr,
                    "threshold": constants[expr]["value"],
                    "check": col,
                    "tolerance": m.group(3).lower(),
                    "source": f"{rel(path)}:{i + 1}",
                })
    # House demolition compares the house's own removal rating with the company's rating.
    for i, line in enumerate(lines):
        if "remove_rating_decrease" in line and "rating" in line:
            fn = enclosing_function(lines, i)
            gates.append({
                "function": fn["name"], "guards": fn["brief"],
                "hint": "a house is removable while the company's rating is at least the house's remove_rating_decrease (NewGRF house property), which is also what the removal costs",
                "op": ">=", "threshold_expr": "hs->remove_rating_decrease", "threshold": None,
                "source": f"{rel(path)}:{i + 1}",
            })
            break
    tolerance = {}
    for i, line in enumerate(read_lines(SRC / "town.h")):
        m = TOLERANCE_RE.search(line)
        if m:
            tolerance[m.group(1)] = {"value": int(m.group(2)), "source": f"src/town.h:{i + 1}"}
    return gates, tolerance


# --- bands ---------------------------------------------------------------------------------------

def derive_bands(constants):
    path = SRC / "script/api/script_town.cpp"
    lines = read_lines(path)
    bands = []
    lo = constants["RATING_MINIMUM"]["value"]
    for i, line in enumerate(lines):
        m = re.search(r"ratings\[c\]\s*<=\s*(RATING_\w+)\)\s*\{", line)
        if m:
            r = re.search(r"return\s+(TOWN_RATING_\w+)", lines[i + 1])
            hi = constants[m.group(1)]["value"]
            bands.append({"name": r.group(1), "lowest": lo, "highest": hi, "source": f"{rel(path)}:{i + 1}"})
            lo = hi + 1
        elif bands and re.search(r"return\s+TOWN_RATING_OUTSTANDING", line):
            bands.append({"name": "TOWN_RATING_OUTSTANDING", "lowest": lo,
                          "highest": constants["RATING_MAXIMUM"]["value"], "source": f"{rel(path)}:{i + 1}"})
            break
    # The enum's order is the value an AI receives.
    hpp = read_lines(SRC / "script/api/script_town.hpp")
    order = [m.group(1) for l in hpp for m in [re.search(r"^\s*(TOWN_RATING_\w+),", l)] if m]
    for b in bands:
        b["enum"] = order.index(b["name"]) if b["name"] in order else None
    initial = constants["RATING_INITIAL"]["value"]
    bands.insert(0, {"name": "TOWN_RATING_NONE", "enum": order.index("TOWN_RATING_NONE") if "TOWN_RATING_NONE" in order else 0,
                     "lowest": initial, "highest": initial,
                     "note": "no rating yet (have_ratings unset): the stored value is RATING_INITIAL",
                     "source": f"{rel(path)}:{next(i for i, l in enumerate(lines) if 'TOWN_RATING_NONE' in l and 'return' in l) + 1}"})
    return bands


# --- actions -------------------------------------------------------------------------------------

def derive_actions(constants):
    town_h = read_lines(SRC / "town.h")
    names = []
    inside = False
    for i, line in enumerate(town_h):
        if "enum class TownAction" in line:
            inside = True
            continue
        if inside:
            m = re.search(r"^\s*(\w+),\s*///<\s*(.*)", line)
            if m and m.group(1) != "End":
                names.append({"name": m.group(1), "doc": m.group(2).strip(), "source": f"src/town.h:{i + 1}"})
            if "};" in line:
                break
    path = SRC / "town_cmd.cpp"
    lines = read_lines(path)
    text = "\n".join(lines)
    costs = [int(x) for x in re.search(r"town_action_costs\[\]\s*=\s*\{([^}]*)\}", text).group(1).split(",")]
    procs = re.findall(r"\b(TownAction\w+)\b", re.search(r"_town_action_proc\[\]\s*=\s*\{([^}]*)\}", text).group(1))
    out = []
    for a, cost, proc in zip(names, costs, procs):
        start = next(i for i, l in enumerate(lines) if re.match(rf"static CommandCost {proc}\(", l))
        end = next(i for i in range(start + 1, len(lines)) if lines[i].startswith("}"))
        body = lines[start:end]
        sets = []
        for j, l in enumerate(body):
            for m in re.finditer(r"t->(\w+)(?:\[[^\]]+\])?\s*=\s*([\w:()]+)\s*;", l):
                sets.append({"field": m.group(1), "value": m.group(2), "source": f"{rel(path)}:{start + j + 1}"})
            for m in re.finditer(r"t->(\w+)\.Set\(\s*_current_company\s*\)", l):
                sets.append({"field": m.group(1), "value": "company", "note": "the company is added to this set of the town", "source": f"{rel(path)}:{start + j + 1}"})
            m = re.search(r"ModifyStationRatingAround\(\s*t->xy\s*,\s*\w+\s*,\s*(\w+)\s*,\s*(\d+)\s*\)", l)
            if m:
                sets.append({"field": "station_rating_around", "value": int(m.group(1), 0), "radius": int(m.group(2)),
                             "note": "raises the company's cargo ratings at stations within the radius to at least this (0-255)",
                             "source": f"{rel(path)}:{start + j + 1}"})
            m = re.search(r"Chance16\((\d+),\s*(\d+)\)", l)
            if m:
                sets.append({"field": "chance", "value": f"{m.group(1)}/{m.group(2)}", "source": f"{rel(path)}:{start + j + 1}"})
            m = re.search(r"ChangeTownRating\(\s*t\s*,\s*(RATING_\w+)\s*,\s*(RATING_\w+)", l)
            if m:
                sets.append({"field": "rating", "delta": constants[m.group(1)]["value"], "limit": constants[m.group(2)]["value"],
                             "source": f"{rel(path)}:{start + j + 1}"})
            m = re.search(r"_settings_game\.(\w+\.\w+)", l)
            if m and "return CMD_ERROR" in l:
                sets.append({"field": "requires_setting", "value": m.group(1), "source": f"{rel(path)}:{start + j + 1}"})
        out.append({**a, "cost_factor": cost, "cost": "price.town_action * cost_factor >> 8 (town_cmd.cpp TownActionCost)",
                    "proc": proc, "brief": doc_brief(lines, start), "effects": sets,
                    "proc_source": f"{rel(path)}:{start + 1}"})
    return out


# --- links: other rules tying a town to a company --------------------------------------------------

def derive_links():
    links = []
    st = SRC / "station_cmd.cpp"
    lines = read_lines(st)
    for i, l in enumerate(lines):
        m = re.search(r"statues\.Test\(st->owner\)\)\s*rating\s*\+=\s*(\d+)", l)
        if m:
            links.append({"id": "statue_cargo_rating", "effect": f"+{m.group(1)} cargo rating (of 255) at every station of the company in a town with its statue",
                          "value": int(m.group(1)), "source": f"{rel(st)}:{i + 1}"})
        if "exclusive_counter > 0 && st->town->exclusivity != st->owner" in l:
            links.append({"id": "exclusivity_blocks_others", "effect": "while a town's exclusive rights run, other companies' stations in it receive no cargo",
                          "source": f"{rel(st)}:{i + 1}"})
        if "noise_reached + newnoise_level) > nearest->MaxTownNoise()" in l:
            links.append({"id": "airport_noise", "effect": "with station_noise_level on, an airport is refused when the nearest town's noise would exceed its MaxTownNoise",
                          "setting": "economy.station_noise_level", "source": f"{rel(st)}:{i + 1}"})
    tc = SRC / "town_cmd.cpp"
    lines = read_lines(tc)
    for i, l in enumerate(lines):
        m = re.search(r"time_since_load <= (\d+) \|\| st->time_since_unload <= (\d+)", l)
        if m:
            fn = enclosing_function(lines, i)["name"]
            if fn == "CountActiveStations":
                links.append({"id": "active_stations_counted", "effect": f"a company station near the town that loaded or unloaded within {m.group(1)} rating periods counts as active for the town's growth rate",
                              "periods": int(m.group(1)), "source": f"{rel(tc)}:{i + 1}"})
                continue
            links.append({"id": "station_service_monthly", "effect": f"each month, every company station near the town that loaded or unloaded within {m.group(1)} rating periods raises the rating by RATING_STATION_UP_STEP, every other one lowers it by RATING_STATION_DOWN_STEP",
                          "periods": int(m.group(1)), "source": f"{rel(tc)}:{i + 1}"})
    text = "\n".join(lines)
    g = re.search(r"_grow_count_values\[2\]\[(\d+)\]\s*=\s*\{\s*\{([^}]*)\},\s*//\s*(.*?)\n\s*\{([^}]*)\}\s*//\s*(.*?)\n", text)
    if g:
        line = text[:g.start()].count("\n") + 1
        links.append({"id": "growth_by_active_stations",
                      "effect": "town ticks between growth steps by the number of active stations (0..5+); fewer is faster; funded buildings use the first row",
                      "funded": [int(x) for x in g.group(2).split(",")], "funded_doc": g.group(3).strip(),
                      "normal": [int(x) for x in g.group(4).split(",")], "normal_doc": g.group(5).strip(),
                      "source": f"{rel(tc)}:{line}"})
    tree = SRC / "tree_cmd.cpp"
    for i, l in enumerate(read_lines(tree)):
        if "terraform_tile_proc" in l and "LandscapeClear" in l:
            links.append({"id": "terraform_clears_trees", "effect": "terraforming a tree tile clears it (LandscapeClear), which costs the tree-removal rating",
                          "source": f"{rel(tree)}:{i + 1}"})
    return links


# --- settings ------------------------------------------------------------------------------------

def derive_settings(names):
    out = {}
    for ini in tracked(SRC / "table/settings", "*.ini"):
        lines = read_lines(ini)
        for i, l in enumerate(lines):
            m = re.match(r"var\s*=\s*(\S+)", l)
            if m and m.group(1) in names:
                block = []
                for j in range(i, min(i + 20, len(lines))):
                    if j > i and lines[j].startswith("["):
                        break
                    block.append(lines[j])
                d = {k: v for k, v in (re.match(r"(\w+)\s*=\s*(.*)", b).groups() for b in block if re.match(r"\w+\s*=", b))}
                out[m.group(1)] = {"default": d.get("def"), "min": d.get("min"), "max": d.get("max"), "source": f"{rel(ini)}:{i + 1}"}
    return out


# --- relations -----------------------------------------------------------------------------------

def derive_relations(effects, gates, actions, links):
    """The graph the parts imply, as edges {from, to, kind, label}."""
    edges = []
    for e in effects:
        if e["kind"] in ("change", "add") and e["function"]:
            d = e["delta"]
            label = f"{d:+d}" if d is not None else e["delta_expr"]
            if e.get("limit") is not None:
                label += f" (stops at {e['limit']})"
            edges.append({"from": f"event:{e['function']}", "to": "rating", "kind": "raises" if (d or 0) > 0 else "lowers", "label": label, "source": e["source"]})
    for g in gates:
        thr = g["threshold"] if g["threshold"] is not None else g["threshold_expr"]
        edges.append({"from": "rating", "to": f"gate:{g['function']}" + (f":{g['check']}:{g['tolerance']}" if g.get("check") else ""),
                      "kind": "allows", "label": f"rating {g['op']} {thr}" if g["op"] in (">", ">=") else f"refused when rating {g['op']} {thr}", "source": g["source"]})
    for a in actions:
        for s in a["effects"]:
            if s["field"] in ("requires_setting",):
                edges.append({"from": f"setting:{s['value']}", "to": f"action:{a['name']}", "kind": "enables", "label": "must be on", "source": s["source"]})
            elif s["field"] == "rating":
                edges.append({"from": f"action:{a['name']}", "to": "rating", "kind": "raises", "label": f"{s['delta']:+d} (stops at {s['limit']})", "source": s["source"]})
            elif s["field"] == "station_rating_around":
                edges.append({"from": f"action:{a['name']}", "to": "station_cargo_rating", "kind": "raises", "label": f"to {s['value']} within {s['radius']} tiles", "source": s["source"]})
            elif s["field"] not in ("chance",):
                edges.append({"from": f"action:{a['name']}", "to": f"town:{s['field']}", "kind": "sets", "label": str(s["value"]), "source": s["source"]})
    for l in links:
        edges.append({"from": f"rule:{l['id']}", "to": "station_cargo_rating" if "cargo" in l["effect"] else "rating", "kind": "links", "label": l["effect"], "source": l["source"]})
    return edges


# --- emitters ------------------------------------------------------------------------------------

HEADER = "Generated by tools/research/town-model/derive.py from the OpenTTD sources; do not edit."


def emit_squirrel(model):
    def sq(v):
        if isinstance(v, bool):
            return "true" if v else "false"
        if v is None:
            return "null"
        if isinstance(v, (int, float)):
            return str(v)
        if isinstance(v, str):
            return json.dumps(v)
        if isinstance(v, list):
            return "[" + ", ".join(sq(x) for x in v) + "]"
        return "{" + ", ".join(f"{json.dumps(k) if not re.fullmatch(r'[A-Za-z_]\w*', k) else k} = {sq(x)}" for k, x in v.items()) + "}"
    consts = {k: v["value"] for k, v in model["constants"].items()}
    bands = [{"name": b["name"], "enum": b["enum"], "lowest": b["lowest"], "highest": b["highest"]} for b in model["bands"]]
    gates = {}
    for g in model["gates"]:
        if g.get("check"):
            gates.setdefault(g["check"], {})[g["tolerance"]] = g["threshold"]
    station = next(g["threshold"] for g in model["gates"] if g["function"] == "CheckIfAuthorityAllowsNewStation")
    actions = [{"name": a["name"], "cost_factor": a["cost_factor"]} for a in model["actions"]]
    body = [
        f"/* {HEADER}",
        " * The town-authority model (research/mechanisms/town-authority.md) as data for PrismaticAI:",
        " * RATING_* constants, the rating bands ScriptTown::GetRating returns (by enum value), the",
        " * removal thresholds by town_council_tolerance, the new-station threshold and the town",
        " * actions' cost factors. Every value carries its source line in town_model.json. */",
        "",
        f"TOWN_MODEL <- {{",
        f"\tsource_rev = {sq(model['source_rev'])},",
        f"\trating = {sq(consts)},",
        f"\tbands = {sq(bands)},",
        f"\tremove_needs = {sq(gates)},",
        f"\tnew_station_above = {station},",
        f"\tactions = {sq(actions)},",
        "};",
        "",
    ]
    return "\n".join(body)


def rhai_literal(v, indent=1):
    """A JSON value as a Rhai literal: maps as #{...}, null as ()."""
    pad = "    " * indent
    if v is None:
        return "()"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        return str(v)
    if isinstance(v, str):
        return json.dumps(v)
    if isinstance(v, list):
        if not v:
            return "[]"
        return "[\n" + ",\n".join(pad + "    " + rhai_literal(x, indent + 1) for x in v) + "\n" + pad + "]"
    items = []
    for k, x in v.items():
        key = k if re.fullmatch(r"[A-Za-z_]\w*", k) else json.dumps(k)
        items.append(f"{pad}    {key}: {rhai_literal(x, indent + 1)}")
    return "#{\n" + ",\n".join(items) + "\n" + pad + "}" if items else "#{}"


def compact(model):
    """The part of the model a script decides by: values and sources, without the long prose."""
    return {
        "source_rev": model["source_rev"],
        "rating": {k: v["value"] for k, v in model["constants"].items()},
        "bands": [{"name": b["name"], "enum": b["enum"], "lowest": b["lowest"], "highest": b["highest"]} for b in model["bands"]],
        "new_station_above": next(g["threshold"] for g in model["gates"] if g["function"] == "CheckIfAuthorityAllowsNewStation"),
        "remove_needs": {g["check"]: {} for g in model["gates"] if g.get("check")} | {
            c: {g["tolerance"]: g["threshold"] for g in model["gates"] if g.get("check") == c}
            for c in {g["check"] for g in model["gates"] if g.get("check")}},
        "tolerance": {k.replace("TOWN_COUNCIL_", "").lower(): v["value"] for k, v in model["tolerance"].items()},
        "effects": [{"function": e["function"], "trigger": e["trigger"], "kind": e["kind"], "delta": e["delta"],
                     "to": e["to"], "limit": e["limit"], "source": e["source"]} for e in model["effects"]],
        "actions": [{"name": a["name"], "doc": a["doc"], "cost_factor": a["cost_factor"],
                     "effects": [{k: x for k, x in s.items() if k != "note"} for s in a["effects"]]} for a in model["actions"]],
        "links": model["links"],
        "settings": {k: v["default"] for k, v in model["settings"].items()},
    }


def emit_rhai(model):
    """One function per part (a single literal nests too deep for Rhai's parser), and model() joins them."""
    parts = compact(model)
    out = [
        f"// {HEADER}",
        "// The town-authority model (tools/research/town-model/town_model.json) as Rhai data: rating",
        "// constants, bands, the new-station threshold, removal thresholds by tolerance, effects, town",
        "// actions, links and setting defaults, each rule with its source line. town_lib.rhai reads it.",
        "",
    ]
    for key, value in parts.items():
        out += [f"/// The model's `{key}`.", f"fn {key}() {{", "    " + rhai_literal(value, 1), "}", ""]
    out += ["/// The town-authority model derived from the engine sources, as one map.", "fn model() {",
            "    #{ " + ", ".join(f"{k}: {k}()" for k in parts) + " }", "}", ""]
    return "\n".join(out)


def md(text):
    return str(text).replace("|", "\\|")


def emit_doc_tables(model):
    """The model as the tables of research/mechanisms/town-authority.md."""
    out = [DOC_BEGIN, "", f"Generated from the engine sources by `tools/research/town-model/derive.py` (model "
           f"`{model['source_rev']}`); do not edit between the markers.", ""]
    out += ["**What raises and lowers a company's rating** (one row per place in the engine):", "",
            "| Trigger | Function | Change | Stops at | Source |", "|---|---|---|---|---|"]
    for e in model["effects"]:
        if e["kind"] in ("change", "add"):
            if e["delta"] is not None:
                change = f"{e['delta']:+d}"
            elif e.get("delta_candidates"):
                change = " or ".join(f"{c['value']:+d}" if c["value"] is not None else c["expr"] for c in e["delta_candidates"])
            else:
                change = e["delta_expr"]
            lim = e["limit"] if e["limit"] is not None else ""
        else:
            change, lim = f"set to {e['to']}", ""
        out.append(f"| {md(e['trigger'] or e['hint'])} | `{e['function']}` | {change} | {lim} | `{e['source']}` |")
    out += ["", "**Where the engine compares a rating with a threshold** (the gates are the authority's refusals):", "",
            "| Guarded | Function | Condition | Tolerance | Source |", "|---|---|---|---|---|"]
    for g in model["gates"]:
        need = f"{g['op']} {g['threshold'] if g['threshold'] is not None else g['threshold_expr']}"
        what = g.get("check") or md(g["guards"] or g["hint"])
        out.append(f"| {what} | `{g['function']}` | {need} | {g.get('tolerance', '')} | `{g['source']}` |")
    out += ["", "**Town actions** (cost = the town-action base price x factor / 256):", "",
            "| Action | Factor | What it does | Source |", "|---|---|---|---|"]
    for a in model["actions"]:
        eff = []
        for x in a["effects"]:
            if x["field"] == "station_rating_around":
                eff.append(f"cargo ratings to {x['value']} within {x['radius']} tiles")
            elif x["field"] == "rating":
                eff.append(f"rating {x['delta']:+d} up to {x['limit']}")
            elif x["field"] == "requires_setting":
                eff.append(f"needs `{x['value']}`")
            elif x["field"] == "chance":
                eff.append(f"fails with chance {x['value']}")
            else:
                eff.append(f"`{x['field']}` = {x['value']}")
        out.append(f"| {a['name']} ({md(a['doc'])}) | {a['cost_factor']} | {md('; '.join(eff))} | `{a['proc_source']}` |")
    out += ["", "**Other rules tying a town to a company's stations:**", "", "| Rule | Effect | Source |", "|---|---|---|"]
    for l in model["links"]:
        extra = ""
        if l["id"] == "growth_by_active_stations":
            extra = f" (normal {l['normal']}, funded {l['funded']})"
        out.append(f"| `{l['id']}` | {md(l['effect'])}{extra} | `{l['source']}` |")
    out += ["", DOC_END]
    return "\n".join(out)


def splice_doc(text, tables):
    a, b = text.index(DOC_BEGIN), text.index(DOC_END) + len(DOC_END)
    return text[:a] + tables + text[b:]


def build():
    constants = derive_constants()
    effects = derive_effects(constants)
    gates, tolerance = derive_gates(constants)
    actions = derive_actions(constants)
    links = derive_links()
    bands = derive_bands(constants)
    setting_names = {"difficulty.town_council_tolerance", "economy.dist_local_authority", "economy.bribe",
                     "economy.exclusive_rights", "economy.fund_buildings", "economy.fund_roads",
                     "economy.station_noise_level"}
    for a in actions:
        for s in a["effects"]:
            if s["field"] == "requires_setting":
                setting_names.add(s["value"])
    sources = sorted({e["source"].rsplit(":", 1)[0] for e in effects} | {g["source"].rsplit(":", 1)[0] for g in gates}
                     | {"src/town_type.h", "src/town.h", "src/town_cmd.cpp", "src/station_cmd.cpp", "src/tree_cmd.cpp",
                        "src/script/api/script_town.cpp", "src/script/api/script_town.hpp"})
    blobs = {s: git_blob(REPO / s) for s in sources}
    import hashlib
    rev = hashlib.sha1(json.dumps(blobs, sort_keys=True).encode()).hexdigest()[:12]
    model = {
        "schema": "town-model/v1",
        "generated_by": "tools/research/town-model/derive.py",
        "source_rev": rev,
        "sources": blobs,
        "constants": constants,
        "bands": bands,
        "effects": effects,
        "gates": gates,
        "tolerance": tolerance,
        "actions": actions,
        "links": links,
        "settings": derive_settings(setting_names),
    }
    model["relations"] = derive_relations(effects, gates, actions, links)
    return model


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="fail when the committed outputs differ from the sources")
    ap.add_argument("--print", action="store_true", help="print a summary of the model")
    args = ap.parse_args()
    model = build()
    outputs = {
        OUT_JSON: json.dumps(model, indent=1, sort_keys=False) + "\n",
        OUT_NUT: emit_squirrel(model),
        OUT_RHAI: emit_rhai(model),
    }
    if OUT_DOC.is_file() and DOC_BEGIN in OUT_DOC.read_text():
        outputs[OUT_DOC] = splice_doc(OUT_DOC.read_text(), emit_doc_tables(model))
    if args.check:
        stale = [rel(p) for p, text in outputs.items() if not p.is_file() or p.read_text() != text]
        if stale:
            print("town-model: STALE: " + ", ".join(stale) + " (run tools/research/town-model/derive.py)")
            return 1
        print(f"town-model: current ({model['source_rev']})")
        return 0
    for p, text in outputs.items():
        p.write_text(text)
    if args.print or True:
        print(f"town-model {model['source_rev']}: {len(model['constants'])} constants, {len(model['bands'])} bands, "
              f"{len(model['effects'])} effects, {len(model['gates'])} gates, {len(model['actions'])} actions, "
              f"{len(model['links'])} links, {len(model['settings'])} settings, {len(model['relations'])} relations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
