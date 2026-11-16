# tools/knowledge — source references in the AI layer

The `.ai/` records cite the code they describe: a subsystem map names `src/command.cpp:InternalPostBefore`,
a knowledge record names the file its evidence is in. Nothing kept those citations true, so a
renamed file or a removed function left a record that still sent the next worker there.
`refs` reads what git tracks and answers three questions:

| command | answers | exit |
|---|---|---|
| `tools/knowledge/refs check` | does every file, directory, symbol and `tracks` pathspec cited by a record still exist? | 1 naming each one that does not |
| `tools/knowledge/refs impact <rev-range>` | which records cite or track a file this range changes, and which symbols to re-read? | 0 |
| `tools/knowledge/refs index [--out <file>]` | the reverse index: cited file → records → symbols, as JSON | 0 |
| `tools/knowledge/refs coverage <dir>... [--strict]` | which tracked files under the directories no record's `tracks` attaches to? A `<dir>/**` umbrella is not counted | 0 (1 with `--strict` and a gap) |

`impact` is the incremental half: a diff is mapped to the records it can make stale, instead of
re-reading the repository. Typical ranges:

```bash
tools/knowledge/refs impact HEAD                         # the working tree against HEAD
tools/knowledge/refs impact upstream/master@{1}..upstream/master   # what an upstream fetch moved
tools/knowledge/refs impact "$(git merge-base HEAD master)..HEAD"   # a branch before review
```

A record changed inside the same range is left out (whoever changed it read it). The index is a
rebuildable product: write it under `.ai/local/cache/` (`--out .ai/local/cache/knowledge-refs.json`),
never into a tracked file.

## What is a reference

In a scanned Markdown file, outside fenced code blocks: an inline code span whose first segment is
a top-level entry git tracks — `path`, `path/` (a directory) or `path:Symbol` — and each pathspec in
the front matter's `tracks:` list. For `A::B::name` the last component must occur as a whole word in
the cited file, so cite a symbol in the file that defines or declares it. Line numbers are never
cited: they rot on every upstream rebase, and symbols do not.

Not judged, because they are not claims about this tree: a span with no extension, no trailing `/`
and no symbol that resolves to nothing (a branch such as `research/x`, a method such as
`tools/list`); a path git ignores (`.ai/local/`); and every span of a record whose `status` is
`superseded` or `deprecated`, since history cites what was true then.

The default scan is `.ai/**/*.md` without the vendored rule package (another repository's text);
`--pathspec` replaces it. Nothing in the tool is specific to this repository.

## Where it runs

- `tools/gate.sh change` runs `check` (blocking) and `impact HEAD` (informational).
- Tests: `python3 -m unittest tools/knowledge/test_refs.py` (a throwaway git repository; no network).

## consult — an external model, with derived context and checked citations

`tools/knowledge/consult ask "<question>" --path <p>...` asks another model (ChatGPT through
`codex exec -s read-only`, `llm`, or any command in `CONSULT_CMD`) about this repository. The prompt's
context is what `majordomus context resolve` gives for the paths, nearest first, within `--budget`
lines; `--dry-run` prints it. Every `path` and `path:Symbol` in the answer goes through the same check
as `refs check`, so an invented file or function is named (`UNRESOLVED ...`). The answer and its check
are kept in `.ai/local/cache/consultations/` (`consult list`), never in a tracked file: a model's
statement becomes a record only after a worker verified it (skill `consult-an-external-model`).

Tests: `python3 -m unittest tools/knowledge/test_consult.py` (a stub provider; no network) and
`tools/knowledge/test_focused_context.py` (end to end against this repository: every subsystem map
attaches to the files it tracks, and its citations resolve).
