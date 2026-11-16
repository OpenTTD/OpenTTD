#!/usr/bin/env python3
"""Tests for consult.py with a stub provider (CONSULT_CMD), no network:
python3 -m unittest tools/knowledge/test_consult.py"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("consult", HERE / "consult.py")
consult = importlib.util.module_from_spec(spec)
sys.modules["consult"] = consult
spec.loader.exec_module(consult)


def sh(root: Path, *args: str) -> None:
    subprocess.run(args, cwd=root, check=True, capture_output=True)


class ConsultTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        sh(self.root, "git", "init", "-q", "-b", "main")
        sh(self.root, "git", "config", "user.email", "t@example.invalid")
        sh(self.root, "git", "config", "user.name", "t")
        (self.root / "src").mkdir()
        (self.root / "src/loop.cpp").write_text("void StateGameLoop() {}\n")
        (self.root / ".gitignore").write_text(".ai/local/\n")
        sh(self.root, "git", "add", "-A")
        sh(self.root, "git", "commit", "-q", "-m", "base")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def ask(self, *argv: str, env: dict[str, str] | None = None) -> tuple[int, str]:
        buf = io.StringIO()
        with mock.patch.dict(os.environ, env or {}, clear=False), \
                contextlib.redirect_stdout(buf), contextlib.redirect_stderr(io.StringIO()):
            code = consult.main(["--repo", str(self.root), *argv])
        return code, buf.getvalue()

    def test_answer_is_recorded_locally_and_its_citations_judged(self) -> None:
        stub = "cat >/dev/null; printf '%s\\n' '`src/loop.cpp:StateGameLoop` [verified]' '`src/loop.cpp:Invented` [inferred]'"
        code, out = self.ask("ask", "where is the loop?", "--path", "src/loop.cpp",
                             "--provider", "cmd", env={"CONSULT_CMD": stub})
        self.assertEqual(code, 0, out)
        self.assertIn("2 reference(s) cited, 1 unresolved", out)
        self.assertIn("UNRESOLVED src/loop.cpp:Invented", out)
        records = list((self.root / ".ai/local/cache/consultations").glob("*.md"))
        self.assertEqual(len(records), 1)
        meta = json.loads(records[0].read_text().split("---\n")[1])
        self.assertEqual(meta["provider"], "cmd")
        self.assertEqual(meta["unresolved"], 1)
        self.assertEqual(meta["paths"], ["src/loop.cpp"])
        status = subprocess.run(["git", "status", "--porcelain"], cwd=self.root,
                                capture_output=True, text=True).stdout
        self.assertEqual(status, "", "a consultation must never leave a tracked or untracked file")

    def test_prompt_carries_question_instructions_and_commit(self) -> None:
        code, out = self.ask("ask", "why?", "--dry-run")
        self.assertEqual(code, 0)
        self.assertIn("# Question\n\nwhy?", out)
        self.assertIn("never invent a file", out)
        self.assertIn("Repository commit: ", out)

    def test_nearest_document_of_every_path_precedes_any_ancestor(self) -> None:
        for name, lines in (("root.md", 50), ("a.md", 10), ("b.md", 10)):
            (self.root / name).write_text("x\n" * lines)
        chains = {"src/a": [("root", "root.md"), ("a", "a.md")],
                  "src/b": [("root", "root.md"), ("b", "b.md")]}
        with mock.patch.object(consult, "resolve_chain", lambda r, p: (chains[p], None)):
            _, ids, notes = consult.build_context(self.root, ["src/a", "src/b"], budget=25)
        self.assertEqual(ids, ["a", "b"])
        self.assertTrue(any("root left out" in n for n in notes))

    def test_failing_provider_exits_3(self) -> None:
        code, _ = self.ask("ask", "x", "--provider", "cmd", env={"CONSULT_CMD": "exit 7"})
        self.assertEqual(code, 3)

    def test_no_provider_exits_3(self) -> None:
        with mock.patch.object(consult.shutil, "which", return_value=None), \
                mock.patch.dict(os.environ, {"CONSULT_CMD": ""}):
            code, _ = self.ask("ask", "x")
        self.assertEqual(code, 3)


if __name__ == "__main__":
    unittest.main()
