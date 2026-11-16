#!/usr/bin/env python3
"""End-to-end check of the skill load-focused-context-for-a-change against this repository:
a source path resolves to its subsystem map, the maps' citations resolve, and a diff names the
maps it touches. python3 -m unittest tools/knowledge/test_focused_context.py
"""
# majordomus-skill: load-focused-context-for-a-change

from __future__ import annotations

import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True,
                           cwd=Path(__file__).parent, check=True).stdout.strip())
MAPS = ROOT / ".ai/repo/workflows/substrate"


def run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True)


@unittest.skipUnless(shutil.which("majordomus"), "majordomus is not on PATH")
class FocusedContextTest(unittest.TestCase):
    def test_every_map_attaches_to_the_files_it_tracks(self) -> None:
        maps = sorted(p.parent.name for p in MAPS.glob("*/README.md"))
        self.assertTrue(maps, "no subsystem map found")
        for name in maps:
            text = (MAPS / name / "README.md").read_text(encoding="utf-8")
            spec = text.split("tracks:\n", 1)[1].split("\n", 1)[0].strip(" -'\"")
            files = run("git", "ls-files", "--", f":(glob){spec}").stdout.split()
            self.assertTrue(files, f"{name}: tracks '{spec}' matches nothing")
            out = run("majordomus", "context", "resolve", files[0])
            self.assertEqual(out.returncode, 0, out.stderr)
            self.assertIn(f"fork.substrate.{name}", out.stdout, f"{files[0]} does not get {name}")

    def test_the_maps_cite_only_what_exists(self) -> None:
        out = run("tools/knowledge/refs", "check", "--pathspec",
                  ":(glob).ai/repo/workflows/substrate/**/*.md")
        self.assertEqual(out.returncode, 0, out.stdout)

    def test_a_diff_to_a_tracked_file_names_its_map(self) -> None:
        base = run("git", "rev-list", "--max-parents=0", "HEAD").stdout.split()[0]
        # The fork's own history changed src/openttd.cpp; the core-loop map tracks it.
        out = run("tools/knowledge/refs", "impact", f"{base}..HEAD", "--json")
        self.assertEqual(out.returncode, 0, out.stderr)


if __name__ == "__main__":
    unittest.main()
