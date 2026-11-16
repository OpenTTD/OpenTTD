---
schema: context/v1
id: fork.substrate.build-and-packaging
kind: context
title: "Build and packaging: CMake options, add_files source lists, generated sources, version stamping, compilers, packaging and the fork's build profiles"
description: "Read before changing a CMakeLists.txt or cmake/ module, adding or moving a source file, adding a CMake option or dependency, touching version stamping or packaging, or choosing which build configuration validates a change."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'CMakeLists.txt'
  - 'src/CMakeLists.txt'
  - 'src/**/CMakeLists.txt'
  - 'cmake/**'
  - 'media/CMakeLists.txt'
  - 'media/**/CMakeLists.txt'
  - 'bin/CMakeLists.txt'
  - 'src/rev.*'
  - 'COMPILING.md'
---

# Build and packaging

Read at f8b1d52e80.

## Purpose and ownership

Owns the CMake build (ADR 0027): the top-level `CMakeLists.txt`, the per-directory source lists,
`cmake/` modules (options, compile flags, dependency finders, packaging), the generated sources
and data (`rev.cpp`, `table/strings.h`, `lang/*.lng`, settings tables, script API, base-set
metadata) and installation/CPack. The generators' inputs belong to their maps:
[strings](../strings-and-languages/README.md) (strgen), [settings](../settings/README.md)
(settingsgen), [scripting](../scripting/README.md) (script API, regression targets),
[unit tests](../unit-tests/README.md) (`openttd_test`), [third party](../third-party/README.md).
The fork's validation front ends (`tools/gate.sh`, `tools/research/research`) wrap this build.

## Entry points

- `CMakeLists.txt` - project, C++20, `find_package` for every library, the `openttd_lib` object
  library plus the `openttd` and `openttd_test` executables, `link_package` calls, `regression`.
- `cmake/Options.cmake:set_options` / `cmake/Options.cmake:add_definitions_based_on_options` -
  every `OPTION_*` and the macros it turns into (`DEDICATED`, `WITH_ASSERT`/`NDEBUG`,
  `ALLOW_INVALID_SIGNATURE`, `SURVEY_KEY`).
- **[fork]** `OPTION_RESEARCH_INSTRUMENTATION` -> `-DOTTD_RESEARCH_INSTRUMENTATION`;
  `OPTION_PRISMATIC_MCP_SERVER` -> `-DOTTD_PRISMATIC_MCP_SERVER`, with a `FATAL_ERROR` unless the
  first is ON (`cmake/Options.cmake`); `src/prismatic_mcp/CMakeLists.txt` adds its sources
  `CONDITION OPTION_PRISMATIC_MCP_SERVER`. Fork hunks:
  `git diff --stat upstream/master..HEAD -- CMakeLists.txt cmake '*CMakeLists.txt'`.
- `cmake/SourceList.cmake:add_files` / `add_test_files` - add sources to `openttd_lib` /
  `openttd_test`, optionally `CONDITION <if-expression>`.
- `cmake/LinkPackage.cmake:link_package` - a found package defines `WITH_<NAME>` and links
  `openttd_lib`; `ENCOURAGED` only warns when it is missing.
- `cmake/scripts/FindVersion.cmake` - run by the `find_version` target on every build; writes
  `generated/rev.cpp` from `src/rev.cpp.in` (`src/rev.h:_openttd_revision`).
- `cmake/AddCustomXXXTimestamp.cmake:add_custom_command_timestamp` - generator commands that only
  touch their outputs when the content changed.
- `cmake/InstallAndPackage.cmake` - `install()` rules and the CPack generator per platform
  (`Bundle` on macOS, `ZIP`/`NSIS` on Windows, `DEB`/`RPM`/`TXZ` on Linux;
  `cmake/PackageBundle.cmake`, `cmake/PackageNSIS.cmake`, `cmake/PackageDeb.cmake`,
  `cmake/PackageRPM.cmake`).
