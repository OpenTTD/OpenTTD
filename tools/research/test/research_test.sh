#!/usr/bin/env bash
# Minimal assertion-style test suite for tools/research/research. No external
# test framework -- this repo has no bash-test convention to reuse, so a
# small self-contained script is the minimal choice (called out in
# research/tooling.md as new, not adapted from an existing pattern).
set -uo pipefail

TEST_ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
RESEARCH="$TEST_ROOT_DIR/tools/research/research"
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/research_test.XXXXXX")"
trap 'rm -rf "$WORKDIR"' EXIT

PASS=0
FAIL=0

assert_eq() {
  local desc="$1" expected="$2" actual="$3"
  if [[ "$expected" == "$actual" ]]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
    printf 'FAIL: %s (expected [%s], got [%s])\n' "$desc" "$expected" "$actual" >&2
  fi
}

assert_true() {
  local desc="$1" cond="$2"
  if [[ "$cond" == "0" ]]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
    printf 'FAIL: %s (exit code %s)\n' "$desc" "$cond" >&2
  fi
}

assert_contains() {
  local desc="$1" haystack="$2" needle="$3"
  if [[ "$haystack" == *"$needle"* ]]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
    printf 'FAIL: %s (expected to find [%s])\n' "$desc" "$needle" >&2
  fi
}

# --- --help ---
out=$("$RESEARCH" --help 2>&1); rc=$?
assert_true "--help exits 0" "$rc"
assert_contains "--help mentions doctor" "$out" "doctor"

# --- unknown command ---
out=$("$RESEARCH" frobnicate 2>&1); rc=$?
assert_eq "unknown command exit code" "2" "$rc"
assert_contains "unknown command message" "$out" "unknown command"

# --- doctor never mutates (no new files under repo root) ---
before=$(cd "$TEST_ROOT_DIR" && git status --porcelain | wc -l | tr -d ' ')
"$RESEARCH" doctor >/dev/null 2>&1
after=$(cd "$TEST_ROOT_DIR" && git status --porcelain | wc -l | tr -d ' ')
assert_eq "doctor does not change git status" "$before" "$after"

# --- missing prerequisite: init without --experiment-id ---
out=$("$RESEARCH" init 2>&1); rc=$?
assert_eq "init without --experiment-id exit code" "2" "$rc"

# --- missing prerequisite: build without configure ---
out=$("$RESEARCH" build --profile research-debug --experiment-id "test-$$" --output-dir "$WORKDIR" 2>&1)
rc=$?
if [[ ! -f "$TEST_ROOT_DIR/build-research/CMakeCache.txt" ]]; then
  assert_eq "build without configured build dir -> missing prereq" "3" "$rc"
else
  # build-research/ already configured on this dev machine by design (reused
  # profile dir) -- in that case this specific check is NOT APPLICABLE rather
  # than faked; init not having been called is still the real prerequisite gap.
  assert_eq "build without init'd experiment -> missing prereq" "3" "$rc"
fi

# --- unsafe output-dir refused ---
out=$("$RESEARCH" init --experiment-id "x-$$" --output-dir "$TEST_ROOT_DIR" 2>&1); rc=$?
assert_eq "unsafe output-dir (repo root) refused" "9" "$rc"

out=$("$RESEARCH" init --experiment-id "x-$$" --output-dir "$TEST_ROOT_DIR/src" 2>&1); rc=$?
assert_eq "unsafe output-dir (tracked src/) refused" "9" "$rc"

# --- --dry-run performs no filesystem mutation ---
"$RESEARCH" init --experiment-id "dryrun-$$" --output-dir "$WORKDIR" --dry-run >/dev/null 2>&1
assert_true "dry-run init leaves no experiment dir" "$([[ -d "$WORKDIR/dryrun-$$" ]] && echo 1 || echo 0)"

