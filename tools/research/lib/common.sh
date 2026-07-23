# Shared helpers for tools/research/research. Sourced, not executed directly.
# Bash-only (no jq/python3 hard dependency), portable across GNU/BSD userlands
# (macOS ships BSD sed/date; avoid GNU-only flags).

EXIT_OK=0
EXIT_INVALID_ARGS=2
EXIT_MISSING_PREREQ=3
EXIT_CONFIGURE_FAIL=4
EXIT_BUILD_FAIL=5
EXIT_VALIDATE_FAIL=6
EXIT_RUNTIME_FAIL=7
EXIT_MANIFEST_FAIL=8
EXIT_UNSAFE_PATH=9

log_info() { printf '[research] %s\n' "$*" >&2; }
log_warn() { printf '[research] warning: %s\n' "$*" >&2; }
log_error() { printf '[research] error: %s\n' "$*" >&2; }

die() {
  local code="$1"; shift
  log_error "$*"
  exit "$code"
}

utc_now() { date -u +%Y-%m-%dT%H:%M:%SZ; }

# Minimal JSON string escaper: backslash, double-quote, newline, tab. Not a
# full RFC 8259 escaper -- sufficient for the paths/commands/timestamps this
# tool itself generates, which don't contain other control characters.
json_escape() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  s="${s//$'\n'/\\n}"
  s="${s//$'\t'/\\t}"
  printf '%s' "$s"
}

# Emit a JSON string, or the literal "NOT APPLICABLE" when the value is empty.
json_str_or_na() {
  local v="$1"
  if [[ -z "$v" ]]; then
    printf '"NOT APPLICABLE"'
  else
    printf '"%s"' "$(json_escape "$v")"
  fi
}

# Redact common secret-bearing argument patterns from a command string before
# it is ever written to a log or the manifest. Allowlist-style, not a general
# secret scanner -- documented as such in research/tooling.md.
redact() {
  local s="$1"
  s=$(printf '%s' "$s" | sed -E \
    -e 's/(--?[Pp]assword[=][^ ]*)/--password=[REDACTED]/g' \
    -e 's/(--?[Tt]oken[=][^ ]*)/--token=[REDACTED]/g' \
    -e 's/(--?[Ss]ecret[=][^ ]*)/--secret=[REDACTED]/g' \
    -e 's/(--?[Aa]pi[_-]?[Kk]ey[=][^ ]*)/--api-key=[REDACTED]/g' \
    -e 's/([Aa]uthorization:)[^ ]*/\1 [REDACTED]/g' \
    -e 's/((PASSWORD|TOKEN|SECRET|API_KEY)=)[^ ]*/\1[REDACTED]/g')
  printf '%s' "$s"
}

# Portable `timeout N cmd...`: uses timeout/gtimeout if present, else a
# background-process + watchdog fallback (no GNU coreutils dependency, since
# macOS ships neither `timeout` nor `gtimeout` by default).
run_with_timeout() {
  local secs="$1"; shift
  if command -v timeout >/dev/null 2>&1; then
    timeout "$secs" "$@"
    return $?
  elif command -v gtimeout >/dev/null 2>&1; then
    gtimeout "$secs" "$@"
    return $?
  fi
  "$@" &
  local pid=$!
  ( sleep "$secs" && kill -TERM "$pid" 2>/dev/null ) &
  local watchdog=$!
  local rc=0
  wait "$pid" || rc=$?
  kill "$watchdog" 2>/dev/null || true
  wait "$watchdog" 2>/dev/null || true
  return "$rc"
}

# --- generic append-only JSON line files, embedded as arrays at report time ---

# Append a fully-formed JSON object (already valid JSON text) as one line.
append_json_object_line() {
  local file="$1" obj="$2"
  printf '%s\n' "$obj" >> "$file"
}

# Append a single JSON string value (already escaped by the caller) as one line.
append_json_string_line() {
  local file="$1" text="$2"
  printf '"%s"\n' "$(json_escape "$text")" >> "$file"
}

# Render a JSONL file of already-valid JSON values as a `[...]` array.
embed_array() {
  local file="$1"
  if [[ -s "$file" ]]; then
    printf '[%s]' "$(paste -sd, "$file")"
  else
    printf '[]'
  fi
}

