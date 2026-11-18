"""Rule project.docs-are-derived-and-current: a diagram in Markdown is Mermaid.

tools/surfaces/surfaces.py check_diagrams flags a fenced block drawn in characters (box-drawing or
arrow glyphs, ASCII boxes and arrows) and leaves code, data, Mermaid and directory listings alone.
Run by tools/gate.sh change (python3 -m unittest discover -s tools/research -p 'test_*.py').
"""
import json
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SURFACES = ROOT / "tools" / "surfaces"
# The generator's own environment (tools/surfaces/surfaces creates it; the gate runs surfaces check
# before these tests): markdown-it lives there, not in the system python.
PYTHON = SURFACES / ".venv" / "bin" / "python"

PROBE = """
import importlib.util, json, sys
spec = importlib.util.spec_from_file_location("surfaces", sys.argv[1])
s = importlib.util.module_from_spec(spec); sys.modules["surfaces"] = s; spec.loader.exec_module(s)
body = sys.stdin.read()
doc = s.Doc(path="x.md", text=body, fm=None, body=body, offset=0)
print(json.dumps([[f.kind, f.line] for f in s.check_diagrams(doc, s.markdown().parse(body))]))
"""


class DiagramsAreMermaid(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not PYTHON.exists():
            raise unittest.SkipTest("no tools/surfaces/.venv yet: run tools/surfaces/surfaces check once")

    def findings(self, body):
        out = subprocess.run([str(PYTHON), "-B", "-c", PROBE, str(SURFACES / "surfaces.py")], input=body,
                             capture_output=True, text=True, check=True)
        return json.loads(out.stdout)

    def test_a_box_drawing_is_a_finding(self):
        body = "# T\n\n```\n+------+     +-----+\n| a    | --> | b   |\n+------+     +-----+\n```\n"
        self.assertEqual(self.findings(body), [["diagram", 3]])

    def test_an_arrow_drawing_in_text_is_a_finding(self):
        body = "```text\nclient ──stdio──▶ server ──▶ game\n          │\n          ▼\n```\n"
        self.assertEqual(len(self.findings(body)), 1)

    def test_mermaid_code_and_trees_are_not(self):
        body = ("```mermaid\nflowchart LR\n  a --> b --> c\n```\n\n"
                "```bash\necho a | grep b --> c --> d\n```\n\n"
                "```\nrepo/\n├── src/      <- code\n├── docs/     <- docs\n└── tools/    <- tools\n```\n")
        self.assertEqual(self.findings(body), [])


if __name__ == "__main__":
    unittest.main()
