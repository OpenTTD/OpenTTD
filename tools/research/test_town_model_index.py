"""The town model derives only from source files the next commit can hold.

tools/research/town-model/derive.py `tracked()` counts what git tracks plus new files it would not
ignore; inside a git hook (GIT_INDEX_FILE set) only the index, as tools/surfaces/surfaces.py does.
Run by tools/gate.sh change (python3 -m unittest discover -s tools/research -p 'test_*.py').
"""
import importlib.util
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("town_model_derive", HERE / "town-model" / "derive.py")
derive = importlib.util.module_from_spec(spec)
sys.modules["town_model_derive"] = derive
spec.loader.exec_module(derive)


def run(*args, cwd):
    subprocess.run(list(args), cwd=cwd, check=True, capture_output=True, text=True)


class TrackedSources(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name).resolve()
        run("git", "init", "-q", cwd=self.root)
        src = self.root / "src"
        (src / "deep").mkdir(parents=True)
        (src / "added.cpp").write_text("int a;\n")
        (src / "deep" / "nested.cpp").write_text("int n;\n")
        (src / "written.cpp").write_text("int w;\n")
        (src / "notes.txt").write_text("x\n")
        (self.root / ".gitignore").write_text("src/ignored.cpp\n")
        (src / "ignored.cpp").write_text("int i;\n")
        run("git", "add", ".gitignore", "src/added.cpp", "src/deep/nested.cpp", cwd=self.root)
        self.saved = (derive.REPO, os.environ.get("GIT_INDEX_FILE"))
        derive.REPO = self.root
        os.environ.pop("GIT_INDEX_FILE", None)

    def tearDown(self):
        derive.REPO, index = self.saved
        if index is None:
            os.environ.pop("GIT_INDEX_FILE", None)
        else:
            os.environ["GIT_INDEX_FILE"] = index
        self.tmp.cleanup()

    def names(self):
        return [str(p.relative_to(self.root)) for p in derive.tracked(self.root / "src", "*.cpp")]

    def test_outside_a_hook_a_new_file_counts_and_an_ignored_one_does_not(self):
        self.assertEqual(self.names(), ["src/added.cpp", "src/deep/nested.cpp", "src/written.cpp"])

    def test_inside_a_hook_only_the_index_counts(self):
        os.environ["GIT_INDEX_FILE"] = str(self.root / ".git" / "index")
        self.assertEqual(self.names(), ["src/added.cpp", "src/deep/nested.cpp"])

    def test_outside_a_checkout_of_the_repo_the_disk_counts(self):
        derive.REPO = self.root / "elsewhere"
        self.assertEqual(self.names(), ["src/added.cpp", "src/deep/nested.cpp", "src/ignored.cpp",
                                        "src/written.cpp"])


if __name__ == "__main__":
    unittest.main()
