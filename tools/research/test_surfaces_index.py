"""A document the next commit does not hold is not indexed inside a git hook.

tools/surfaces/surfaces.py load_repo counts what git tracks plus new files it would not ignore, so
`sync` and `check` agree before and after a `git add`. Inside a git hook (git exports
GIT_INDEX_FILE to pre-commit) only the index is counted: a document written but never added used to
pass the hook, enter the generated indexes and break CI, whose checkout does not have it.
Run by tools/gate.sh change (python3 -m unittest discover -s tools/research -p 'test_*.py').
"""
import json
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SURFACES = ROOT / "tools" / "surfaces"
# The generator's own environment (tools/surfaces/surfaces creates it): markdown-it lives there.
PYTHON = SURFACES / ".venv" / "bin" / "python"

PROBE = """
import importlib.util, json, pathlib, sys
spec = importlib.util.spec_from_file_location("surfaces", sys.argv[1])
s = importlib.util.module_from_spec(spec); sys.modules["surfaces"] = s; spec.loader.exec_module(s)
print(json.dumps(sorted(s.load_repo(pathlib.Path(sys.argv[2])).tracked)))
"""


def run(*args, cwd, env=None):
    return subprocess.run(list(args), cwd=cwd, env=env, check=True, capture_output=True, text=True)


class TheIndexIsTheCommit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not PYTHON.exists():
            raise unittest.SkipTest("no surfaces .venv: run tools/surfaces/surfaces check once")

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        run("git", "init", "-q", cwd=self.root)
        (self.root / "README.md").write_text("# R\n")
        (self.root / "added.md").write_text("# Added\n")
        (self.root / "written.md").write_text("# Written, not added\n")
        (self.root / ".gitignore").write_text("ignored.md\n")
        (self.root / "ignored.md").write_text("# Ignored\n")
        run("git", "add", "README.md", ".gitignore", cwd=self.root)
        run("git", "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", "one",
            cwd=self.root)
        run("git", "add", "added.md", cwd=self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def tracked(self, index_file=None):
        env = {k: v for k, v in os.environ.items() if k != "GIT_INDEX_FILE"}
        if index_file:
            env["GIT_INDEX_FILE"] = index_file
        out = run(str(PYTHON), "-B", "-c", PROBE, str(SURFACES / "surfaces.py"), str(self.root),
                  cwd=self.root, env=env)
        return json.loads(out.stdout)

    def test_outside_a_hook_a_new_file_counts(self):
        self.assertEqual(self.tracked(), [".gitignore", "README.md", "added.md", "written.md"])

    def test_inside_a_hook_only_the_index_counts(self):
        index = str(self.root / ".git" / "index")
        self.assertEqual(self.tracked(index), [".gitignore", "README.md", "added.md"])


if __name__ == "__main__":
    unittest.main()
