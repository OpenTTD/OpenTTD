# 01 — OpenTTD System Map (Phase 1)

Actual call paths, ownership and thread context, recovered by reading the source
(not inferred from filenames). Every claim is anchored `file:line`. This is the
foundation the architecture is built on.

## §5.1 Build / feature gating

- `cmake/Options.cmake` has three choke-points every option touches: `option()` in
  `set_options()` (`:57-65`), a status line in `show_options()` (`:97-115`), and
  `add_definitions(-D...)` in `add_definitions_based_on_options()` (`:121-140`).
- `OPTION_RESEARCH_INSTRUMENTATION` (`:65`) → `-DOTTD_RESEARCH_INSTRUMENTATION` (`:136-137`).
  Its only current consumer is `src/console_cmds.cpp` (the `research_status` command at
  `:49-53`, `:1753-1780`, `:3079-3081`) — the exact template this feature follows.
- Sources are added via `add_files(... CONDITION <expr>)` (`cmake/SourceList.cmake:1-36`);
  a new subsystem gets `add_subdirectory(<dir>)` in `src/CMakeLists.txt:1-30`. Tests use
  `add_test_files(...)` (`SourceList.cmake:46-48`) auto-discovered by
  `catch_discover_tests(openttd_test)` (`CMakeLists.txt:294-295`).
- `OPTION_DEDICATED` → `-DDEDICATED` (`Options.cmake:122-123`); GUI omission is mostly by
  *not compiling* `*_gui.cpp`, plus `if(NOT OPTION_DEDICATED)` link guards. `toolbar_gui.cpp`
  is compiled unconditionally (`src/CMakeLists.txt`), so toolbar work needs no CMake change.
- **Applied here:** `OPTION_PRISMATIC_MCP_SERVER` added at all three choke-points, with a
  `message(FATAL_ERROR ...)` if it is ON while research instrumentation is OFF, and a new
  `src/prismatic_mcp/` subdirectory gated `CONDITION OPTION_PRISMATIC_MCP_SERVER`.

## §5.2 Main loop & lifecycle hooks

- `openttd_main` (`src/openttd.cpp:496`) inits subsystems then blocks in
  `VideoDriver::MainLoop()` (`:807`); on return `PostMainLoop()` → `ShutdownGame()` (`:290-315`).
- Per-frame pump: `GameLoop()` (`:1332-1392`), invoked by `VideoDriver::GameLoop()`
  (`video_driver.cpp:30-43`) under `game_state_mutex`. It hosts `NetworkBackgroundLoop()`
  (`:1370`/`:1382`) — the exact idiom for "poll something every frame on the main thread,
  in all game modes, GUI and dedicated." **The MCP `Poll()` hook is placed here.**
- Deterministic simulation is `StateGameLoop()` (`:1207-1282`) — do NOT poll there (skipped on pause).
- Mode transitions funnel through `SwitchToMode()` (`:1015-1198`): new game (`MakeNewGameDone` `:856`,
  `OnStartGame` `:844`), load (`LoadGame` case `:1096`), return to menu (`Menu` case → `LoadIntroGame` `:1157`).
  `OnGameModeChanged()` is the MCP invalidation hook for these.
- **Applied here:** `PrismaticMCP::Init()` after `NetworkStartUp()` (`:764`), `Poll()` at the
  `GameLoop` background-loop site, `Shutdown()` in `ShutdownGame()` beside `NetworkShutDown()` (`:294`).

## §5.3 Networking — verdict: reusable listener yes, HTTP server no

- **No HTTP server exists.** `network/core/http.h` is an outbound libcurl/WinHTTP *client* only.
  This feature implements the first inbound HTTP/1.1 server in the tree.
- Reusable primitives: `TCPListenHandler<...>` (`tcp_listen.h:28`) gives `Listen/AcceptClient/select`;
  the admin server (`network_admin.cpp`, `network_admin.h:26`) is the closest analog — "a second TCP
  listener on its own port, polled from the game tick." Low-level: `SOCKET`/`SetNonBlocking`/`SetNoDelay`
  (`os_abstraction.h:63,131-132`), `NetworkError::GetLast().WouldBlock()` (`:17-31`), `closesocket`.
