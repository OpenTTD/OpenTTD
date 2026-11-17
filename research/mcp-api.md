# The public C++ API through MCP and OpenAPI

RESEARCH-ONLY. Issue I0036, rule `project.public-api-is-exposed`.

`tools/research/mcp-rhai/target/release/mcp-api-coverage --repo . [--panel URL] [--json] [--report]`
derives OpenTTD's public API from the sources and the build and checks what a running game
exposes through MCP and OpenAPI: the `Commands` enum, the registered console commands, and the
Script API's static functions.

Rule `project.public-api-is-exposed` requires 100% in every dimension. On 2026-11-15 it reported
commands 0/146, console 83/86 and Script API 0/660. On 2026-11-17, against a panel on this build:
commands 146/146, Script API 630/630, console 87/90 (`resettile` exists only in `_DEBUG` builds;
`reload_newgrfs` and `newgrf_profile` are hidden by their hook until `newgrf_developer_tools` is on).

The Script API is counted as the build publishes it to scripts: every `DefSQStaticMethod` and
`DefSQAdvancedStaticMethod` of the AI and Game Script registrations the build generates from
`src/script/api/script_*.hpp` (`build*/generated/script/api/{ai,game}/*.sq.hpp`), plus the
controllers' `*_controller.sq.hpp`. The earlier count, every `static` line of the headers (660),
took in what scripts never see: `ScriptObject`'s protected statics, mode callbacks and internal
helpers.

## The Script API as MCP tools

`openttd.api.Script<Class>.<Function>` is one tool per published static function
(`src/prismatic_mcp/prismatic_mcp_api.cpp`). Nothing names a function. The server starts two
private script engines, never run as scripts: one with the Game Script API exactly as a game
script gets it (GameInstance's own `SQGS_RegisterAll`), one with the AI API for the functions
only AIs get. It reads the catalogue back from them: each class's native static functions, a
function under the class that declares it, its parameter types from the engine's own checks
(`.i` becomes `[integer]`). A call goes through the same binding a script's call goes through,
so argument conversion, validity and error codes are the game's.

- `{args: [...], company?, execute?}`. Without `execute` the call runs in test mode
  (`GSTestMode`): a reading function answers as in a script, a function that issues a command
  answers whether it would succeed, its cost and the script's last error, and nothing changes.
  No approval is needed.
- `execute: true` goes through the approval gate as action `api.Script<Class>.<Function>`, and
  once approved runs in asynchronous mode (`GSAsyncMode`), so the command is posted without
  suspending a script that is not running. In single player it has run when the call returns and
  the decision's verdict comes from its result; on a network server it is posted to the command
  queue with no callback and runs on a later tick, so the decision says `posted`,
  `EXECUTED_UNVERIFIED`, and the effect is read back with the API's own getters.
- `company` acts as that company (`GSCompanyMode`); the default is the deity. The engines have
  storage of their own (last error, cost, modes), so a call never disturbs a running AI or game
  script, and the game's current company is restored after every call.
- A GSList comes back as `{list: [{item, value}], count}`; a JSON object or array becomes a
  table or array (`GSAdmin.Send`). A parameter that is a script object (GSText, an event) is
  not convertible from JSON yet; its schema says so.

Seen live on seed 31337 (2026-11-17): 630 tools; `ScriptTown.GetPopulation [0]` answers 1443,
`["x"]` is refused by the game's own check ("parameter 1 has an invalid type 'string'");
`ScriptTown.SetName [0, "Prismatic City"]` in test mode answers `true` and the town keeps its
name; with `execute` it waits for approval, and once approved it is posted (`EXECUTED_UNVERIFIED` on
the dedicated server) and `ScriptTown.GetName [0]` reads back "Prismatic City".