- `cmake/CompileFlags.cmake:compile_flags` - warning set (`-Wall -Wextra -Wcast-qual -Wundef ...`).

## Lifecycle

- Configure: `cmake -S . -B <dir>` refuses an in-source build, defaults to `Debug`, reads options,
  finds libraries, adds `bin/`, `src/` (each `src/<dir>/CMakeLists.txt`), `media/`, then
  `add_definitions_based_on_options` and `regression/`; writes `compile_commands.json`.
- Tools first: `src/strgen/` and `src/settingsgen/` define host executables (a separate project
  when `HOST_BINARY_DIR` names a native build, imported from `strgen.cmake`/`settingsgen.cmake`);
  `OPTION_TOOLS_ONLY` stops after them.
- Build: `find_version` -> `rev.cpp`; `table_strings`/`language_files` (strgen);
  `table_settings` (settingsgen); `script_window` and the API docs/includes
  (`cmake/scripts/GenerateWidget.cmake`, `cmake/scripts/SquirrelExport.cmake`); base-set metadata
  (`cmake/scripts/Baseset.cmake` reads `STR_*` descriptions from every `src/lang/*.txt`); then
  `openttd_lib` with precompiled `src/stdafx.h`, linked into `openttd` and `openttd_test`.
- Test: `ctest` runs the Catch2 cases discovered from `openttd_test` and one `regression_<name>`
  per script suite. Package: `cmake --build <dir> --target package` (CPack).

## Invariants

- **[verified]** `add_files` with a false `CONDITION` adds nothing, and a basename already in the
  target is a configure `FATAL_ERROR` (3rdparty headers excepted)
  (`cmake/SourceList.cmake:_add_files_tgt`).
- **[verified]** `main()` is not in `openttd_lib`: each `src/os/*/CMakeLists.txt` adds its
  `*_main.cpp` to `openttd` only, which is why `openttd_test` can link the whole library.
- **[verified]** `OPTION_USE_ASSERTS` defaults ON for every build type, so `RelWithDebInfo`
  keeps asserts unless `-DOPTION_USE_ASSERTS=OFF` is given (`cmake/Options.cmake:set_options`).
- **[verified]** The version string is the tag, or `<isodate>-<branch>-g<hash>` with `-m` instead
  of `-g` when `git diff-index HEAD` lists anything; it is regenerated on every build. Untagged
  peers join only when the suffix after the last `-`, marker letter included, is equal
  (`src/network/core/network_game_info.cpp:IsNetworkCompatibleVersion`).
- **[verified]** **[fork]** `OPTION_PRISMATIC_MCP_SERVER=ON` without
  `OPTION_RESEARCH_INSTRUMENTATION=ON` fails the configure; both default OFF, so a default build
  compiles no research code (`project.research-code-is-gated-and-labelled`).
- **[supported]** Definitions from `add_definitions` reach `openttd_lib`/`openttd_test` because
  both are defined in the top-level directory (CMake semantics); a new target defined in a
  subdirectory before those calls would not see them.

## Extension points

- New source file: list it in the `add_files(...)` of its directory's `CMakeLists.txt`, with
  `CONDITION` when it needs an optional library or platform; a new directory also needs an
  `add_subdirectory` in `src/CMakeLists.txt`. Tests use `add_test_files` in `src/tests/`.
- New dependency: `find_package` in `CMakeLists.txt` (a `cmake/Find<Name>.cmake` if CMake has
  none), then `link_package(<Name> [TARGET ...] [ENCOURAGED])`, guard code with `WITH_<NAME>`.
- New option: `option()` in `cmake/Options.cmake:set_options`, a status line in `show_options`,
  its macro in `add_definitions_based_on_options`; research-only options stay OFF by default and
  are added to the `research` tier of `tools/gate.sh`.
- New generated file: an `add_custom_command_timestamp` plus `add_custom_target_timestamp`, and
  an `add_dependencies` from the library that includes it.

## Traps and forbidden operations

