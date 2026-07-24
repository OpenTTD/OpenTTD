# Experiment report: baseset-prerequisite-macos

- **Date:** 2026-07-24
- **Base revision:** `87f4762554`
- **Goal / hypothesis:** Establish a clean, fully-validated build of this fork on macOS and get `tools/gate.sh change`/`full` to a real 98/98 PASS. Hypothesis under test once the build succeeded: the four `regression_*` ctest cases were timing out (120 s) not because of a code or build defect, but because a runtime prerequisite (a base graphics set) was missing.
- **Changed files:** none (no tracked files touched — see "Known limitations"). Work was environment setup: system package install, out-of-tree baseset download, and an out-of-tree build.
- **Build/debug configuration:** default `tools/gate.sh` configuration — `cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo`, `OPTION_USE_ASSERTS=ON` (default), `OPTION_DEDICATED=OFF`, ThinLTO enabled automatically for RelWithDebInfo (`-flto=thin`, CMakeLists.txt:301 via `check_ipo_supported`). Toolchain: AppleClang 17.0.0 (Xcode 26) on macOS 26.2 arm64; CMake 4.3.3. No sanitizers, no added instrumentation. The system compiler was sufficient — the Homebrew-LLVM workaround in `CLAUDE.md` (for `<source_location>` on older CLT) was **not** needed on this Xcode.

## Prerequisite discovered

The four `regression_*` ctest cases (`regression_regression`, `regression_stationlist`, `regression_gs`, `regression_gs_compat`) require a **base graphics set** to be installed, even though they run headless (`-vnull -snull -mnull`). Without one, on macOS the game does not fail fast: `HandleBootstrap()` raises a fatal user error that is rendered as a **modal Cocoa dialog** (`NSAlert runModal`), which blocks forever waiting for a click. The test therefore *hangs* rather than failing, and ctest kills it at the 120 s timeout.

Call path confirmed by sampling the stuck process:

```
openttd_main               openttd.cpp:766
  HandleBootstrap()        bootstrap_gui.cpp:420
    UserErrorI()           openttd.cpp:121
      CocoaDialog()        cocoa_wnd.mm:403
        -[NSAlert runModal]   ← blocks indefinitely
```

**Fix / prerequisite:** install a base set into an OpenTTD search path. Used the official OpenGFX/OpenSFX/OpenMSX free sets, placed in the macOS personal dir (`~/Documents/OpenTTD/baseset/`, out of tree):

```
opengfx-8.0.tar     # https://cdn.openttd.org/opengfx-releases/8.0/opengfx-8.0-all.zip
opensfx-1.0.3.tar   # https://cdn.openttd.org/opensfx-releases/1.0.3/opensfx-1.0.3-all.zip
openmsx-0.4.2.tar   # https://cdn.openttd.org/openmsx-releases/0.4.2/openmsx-0.4.2-all.zip
```

Only OpenGFX (graphics) is strictly required to unblock the regressions; SFX/MSX are `null`-driven in the tests but were installed for a complete runnable game. The `.tar` files can stay packed — OpenTTD reads base sets from inside the tarball (`.../opengfx.obg`).

Note: `README.md` §0.5 cites a prior "98/98 in 7.40 s" run. That is not reproducible on a machine with an empty `~/Documents/OpenTTD/` and no base set on any search path — the regressions hang until the base set exists. The prerequisite is environmental and unversioned, so it is easy to miss on a fresh checkout/machine.

## Validation performed

| Command | Exit status | Notes |
|---|---|---|
| `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo` | 0 | breakpad + GRFCodec/NFORenum absent (expected/desired); all other deps found |
| `cmake --build build -j12` | 0 | ~84 s; 0 warnings from own code |
| ctest regression, **no base set** | timeout | 4/98 hang at 120 s (`regression_*`) — root cause above |
| ctest regression, **after OpenGFX install** | 0 | each regression now 0.13–0.35 s |
| `tools/gate.sh change` | 0 | 98/98 tests, 1.06 s |
| `tools/gate.sh full` (clean room `build-fullgate/`) | 0 | clean build ~91 s, 98/98 tests, 1.48 s; `build-fullgate/` removed after |
| `openttd -x -g -vnull:ticks=20000 -snull -mnull -Q` | 0 | headless new-game run, no output, OpenGFX 8.0 detected |

Warning census from the clean-room `full` build: **0 warnings from this codebase**. The only 10 warnings are linker `ld: warning: building for macOS-11.0, but linking with dylib … built for newer version` (5 unique × `openttd`/`openttd_test`), caused by `-mmacosx-version-min=10.15` against Homebrew libs built for macOS 15/26.

## Result

Full build + both gate tiers PASS at 98/98 once a base graphics set is present. The regression timeouts were fully explained by a missing runtime prerequisite (base set), not a code/build defect: on macOS the missing-base-set error surfaces as a blocking modal dialog even under headless null drivers, so the tests hang instead of failing. Installing OpenGFX resolves it; regressions then complete in sub-second time.

## Known limitations

- The prerequisite lives **outside the repo** (`~/Documents/OpenTTD/baseset/`) and is not captured by any versioned artifact or by `tools/gate.sh`, which assumes a base set is already reachable. A fresh machine/checkout will reproduce the hang until a base set is installed. This report documents the requirement; it does not automate it.
- The linker version-min warnings mean the produced binary is **not** guaranteed to run on macOS < 15 despite the 10.15 deployment target; not exercised here.
- GUI was not launched (headless only). Visual/interactive rendering of the installed base set was not verified — only that OpenGFX loads and a headless game advances ticks and exits cleanly.
- Results are for a Debug-info **release** build (`RelWithDebInfo`, asserts on) on macOS 26.2 arm64 / AppleClang 17; not generalized to other build types, compilers, or platforms.

## Status

`PASS` — build and both applicable gate tiers (`change`, `full`) ran to exit 0 at 98/98, and the stated hypothesis (missing base set, not a defect, caused the regression timeouts) was confirmed and resolved.
