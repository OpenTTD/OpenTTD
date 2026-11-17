# 16 — Decision Contract & Approval Workflow

Implemented in `src/prismatic_mcp/prismatic_mcp_server.cpp`. Verified live (Vertical B), and on every
run of `tools/research/mcp-live-check`.

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
and `mutating` (`ValidateAction()` in `prismatic_mcp_server.cpp`). Checks: known action type,
argument shape/types; for `command.<Name>` also the actor (`company`), the `location` and a
side-effect-free test run of the command (`would_succeed`, `estimated_cost`). `approval_required` is
true for every action type the gate executes. An `api.Script<Class>.<Function>` action is validated
by `Api::Validate()` instead, and an `openttd.api.*` tool call without `execute: true` runs in
`GSTestMode`, changes nothing and needs no approval (`research/mcp-api.md`, "The Script API as MCP
tools"). Snapshot-staleness checks are staged.

## Approval (two-phase, digest-bound, one-time)

- **Digest**: FNV-1a of `type + "|" + canonical(args)`, reported as `argument_digest`. The approval
  itself is matched on the exact canonical arguments, not on the 64-bit digest, which could collide
  (`detail::ClassifySubmit`, unit-tested in `src/tests/test_prismatic_mcp.cpp`).
- **Two-phase**: a mutation never executes on first submit; it returns `pending_approval` until an
  operator approves in the OpenTTD console.
- **Argument binding**: an approval for `pause:true` does NOT authorize `pause:false` (different digest)
  — verified live. Defends the "change args after approval" abuse case.
- **One-time**: on execute the approval is `Consumed`; a replay of the same approved call returns a new
  `pending_approval` — verified live. Defends replay.
- **TTL**: approvals expire (`APPROVAL_TTL_TICKS`), bounded store (`MAX_APPROVALS`).

## Execution & honest verdict

Execution goes through the normal deterministic command path (`Command<...>::Post`), on the main
thread (handlers run inside `Poll()`), never a raw pool write (`ExecuteAction()` in
`prismatic_mcp_server.cpp`). `game.set_pause` posts `Commands::Pause`; `command.<Name>` posts that
command as the given actor; the `ai.*` actions post `Commands::CompanyControl`; an `api.*` action
runs the Script API function in `GSAsyncMode`.

The submit answer carries `verdict: PENDING_OBSERVATION` while the command is queued. The verdict is
taken when the command has run, from its result callback (`Conclude()`); read it with
`openttd.decision.get {decision_id}`:

- `VERIFIED_SUCCESS` — command ok AND, where the action declares an observable effect (pause state,
  AI state), the effect observed after the command ran matches intent.
- `EXECUTED_UNVERIFIED` — command ok but the effect does not match or cannot be observed. An
  `api.*` action on a network server is posted with no callback and always reports this, with
  `command_result: posted`; read its effect back with the API's own getters.
- `VERIFIED_FAILURE` — command failed.

`isError` in the MCP `tools/call` result reflects `command_result`, never mere HTTP acceptance.

## Provenance (`openttd://decisions`, `prismatic_mcp decisions`)

Bounded trace of `{decision_id, approval_id, action_type, arg_digest, command_result, verdict,
requested_tick, observed_tick}` — no bearer token, no secrets. Distinguishes command execution from
observed outcome per the mission's provenance requirement.

## Action catalogue (this build)

The fixed types are named once, in `detail::ACTION_TYPES` (`src/prismatic_mcp/prismatic_mcp_detail.h`):
`game.set_pause` (SERVER_ADMIN), `ai.set_setting`, `ai.start`, `ai.stop`, `ai.reload` (AI_CONTROL).
`ActionTypes()` adds one `command.<Name>` (GAME_MUTATION) per game command whose parameters convert
from JSON, derived from the command table, and `openttd.decision.list_action_types` lists all of
them. `api.Script<Class>.<Function>` (SCRIPT_API) is the action of an `openttd.api.*` call with
`execute: true`. Company renames, vehicle start/stop and send-to-depot are no separate adapters:
they are the commands `command.RenameCompany`, `command.StartStopVehicle` and
`command.SendVehicleToDepot`, offered while `openttd://commands` marks them `exposable`.