# Record one executed (or dry-run) command: logs it, runs it (unless
# DRY_RUN=1), appends a record to $COMMANDS_JSONL. Relies on the caller having
# set COMMANDS_JSONL and DRY_RUN.
run_and_record() {
  local purpose="$1" logfile="$2"; shift 2
  local cmd_str start end rc
  cmd_str=$(printf '%q ' "$@")
  cmd_str=$(redact "$cmd_str")
  mkdir -p "$(dirname "$logfile")"
  {
    printf '==> %s\n' "$cmd_str"
  } >> "$logfile"
  start=$(utc_now)
  if [[ "${DRY_RUN:-0}" == "1" ]]; then
    printf '[dry-run] not executed\n' >> "$logfile"
    rc=0
  else
    "$@" >>"$logfile" 2>&1
    rc=$?
  fi
  end=$(utc_now)
  if [[ -n "${COMMANDS_JSONL:-}" ]]; then
    local obj
    # Guarantee the file exists before the `<` redirection below -- on a
    # brand-new experiment's first recorded command, COMMANDS_JSONL doesn't
    # exist yet, and a shell input redirection to a missing file prints its
    # own "No such file or directory" to stderr that the command's *own*
    # `2>/dev/null` cannot suppress (that only covers wc's stderr, not the
    # shell's redirection-setup error) -- harmless but confusing noise.
    : >> "$COMMANDS_JSONL"
    obj=$(printf '{"seq":%d,"purpose":"%s","command":"%s","cwd":"%s","started_at":"%s","ended_at":"%s","exit_code":%d,"log":"%s"}' \
      "$(( $(wc -l < "$COMMANDS_JSONL" 2>/dev/null || echo 0) + 1 ))" \
      "$(json_escape "$purpose")" \
      "$(json_escape "$cmd_str")" \
      "$(json_escape "$ROOT_DIR")" \
      "$start" "$end" "$rc" \
      "$(json_escape "${logfile#"$EXPERIMENT_DIR"/}")")
    append_json_object_line "$COMMANDS_JSONL" "$obj"
  fi
  return $rc
}

append_observation() {
  [[ -n "${OBSERVATIONS_JSONL:-}" ]] || return 0
  append_json_string_line "$OBSERVATIONS_JSONL" "$1"
}

append_limitation() {
  [[ -n "${LIMITATIONS_JSONL:-}" ]] || return 0
  append_json_string_line "$LIMITATIONS_JSONL" "$1"
}

append_validation_record() {
  # usage: append_validation_record <gate> <command> <status> <exit_code|null> <scope> <reason>
  [[ -n "${VALIDATIONS_JSONL:-}" ]] || return 0
  local gate="$1" cmd="$2" status="$3" exit_code="$4" scope="$5" reason="$6"
  local obj
  obj=$(printf '{"gate":"%s","command":"%s","status":"%s","exit_code":%s,"scope":"%s","reason":"%s"}' \
    "$(json_escape "$gate")" "$(json_escape "$cmd")" "$(json_escape "$status")" \
    "$exit_code" "$(json_escape "$scope")" "$(json_escape "$reason")")
  append_json_object_line "$VALIDATIONS_JSONL" "$obj"
}

# --- state persisted across CLI invocations for one experiment ---
# A plain shell-assignment file (values written via printf %q), sourced
# wholesale rather than parsed key-by-key. Internal plumbing only -- the
# versioned, canonical output is manifest.json, produced from this state.
set_state() {
  local key="$1"; shift
  local val="$*"
  local statefile="$EXPERIMENT_DIR/.state.sh"
  touch "$statefile"
  grep -v "^${key}=" "$statefile" > "${statefile}.tmp" 2>/dev/null || true
  mv "${statefile}.tmp" "$statefile"
  printf '%s=%q\n' "$key" "$val" >> "$statefile"
}

load_state() {
  local statefile="$EXPERIMENT_DIR/.state.sh"
  # shellcheck disable=SC1090
  [[ -f "$statefile" ]] && source "$statefile"
}

# --- safety checks ---

is_safe_experiment_id() {
  local id="$1"
  [[ -n "$id" ]] || return 1
  [[ "$id" != "." && "$id" != ".." ]] || return 1
  [[ "$id" =~ ^[A-Za-z0-9._-]+$ ]] || return 1
  return 0
}

is_safe_output_dir() {
  local dir="$1"
  [[ "$dir" = /* ]] || return 1
  [[ "$dir" != "$ROOT_DIR" ]] || return 1
  [[ "$dir" != "$HOME" ]] || return 1
  [[ "$dir" != "/" ]] || return 1
  local rel="${dir#"$ROOT_DIR"/}"
  if [[ "$rel" != "$dir" ]]; then
    if git -C "$ROOT_DIR" ls-files --error-unmatch -- "$rel" >/dev/null 2>&1; then
      return 1
    fi
    if [[ -n "$(git -C "$ROOT_DIR" ls-files -- "$rel" 2>/dev/null)" ]]; then
      return 1
    fi
  fi
  return 0
}
