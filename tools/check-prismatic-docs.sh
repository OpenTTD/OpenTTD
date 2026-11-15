#!/usr/bin/env bash
# Doctrine check: contributor-facing Prismatic MCP docs must be generated from
# research/prismatic-mcp/contributor-catalog.json, never hand-edited. This fails
# the gate if the working tree disagrees with what the generator would produce --
# i.e. the catalog was edited without rerunning the generator, or a generated
# block/file was hand-edited out of sync. See tools/research/generate_prismatic_docs.py.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

GENERATED_PATHS=(
  README.md
  research/prismatic-mcp/README.md
)

# Snapshot the working tree as it is *before* regenerating (regardless of commit
# state -- this must work with legitimate uncommitted in-progress edits), then
# regenerate and diff against that snapshot. A diff means the working tree was
# NOT what the catalog would currently produce -- i.e. it's stale or was hand-edited.
SNAPSHOT_DIR="$(mktemp -d)"
trap 'rm -rf "$SNAPSHOT_DIR"' EXIT

for p in "${GENERATED_PATHS[@]}"; do
  mkdir -p "$SNAPSHOT_DIR/$(dirname "$p")"
  if [[ -f "$p" ]]; then
    cp "$p" "$SNAPSHOT_DIR/$p"
  fi
done

echo "==> tools/research/generate_prismatic_docs.py"
python3 tools/research/generate_prismatic_docs.py

fail=0
for p in "${GENERATED_PATHS[@]}"; do
  before="$SNAPSHOT_DIR/$p"
  if [[ ! -f "$before" ]]; then
    echo "NOTE: $p did not exist before generation (first-time creation)."
    continue
  fi
  if ! diff -u "$before" "$p"; then
    echo "STALE: $p" >&2
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo
  echo "FAIL: generated Prismatic MCP docs were stale relative to research/prismatic-mcp/contributor-catalog.json." >&2
  echo "tools/research/generate_prismatic_docs.py has now refreshed them in place -- review and commit the result." >&2
  exit 1
fi

echo "check-prismatic-docs: PASS"