- macOS: `src/stdafx.h` needs `<source_location>` (Xcode 15+ libc++). With an older CLT use
  Homebrew LLVM (`CC=$(brew --prefix llvm)/bin/clang`); with a current Xcode do not, its libc++
  headers fail against the SDK (ADR 0006, knowledge `research-cli-keeps-the-system-compiler`).
- A found GRFCodec/NFORenum regenerates `media/baseset/openttd.grf` and `orig_extra.grf` in the
  source tree (`media/baseset/CMakeLists.txt`); clear `GRFCODEC_EXECUTABLE` in the cache to keep
  the tree clean (`COMPILING.md`).
- Docs are behind the tree: `COMPILING.md` still says CMake 3.16 (the tree requires 3.17) and that
  release builds disable asserts; `CLAUDE.md` names SDL2 where `CMakeLists.txt` finds SDL3.
- A missing `ENCOURAGED` library is a warning, not an error: the binary silently lacks PNG
  screenshots, compressed saves or HTTP. Read the `-- <NAME> found` lines of the configure.
- Gated code is invisible to a default build: a green `tools/gate.sh change` says nothing about
  `#ifdef OTTD_*` blocks (knowledge `gated-code-is-invisible-to-a-default-build`).
- Never share one build directory between profiles: `tools/research/research configure` refuses a
  directory whose cached `CMAKE_BUILD_TYPE`/`OPTION_RESEARCH_INSTRUMENTATION` differ.

## Tests and validation

- `tools/gate.sh smoke [-R <regex>]` - incremental build of `build/`; `change` - configures
  `build/` as `RelWithDebInfo` (asserts on) if missing, builds, full `ctest`, plus the docs, surfaces
  and `tools/knowledge/refs check` gates; `research` - `build-gate-research/` with both fork
  options ON, full `ctest` and `openttd_test '[prismatic_mcp]'`; `full` - clean `build-fullgate/`.
- Research profiles (`tools/research/lib/profiles.sh`): `research-debug` (`build-research/`,
  Debug, instrumentation ON, MCP server not enabled), `baseline-release`
  (`build-research-baseline-release/`, RelWithDebInfo, asserts and instrumentation OFF),
  `research-asan` (configure-only); see `research/tooling.md`.
- A CMake change touches every platform; name the ones not configured in the report.
- `regression_*` cases need a base set and report NOT RUN without one (`tools/gate.sh` exits 3).

## Decisions, rules and knowledge

- ADR 0027 (CMake only, per-directory `add_files` with conditions), ADR 0002 (three gate tiers),
  ADR 0005 (research CLI and profiles), ADR 0006 (compiler choice on macOS), ADR 0022 (driver
  files selected by condition), ADR 0028 (base sets built from `media/`).
- Rules: `project.research-code-is-gated-and-labelled`, `project.never-report-unrun-validation-as-pass`.
- Knowledge: `gated-code-is-invisible-to-a-default-build`, `research-cli-keeps-the-system-compiler`,
  `a-research-build-and-a-default-build-join-the-same-game`, `widget-enums-are-part-of-the-script-api`,
  `a-build-from-a-modified-tree-cannot-join-a-clean-build`.

## Typical tasks

- To add a source file: add it to its directory's `add_files`, reconfigure is automatic; validate
  with `tools/gate.sh change` (and `research` if it is gated).
- To add a research-only module: a directory with `add_files(... CONDITION OPTION_<X>)`, an
  `add_subdirectory` line, the option in `cmake/Options.cmake` depending on
  `OPTION_RESEARCH_INSTRUMENTATION`; validate with `tools/gate.sh research`.
- To compare instrumented and plain builds: `tools/research/research build --profile research-debug`
  and `--profile baseline-release` with one experiment id; `report` records both configurations.
- To make a release-like package: configure `-DCMAKE_BUILD_TYPE=RelWithDebInfo
  -DOPTION_USE_ASSERTS=OFF`, build, `cmake --build <dir> --target package`.
