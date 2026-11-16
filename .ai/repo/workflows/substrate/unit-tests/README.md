---
schema: context/v1
id: fork.substrate.unit-tests
kind: context
title: "Unit tests: the Catch2 openttd_test binary, its ctest registration, mocks, coverage and gate tiers"
description: "Read before adding or changing a test under src/tests/, a mock, the Catch2 CMake glue, or before claiming a change is covered by a unit test."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/tests/**'
  - 'src/3rdparty/icu/tests/**'
  - 'cmake/Catch.cmake'
  - 'cmake/CatchAddTests.cmake'
---

# Unit tests

Read at f8b1d52e80.

## Purpose and ownership

`src/tests/` is OpenTTD's C++ unit test suite: Catch2 v2 cases compiled into one executable,
`openttd_test`, which links the whole game object library. It tests pure helpers (bit math,
strings, containers, slopes, tile spirals), the window-description tables and the network
authentication handshake. Game behaviour end to end is the job of the Squirrel regression suite
(`regression/`, owned by the [scripting](../scripting/README.md) map). **[fork]** The fork adds
one file, `src/tests/test_prismatic_mcp.cpp` (also tracked by
[prismatic-mcp](../prismatic-mcp/README.md)), and its line in `src/tests/CMakeLists.txt`.

## Entry points

- Top-level `CMakeLists.txt`: `add_executable(openttd_test)`,
  `target_link_libraries(openttd_test PRIVATE openttd_lib)`, then `include(Catch)` and
  `catch_discover_tests(openttd_test)`.
- `src/tests/CMakeLists.txt` - one `add_test_files(...)` list; `add_test_files` targets
  `openttd_test` exactly as `add_files` targets `openttd_lib` (`cmake/SourceList.cmake`).
- `src/tests/test_main.cpp` - the only `CATCH_CONFIG_MAIN`; the game's `main()` lives in
  `src/os/*/..._main.cpp`, added to the `openttd` target only, so it is not in the test binary.
- `src/3rdparty/icu/tests/test_srtest.cpp` - ICU script-run case, added only with `ICU_I18N_FOUND`.
- Mocks: `src/tests/mock_environment.h:MockEnvironment` (singleton that loads mock sprites and
  fonts), `src/tests/mock_spritecache.cpp:MockGfxLoadSprites` (selects the `null` blitter and fills
  the sprite cache with empty sprites), `src/tests/mock_fontcache.h:MockFontCache`;
  `MockNetworkSocketHandler` lives inside `src/tests/test_network_crypto.cpp`.
- Discovery: `cmake/Catch.cmake` adds a `POST_BUILD` step that runs
  `cmake/CatchAddTests.cmake`, which lists the cases with `--list-test-names-only` and writes one
  `add_test` per case.

## Data flow

- Build `openttd_test` -> post-build discovery runs the binary -> ctest file with one test per
  `TEST_CASE` name -> `ctest` runs `openttd_test "<case name>"` in its own process per case.
- `ctest -N --test-dir build` lists every case; `regression_*` entries come from
  `cmake/CreateRegression.cmake`, not from Catch2. Count with
  `ctest -N --test-dir build | grep -c 'Test #'`.

## Invariants

- **[verified]** The ctest name of a case is its `TEST_CASE` name, and each case runs as a
  separate process (`cmake/CatchAddTests.cmake`); global state does not leak between cases under
  ctest, but it does when `build/openttd_test` is run directly with several cases.
- **[verified]** `openttd_test` links all of `openttd_lib`, so tests see the real globals
  (`_settings_game`, pools, `Map`) in their static default state, with no game loaded; a case must
  set up what it reads, e.g.
  `Map::Allocate(64, 64)` in `src/tests/tilearea.cpp`.
- **[verified]** Only `TEST_CASE_METHOD(WindowDescTestsFixture, ...)` in
  `src/tests/test_window_desc.cpp` instantiates `MockEnvironment`; the other cases there run
  against `_window_descs` without sprites or fonts.
- **[verified]** **[fork]** `src/tests/test_prismatic_mcp.cpp` is wrapped in
  `#ifdef OTTD_PRISMATIC_MCP_SERVER` and tags its cases `[prismatic_mcp]`; a default build compiles
  an empty file and lists none of them.
- **[verified]** The fork's `tools/gate.sh` runs `ctest --timeout 120` and, without a base graphics
  set, excludes `^regression_` and reports PARTIAL rather than PASS (`tools/gate.sh:run_ctest`).

## Extension points

- New test file: `src/tests/<topic>.cpp` with `#include "../stdafx.h"`,
  `#include "../3rdparty/catch2/catch.hpp"`, the headers under test, `#include "../safeguards.h"`
  last; add it to `add_test_files` in `src/tests/CMakeLists.txt` (a duplicate file name anywhere
  in the target is a CMake `FATAL_ERROR`).