- **Loopback bind:** construct `NetworkAddress("127.0.0.1", port).Listen(SOCK_STREAM, &sockets)` and the
  same for `"::1"`, bypassing `GetBindAddresses()` which otherwise defaults to *all interfaces*
  (`network.cpp:745`) — that default must never be used for this endpoint.
- **All network I/O is on the main game thread** (`network.cpp:1086-1136`); the only network worker
  threads are outbound DNS (`tcp.h:88`) and the libcurl client (`http_curl.cpp:82`). Neither touches pools.
- `Packet` (`packet.h`) is OpenTTD's binary length-prefixed framing — **unusable for HTTP text**, so the
  MCP connection does raw `recv`/`send` byte buffering with its own HTTP/1.1 parser.
- **Token CSPRNG:** `RandomBytesWithFallback(std::span<uint8_t>)` (`random_func.cpp:94`, `arc4random_buf`
  on macOS) — the same primitive the network crypto layer uses. Constant-time compare available via
  Monocypher `crypto_verify32` (this build uses a hex-string constant-time compare instead — see server).

## §5.4 Command architecture (the mutation boundary)

- Every synchronized mutation is a `Commands` enum value (`command_type.h:200`) dispatched via
  `Command<Cmd>::Post(...)` (`command_func.h:201`) — the network-safe, player-context path.
  `Command<Cmd>::Do(...)` is engine-internal only (`:159-162`). Registration via
  `DEF_CMD_TRAIT(cmd, proc, flags, type)` in each `*_cmd.h`.
- Company context is the global `_current_company`, saved/restored with `Backup<CompanyID>`
  (`command_func.h:416`), exactly as scripts (`script_object.cpp:213`) and the network execute loop
  (`network_command.cpp:275`) do.
- **Critical async caveat:** in a network game `Post` only *queues*; it returns an empty result and the
  real outcome arrives 1+ ticks later via a **network-registered** `CommandCallback`
  (`command_func.h:232,340-361`, `network_command.cpp:58-87`). MCP must observe actual post-command
  state (or the callback), never treat `Post`'s return as success. Single-player executes inline.
- Callbacks passed to `Post` MUST be in `_callback_tuple` — arbitrary lambdas are asserted against.

### Operation classification (mission §5.4)

| Operation | Mechanism | Class |
|---|---|---|
| pause / unpause | `Commands::Pause` `CmdPause` `{Server,NoEst}` (`misc_cmd.cpp:170`) | **SERVER_ADMIN** (server-only synchronized command) |
| vehicle start/stop | `Commands::StartStopVehicle` (Location, `vehicle_cmd.cpp:582`) | **SYNCHRONIZED_GAME_COMMAND** (company-scoped, async result) |
| send to depot | `Commands::SendVehicleToDepot` (`vehicle_cmd.cpp:1063`) | **SYNCHRONIZED_GAME_COMMAND** |
| rename company | `Commands::RenameCompany` (`company_cmd.cpp:1210`) | **SYNCHRONIZED_GAME_COMMAND** |
| AI start/reload/stop | console procs → `Command<Commands::CompanyControl>::Post` (`console_cmds.cpp:1503-1658`) | **SERVER_ADMIN** |
| save | direct `SaveOrLoad(...)` / `_switch_mode = SwitchMode::SaveGame` — **not a command** | **CLIENT_LOCAL / main-thread** |
| iterate pools, read money | not a `Commands` value | **PURE_QUERY** |

## §5.5 Object pools (resource sources)

`Pool<Titem,Tindex,...>` (`core/pool_type.hpp:140`): `T::GetIfValid(id)` (`:406`, the safe accessor to
expose), `T::Iterate()` (`:444`, yields only valid items in ascending-index order → deterministic),
`T::GetNumItems()` (`:425`). Pools are unsynchronized main-thread simulation state — **MCP touches them
only from `Poll()` on the main thread.**

