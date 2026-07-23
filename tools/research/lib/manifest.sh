# Manifest rendering + the research/evaluation gate for tools/research/research.
# Requires common.sh to already be sourced (json_escape, json_str_or_na,
# embed_array, load_state) and the caller to have set: EXPERIMENT_DIR,
# EXPERIMENT_ID, MANIFEST_JSON, COMMANDS_JSONL, VALIDATIONS_JSONL,
# OBSERVATIONS_JSONL, LIMITATIONS_JSONL, ROOT_DIR.

MANIFEST_SCHEMA_VERSION="1.0.0"

# manifest_write <phase>
#   phase=initial : called by `init`, empty commands/validations, status "NOT RUN"
#   phase=final   : called by `validate --gate research` and `report`, embeds
#                   accumulated commands/validations/observations/limitations
manifest_write() {
  local phase="$1"
  load_state

  local commands_json validations_json observations_json limitations_json
  if [[ "$phase" == "final" ]]; then
    commands_json=$(embed_array "$COMMANDS_JSONL")
    validations_json=$(embed_array "$VALIDATIONS_JSONL")
    observations_json=$(embed_array "$OBSERVATIONS_JSONL")
    limitations_json=$(embed_array "$LIMITATIONS_JSONL")
  else
    commands_json="[]"
    validations_json="[]"
    observations_json="[]"
    limitations_json="[]"
  fi

  local completed_field="null"
  if [[ -n "${COMPLETED_AT:-}" ]]; then
    completed_field="\"$(json_escape "$COMPLETED_AT")\""
  fi

  local dirty_after_field="null"
  if [[ -n "${DIRTY_AFTER:-}" ]]; then
    dirty_after_field="$DIRTY_AFTER"
  fi

  local build_exit_field="null"
  [[ -n "${BUILD_RESULT_EXIT:-}" ]] && build_exit_field="$BUILD_RESULT_EXIT"
  local runtime_exit_field="null"
  [[ -n "${RUNTIME_EXIT:-}" ]] && runtime_exit_field="$RUNTIME_EXIT"

  cat > "$MANIFEST_JSON" <<JSON
{
  "schema_version": "$MANIFEST_SCHEMA_VERSION",
  "experiment_id": "$(json_escape "$EXPERIMENT_ID")",
  "run_id": "$(json_escape "${RUN_ID:-$EXPERIMENT_ID}")",
  "created_at_utc": "$(json_escape "${CREATED_AT:-}")",
  "completed_at_utc": $completed_field,
  "repository": {
    "name": "korczis/OpenTTD",
    "root": "$(json_escape "$ROOT_DIR")",
    "revision": "$(json_escape "${REPO_REVISION:-}")",
    "branch": "$(json_escape "${REPO_BRANCH:-}")",
    "dirty_before": ${DIRTY_BEFORE:-false},
    "dirty_after": $dirty_after_field
  },
  "task": {
    "description": $(json_str_or_na "${TASK_DESC:-}"),
    "research_question": $(json_str_or_na "${RESEARCH_QUESTION:-}"),
    "hypothesis": $(json_str_or_na "${HYPOTHESIS:-}"),
    "scope": $(json_str_or_na "${SCOPE:-}"),
    "acceptance_criteria": $(json_str_or_na "${ACCEPTANCE_CRITERIA:-}")
  },
  "orchestrator": {
    "run_id": $(json_str_or_na "${ORCH_RUN_ID:-}")
  },
  "agent_configuration": {
    "agent_run_id": $(json_str_or_na "${AGENT_RUN_ID:-}"),
    "agent_role": $(json_str_or_na "${AGENT_ROLE:-}"),
    "task_id": $(json_str_or_na "${TASK_ID:-}")
  },
  "retrieval_context": $(json_str_or_na "${RETRIEVAL_REF:-}"),
  "blackboard_context": $(json_str_or_na "${BLACKBOARD_REF:-}"),
  "evaluation_reference": $(json_str_or_na "${EVAL_REF:-}"),
  "build": {
    "profile": $(json_str_or_na "${BUILD_PROFILE:-}"),
    "build_type": $(json_str_or_na "${BUILD_TYPE:-}"),
    "build_dir": $(json_str_or_na "${BUILD_DIR:-}"),
    "research_instrumentation": $(json_str_or_na "${BUILD_RESEARCH_MODE:-}"),
    "configure_command": $(json_str_or_na "${BUILD_CONFIGURE_CMD:-}"),
    "result_status": "$(json_escape "${BUILD_RESULT_STATUS:-NOT RUN}")",
    "result_exit_code": $build_exit_field
  },
  "runtime": {
    "command": $(json_str_or_na "${RUNTIME_COMMAND:-}"),
    "flags": $(json_str_or_na "${RUNTIME_FLAGS:-}"),
    "started_at": $(json_str_or_na "${RUNTIME_STARTED:-}"),
    "ended_at": $(json_str_or_na "${RUNTIME_ENDED:-}"),
    "exit_code": $runtime_exit_field
  },
  "internal_access": {
    "apis_used": $(if [[ "${BUILD_RESEARCH_MODE:-}" == "ON" ]]; then printf '["src/console_cmds.cpp: ConResearchStatus (research_status console command)"]'; else printf '[]'; fi),
    "internals_exposed": $(if [[ "${BUILD_RESEARCH_MODE:-}" == "ON" ]]; then printf '["TimerGameTick::counter","TimerGameCalendar::date","Vehicle::GetNumItems()","Company::GetNumItems()"]'; else printf '[]'; fi),
    "mutating": false
  },
  "instrumentation": {
    "compile_time_gate": "OPTION_RESEARCH_INSTRUMENTATION / OTTD_RESEARCH_INSTRUMENTATION",
    "points": $(if [[ "${BUILD_RESEARCH_MODE:-}" == "ON" ]]; then printf '["research_status console command (read-only snapshot: tick, date, vehicle count, company count)"]'; else printf '[]'; fi),
    "active": $(json_str_or_na "${BUILD_RESEARCH_MODE:-}")
  },
  "commands": $commands_json,
  "validations": $validations_json,
  "observations": $observations_json,
  "preliminary_findings": [],
  "claims": [],
  "observer_effects": $(if [[ "${BUILD_TYPE:-}" == "Debug" ]]; then printf '["Debug build: asserts enabled, unoptimized -- timing/perf not representative of a release build.","research_status reflects a freshly-started headless game (GENERATE_NEW_SEED, tick near 0) unless a savegame was loaded."]'; else printf '[]'; fi),
  "limitations": $limitations_json,
  "status": "$(json_escape "${STATUS:-NOT RUN}")"
}
JSON
}