- Tags: `TEST_CASE("name", "[tag]")` lets `openttd_test "[tag]"` run a group; ctest filters by
  name (`ctest -R`).
- A test needing widgets, sprites or fonts: a fixture holding `MockEnvironment::Instance()` and
  `TEST_CASE_METHOD`, as in `src/tests/test_window_desc.cpp`.
- **[fork]** Research-only test: wrap the whole file in the research macro, tag it, and run it in
  the research build (`tools/gate.sh research`).

## Traps and forbidden operations

- Test names must be unique and stable: ctest identifies cases by name, and a renamed case looks
  like a removed one plus a new one in any comparison of runs.
- A case that is compiled out by a macro does not fail, it disappears. Tags are not part of the
  ctest name, so `ctest -R` cannot select `[prismatic_mcp]`; list them with
  `build-gate-research/openttd_test --list-tests '[prismatic_mcp]'` before reporting them as
  passing (`project.never-report-unrun-validation-as-pass`). `tools/gate.sh research` runs
  `openttd_test '[prismatic_mcp]'` itself, after ctest.
- Do not start a video, sound or music driver, read a base set or open a socket in a unit test:
  nothing in the test process initialises them. Use the `null` blitter route of the mocks.
- `catch_discover_tests` executes the freshly built binary at build time (`POST_BUILD`); a binary
  that cannot list its cases breaks the build step, not a ctest case.
- The suite says nothing about commands, savegames, pathfinding, the economy or determinism:
  `git grep -lE 'command_func|saveload/|pathfinder/|random_func|economy_func' -- src/tests` finds no
  file. Evidence for those is a `regression_*` run, a research experiment or play.

## Tests and validation

- Cases by area (`git grep -c TEST_CASE -- src/tests` for counts): core helpers
  (`src/tests/bitmath_func.cpp`, `src/tests/math_func.cpp`, `src/tests/enum_over_optimisation.cpp`,
  `src/tests/flatset_type.cpp`, `src/tests/alternating_iterator.cpp`, `src/tests/history_func.cpp`);
  strings (`src/tests/string_builder.cpp`, `src/tests/string_consumer.cpp`,
  `src/tests/string_inplace.cpp`, `src/tests/string_func.cpp`, `src/tests/utf8.cpp`); map
  geometry (`src/tests/landscape_partial_pixel_z.cpp`, `src/tests/tilearea.cpp`); GUI tables
  (`src/tests/test_window_desc.cpp`); network crypto (`src/tests/test_network_crypto.cpp`); script
  JSON (`src/tests/test_script_admin.cpp`); **[fork]** MCP helpers (`src/tests/test_prismatic_mcp.cpp`).
- Tiers: `tools/gate.sh smoke -R '<case regex>'` while iterating; `tools/gate.sh change` runs the
  whole ctest suite; `tools/gate.sh research` builds `build-gate-research/` and runs the
  `[prismatic_mcp]` cases; `tools/gate.sh full` from a clean configure.
- By hand: `ctest --test-dir build -R 'StringConsumer' --output-on-failure`, or
  `build/openttd_test '[prismatic_mcp]'` in a research build for Catch2's own reporter.

## Decisions, rules and knowledge

- ADR 0002 (validation in three cost tiers behind one gate script), ADR 0027 (CMake only,
  `add_files`/`add_test_files`), ADR 0004 (research access to internals).
- Rules: `project.never-report-unrun-validation-as-pass`, `project.research-code-is-gated-and-labelled`.
- Knowledge: `gated-code-is-invisible-to-a-default-build`, `macos-missing-baseset-hangs-headless-runs`,
  `a-regression-case-fails-on-any-stdout-or-changed-stderr-line`.
- Neighbours: [core utilities](../core-utilities/README.md), [third-party code](../third-party/README.md)
  (Catch2), [scripting](../scripting/README.md) (regression suite), [network](../network/README.md).

## Typical tasks

- To test a new pure helper: add `src/tests/<topic>.cpp`, list it in `src/tests/CMakeLists.txt`,
  run `tools/gate.sh smoke -R '<case name>'`, then `tools/gate.sh change`.
- To test a widget or window table: extend `src/tests/test_window_desc.cpp` with a
  `TEST_CASE_METHOD` on the fixture.
- To test a fork helper: keep it free of game state (as `src/prismatic_mcp/prismatic_mcp_detail.h`),
  add a `[prismatic_mcp]` case, validate with `tools/gate.sh research`.
- To check that a test really ran: `ctest -N` in the build directory that the gate used.
