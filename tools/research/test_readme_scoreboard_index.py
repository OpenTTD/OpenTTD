"""A duel record the next commit does not hold does not enter the README scoreboard inside a hook.

tools/research/readme-scoreboard counts the research/results/duel-*.json records git tracks plus
new ones it would not ignore, so a rewrite and `--check` agree before and after a `git add`. Inside
a git hook (git exports GIT_INDEX_FILE to pre-commit) only the index is counted: on 2026-11-19 two
duel records written but never added passed the hook, entered README.md and broke CI, whose
checkout does not have them. The same rule as tools/surfaces/surfaces.py load_repo
(test_surfaces_index.py).
Run by tools/gate.sh change (python3 -m unittest discover -s tools/research -p 'test_*.py').
"""
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
BOARD = HERE / "readme-scoreboard"

# Loads the tool, points it at the temporary checkout (ROOT, README, RESULTS; the NoirGFX style
# stays the real one) and prints the records it counts and its --check exit code.
PROBE = """
import importlib.machinery, importlib.util, json, pathlib, sys
loader = importlib.machinery.SourceFileLoader("readme_scoreboard", sys.argv[1])
b = importlib.util.module_from_spec(importlib.util.spec_from_loader("readme_scoreboard", loader))
loader.exec_module(b)
b.ROOT = pathlib.Path(sys.argv[2]).resolve()
b.README, b.RESULTS = b.ROOT / "README.md", b.ROOT / "research" / "results"
runs = [r["path"].name for r in b.load()]
sys.argv = ["readme-scoreboard", *sys.argv[3:]]
try:
    b.main()
    code = 0
except SystemExit as e:
    code = e.code
print(json.dumps({"runs": runs, "exit": code}))
"""


def record(planned, unplanned):
    return {"seed": 31337, "months": 36, "label": "fixture", "winner": "A (planned)",
            "final": {"1": {"value": planned}, "2": {"value": unplanned}}}


def run(*args, cwd, env=None):
    return subprocess.run(list(args), cwd=cwd, env=env, check=True, capture_output=True, text=True)


class TheIndexIsTheCommit(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name).resolve()
        results = self.root / "research" / "results"
        results.mkdir(parents=True)
        run("git", "init", "-q", cwd=self.root)
        (self.root / "README.md").write_text(
            "# R\n\n<!-- scoreboard:begin -->\n<!-- scoreboard:end -->\n")
        (self.root / ".gitignore").write_text("research/results/duel-ignored.json\n")
        for name, values in (("duel-1-added", (900, 100)), ("duel-2-written", (800, 200)),
                             ("duel-ignored", (700, 300))):
            (results / f"{name}.json").write_text(json.dumps(record(*values)))
            (results / f"{name}.md").write_text(f"# {name}\n")
        run("git", "add", "README.md", ".gitignore", cwd=self.root)
        run("git", "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", "one",
            cwd=self.root)
        run("git", "add", "research/results/duel-1-added.json", "research/results/duel-1-added.md",
            cwd=self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def board(self, *args, index_file=None):
        env = {k: v for k, v in os.environ.items() if k != "GIT_INDEX_FILE"}
        if index_file:
            env["GIT_INDEX_FILE"] = index_file
        out = run(sys.executable, "-B", "-c", PROBE, str(BOARD), str(self.root), *args,
                  cwd=self.root, env=env)
        return json.loads(out.stdout)

    def test_outside_a_hook_a_new_record_counts(self):
        self.assertEqual(self.board("--check")["runs"],
                         ["duel-1-added.json", "duel-2-written.json"])

    def test_inside_a_hook_only_the_index_counts(self):
        index = str(self.root / ".git" / "index")
        self.assertEqual(self.board("--check", index_file=index)["runs"], ["duel-1-added.json"])

    def test_inside_a_hook_a_relative_index_counts_the_same(self):
        # git commit exports GIT_INDEX_FILE=.git/index, relative to the hook's directory.
        self.assertEqual(self.board("--check", index_file=".git/index")["runs"],
                         ["duel-1-added.json"])

    def test_a_scoreboard_naming_an_unadded_record_fails_the_hook(self):
        self.assertEqual(self.board()["exit"], 0)  # rewritten from both records on disk
        self.assertEqual(self.board("--check")["exit"], 0)
        index = str(self.root / ".git" / "index")
        self.assertEqual(self.board("--check", index_file=index)["exit"], 11)


if __name__ == "__main__":
    unittest.main()
