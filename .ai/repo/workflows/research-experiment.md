# Running a research experiment that needs internals

The policy is ADR 0004 and rule `project.research-code-is-gated-and-labelled`; the tooling
is ADR 0005 (`tools/research/research`, `research/tooling.md`). This is the order of work.

1. **Question.** State the research question and what would answer it.
2. **Observable.** Name the internal state or behaviour that must become observable.
3. **Existing interface first.** `-d <facility>=<level>` (facilities in `src/debug.h`: `driver`,
   `grf`, `map`, `misc`, `net`, `sprite`, `oldloader`, `yapf`, `fontcache`, `script`, `sl`,
   `gamelog`, `desync`, `console`, `random`), `OPTION_USE_ASSERTS`, existing logging, and
   `research_status` (research-debug builds).
4. **Smallest new hook** only when nothing existing suffices, behind
   `OPTION_RESEARCH_INSTRUMENTATION`, labelled RESEARCH-ONLY, UNSTABLE or DEBUG-ONLY.
5. **Record the configuration.** `tools/research/research init --experiment-id <id>`, then
   `configure` and `build --profile research-debug` (or `baseline-release` for the comparison).
   The manifest records CMake options, compiler and runtime flags.
6. **Validate.** `tools/research/research validate --experiment-id <id> --gate change` and/or
   `--gate research` (manifest completeness).
7. **Report.** `tools/research/research report --experiment-id <id>` writes `summary.md`
   from what was recorded. Use the fail-closed taxonomy (rule
   `project.never-report-unrun-validation-as-pass`).
8. **Do not generalise** beyond the tested configuration. A debug result is not release
   behaviour.

## Final-report checklist

Internal APIs used, internals exposed, instrumentation added, debug flags, runtime flags,
generated diagnostic artefacts, build type, validation performed, observer effects,
release-build applicability, and cleanup or containment status.
