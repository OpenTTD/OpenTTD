#!/usr/bin/env python3
"""Ask an external model a question about this repository, with the context the AI layer
resolves for the paths involved, and judge every source reference in its answer.

The context is never hand-written into a prompt. For each `--path`, the chain of context
documents that applies is taken from `majordomus context resolve <path>`, and those documents
are what the model is given, nearest first, within a line budget. The answer comes back as
evidence of the weakest kind: a model's statement. Every `path` and `path:Symbol` it cites is
checked against the tree with `refs`, so a fabricated file or function is named before anyone
relies on it. Nothing the answer says reaches a tracked record by this tool: a finding becomes a
knowledge candidate only after a worker verified it in the source.

Providers are found, not configured: `CONSULT_CMD` (a shell command reading the prompt on
standard input and writing the answer to standard output), then `codex` (which can also read
the repository itself, in a read-only sandbox), then `llm`. `--provider` picks one.

  consult ask "<question>" [--path P]... [--provider auto|codex|llm|cmd] [--budget N]
                           [--timeout S] [--dry-run]
  consult list                       the consultations of this checkout, newest first

Each consultation is written to `.ai/local/cache/consultations/` (git ignores `.ai/local/`): the
question, the paths, the context ids, the provider, the commit, the answer and the reference
check. Local, rebuildable by asking again, never published.

Generic: nothing here names this repository. Exit 0 with an answer, 2 on usage, 3 when no
provider is reachable or the provider failed.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import importlib.util
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT_DIR = Path(".ai/local/cache/consultations")
CHAIN_LINE = re.compile(r"^\d+\s+(?P<id>\S+)\s+(?P<path>\S+)\s")

INSTRUCTIONS = """You are consulted as an independent reviewer of a source repository. Work read-only.
Answer the question below. Ground every claim in the repository as it is now:
- cite source as `path` or `path:Symbol` (the symbol must occur in that file; no line numbers);
- label each non-trivial claim [verified] (you read the code), [supported] (a document or
  commit says so), [inferred] (your interpretation) or [unknown];
