# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Private fork — see AGENTS.md first

`korczis/OpenTTD` is a private academic research fork used as a validation target for the Prismatic platform, not staged for upstream contribution. The full ground rules, the four-kinds-of-change distinction, and the layered validation model (`tools/gate.sh`, `research/`) live in [AGENTS.md](./AGENTS.md) and apply to Claude Code exactly as to any other agent — read that first. This file only adds Claude-specific workflow notes and OpenTTD's own technical reference (build/run/test, architecture, code style) on top.

## Research mode: internals, debugging, and instrumentation

The full policy is defined once, vendor-neutrally, in [AGENTS.md](./AGENTS.md) under "Research-mode access to internals" — read it first; this section only adds Claude Code-specific workflow on top.

- Internals are not off-limits here. Upstream's own contribution norms (public-API purity, ABI stability, etc.) explain *why* the code looks the way it does, but they are not this fork's local process authority (see AGENTS.md, and the "Project culture" section below, which documents *upstream's* norms for context only).
- A debug build, an added trace point, a state dump, or a research-only hook is a legitimate deliverable of a session here — not something to avoid or apologize for.
- Research-only code is allowed to be intrusive (added counters, timestamps, invariant checks, state snapshots) when the experiment needs it — see AGENTS.md's containment rules for keeping it scoped and removable.
- Changes must still be scoped, auditable in the diff, and reversible; report the debug/build configuration actually used.
- Never mark a validation step as PASS that wasn't actually run — this applies to research/debug validation exactly as much as to build/test gates (`research/README.md`).
- Use an internal API directly rather than inventing an artificial public abstraction just to preserve a production design ideal the experiment doesn't need.

When an experiment depends on an internal implementation detail, record that dependency explicitly rather than hiding it behind misleadingly generic terminology.

Workflow for a task that needs internal observability:

1. Identify the research question.
2. Identify the internal state or behavior that must become observable.
3. Check for an existing debug or internal interface first — e.g. `-d <facility>=<level>` (facilities in `src/debug.h`: `driver`, `grf`, `map`, `misc`, `net`, `sprite`, `oldloader`, `yapf`, `fontcache`, `script`, `sl`, `gamelog`, `desync`, `console`, `random`), `OPTION_USE_ASSERTS`, or existing logging.
4. Reuse it when practical.
5. Otherwise add the smallest research-only hook or instrumentation point.
6. Gate or clearly label the interface (e.g. `RESEARCH-ONLY` / `DEBUG-ONLY` / `UNSTABLE` — see AGENTS.md).
7. Capture the exact build and runtime configuration — `tools/research/research init` then
   `configure`/`build --profile research-debug` (or `baseline-release` for a comparison run)
   records the CMake options, compiler flags, and runtime flags into `manifest.json`
   automatically; see `research/tooling.md`.
8. Run the relevant validation tier — `tools/research/research validate --gate change` (wraps
   `tools/gate.sh change`) and/or `--gate research` (manifest completeness), see README.md 0.5.
9. Report observer effects and limitations — `tools/research/research report` finalizes
   `manifest.json` and generates `summary.md` from what was actually recorded.
10. Do not generalize beyond the tested configuration.

Final-report checklist for a session that touched internals: internal APIs used, internals exposed, instrumentation added, debug flags enabled, runtime flags enabled, generated diagnostic artifacts, build type, validation performed, observer effects, release-build applicability, and cleanup/containment status.

## What this is

OpenTTD is a C++20 open-source transport simulation game (a re-implementation/extension of Transport Tycoon Deluxe). It's a mature, long-lived project (20+ years) with a large, careful, mostly-volunteer maintainer base upstream. The "Project culture" section below documents *their* norms for context (e.g. if reading upstream docs/history), not rules this fork must follow.

## Build

CMake is the only supported build system, minimum version 3.16.

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)          # or: cmake --build . -j$(nproc)
```

Useful CMake options (pass as `-DOPTION=...` at configure time):
- `-DCMAKE_BUILD_TYPE=RelWithDebInfo` — release build (default is an unoptimized Debug build with asserts, much slower to run).
- `-DOPTION_DEDICATED=ON` — headless dedicated-server build, no GUI/video/sound deps needed.
- `-DOPTION_USE_ASSERTS=OFF` — disable asserts (on by default outside release builds).
- `-DOPTION_TOOLS_ONLY=ON` — only build tools like `strgen`/`settingsgen` (useful for cross-compiling).

Requires a compiler with full C++20 support. Encouraged (not required) external libs: zlib, liblzma, libpng, breakpad; Linux additionally wants libcurl, SDL2, freetype, fontconfig, harfbuzz, libicu. Windows dependencies are managed via vcpkg (see `COMPILING.md`).

**macOS-specific gotcha**: the game uses Cocoa (not SDL2) for video on macOS, so SDL2 isn't needed there. However, `<source_location>` (used in `src/stdafx.h`) is only available in the libc++ shipped with newer Xcode/Command Line Tools (Xcode 15+). If your CLT is older (e.g. AppleClang 14.x from an older CLT-only install), compilation fails with `'source_location' file not found`. Fix by building with Homebrew's LLVM toolchain instead of the system compiler:
```bash
CC="$(brew --prefix llvm)/bin/clang" CXX="$(brew --prefix llvm)/bin/clang++" cmake ..
```

Do not run GRFCodec/NFORenum locally unless intentional — if installed, CMake will regenerate `.grf` baseset files in-tree during the build, which can leave the working tree dirty with binary diffs.

## Running

The built binary is `build/openttd` (or `build/openttd.exe` on Windows). It needs base graphics/sound/music files (OpenGFX/OpenSFX/OpenMSX, or original TTD data files) in a `baseset/` directory it can find — see `docs/directory_structure.md` for search paths. Without any base set it will fail to find graphics.

Useful flags: `-g` (start a new game immediately), `-D` (start dedicated server), `-d <facility>=<level>` (debug logging, see `src/debug.h`).

## Test

```bash
cd build
ctest -j$(nproc) --timeout 120
```

This runs both the C++ unit tests (`src/tests/`, Catch2-based, see `src/tests/CMakeLists.txt`) and the regression suite (`regression/`) which exercises the AI/GameScript API (Squirrel) end-to-end. `CTEST_OUTPUT_ON_FAILURE=1` is useful to set. To run a single ctest test: `ctest -R <name>`.

For this fork's own layered validation (smoke/change/full — see [AGENTS.md](./AGENTS.md)), use `tools/gate.sh` instead of calling cmake/ctest by hand; it wraps exactly these commands with the right tier of cost for the situation.

There is no separate lint step beyond compiler warnings (build is warning-heavy: `-Wall -Wextra -Wcast-qual -Wundef ...`) and the coding-style/commit-message checks enforced by CI and the git hooks described below.

## Commit messages (enforced by CI)

First line must match:
```
<keyword>( #<issue>|<commit>(, (#<issue>|<commit>))*)?: ([<component>])? <details>
```
Player-facing keywords: `Feature`, `Add`, `Change`, `Fix`, `Remove`, `Revert`, `Doc`. Developer-facing: `Codechange`, `Cleanup`, `Codefix`. NewGRF/Script-facing changes reuse the player-facing keywords with a `[NewGRF]`/`[Script]` component tag. `<details>` starts capitalized, no trailing period, and describes player-visible effect rather than the code change. Examples: `Fix #5926: [YAPF] Infinite loop in pathfinder`, `Codechange #1264: Rewrite the autoreplace kernel`. Full rules and rationale in `CODINGSTYLE.md`.

Client-side commit-message-checking git hooks are available at `https://github.com/OpenTTD/OpenTTD-git-hooks` (symlinked into `.git/hooks`) — see `CONTRIBUTING.md` for setup.

## Code style highlights

Full rules in `CODINGSTYLE.md`; the parts most likely to trip up unfamiliar contributors:
- Functions/classes: `CamelCase`. Variables: `lower_case_with_underscores`. Globals prefixed `_`. Class members accessed via explicit `this->`.
- Opening brace on the same line as the function signature is the *exception* — for functions the `{` goes on its own next line, but for `if`/`for`/`while`/`switch` control flow the `{` stays on the statement's line (see `CODINGSTYLE.md` "Control flow" section for the full brace/space conventions).
- Unscoped enum values are `ALL_CAPS`; scoped enum values are `CamelCase`. Enums commonly include `_BEGIN`/`_END`/`INVALID_*` sentinels.
- Everything is documented with Doxygen/JavaDoc-style (`/** ... */`) comments — briefs end at the first `. ` or newline.
- Every project-defined pointer/reference variable convention exists (`Vehicle *v`, `Station *st`, `Town *t`, etc.) — grep existing code for the established name before inventing a new one.
- Templates go in `.hpp` files (not `.h`) to signal they include implementation.

## Architecture

The codebase (`src/`) is organized by subsystem, but a few cross-cutting patterns matter more than any single directory:

### The command system is the core invariant

**All game-state mutation must go through `src/command.cpp` / `command_type.h`.** Each mutation is registered as a `Commands` enum value, implemented as a `CommandCost Cmd...(DoCommandFlags flags, ...)` function in a per-subsystem `*_cmd.cpp` file (`vehicle_cmd.cpp`, `company_cmd.cpp`, `road_cmd.cpp`, `terraform_cmd.cpp`, etc.). Commands run in two phases — a no-side-effects "test" pass, then an "execute" pass — and when in a network game, the command is instead sent over the wire (`network_command.cpp`) and executed identically by every client at the same game tick.

This exists because OpenTTD multiplayer is **deterministic lockstep**: every client must compute bit-identical results from the same command stream. Practical implications for any code touching game state:
- Never mutate persistent game state (vehicles, companies, map tiles, etc.) outside a command handler.
- Command handlers must be deterministic: no wall-clock time, no host-dependent iteration/float order, no uninitialized reads.
- AI and Game Scripts (Squirrel) issue the same `Commands` as human players via the script API — they don't get a side channel.

### Game loop

`openttd_main()` (`src/openttd.cpp`) initializes subsystems and hands control to a `VideoDriver` (`src/video/`, backends per-platform: `cocoa/`, `win32_v.cpp`, `sdl2_v.cpp`, `dedicated_v.cpp`, `null_v.cpp`). Each driver's `MainLoop()` calls `GameLoop()` (`openttd.cpp`) once per real-time tick, then draws a frame. `GameLoop()` either runs `NetworkGameLoop()` (multiplayer) or `StateGameLoop()` (single player) — the latter is where the actual tick-by-tick simulation (vehicles, economy, `AI::GameLoop()`, `Game::GameLoop()`) advances. Video drivers are just the real-time pump; determinism lives in `StateGameLoop`.

### Object storage: pools

Game objects (`Vehicle`, `Station`, `Town`, `Company`, `Order`, ...) live in slot-based pools (`src/core/pool_type.hpp`, `Pool<Titem, Tindex, ...>`), addressed by strongly-typed IDs rather than raw pointers. This makes handles stable across savegame serialization and network sync, and gives free-list slot reuse when objects are destroyed.

### Savegame compatibility

`src/saveload/` splits savegames into per-subsystem "chunks" (`vehicle_sl.cpp`, `company_sl.cpp`, ...), each a `ChunkHandler` with `Save()`/`Load()`. Every revision must be able to load every older savegame, so field layouts are versioned against a monotonically increasing `SaveLoadVersion`, gated with `IsSavegameVersionBefore()`-style checks and `SaveLoadCompat` tables. **Never freely remove, reorder, or renumber existing saved fields/chunks** — old fields get a version-gated load-and-discard path instead of deletion; new fields must be added behind a version check.

### NewGRF (moddable content)

`src/newgrf/` parses and applies NewGRFs — user-supplied binary mod files that redefine vehicles, industries, stations, cargo, sprites, etc. via "action" opcodes (`newgrf_act0_*.cpp` per feature). This is the primary runtime content-modding layer, loaded at game/save init and revalidated against savegames.

### Scripting (AI / Game Scripts)

`src/script/squirrel.cpp/hpp` embeds the Squirrel language VM. `src/ai/` (company AI opponents) and `src/game/` (scenario/goal "Game Scripts") each layer a scanner/info/instance (`ai_instance.cpp`, `game_instance.cpp`) on top, loading `.nut` scripts against a stable API surface in `src/script/api/`.

### Pathfinding

`src/pathfinder/yapf/` ("Yet Another Path Finder") is the main pathfinder — a generic A*-like framework (`yapf_base.hpp` plus cost/node/destination policy headers) specialized per vehicle type (`yapf_rail.cpp`, `yapf_road.cpp`, `yapf_ship.cpp`), plus `water_regions.cpp`/`yapf_ship_regions.cpp` for ship-routing region precomputation.

### Networking

`src/network/`: classic client/server model (`network_server.cpp` authoritative host, `network_client.cpp` per client). `network_command.cpp` is the bridge between the command system and the wire — it's the transport for the deterministic command stream described above. Separate pieces handle chat, content downloads, and the admin/coordinator connections (see `docs/admin_network.md`, `docs/game_coordinator.md`).

### Directory map (top-level `src/` only; don't re-derive substructure that's obvious from listing the directory)

`ai/`, `game/`, `script/` — scripting (see above). `network/` — multiplayer. `newgrf/` — mod content. `saveload/` — savegame chunks. `pathfinder/` — YAPF. `video/`, `sound/`, `music/`, `blitter/`, `spriteloader/`, `fontcache/` — platform/rendering/audio backends. `core/` — generic containers/utilities (pools, math, string handling) with no game-domain knowledge. `3rdparty/` — vendored deps (Squirrel, fmt, ICU wrapper, Catch2, monocypher, nlohmann json). `table/` — generated/static data tables. `os/` — per-OS glue (macosx/unix/windows). `strgen/`, `settingsgen/` — build-time code generators for translated strings and settings.

## Project culture (from `CONTRIBUTING.md`)

- **Ask before large changes.** Significant features, refactors, or ports should be discussed (Discord/IRC/forums) before investing implementation time — PRs without prior buy-in risk not being merged.
- **AI-generated contributions are explicitly against project policy.** From `CONTRIBUTING.md`: issues/PRs that are LLM-generated will be closed, and repeat submitters may be blocked. A human must understand and be able to explain every line of a PR; AI may only be used to proofread text, not to write it. Keep this in mind if this repository's own contribution workflow is ever the subject of a task (vs. using Claude Code as a personal/local dev tool, which is a separate matter from what gets *submitted upstream*).
- **Docs entry points**: `docs/savegame_format.md`, `docs/linkgraph.md`, `docs/game_coordinator.md`, `docs/admin_network.md`, `docs/logging_and_performance_metrics.md`, `docs/eints.md` (translation workflow — don't hand-edit language files, they're synced from the translator tool).