| Entity | Header | Pool / ID / max | Notes |
|---|---|---|---|
| Company | `company_base.h:70` | `CompanyID` (`company_type.h:16`), max 15 | `money` (`:88`), `name` (`:77`), `is_ai` (`:118`); server sees all — "privacy" is UI-only, no per-company password in this tree |
| Vehicle | `vehicle_base.h:162` | `VehicleID`, ~1.04M | `owner`, `engine_type`, `cargo_type`, profit — high cardinality → must paginate |
| Town | `town.h:37` | `TownID`, 64000 | `xy`, cached name, `cache.population` |
| Industry | `industry.h:25` | `IndustryID`, 64000 | production, town link |
| Station | `base_station_base.h:19` | `StationID`, 64000 | pool item base is `BaseStation` |
| NetworkClientInfo | `network_base.h:20` | `ClientPoolID` | **name/IP are privacy-sensitive → redact by default** |

Globals: `_game_mode` (`openttd.h:64`), `_pause_mode` (`:86`), `_switch_mode` (`:65`), `_current_company`.
Money = `OverflowSafeInt64` (`economy_type.h:17`); date via `TimerGameCalendar::ConvertDateToYMD` → `YearMonthDay`
(`timer_game_common.h:54`); map dims `Map::SizeX/SizeY()` (`map_func.h:262/271`).

## §5.6 Native GUI (toolbar + window) — index-sync hazards

The top toolbar dispatches by **widget-enum ordinal** through parallel arrays that must stay
index-synchronized when a button is inserted (`toolbar_gui.cpp`): `ToolbarNormalWidgets`
(`toolbar_widget.h:14-48`), `_toolbar_button_procs[]` (`:1970-2002`), `_menu_clicked_procs[]`
(`:1341-1372`), `_toolbar_button_sprites[]` (`:2188-2220`), the **offset-computed tooltip block**
`STR_TOOLBAR_TOOLTIP_PAUSE_GAME + widget` (`:2254` ↔ `english.txt:386-417`), and the responsive
`arrangeNN`/`arrange_all` layout arrays (`:1533-1823`). Safest insertion is at the tail of the real
buttons (before `WID_TN_SWITCH_BAR`), with a matching parallel insertion in every array.

Window template: `framerate_gui.cpp` (`FramerateWindow : Window`, `:458`; `WindowDesc` `:741`;
`ShowFramerateWindow` `:1047`). Dynamic text without touching lang files: `STR_JUST_RAW_STRING`
(`english.txt:6031`) + a runtime `std::string` (usage `console_gui.cpp:203`). New `WindowClass` member
in `window_type.h:61` is low-risk (opaque key).

**Status for this vertical:** the toolbar button + Control Center window are DESIGNED here but the
first implementation ships the console command surface (`prismatic_mcp`) instead; the native window is
staged (see 05-implementation-plan Phase D). Rationale: the console command exercises the same
control API with far less index-sync risk, giving a verifiable vertical first.

## §5.7 Console

`using IConsoleCmdProc = bool(std::span<std::string_view>)` (`console_internal.h:29`);
`IConsole::CmdRegister(name, proc)` (`:83`), registered in `IConsoleStdLibRegister()`
(`console_cmds.cpp:3000+`). Research gating is the three-site `#ifdef` pattern
(`:49-53`, `:1753-1780`, `:3079-3081`). Colors in `console_type.h:23-30` (`CC_HELP`, `CC_INFO`, `CC_ERROR`,
`CC_WARNING`). **Applied here:** `prismatic_mcp <status|start|stop|endpoint|token|rotate>` under
`#ifdef OTTD_PRISMATIC_MCP_SERVER`.

## §5.10 JSON

nlohmann/json **3.11.3** vendored at `src/3rdparty/nlohmann/json.hpp`, `JSON_ASSERT` pre-wired in
`stdafx.h:286`; used already by `crashlog.cpp`, `genworld.cpp`, `script/api/script_admin.cpp`. Include
`"3rdparty/nlohmann/json.hpp"` and use `::array()`/`::parse`/`operator[]`. **No new dependency.**
