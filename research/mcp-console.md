# The MCP catalogue in the in-game console

RESEARCH-ONLY, UNSTABLE. Issue I0067; rule `project.mcp-surfaces-are-derived`.

Everything the embedded MCP server offers is a command in the game's own console (the tilde
key in the GUI client, standard input in a dedicated server). The console code names no tool,
resource or parameter. It asks the server's in-process dispatch for `tools/list` and
`resources/list`, and derives every subcommand, help line and argument type from the answer.
It then calls through that same dispatch, so a tool added to the server is a console command as
soon as it is in the catalogue.

| What | Derived from |
|---|---|
| The command list | `tools/list` and `resources/list`, through `PrismaticMCP::Dispatch` |
| A help line (`mcp help <tool>`) | the tool's `inputSchema`: required keys first, then optional ones in `[ ]`, enums as `a\|b` |
| Argument types | the schema property's `type` (boolean, integer, number, string, array, object, `null` in a union) and `enum` |
| Which keys are allowed | `properties`, unless the schema has none or sets `additionalProperties` |
| What a remote console may run | the tool's `annotations.readOnlyHint` |

## Commands

```text
mcp                                   help, with the size of the catalogue it read
mcp tools [filter]                    every tool, "(read)" when annotated read-only
mcp resources [filter]                every resource
mcp help <tool>                       description and usage line
mcp read <uri> [--all] [--compact]    read a resource
mcp <tool> [key=value ...] [{json}] [--all] [--compact]
mcp rpc <method> [{json params}]      any JSON-RPC method (local console only)
```

A name can be given in full, without the `openttd.` (or `openttd://`) prefix, or as a unique
suffix after a `.` or `/`. For example, `mcp view.zoom 2`, `mcp zoom level=2` and
`mcp openttd.view.zoom level=2` all name the same tool. A name that matches several entries
lists them, and a name that only matches as a substring is offered, never run. Each bare value
fills the next required key that has no value yet. A `{...}` token is merged into the
arguments as JSON. Output is pretty-printed JSON, cut at 200 lines unless `--all` is given;
`--compact` prints each value on one line.

## The short words, a leading slash, and Tab

The words a player reaches for first are the same commands, so `~` works the way a chat or a
shell does:

```text
/mcp ...  = mcp ...         a leading '/' is optional on every console command
ask <request>, do <request> = plan <request>   a request in your own words, for the planner
ai ...    = bot ...         the company AIs (status, set, start, stop, reload)
```

`ask`, `do` and `ai` are console aliases (`alias` lists them); the slash is stripped only when
the bare name is a command or an alias. An unknown command answers with what the console offers
(`mcp`, `bot`/`ai`, `plan`/`ask`/`do`, `help`). Everything a player can do in the console is
also an MCP tool (`openttd.cmd.*`, `openttd.ai.*`) and a route in the panel's OpenAPI document.

Tab completes from the live catalogue, never from a list in the code:

| Typed | Tab offers |
|---|---|
| a command, with or without `/` | every console command and alias |
| `mcp ` | `tools`, `resources`, `help`, `read`, `rpc` and every tool without `openttd.` |
| `mcp read ` | every resource URI |
| `mcp help ` | every tool |
| `mcp <tool> ` | the tool's parameters not given yet, as `key=` (the value goes right after `=`) |
| `bot ` or `ai ` | `status`, `set`, `start`, `stop`, `reload`, `help` |

## What does not change

- **Approvals.** A console call goes through the same dispatch, approvals and input arming as
  an HTTP call. `mcp set_pause pause=true` answers `pending_approval`. After
  `prismatic_mcp approve <id>`, the same call executes through `Commands::Pause` and reports its
  verdict (ADR 0008).
- **Coverage.** The coverage counters count console calls like HTTP calls.
- **Remote consoles.** An rcon or admin console (`_redirect_console_to_client` set) runs only
  resources and tools annotated `readOnlyHint`. It is refused `mcp rpc` outright.
- **No server needed.** The server does not have to be listening: the console calls the
  dispatch in process.

## Validation

- `build-gate-research/openttd_test '[prismatic_mcp]'` covers name resolution, value typing,
  `key=value`, bare values, unknown and missing keys, `anyOf`, usage lines, result lines and Tab
  completion (with and without a leading slash).
- `tools/research/mcp-console-check` uses only the console of a dedicated server, with no HTTP
  request. Every tool the console lists must answer `mcp help`, and every resource it lists must
  answer `mcp read` with JSON. Refusals and the gated pause are checked end to end, and the
  coverage resource must count every resource the console read. `/mcp`, `ask`, `/do` and `/ai`
  reach `mcp`, `plan` and `bot`, and an unknown `/nosuch` says what the console offers.

Observer effect: one catalogue build per command (two list calls), and the coverage increments
for the calls themselves.
