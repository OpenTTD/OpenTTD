# 16 — Decision Contract & Approval Workflow

Implemented in `src/prismatic_mcp/prismatic_mcp_server.cpp`. Verified live (Vertical B).

## Flow

```
client: tools/call openttd.decision.validate {action:{type,arguments}}   -> non-mutating validation
client: tools/call openttd.decision.submit   {action:{type,arguments}}   -> {status: pending_approval, approval_id, argument_digest}
operator (OpenTTD console): prismatic_mcp approve <approval_id>            -> approval bound to that exact digest
client: tools/call openttd.decision.submit   {same action}                -> {status: executed, command_result, verdict, observed}
```

## DecisionEnvelope (accepted shape)

`openttd.decision.submit` accepts `{"action": {"type": "<action>", "arguments": {...}}}` (plus optional
`decision_id`, `correlation_id`, `company_id`, `snapshot_id`, `confidence`, `rationale`, `objective`,
`expected_effects` — retained for forward-compat, not interpreted as authority). Unknown fields never
grant privilege.

## Validation (`openttd.decision.validate`, never mutates)

Returns `valid`, `errors`, `current_snapshot_id` (`tick-<n>`), `required_scope`, `approval_required`
(always true for mutations in this build), `mutating`. Checks: known action type, argument shape/types.
Company/snapshot-staleness checks are the extension point for company-scoped actions (staged).

## Approval (two-phase, digest-bound, one-time)

- **Digest**: FNV-1a of `type + "|" + canonical(args)`. The approval is bound to this digest.
- **Two-phase**: a mutation never executes on first submit; it returns `pending_approval` until an
  operator approves in the OpenTTD console.
- **Argument binding**: an approval for `pause:true` does NOT authorize `pause:false` (different digest)
  — verified live. Defends the "change args after approval" abuse case.
- **One-time**: on execute the approval is `Consumed`; a replay of the same approved call returns a new
  `pending_approval` — verified live. Defends replay.
- **TTL**: approvals expire (`APPROVAL_TTL_TICKS`), bounded store (`MAX_APPROVALS`).

## Execution & honest verdict

Execution is `Command<Commands::Pause>::Post(PauseMode::Normal, pause)` — the normal deterministic
command path, on the main thread (handlers run inside `Poll()`), never a raw pool write. The tool then
reads `_pause_mode` and reports:

- `VERIFIED_SUCCESS` — command ok AND observed effect matches intent.
- `EXECUTED_UNVERIFIED` — command ok but effect not yet observable. **This is the normal dedicated/MP
  case**: `Post` queues the command for a later frame, so the pause isn't visible at the instant of the
  call. The real delta appears in the next `resources/read openttd://game/state` (verified: `paused`
  went `false → true`). The tool deliberately does not claim success it cannot yet observe.
- `VERIFIED_FAILURE` — command failed.

`isError` in the MCP `tools/call` result reflects `command_result`, never mere HTTP acceptance.

## Provenance (`openttd://decisions`, `prismatic_mcp decisions`)

Bounded trace of `{decision_id, approval_id, action_type, arg_digest, command_result, verdict,
requested_tick, observed_tick}` — no bearer token, no secrets. Distinguishes command execution from
observed outcome per the mission's provenance requirement.

## Action catalogue (this build)

`game.set_pause` (SERVER_ADMIN) only — the smallest cleanly-observable typed mutation. `company.rename`,
`vehicle.start_stop`, `vehicle.send_to_depot` are the mapped next adapters (each needs `_current_company`
setup + company-scope binding); staged with tests per `08-tool-catalog.md`.
