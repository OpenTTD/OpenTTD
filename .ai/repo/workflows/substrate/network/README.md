---
schema: context/v1
id: fork.substrate.network
kind: context
title: "Network: lockstep multiplayer transport, command distribution, desync checks, admin port and coordinator"
description: "Read before changing anything under src/network/, or when a feature must work in a multiplayer game, over rcon or the admin port."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/network/**'
---

# Network

## Purpose and ownership

Owns the client/server transport of the deterministic lockstep game: which frame a client may run
to, which commands run at which frame, the join handshake and map transfer, the RNG-based desync
check, the admin port, rcon, and coordinator/STUN/TURN connection setup. It does not own the
command procs (`src/command.cpp`, `*_cmd.cpp`); it only queues, ships and replays them.

## Entry points

- `src/openttd.cpp:GameLoop` - calls `src/network/network.cpp:NetworkBackgroundLoop` every tick, and `src/network/network.cpp:NetworkGameLoop` while `_networking` (else `StateGameLoop` directly).
- `src/network/network.cpp:NetworkGameLoop` - server: distribute, advance `_frame_counter`, execute, `StateGameLoop`, record `_sync_seed_1`, then `src/network/network_server.cpp:NetworkServer_Tick`. Client: run frames up to `_frame_counter_max` via `src/network/network_client.cpp:ClientNetworkGameSocketHandler::GameLoop`.
- `src/network/network_command.cpp:NetworkSendCommand` - called from `src/command_func.h:Execute` when `send_net` is set; the server queues into `_local_wait_queue`, a client sends to the server with frame 0.
- `src/network/network_command.cpp:NetworkDistributeCommands` / `DistributeQueue` / `DistributeCommandPacket` - the server stamps each queued command with one frame and copies it to every client at status Map or later and to its own `_local_execution_queue`.
- `src/network/network_command.cpp:NetworkExecuteLocalCommandQueue` - runs commands whose `frame` equals `_frame_counter` through `src/command_func.h:PostFromNet`.
- `src/network/network_server.cpp:ServerNetworkGameSocketHandler::ReceiveClientCommand` - the trust boundary for gameplay commands (see Invariants).
- `src/network/network_server.cpp:ServerNetworkGameSocketHandler::ReceiveClientRemoteConsoleCommand` and `src/network/network_admin.cpp:ServerNetworkAdminSocketHandler::ReceiveAdminRemoteConsoleCommand` - rcon; both call `IConsoleCmdExec` with output redirected via `_redirect_console_to_client` / `_redirect_console_to_admin`, consumed in `src/console.cpp:IConsolePrint`.
- `src/network/network_admin.cpp:ServerNetworkAdminSocketHandler::ReceiveAdminGameScript` - admin JSON becomes a `ScriptEventAdminPort` for the Game Script. The admin protocol has no packet that issues a command.
- `src/network/core/tcp_game.cpp:NetworkGameSocketHandler::HandlePacket` - dispatches `src/network/core/tcp_game.h:PacketGameType` to the `Receive*` virtuals; the admin port uses `src/network/core/tcp_admin.h:PacketAdminType`.
- `src/network/network_crypto.h:NetworkAuthenticationServerHandler` - X25519 key exchange, PAKE or authorized keys (`NetworkAuthenticationMethod`), then one `NetworkEncryptionHandler` per direction, applied in `src/network/core/packet.cpp:Packet::PrepareToSend` / `Packet::PrepareToRead`; monocypher calls live in `src/network/network_crypto.cpp`.
- `src/network/network_coordinator.h:ClientNetworkCoordinatorSocketHandler`, `src/network/network_stun.h:ClientNetworkStunSocketHandler`, `src/network/network_turn.h:ClientNetworkTurnSocketHandler` - invite codes and NAT traversal (`src/network/core/tcp_coordinator.h:ConnectionType`).
- **[fork]** No fork hunk lives under the `src/network/` directory. The fork's MCP server reuses only socket primitives (`src/network/core/os_abstraction.h`, `src/network/core/address.h`) in `src/prismatic_mcp/prismatic_mcp_server.cpp`, and is polled in `src/openttd.cpp:GameLoop` just before `NetworkGameLoop`.

## Lifecycle

- Init: `src/openttd.cpp:openttd_main` calls `src/network/network.cpp:NetworkStartUp` (sets `_network_available`). `NetworkServerStart` zeroes `_frame_counter`, `_frame_counter_max`, `_last_sync_frame` and execs the `pre_server.scr` / `on_server.scr` console scripts; `NetworkOnGameStart` runs after a game loads.
- Per tick, server: `NetworkReceive` -> when `_frame_counter >= _frame_counter_max`: `CheckPauseOnJoin`, `CheckMinActiveClients`, `NetworkDistributeCommands` -> `_frame_counter++` -> `NetworkExecuteLocalCommandQueue` -> `StateGameLoop` -> `_sync_seed_1 = _random.state[0]` -> `NetworkServer_Tick` (lag and timeout kicks, flush `outgoing_queue`, `SendFrame`, `SendSync` every `sync_freq` frames) -> `NetworkSend`.
- Per tick, client: `ReceiveServerFrame` moves `_frame_counter_server` / `_frame_counter_max`; `ReceiveServerSync` sets `_sync_frame` / `_sync_seed_1`; `ClientNetworkGameSocketHandler::GameLoop` compares at `_sync_frame` and returns `NetworkRecvStatus::Desync` on mismatch.
- Join: `src/network/network_server.h:ClientStatus` runs Inactive, AuthGame, Identify, NewGRFsCheck, Authorized, MapWait/Map, DoneMap, PreActive, Active. `src/network/network_server.cpp:ServerNetworkGameSocketHandler::SendMap` saves the live game with `SaveWithFilter` and calls `NetworkSyncCommandQueue` so commands already scheduled reach the joiner.
- Teardown: `NetworkDisconnect`, `NetworkClose`, `NetworkShutDown`; sockets die in `NetworkGameSocketHandler::ProcessDeferredDeletions`.
- Save/load: nothing of this layer is saved. The `network.*` settings in `src/table/settings/network_settings.ini` carry `NotInSave`; the join map is an ordinary savegame stream.

## Invariants

- **[verified]** A command runs at the same frame everywhere: `src/network/network_command.cpp:DistributeCommandPacket` sets `cp.frame = _frame_counter_max + 1`, and `NetworkExecuteLocalCommandQueue` calls `FatalError` on a packet older than `_frame_counter`.
- **[verified]** The server's own commands are delayed like a client's: `NetworkSendCommand` queues them in `_local_wait_queue` and never executes them directly.
- **[verified]** In a network game a local `Post` runs only the test phase, then sends; execution happens when the packet comes back (`src/command.cpp:CommandHelperBase::InternalExecuteValidateTestAndPrepExec` sets `send_net = _networking && !_generating_world && !network_command`).
- **[verified]** `ReceiveClientCommand` drops or kicks a client for: status before DoneMap; a queue above `max_commands_in_queue`; an unknown or `CommandFlag::Offline` command or a callback index without an unpack proc (`src/network/network_command.cpp:NetworkGameSocketHandler::ReceiveCommand`); a `CommandFlag::Server` command; a non-`Spectator` command without a valid company; a company other than `client_playas`. `CommandFlag::ClientID` arguments are overwritten with the sender's id (`NetworkReplaceCommandClientId`). Flags: `src/command_type.h:CommandFlag`.
- **[verified]** Strings inside command data are re-validated on receipt (`src/network/network_command.cpp:SanitizeCmdStrings`); control codes survive only on clients and only for `CommandFlag::StrCtrl`.
- **[verified]** Reading past the end of a packet marks the socket closed (`src/network/core/packet.cpp:Packet::CanReadFromPacket`); handlers re-check `HasClientQuit()` after parsing.
- **[verified]** The desync check is the game RNG only: `_random.state[0]`, plus `[1]` only with `NETWORK_SEND_DOUBLE_SEED`, which `src/network/network_internal.h` defines under `RANDOM_DEBUG`. State that never feeds `_random` can drift unseen.
- **[verified]** Game-client rcon is accepted for a key in `rcon_authorized_keys` or the non-empty `rcon_password` (`_rcon_authorized_key_handler` in `src/network/network_server.cpp`); admin-port rcon needs only an authenticated admin. The console hooks then judge with the server's context, so `ConHookServerOnly` commands pass.
- **[verified]** Peers are compatible when the git hash of the revision string matches (`src/network/core/network_game_info.cpp:IsNetworkCompatibleVersion`); compile options do not enter it, except the `dbg_seed` / `dbg_sync` prefixes from `GetNetworkRevisionString`.
- **[supported]** Desync detection is a late failure signal and does not locate the cause (`docs/desync.md`, section 1.2).

## Extension points

- New game packet: a value in `PacketGameType` (`src/network/core/tcp_game.h`; its header comment freezes the order of the first ten), a `Receive*` virtual on `NetworkGameSocketHandler`, a `case` in `src/network/core/tcp_game.cpp:NetworkGameSocketHandler::HandlePacket`, overrides in `src/network/network_server.cpp` / `src/network/network_client.cpp`. Admin: `PacketAdminType` and `src/network/network_admin.cpp`.
- New command callback used by a GUI `Post`: append it to `_callback_tuple` in `src/network/network_command.cpp`; `src/command_func.h:Post` asserts `IsNetworkRegisteredCallback`.
- New network setting: `src/table/settings/network_settings.ini`, field in `src/settings_type.h`.
- New authentication method: `src/network/network_crypto.h:NetworkAuthenticationMethod` and its handlers in `src/network/network_crypto.cpp`.

## Traps and forbidden operations

- Never mutate game state from a packet handler; queue a `CommandPacket` instead (rule `project.game-state-mutates-only-through-commands`).
- Network-side randomness uses `InteractiveRandom` (the ack token in `ServerNetworkGameSocketHandler::SendFrame`); a `Random()` call advances the synced state the desync check compares. [inferred from `src/core/random_func.hpp` and the sync code above]
- Reordering `_callback_tuple` changes the wire index of every later callback; `NetworkGameSocketHandler::SendCommand` sends the index as one byte.
- `DEBUG_DUMP_COMMANDS` (`src/network/network_func.h`) makes the server replay `commands.log` and lifts the per-frame command limit; never commit it enabled.
- While paused, `DistributeQueue` holds back commands not allowed while paused; they wait in the queue rather than fail.
- rcon is a console channel, not a gameplay channel (knowledge `bridge-control-is-coarse-ai-restart`).

## Tests and validation

- `src/tests/test_network_crypto.cpp` (Catch2 cases `Authentication_*`, `Encryption handling`): `tools/gate.sh smoke -R 'Authentication|Encryption'`, then `tools/gate.sh change`.
- No unit or regression test covers command distribution, the sync check, rcon or the admin protocol. A change there needs a real server plus client run, and is reported as NOT RUN if none happened.
- Debug: `-d net=N` (trace lines in each `Send*` / `Receive*`). `-d desync=1` writes every executed command (`src/command.cpp:CommandHelperBase::LogCommandExecution`) and the daily `sync:` line to `commands-out.log` in the autosave folder (`src/debug.cpp:DebugPrint`) and periodic `dmp_cmds_*.sav`; `-d desync=2` adds the cache checks. Replay: `docs/desync.md` section 3, `docs/debugging_desyncs.md`.

## Decisions, rules and knowledge

- Upstream architecture: [ADR 0016](../../../adrs/0016-multiplayer-is-deterministic-lockstep-every-peer-runs-the-sa.md) (deterministic lockstep), [ADR 0019](../../../adrs/0019-settings-are-declared-once-in-ini-tables-that-settingsgen-co.md) (settings tables).
- Rules: `project.game-state-mutates-only-through-commands`, `project.research-code-is-gated-and-labelled`, `project.never-report-unrun-validation-as-pass`, `project.no-upstream-interaction`.
- ADR 0007 (the bridge drives a Company AI over admin-port rcon), ADR 0008 (embedded loopback MCP server; mutations pass the command system).
- Knowledge: `bridge-control-is-coarse-ai-restart`, `dedicated-server-stdin-startup-race`, `fixed-loopback-ports-collide`.
- Upstream docs: `docs/desync.md`, `docs/debugging_desyncs.md`, `docs/admin_network.md`, `docs/game_coordinator.md`, `docs/multiplayer.md`.

## Typical tasks

- To add a field to a game packet: start at the `Send*` in `src/network/network_server.cpp:ServerNetworkGameSocketHandler::SendFrame` (or its sibling) and the matching `Receive*` in `src/network/network_client.cpp`, append rather than reorder, guard the read with `CanReadFromPacket` when older peers matter; validate with `tools/gate.sh change` and a two-process join.
- To let an external tool steer the game: start at the admin port (`src/network/network_admin.cpp`) or the MCP server (ADR 0008); the effect itself still goes through `Command<...>::Post`, never through a packet handler; validate with `tools/gate.sh research` when the code is research-gated.
- To investigate a desync: run the server with `-d desync=1`, keep `commands-out.log` and the `dmp_cmds_*.sav` files, then replay per `docs/desync.md` section 3 with `DEBUG_DUMP_COMMANDS` in a local build only.
- To tighten what a client may send: start at `src/network/network_server.cpp:ServerNetworkGameSocketHandler::ReceiveClientCommand`, then the command's flags in its `*_cmd.h`; validate with `tools/gate.sh change`.
