#!/usr/bin/env python3
"""Tests for refs.py against a throwaway git repository: python3 -m unittest tools/knowledge/test_refs.py"""

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

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("refs", HERE / "refs.py")
refs = importlib.util.module_from_spec(spec)
sys.modules["refs"] = refs
spec.loader.exec_module(refs)

RECORD = """---
schema: context/v1
id: probe
status: active
tracks:
  - 'src/net/**'
---

# Probe

- `src/net/server.cpp:Server::Accept` accepts.
- `src/core/pool.hpp:Pool` and the `src/core/` directory.
- A branch `research/topic` and a method `tools/list` are not paths.
- A local path `.ai/local/state` is ignored.

```
`src/gone.cpp` inside a fence is not read
```
"""


def sh(root: Path, *args: str) -> str:
    return subprocess.run(args, cwd=root, check=True, capture_output=True, text=True).stdout


class RefsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        sh(self.root, "git", "init", "-q", "-b", "main")
        sh(self.root, "git", "config", "user.email", "t@example.invalid")
        sh(self.root, "git", "config", "user.name", "t")
        self.write("src/net/server.cpp", "void Server::Accept() {}\n")
        self.write("src/core/pool.hpp", "template <class T> struct Pool {};\n")
        self.write("research/notes.md", "notes\n")
        self.write("tools/x.sh", "true\n")
        self.write(".gitignore", ".ai/local/\n")
        self.write(".ai/repo/map/README.md", RECORD)
        self.commit("base")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def write(self, path: str, text: str) -> None:
        p = self.root / path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")

    def commit(self, msg: str) -> str:
        sh(self.root, "git", "add", "-A")
        sh(self.root, "git", "commit", "-q", "-m", msg)
        return sh(self.root, "git", "rev-parse", "HEAD").strip()

    def run_cli(self, *argv: str) -> tuple[int, str]:
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            code = refs.main(["--repo", str(self.root), *argv])
        return code, buf.getvalue()

    def test_check_passes_on_resolving_references(self) -> None:
        code, out = self.run_cli("check")
        self.assertEqual(code, 0, out)
        self.assertIn("0 failure(s)", out)

    def test_fence_branch_method_and_ignored_paths_are_not_references(self) -> None:
        repo = refs.Repo.load(self.root)
        labels = {r.label() for r in refs.scan(repo, ".ai/repo/map/README.md")}
        self.assertEqual(labels, {"tracks 'src/net/**'", "src/net/server.cpp:Server::Accept",
                                  "src/core/pool.hpp:Pool", "src/core"})

    def test_missing_symbol_file_and_empty_tracks_fail(self) -> None:
        self.write(".ai/repo/bad/README.md", "---\nid: bad\ntracks:\n  - 'lib/**'\n---\n\n"
                   "`src/net/server.cpp:Reject` and `src/net/client.cpp` and `src/nowhere/`\n")
        code, out = self.run_cli("check")
        self.assertEqual(code, 1)
        self.assertIn("does not contain 'Reject'", out)
        self.assertIn("src/net/client.cpp is not a tracked file", out)
        self.assertIn("src/nowhere is not a directory", out)
        self.assertIn("tracks 'lib/**' matches no tracked file", out)

    def test_untracked_new_record_is_checked_before_git_add(self) -> None:
        self.write(".ai/repo/new/README.md", "`src/net/server.cpp:Missing`\n")
        code, out = self.run_cli("check")
        self.assertEqual(code, 1)
        self.assertIn(".ai/repo/new/README.md", out)

    def test_inside_a_hook_a_file_written_but_not_added_does_not_resolve(self) -> None:
        # A record citing an unadded file would pass a pre-commit check and fail in CI, whose
        # checkout does not have it: inside a hook (GIT_INDEX_FILE set) only the index counts.
        self.write("src/net/client.cpp", "void Client::Join() {}\n")
        self.write(".ai/repo/new/README.md", "`src/net/client.cpp:Client::Join`\n")
        sh(self.root, "git", "add", ".ai/repo/new/README.md")
        code, out = self.run_cli("check")
        self.assertEqual(code, 0, out)
        old = os.environ.get("GIT_INDEX_FILE")
        os.environ["GIT_INDEX_FILE"] = str(self.root / ".git" / "index")
        try:
            code, out = self.run_cli("check")
        finally:
            if old is None:
                os.environ.pop("GIT_INDEX_FILE")
            else:
                os.environ["GIT_INDEX_FILE"] = old
        self.assertEqual(code, 1, out)
        self.assertIn("src/net/client.cpp:Client::Join is not a tracked file", out)

    def test_superseded_record_is_history_and_not_judged(self) -> None:
        self.write(".ai/repo/old/README.md", "---\nid: old\nstatus: superseded\n---\n\n`src/gone.cpp`\n")
        code, _ = self.run_cli("check")
        self.assertEqual(code, 0)

    def test_index_maps_files_to_records_with_symbols(self) -> None:
        code, out = self.run_cli("index")
        self.assertEqual(code, 0)
        files = json.loads(out)["files"]
        self.assertEqual(files["src/net/server.cpp"][".ai/repo/map/README.md"], ["Server::Accept"])
        self.assertIn(".ai/repo/map/README.md", files["src/core/pool.hpp"])

    def test_impact_names_records_and_symbols_to_reread(self) -> None:
        base = sh(self.root, "git", "rev-parse", "HEAD").strip()
        self.write("src/net/server.cpp", "void Server::Accept() { /* changed */ }\n")
        self.write("src/core/new.hpp", "struct New {};\n")
        self.write("tools/x.sh", "false\n")
        self.commit("change")
        code, out = self.run_cli("impact", f"{base}..HEAD", "--json")
        self.assertEqual(code, 0)
        recs = json.loads(out)["records"]
        hit = recs[".ai/repo/map/README.md"]
        self.assertEqual(hit["src/net/server.cpp"]["symbols"], ["Server::Accept"])
        self.assertEqual(hit["src/net/server.cpp"]["via"], "reference")
        self.assertIn("src/core/new.hpp", hit)  # through the src/core/ directory reference
        self.assertNotIn("tools/x.sh", hit)

    def test_impact_skips_a_record_changed_in_the_same_range(self) -> None:
        base = sh(self.root, "git", "rev-parse", "HEAD").strip()
        self.write("src/net/server.cpp", "void Server::Accept() { /* changed */ }\n")
        self.write(".ai/repo/map/README.md", RECORD + "\nReviewed.\n")
        self.commit("change with review")
        _, out = self.run_cli("impact", f"{base}..HEAD", "--json")
        self.assertEqual(json.loads(out)["records"], {})

    def test_impact_reports_a_deleted_file_through_tracks(self) -> None:
        base = sh(self.root, "git", "rev-parse", "HEAD").strip()
        sh(self.root, "git", "rm", "-q", "src/net/server.cpp")
        self.commit("delete")
        _, out = self.run_cli("impact", f"{base}..HEAD", "--json")
        entry = json.loads(out)["records"][".ai/repo/map/README.md"]["src/net/server.cpp"]
        self.assertEqual(entry["status"], "D")

    def test_an_ignored_directory_that_does_not_exist_here_is_still_ignored(self) -> None:
        # `.ai/local/` is a directory pattern, and a fresh checkout has no `.ai/local` at all.
        self.assertFalse((self.root / ".ai/local").exists())
        self.write(".ai/repo/local/README.md", "`.ai/local` and `.ai/local/state` are this checkout's own\n")
        code, out = self.run_cli("check")
        self.assertEqual(code, 0, out)

    def test_line_citation_checks_the_file_only(self) -> None:
        self.write(".ai/repo/lines/README.md", "`src/net/server.cpp:999` and `src/net/gone.cpp:12-14`\n")
        code, out = self.run_cli("check")
        self.assertEqual(code, 1)
        self.assertIn("src/net/gone.cpp is not a tracked file", out)
        self.assertNotIn("server.cpp", out)

    def test_coverage_names_files_no_record_tracks_and_ignores_umbrellas(self) -> None:
        self.write(".ai/repo/umbrella/README.md", "---\nid: u\ntracks:\n  - 'src/**'\n---\n")
        code, out = self.run_cli("coverage", "src/", "--json")
        self.assertEqual(code, 0)
        data = json.loads(out)
        self.assertEqual(data["files"], 2)
        self.assertEqual(data["uncovered"], ["src/core/pool.hpp"])
        self.assertEqual(data["records"]["src/net/server.cpp"], [".ai/repo/map/README.md"])
        code, out = self.run_cli("coverage", "src", "--strict")
        self.assertEqual(code, 1)
        self.assertIn("UNCOVERED src/core/ 1: pool.hpp", out)
        self.assertIn("1/2 file(s)", out)

    def test_glob_subset(self) -> None:
        rx = refs.glob_regex(":(glob)src/*_cmd.cpp")
        self.assertTrue(rx.match("src/road_cmd.cpp"))
        self.assertFalse(rx.match("src/sub/road_cmd.cpp"))
        self.assertTrue(refs.glob_regex("src/**").match("src/a/b/c.h"))
        self.assertTrue(refs.glob_regex("src/**/x.h").match("src/x.h"))


if __name__ == "__main__":
    unittest.main()
