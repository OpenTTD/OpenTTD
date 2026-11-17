#!/usr/bin/env bash
# Layered local validation gate for this OpenTTD research fork.
# See AGENTS.md and research/README.md for what each tier means and why.
set -euo pipefail

SELF="$(basename "$0")"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# Exit status of a tier whose run was incomplete for a reason outside the code (PARTIAL in the
# research/README.md taxonomy): distinct from a failure (1) and from a usage error (2).
EXIT_PARTIAL=3

BASESET_DOC="research/baseset-prerequisite-macos.md"
RESEARCH_BUILD_DIR="build-gate-research"
# Every guard of research-only code, as cmake/Options.cmake defines it.
GATED_MACROS='OTTD_RESEARCH_INSTRUMENTATION|OTTD_PRISMATIC_MCP_SERVER'

usage() {
  cat <<EOF
Usage: $SELF <smoke|change|research|full> [options]

  smoke     Cheapest check, for fast iteration. git diff --check, then an
            incremental build against the existing build/ directory.
            Requires build/ to already be configured (run 'change' or
            'full' first if it isn't).
              -R <ctest-regex>  also run matching tests via 'ctest -R'

  change    Standard check before calling a task done. Configures build/
            if missing, incremental build, full existing ctest suite,
            diff hygiene, and a summary of untracked/modified files to
            review for accidental generated output, plus a check that the
            generated Prismatic MCP contributor docs are not stale
            (tools/check-prismatic-docs.sh), and that every published surface
            agrees with the repository: Markdown regions current, every
            relative link and anchor resolving (tools/surfaces/surfaces check),
            and every file and symbol the .ai/ records cite still exists
            (tools/knowledge/refs check); then lists the records that cite a
            file this change touches (tools/knowledge/refs impact HEAD)
            and how many src/ files no subsystem map tracks
            (tools/knowledge/refs coverage src/, informational).
            Names the research tier when the change touches research-gated
            code.

  research  The change tier for research-gated code, which a default
            build never compiles. Configures $RESEARCH_BUILD_DIR/ with
            OPTION_RESEARCH_INSTRUMENTATION=ON (and OPTION_PRISMATIC_MCP_SERVER=ON
            once cmake/Options.cmake declares it), builds, runs the full
            ctest suite, proves the research console command is in the
            binary, and runs the [prismatic_mcp] tests when the server is built.

  full      Expensive clean-room validation: wipes and recreates
            build-fullgate/, configures + builds + tests from scratch.
            For significant changes or an experimental baseline -- not
            required for small or documentation-only changes.

  -h, --help   Show this help and exit.

Base graphics set: the regression_* cases need one (OpenGFX or the
original TTD data). Without one they hang on macOS instead of failing,
so every tier that runs ctest checks first; when none is reachable it
runs the other cases, reports regression_* as NOT RUN, and exits
$EXIT_PARTIAL (PARTIAL) instead of claiming PASS. See $BASESET_DOC.

Guarantees: never commits or pushes anything, never writes outside this
repository, fails fast on the first failing step.
EOF
}

nproc_portable() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu
  else
    echo 4
  fi
}

JOBS="$(nproc_portable)"

diff_check() {
  echo "==> git diff --check"
  git diff --check
  echo "==> git diff --cached --check"
  git diff --cached --check
}

# The directories OpenTTD searches for base sets that a user or package can populate
# (docs/directory_structure.md): the personal directory per OS, the shared data
# directories, and the build directory's own baseset/.
baseset_search_dirs() {
  local build_dir="$1"
  printf '%s\n' \
    "$ROOT_DIR/$build_dir/baseset" \
    "$ROOT_DIR/baseset" \
    "$HOME/Documents/OpenTTD/baseset" \
    "$HOME/Documents/OpenTTD/content_download/baseset" \
    "${XDG_DATA_HOME:-$HOME/.local/share}/openttd/baseset" \
    "${XDG_DATA_HOME:-$HOME/.local/share}/openttd/content_download/baseset" \
    "$HOME/.openttd/baseset" \
    "/usr/share/games/openttd/baseset" \
    "/usr/local/share/games/openttd/baseset" \
    "/Library/Application Support/OpenTTD/baseset"
}