# --- init field completeness + manifest validity ---
exp_id="unit-$$"
"$RESEARCH" init --experiment-id "$exp_id" --output-dir "$WORKDIR" --task "unit test task" >/dev/null 2>&1
manifest="$WORKDIR/$exp_id/manifest.json"
assert_true "init creates manifest.json" "$([[ -f "$manifest" ]] && echo 0 || echo 1)"

if command -v python3 >/dev/null 2>&1; then
  python3 -m json.tool "$manifest" >/dev/null 2>&1
  assert_true "manifest.json is valid JSON (python3)" "$?"
  desc=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['task']['description'])" "$manifest" 2>/dev/null)
  assert_eq "manifest task.description round-trips" "unit test task" "$desc"
  schema_version=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['schema_version'])" "$manifest" 2>/dev/null)
  assert_eq "manifest schema_version is 1.0.0" "1.0.0" "$schema_version"
  status=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['status'])" "$manifest" 2>/dev/null)
  assert_eq "fresh manifest status is NOT RUN" "NOT RUN" "$status"
fi

# --- redaction ---
out=$(cd "$TEST_ROOT_DIR" && source tools/research/lib/common.sh && redact "mytool --password=hunter2 --other=ok")
assert_contains "redact masks --password value" "$out" "--password=[REDACTED]"
assert_contains "redact leaves other args alone" "$out" "--other=ok"
assert_true "redact does not leak secret" "$([[ "$out" == *"hunter2"* ]] && echo 1 || echo 0)"

# --- status taxonomy validation ---
out=$(cd "$TEST_ROOT_DIR" && source tools/research/lib/manifest.sh && is_valid_status "PASS"; echo $?)
assert_eq "PASS is a valid status" "0" "$out"
out=$(cd "$TEST_ROOT_DIR" && source tools/research/lib/manifest.sh && is_valid_status "NOT RUN"; echo $?)
assert_eq "'NOT RUN' is a valid status" "0" "$out"
out=$(cd "$TEST_ROOT_DIR" && source tools/research/lib/manifest.sh && is_valid_status "SORT-OF"; echo $?)
assert_eq "'SORT-OF' is not a valid status" "1" "$out"

# --- profile discovery ---
out=$("$RESEARCH" doctor 2>&1)
assert_contains "doctor lists research-debug profile" "$out" "research-debug"
assert_contains "doctor lists baseline-release profile" "$out" "baseline-release"
assert_contains "doctor lists research-asan profile" "$out" "research-asan"

# --- unbuildable profile refused cleanly ---
"$RESEARCH" init --experiment-id "asan-$$" --output-dir "$WORKDIR" >/dev/null 2>&1
out=$("$RESEARCH" build --profile research-asan --experiment-id "asan-$$" --output-dir "$WORKDIR" 2>&1); rc=$?
assert_eq "research-asan build refused (configure-only profile)" "2" "$rc"
assert_contains "research-asan refusal explains why" "$out" "configure-only"

# --- exit-code propagation from a failing recorded command ---
(
  cd "$TEST_ROOT_DIR" || exit 1
  # shellcheck source=lib/common.sh
  source tools/research/lib/common.sh
  ROOT_DIR="$TEST_ROOT_DIR"
  logfile="$WORKDIR/dummy.log"
  run_and_record "dummy" "$logfile" false
  echo "rc=$?"
) > "$WORKDIR/dummy_rc.txt" 2>&1
dummy_rc=$(grep -o 'rc=[0-9]*' "$WORKDIR/dummy_rc.txt" | cut -d= -f2)
assert_eq "run_and_record propagates a failing command's exit code" "1" "$dummy_rc"

# --- clean refuses to remove outside its own experiment dir ---
mkdir -p "$WORKDIR/decoy"
out=$("$RESEARCH" clean --experiment-id ".." --output-dir "$WORKDIR/decoy" 2>&1); rc=$?
assert_true "clean refuses a path-escaping experiment-id" "$([[ $rc -ne 0 ]] && echo 0 || echo 1)"

printf '\n%d passed, %d failed\n' "$PASS" "$FAIL"
[[ "$FAIL" -eq 0 ]]
