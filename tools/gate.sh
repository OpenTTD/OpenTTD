#!/usr/bin/env bash
# Layered local validation gate for this OpenTTD research fork.
# See AGENTS.md and research/README.md for what each tier means and why.
set -euo pipefail

SELF="$(basename "$0")"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

usage() {
  cat <<EOF
Usage: $SELF <smoke|change|full> [options]

  smoke   Cheapest check, for fast iteration. git diff --check, then an
          incremental build against the existing build/ directory.
          Requires build/ to already be configured (run 'change' or
          'full' first if it isn't).
            -R <ctest-regex>  also run matching tests via 'ctest -R'

  change  Standard check before calling a task done. Configures build/
          if missing, incremental build, full existing ctest suite,
          diff hygiene, and a summary of untracked/modified files to
          review for accidental generated output.

  full    Expensive clean-room validation: wipes and recreates
          build-fullgate/, configures + builds + tests from scratch.
          For significant changes or an experimental baseline -- not
          required for small or documentation-only changes.

  -h, --help   Show this help and exit.

Guarantees: never commits or pushes anything, never touches files
outside this repository, fails fast on the first failing step.
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
  diff_check

  if [[ ! -f build/CMakeCache.txt ]]; then
    echo "==> cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo (initial configure)"
    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
  fi

  echo "==> cmake --build build -j$JOBS"
  cmake --build build -j"$JOBS"

  echo "==> ctest --test-dir build -j$JOBS --timeout 120"
  CTEST_OUTPUT_ON_FAILURE=1 ctest --test-dir build -j"$JOBS" --timeout 120

  echo "==> git status --porcelain (review below for unwanted generated files)"
  git status --porcelain

  echo "change: PASS"
}

cmd_full() {
  local build_dir="build-fullgate"

  diff_check

  echo "==> rm -rf $build_dir (clean room)"
  rm -rf "$build_dir"

  echo "==> cmake -S . -B $build_dir -DCMAKE_BUILD_TYPE=RelWithDebInfo"
  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=RelWithDebInfo

  echo "==> cmake --build $build_dir -j$JOBS"
  cmake --build "$build_dir" -j"$JOBS"

  echo "==> ctest --test-dir $build_dir -j$JOBS --timeout 120"
  CTEST_OUTPUT_ON_FAILURE=1 ctest --test-dir "$build_dir" -j"$JOBS" --timeout 120

  echo "full: PASS"
}

[[ $# -ge 1 ]] || { usage; exit 2; }

case "$1" in
  -h|--help) usage; exit 0 ;;
  smoke) shift; cmd_smoke "$@" ;;
  change) shift; cmd_change "$@" ;;
  full) shift; cmd_full "$@" ;;
  *) echo "unknown command: $1" >&2; usage; exit 2 ;;
esac