# True when a usable base graphics set is reachable. An .obg file alone is not enough: the
# build copies the metadata of the original TTD sets into baseset/, and those need the
# original data files. Usable means an OpenGFX-style set (an .obg that is not orig_*, loose
# or inside a .tar) or the original graphics (trg1r.grf, any case).
baseset_available() {
  local build_dir="$1" dir f
  while IFS= read -r dir; do
    [[ -d "$dir" ]] || continue
    while IFS= read -r f; do
      case "$f" in
        *.tar)
          if tar -tf "$f" 2>/dev/null | grep -qiE '\.obg$'; then return 0; fi ;;
        *.obg|*.OBG)
          case "$(basename "$f" | tr '[:upper:]' '[:lower:]')" in
            orig_*) ;;
            *) return 0 ;;
          esac ;;
        *)
          return 0 ;;
      esac
    done < <(find "$dir" -maxdepth 3 \( -iname '*.tar' -o -iname '*.obg' -o -iname 'trg1r.grf' \) 2>/dev/null)
  done < <(baseset_search_dirs "$build_dir")
  return 1
}

# Run the ctest suite of one build directory. Returns 0 on PASS, 1 when a case failed, and
# $EXIT_PARTIAL when the regression cases had to be left out for want of a base graphics set
# and everything else passed. Callers run it as `run_ctest dir || status=$?`, which disables
# set -e inside it, so every ctest status is taken explicitly here.
run_ctest() {
  local build_dir="$1" rc=0
  if baseset_available "$build_dir"; then
    echo "==> ctest --test-dir $build_dir -j$JOBS --timeout 120"
    CTEST_OUTPUT_ON_FAILURE=1 ctest --test-dir "$build_dir" -j"$JOBS" --timeout 120 || rc=$?
    [[ "$rc" -eq 0 ]] || return 1
    return 0
  fi
  echo "==> no base graphics set on any search path: regression_* would hang, not fail"
  echo "==> ctest --test-dir $build_dir -j$JOBS --timeout 120 -E '^regression_'"
  CTEST_OUTPUT_ON_FAILURE=1 ctest --test-dir "$build_dir" -j"$JOBS" --timeout 120 \
    -E '^regression_' || rc=$?
  echo "regression_*: NOT RUN -- install a base graphics set, see $BASESET_DOC"
  [[ "$rc" -eq 0 ]] || return 1
  return "$EXIT_PARTIAL"
}

# Finish a tier: PASS, PARTIAL ($EXIT_PARTIAL) or FAIL (anything else), with its exit status.
finish_tier() {
  local tier="$1" status="$2"
  if [[ "$status" -eq 0 ]]; then
    echo "$tier: PASS"
  elif [[ "$status" -eq "$EXIT_PARTIAL" ]]; then
    echo "$tier: PARTIAL (exit $status)"
    exit "$status"
  else
    echo "$tier: FAIL (exit $status)"
    exit "$status"
  fi
}

# The source and build files of this change, staged or not, relative to HEAD, that carry a
# research guard: a preprocessor conditional on one of the macros, or the CMake option that
# defines it. Documents and this script only name the macros, so they are not code to build.
gated_files_changed() {
  { git diff --name-only HEAD 2>/dev/null; git ls-files --others --exclude-standard; } \
    | sort -u \
    | while IFS= read -r f; do
        [[ -f "$f" ]] || continue
        case "$f" in
          src/*|cmake/*|CMakeLists.txt|*/CMakeLists.txt) ;;
          *) continue ;;
        esac
        if grep -qE "^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|elif).*($GATED_MACROS)" "$f" \
          || grep -qE 'OPTION_(RESEARCH_INSTRUMENTATION|PRISMATIC_MCP_SERVER)' "$f"; then
          echo "$f"
        fi
      done
  return 0
}

# Whether this change, staged or not, touches the panel's pages, charts or stylesheet.
panel_changed() {
  { git diff --name-only HEAD 2>/dev/null; git ls-files --others --exclude-standard; } \
    | grep -qE '^tools/research/mcp-panel/(static|test)/|^tools/research/mcp-panel/input\.css$'
}