# Best-effort JSON syntax check: jq if present, else python3 -m json.tool,
# else a minimal brace-balance check. Never hard-fails just because an
# optional tool is missing -- reports what it could verify.
manifest_syntax_check() {
  local file="$1"
  if command -v jq >/dev/null 2>&1; then
    jq empty "$file" 2>&1 && { printf 'jq: valid JSON\n'; return 0; }
    return 1
  elif command -v python3 >/dev/null 2>&1; then
    python3 -m json.tool "$file" >/dev/null 2>&1 && { printf 'python3 -m json.tool: valid JSON\n'; return 0; }
    return 1
  else
    # crude fallback: equal counts of { and }, [ and ]
    local opens closes
    opens=$(tr -cd '{' < "$file" | wc -c)
    closes=$(tr -cd '}' < "$file" | wc -c)
    if [[ "$opens" -eq "$closes" ]]; then
      printf 'brace-balance check only (no jq/python3 available): balanced\n'
      return 0
    fi
    return 1
  fi
}

VALID_STATUSES=("PASS" "FAIL" "PARTIAL" "NOT RUN" "NOT APPLICABLE" "INCONCLUSIVE")

is_valid_status() {
  local s="$1"
  local v
  for v in "${VALID_STATUSES[@]}"; do
    [[ "$v" == "$s" ]] && return 0
  done
  return 1
}

# The "research/evaluation" gate: lints the manifest for the completeness and
# honesty rules from AGENTS.md/research/README.md, per the mission spec's
# Phase 12. Regenerates manifest.json in "final" mode first so it reflects
# everything recorded so far, without setting completed_at/status (report
# does that). Writes findings to stdout, returns 0/1.
gate_research() {
  manifest_write "final"

  local ok=1
  local reasons=()

  [[ -f "$MANIFEST_JSON" ]] || { reasons+=("manifest.json missing"); ok=0; }

  if manifest_syntax_check "$MANIFEST_JSON" >/tmp/.research_gate_syntax.$$ 2>&1; then
    :
  else
    reasons+=("manifest.json failed syntax check")
    ok=0
  fi
  rm -f /tmp/.research_gate_syntax.$$

  load_state
  [[ -n "${EXPERIMENT_ID:-}" ]] || { reasons+=("experiment_id missing"); ok=0; }
  [[ -n "${CREATED_AT:-}" ]] || { reasons+=("created_at_utc missing"); ok=0; }

  if [[ ! -s "$COMMANDS_JSONL" && "${DRY_RUN:-0}" != "1" ]]; then
    reasons+=("no commands recorded yet (run init/build/run before the research gate)")
    ok=0
  fi

  if [[ -s "$VALIDATIONS_JSONL" ]]; then
    while IFS= read -r line; do
      local st
      st=$(printf '%s' "$line" | sed -nE 's/.*"status":"([^"]*)".*/\1/p')
      if [[ -n "$st" ]] && ! is_valid_status "$st"; then
        reasons+=("validations[] has non-taxonomy status: $st")
        ok=0
      fi
    done < "$VALIDATIONS_JSONL"
  fi

  if [[ "${BUILD_RESEARCH_MODE:-}" == "ON" && ! -s "$OBSERVATIONS_JSONL" ]]; then
    reasons+=("research instrumentation is ON but no observations were recorded")
    ok=0
  fi

  if [[ "$ok" -eq 1 ]]; then
    printf 'research gate: PASS\n'
    return 0
  else
    local r
    for r in "${reasons[@]}"; do printf 'research gate: FAIL -- %s\n' "$r"; done
    return 1
  fi
}
