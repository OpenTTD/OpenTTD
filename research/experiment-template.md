# Experiment report: <slug>

- **Date:** YYYY-MM-DD
- **Base revision:** `git rev-parse --short HEAD` output
- **Goal / hypothesis:** what you set out to test or change, and what result would confirm or refute it.
- **Changed files:** `git diff --name-only <base>` output, or a short list.
- **Build/debug configuration (if relevant):** debug vs. release build, `OPTION_USE_ASSERTS`, sanitizer flags, `-d <facility>=<level>`, or any other research-only instrumentation used — see `AGENTS.md` §"Research-mode access to internals". Omit if the experiment used only the default `tools/gate.sh` configuration.

## Validation performed

List the exact commands run and their outcome. Don't summarize away detail — "ran the tests" is not enough, "`tools/gate.sh change` -> exit 0, 98/98 tests" is.

| Command | Exit status | Notes |
|---|---|---|
| e.g. `tools/gate.sh smoke` | 0 | |

## Result

Short, concrete summary of what actually happened — not what was intended.

## Known limitations

What wasn't covered, what's uncertain, what would need a higher validation tier to confirm.

## Status

One of: `PASS` / `FAIL` / `PARTIAL` / `NOT RUN` / `NOT APPLICABLE` — see `research/README.md` for definitions. If in doubt, pick the lower-confidence status.