# The panel's own tests (rule project.panel-charts-go-through-one-layer): chart options rendered by
# ECharts, the one-door scan, the chart registry, and app.css against a fresh build of its sources.
panel_tests() {
  local dir=tools/research/mcp-panel css=""
  [[ -d "$dir/node_modules/echarts" ]] || (cd "$dir" && npm ci --no-audit --no-fund --silent)
  command -v tailwindcss >/dev/null && css=1
  echo "==> node --test $dir/test/*.test.mjs${css:+ (with the app.css freshness check)}"
  PANEL_CSS_CHECK="$css" node --test "$dir"/test/*.test.mjs
}

cmd_smoke() {
  local ctest_regex=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -R) ctest_regex="${2:?-R requires a ctest regex}"; shift 2 ;;
      *) echo "smoke: unknown option: $1" >&2; usage; exit 2 ;;
    esac
  done

  diff_check

  if [[ ! -f build/CMakeCache.txt ]]; then
    echo "error: build/ is not configured yet. Run '$SELF change' or '$SELF full' first." >&2
    exit 1
  fi

  echo "==> cmake --build build -j$JOBS (incremental)"
  cmake --build build -j"$JOBS"

  if [[ -n "$ctest_regex" ]]; then
    echo "==> ctest --test-dir build -R '$ctest_regex' --output-on-failure"
    ctest --test-dir build -R "$ctest_regex" --output-on-failure
  fi

  echo "smoke: PASS"
}

cmd_change() {
  local status=0 gated f
  diff_check

  echo "==> tools/check-prismatic-docs.sh"
  tools/check-prismatic-docs.sh
  echo "==> tools/surfaces/surfaces check"
  tools/surfaces/surfaces check
  echo "==> tools/research/readme-scoreboard --check (README scoreboard derived from research/results)"
  tools/research/readme-scoreboard --check
  echo "==> tools/knowledge/refs check"
  tools/knowledge/refs check
  echo "==> tools/knowledge/refs impact HEAD (records citing what this change touches: re-read them)"
  tools/knowledge/refs impact HEAD
  echo "==> tools/knowledge/refs coverage src/ (tracked files no subsystem map attaches to; informational)"
  tools/knowledge/refs coverage src/ | tail -n 1
  if panel_changed; then panel_tests; fi

  if [[ ! -f build/CMakeCache.txt ]]; then
    echo "==> cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo (initial configure)"
    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
  fi

  echo "==> cmake --build build -j$JOBS"
  cmake --build build -j"$JOBS"

  run_ctest build || status=$?

  echo "==> git status --porcelain (review below for unwanted generated files)"
  git status --porcelain

  gated="$(gated_files_changed)"
  if [[ -n "$gated" ]]; then
    echo "==> this change touches research-gated code, which this build does not compile:"
    while IFS= read -r f; do printf '      %s\n' "$f"; done <<< "$gated"
    echo "    run '$SELF research' before calling it done"
  fi

  finish_tier change "$status"
}

cmd_research() {
  local status=0 mcp=OFF bin
  local -a opts=(-DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPTION_RESEARCH_INSTRUMENTATION=ON)

  diff_check

  if grep -q 'OPTION_PRISMATIC_MCP_SERVER' cmake/Options.cmake; then
    mcp=ON
    opts+=(-DOPTION_PRISMATIC_MCP_SERVER=ON)
  fi

  # Always (re)configure: cmake is cheap when nothing changed, and an options set that drifted
  # in the cache must not pass for the research configuration.
  echo "==> cmake -S . -B $RESEARCH_BUILD_DIR ${opts[*]}"
  cmake -S . -B "$RESEARCH_BUILD_DIR" "${opts[@]}"

  echo "==> cmake --build $RESEARCH_BUILD_DIR -j$JOBS"
  cmake --build "$RESEARCH_BUILD_DIR" -j"$JOBS"

  bin="$RESEARCH_BUILD_DIR/openttd"
  echo "==> research_status is compiled into $bin"
  # grep -c reads all of its input: grep -q would stop early, strings would die of SIGPIPE, and
  # pipefail would turn a match into a failure.
  if [[ "$(strings "$bin" | grep -c '^research_status')" -eq 0 ]]; then
    echo "error: $bin carries no research_status: the research option did not take effect" >&2
    exit 1
  fi

  run_ctest "$RESEARCH_BUILD_DIR" || status=$?

  if [[ "$mcp" == ON ]]; then
    echo "==> $RESEARCH_BUILD_DIR/openttd_test '[prismatic_mcp]'"
    "$RESEARCH_BUILD_DIR/openttd_test" '[prismatic_mcp]'
  fi

  finish_tier research "$status"
}

cmd_full() {
  local build_dir="build-fullgate" status=0

  diff_check

  echo "==> rm -rf $build_dir (clean room)"
  rm -rf "$build_dir"

  echo "==> cmake -S . -B $build_dir -DCMAKE_BUILD_TYPE=RelWithDebInfo"
  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=RelWithDebInfo

  echo "==> cmake --build $build_dir -j$JOBS"
  cmake --build "$build_dir" -j"$JOBS"

  run_ctest "$build_dir" || status=$?

  finish_tier full "$status"
}

[[ $# -ge 1 ]] || { usage; exit 2; }

case "$1" in
  -h|--help) usage; exit 0 ;;
  smoke) shift; cmd_smoke "$@" ;;
  change) shift; cmd_change "$@" ;;
  research) shift; cmd_research "$@" ;;
  full) shift; cmd_full "$@" ;;
  *) echo "unknown command: $1" >&2; usage; exit 2 ;;
esac
