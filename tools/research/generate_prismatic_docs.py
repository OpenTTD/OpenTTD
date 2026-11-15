#!/usr/bin/env python3
"""Regenerate contributor-facing Prismatic MCP docs from one data source.

Single source of truth: research/prismatic-mcp/contributor-catalog.json.
Generated targets (marked blocks are replaced in place, never hand-edited):

  - README.md                                   (marker: prismatic-mcp-getting-started)
  - research/prismatic-mcp/README.md             (marker: prismatic-mcp-readme-cta)

The published site and wiki render both documents as they are (tools/surfaces), so the
catalog no longer generates any page template of its own.

tools/check-prismatic-docs.sh runs this and fails the gate if the working tree
disagrees with what the catalog would generate (see tools/gate.sh 'change').
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOG_PATH = ROOT / "research/prismatic-mcp/contributor-catalog.json"


def load_catalog():
    return json.loads(CATALOG_PATH.read_text())


def replace_marked_block(path: Path, marker: str, new_inner: str) -> None:
    text = path.read_text()
    begin = f"<!-- BEGIN GENERATED: {marker}"
    end = f"<!-- END GENERATED: {marker} -->"
    begin_idx = text.index(begin)
    begin_line_end = text.index("-->", begin_idx) + len("-->")
    end_idx = text.index(end, begin_line_end)
    new_text = text[:begin_line_end] + "\n" + new_inner.rstrip("\n") + "\n" + text[end_idx:]
    path.write_text(new_text)


def render_step(i, step):
    lines = [f"{i}. **{step['title']}** — {step.get('detail', '')}".rstrip(" —")]
    if "command" in step:
        lines.append(f"   ```bash\n   {step['command']}\n   ```")
    return "\n".join(lines)


def render_doc_index_row(entry, root_relative: str = ""):
    path = entry["path"]
    return f"| [`{path}`]({root_relative}{path}) — {entry['title']} | {entry['kind']} |"


def render_getting_started_block(cat, root_relative: str) -> str:
    project = cat["project"]
    cta = cat["cta"]
    steps = "\n".join(render_step(i, s) for i, s in enumerate(cat["getting_started_steps"], 1))
    doc_rows = "\n".join(render_doc_index_row(e, root_relative) for e in cat["doc_index"])
    limitations = "\n".join(f"- {item}" for item in cat["known_limitations"])
    return f"""#### 0.4.1) Getting started — Prismatic MCP for contributors

[**{cta['label']}**]({cta['url']}) — {cta['subtext']}

{project['status_summary']}

{steps}

**Full doc index** (also generated from the same source):

| Doc | Kind |
|---|---|
{doc_rows}

Known limitations (also generated from the same source):
{limitations}"""


def render_prismatic_readme_cta_block(cat) -> str:
    cta = cat["cta"]
    steps = "\n".join(render_step(i, s) for i, s in enumerate(cat["getting_started_steps"], 1))
    return f"""## Getting started for contributors

[**{cta['label']}**]({cta['url']}) — {cta['subtext']}

{steps}

See [`../../README.md`](../../README.md#041-getting-started--prismatic-mcp-for-contributors) for
the same content cross-linked from the top-level README, and `AGENTS.md` / `CLAUDE.md` for the
ground rules that apply before any of this."""



def main():
    cat = load_catalog()

    readme_path = ROOT / "README.md"
    prismatic_readme_path = ROOT / "research/prismatic-mcp/README.md"

    replace_marked_block(readme_path, "prismatic-mcp-getting-started",
                          render_getting_started_block(cat, "./"))
    replace_marked_block(prismatic_readme_path, "prismatic-mcp-readme-cta",
                          render_prismatic_readme_cta_block(cat))

    print("Regenerated from", CATALOG_PATH.relative_to(ROOT), ":")
    for p in (readme_path, prismatic_readme_path):
        print(f"  {p.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