- say plainly when you do not know; never invent a file, symbol, decision or alternative.
The context documents below were resolved for the paths in question by the repository's AI
layer (Majordomus). They are maps written by earlier workers, not proof: check them against the
code when it matters, and say where they are wrong.
"""


def load_refs():
    spec = importlib.util.spec_from_file_location("refs", HERE / "refs.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules["refs"] = mod
    spec.loader.exec_module(mod)
    return mod


def run(args: list[str], cwd: Path, stdin: str | None = None, timeout: int | None = None):
    return subprocess.run(args, cwd=cwd, input=stdin, capture_output=True, text=True,
                          timeout=timeout)


def resolve_chain(root: Path, path: str) -> tuple[list[tuple[str, str]], str | None]:
    """The (id, document path) chain Majordomus resolves for a path, or an error."""
    if not shutil.which("majordomus"):
        return [], "majordomus is not on PATH"
    r = run(["majordomus", "context", "resolve", path], root)
    if r.returncode != 0:
        lines = (r.stderr or r.stdout).strip().splitlines()
        return [], lines[-1] if lines else "failed"
    chain = []
    for line in r.stdout.splitlines():
        m = CHAIN_LINE.match(line.strip())
        if m:
            chain.append((m.group("id"), m.group("path")))
    return chain, None


def build_context(root: Path, paths: list[str], budget: int) -> tuple[str, list[str], list[str]]:
    """The context block (nearest documents first, within `budget` lines), its ids, and notes."""
    # Rank each document by its distance from the nearest path it serves, so the nearest document
    # of every path comes before any ancestor: the shared root never crowds out a second path's map.
    rank: dict[tuple[str, str], tuple[int, int]] = {}
    notes: list[str] = []
    for n, p in enumerate(paths):
        chain, err = resolve_chain(root, p)
        if err:
            notes.append(f"context for {p} not resolved: {err}")
        for depth, item in enumerate(reversed(chain)):
            rank[item] = min(rank.get(item, (depth, n)), (depth, n))
    order = sorted(rank, key=lambda item: rank[item])
    blocks, used, ids = [], 0, []
    for doc_id, doc_path in order:
        text = (root / doc_path).read_text(encoding="utf-8", errors="replace").splitlines()
        if used + len(text) > budget:
            notes.append(f"{doc_id} left out: the {budget}-line budget is spent")
            continue
        blocks.append(f"=== {doc_id} ({doc_path})\n" + "\n".join(text))
        used += len(text)
        ids.append(doc_id)
    return "\n\n".join(blocks), ids, notes


def provider_command(name: str, root: Path, answer_file: Path) -> tuple[str, list[str] | str] | None:
    if name in ("auto", "cmd") and os.environ.get("CONSULT_CMD"):
        return "cmd", os.environ["CONSULT_CMD"]
    if name in ("auto", "codex") and shutil.which("codex"):
        return "codex", ["codex", "exec", "-s", "read-only", "--ephemeral", "-C", str(root),
                         "-o", str(answer_file), "-"]
    if name in ("auto", "llm") and shutil.which("llm"):
        return "llm", ["llm"]
    return None


def ask(root: Path, question: str, paths: list[str], provider: str, budget: int, timeout: int,
        dry_run: bool) -> int:
    head = run(["git", "rev-parse", "HEAD"], root).stdout.strip()
    context, ids, notes = build_context(root, paths, budget)
    prompt = (INSTRUCTIONS + f"\nRepository commit: {head}\nPaths in question: "
              + (", ".join(paths) or "(none)") + "\n\n# Context\n\n" + (context or "(none resolved)")
              + "\n\n# Question\n\n" + question.strip() + "\n")
    for n in notes:
        print(f"note: {n}", file=sys.stderr)
    if dry_run:
        sys.stdout.write(prompt)
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        answer_file = Path(tmp) / "answer.md"
        chosen = provider_command(provider, root, answer_file)
        if not chosen:
            print(f"consult: no provider reachable for '{provider}' (set CONSULT_CMD, or install "
                  "codex or llm)", file=sys.stderr)
            return 3
        name, cmd = chosen
        try:
            if isinstance(cmd, str):
                r = subprocess.run(cmd, shell=True, cwd=root, input=prompt, capture_output=True,
                                   text=True, timeout=timeout)
            else:
                r = run(cmd, root, prompt, timeout)
        except subprocess.TimeoutExpired:
            print(f"consult: {name} did not answer within {timeout}s", file=sys.stderr)
            return 3
        answer = answer_file.read_text(encoding="utf-8") if answer_file.exists() else r.stdout
        if r.returncode != 0 or not answer.strip():
            print(f"consult: {name} failed (exit {r.returncode}): {r.stderr.strip()[-500:]}",
                  file=sys.stderr)
            return 3
    refs = load_refs()
    repo = refs.Repo.load(root)
    cited = refs.scan(repo, "answer", answer)
    bad = [(c, p) for c in cited if (p := refs.problem(repo, c))]
    now = dt.datetime.now(dt.timezone.utc)
    digest = hashlib.sha256((question + head + answer).encode()).hexdigest()[:10]
    out = root / OUT_DIR / f"{now:%Y%m%dT%H%M%SZ}-{digest}.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    meta = {"schema": "consultation/v1", "asked_at": now.isoformat(timespec="seconds"),
            "commit": head, "provider": name, "question": question.strip(), "paths": paths,
            "context": ids, "references": len(cited), "unresolved": len(bad)}
    check = "\n".join(f"- `{c.label()}` {p}" for c, p in bad) or "- every cited reference resolves"
    out.write_text("---\n" + json.dumps(meta, indent=1) + "\n---\n\n" + answer.strip()
                   + "\n\n# Reference check\n\n" + check + "\n", encoding="utf-8")
    print(answer.strip())
    print(f"\nconsult: {name} answered; {len(cited)} reference(s) cited, {len(bad)} unresolved; "
          f"record {out.relative_to(root)}")
    for c, p in bad:
        print(f"UNRESOLVED {c.label()} {p}")
    return 0


def cmd_list(root: Path) -> int:
    d = root / OUT_DIR
    for f in sorted(d.glob("*.md"), reverse=True) if d.exists() else []:
        text = f.read_text(encoding="utf-8")
        meta = json.loads(text.split("---\n")[1])
        print(f"{f.name}  {meta['provider']:6} unresolved={meta['unresolved']}  {meta['question'][:80]}")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="consult", description=__doc__.split("\n\n")[0])
    ap.add_argument("--repo", default=None)
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("ask")
    a.add_argument("question")
    a.add_argument("--path", action="append", default=[])
    a.add_argument("--provider", default=os.environ.get("CONSULT_PROVIDER", "auto"),
                   choices=["auto", "codex", "llm", "cmd"])
    a.add_argument("--budget", type=int, default=400, help="context lines (default 400)")
    a.add_argument("--timeout", type=int, default=1800)
    a.add_argument("--dry-run", action="store_true", help="print the prompt, ask nobody")
    sub.add_parser("list")
    args = ap.parse_args(argv)
    root = Path(args.repo) if args.repo else Path(run(["git", "rev-parse", "--show-toplevel"],
                                                     Path.cwd()).stdout.strip())
    if args.cmd == "list":
        return cmd_list(root)
    return ask(root, args.question, args.path, args.provider, args.budget, args.timeout,
               args.dry_run)


if __name__ == "__main__":
    sys.exit(main())
