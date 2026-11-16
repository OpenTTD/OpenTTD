# The MCP hub: every instance, every operation, every event over one WebSocket

RESEARCH-ONLY, UNSTABLE. Issue I0058; rule `project.mcp-surfaces-are-derived`.

A cluster of OpenTTD instances (`tools/research/mcp-e2e --instances N`, the GUI client, the human
player's stack) has one control panel per stack and one MCP server per process. The hub puts them
behind a single WebSocket JSON-RPC surface, so a page, a Rhai script, a test or another AI reaches
any of them the same way. Nothing in it names an operation:

| What | Derived from |
|---|---|
| Instances | the instance registry (`$PRISMATIC_MCP_REGISTRY`, records `mcp-instance/v1`, dead owners dropped) and `--panel URL` |
| Operations of a panel | its `/openapi.json`: every path and method, `operationId`, body schema, query parameters, `x-mcp-annotations.readOnlyHint`, streaming routes |
| Operations of an MCP server | its `tools/list` (one `tool:<name>` each) and `resources/list` (one `resource:<uri>` each), read with the token file the record names |
| Events | each panel's bus (`/api/events?since=`), fanned in and tagged with the instance |

The set refreshes every two seconds; a new or vanished instance is pushed to subscribers as a
`hub.instances` notification.

## Run it

```bash
cargo build --offline --release --manifest-path tools/research/mcp-rhai/Cargo.toml --bin mcp-hub
tools/research/mcp-rhai/target/release/mcp-hub --port 8790        # then open http://127.0.0.1:8790/
tools/research/mcp-rhai/target/release/mcp-hub --panel http://127.0.0.1:8800   # add a panel the registry lacks
```

## The surface

`ws://127.0.0.1:<port>/ws` speaks JSON-RPC 2.0; `POST /hub/rpc` takes the same requests one at a time
(everything except streaming), and `GET /hub/instances` and `GET /hub/catalogue` serve the lists.

| Method | Params | Answer |
|---|---|---|
| `rpc.discover` | | the methods and their parameters |
| `hub.instances` | | every live instance: name, kind, access (`panel` or `mcp`), base, operation count, last error |
| `hub.catalogue` | `instance?` | every operation: instance, id, method, path, summary, tags, `read_only`, `schema`, `query`, `stream` |
| `hub.call` | `instance`, `id`, `body?`, `query?` | the operation's answer; a panel's HTTP error comes back as `{http_error, body}` |
| `hub.subscribe` | `instances?`, `kinds?` | then `hub.event` notifications `{instance, event}`; a kind matches by prefix (`game` takes `game.month`) |
| `hub.unsubscribe` | | |

`id` is a panel's `operationId`, `tool:<name>` or `resource:<uri>`. A streaming route
(`text/event-stream`) is not called: subscribe instead.

## The page and its components

`tools/research/mcp-panel/static/hub.html` is built only from `static/components.js`, which any
page of the panel can import:

- `HubClient`: the WebSocket client (requests matched to answers, notifications as events,
  reconnects and re-subscribes with the same filter);
- `catalogueView`: every operation of every instance, grouped by instance and tag and filtered by
  text; clicking one opens `operationForm`, a form derived from the operation's schema and query
  (enums as selects, booleans as checkboxes, objects as JSON), which calls it;
- `instancesView`: the instances as a table; clicking one narrows everything to it;
- `eventsView`: the live stream, each event's instance and kind clickable to follow only that;
- `jsonView`: results, with every `openttd://` URI a link that reads that resource.

The state (instance, open operation, filter, followed kinds) is in the URL hash, so a view is a link.
The page uses the panel's compiled Flowbite CSS and only classes it already contains.

## Proof

| Check | What it proves |
|---|---|
| `cargo test --offline --manifest-path tools/research/mcp-rhai/Cargo.toml --bin mcp-hub` | operations derived from OpenAPI and MCP, registry records and dead owners, filters, verdicts; and end to end against a fake panel: list, call over WebSocket and HTTP, a stream refused as a call, only the subscribed kind streamed |
| `node --test 'tools/research/mcp-panel/test/*.test.mjs'` | schema to fields, coercion, call parameters, URL state round trip, grouping, and the client's request matching, notifications and re-subscription |
| `tools/research/mcp-hub-check` | against whatever runs: every instance discovered, every read-only operation that needs no input called through the WebSocket, one event streamed; a 503 ("no GUI client attached") is counted as skipped |

## Limits

- Loopback only and no authentication of its own: it holds MCP tokens read from the registry's token
  files, so whoever reaches the port reaches every instance. Keep it on 127.0.0.1.
- It calls what a person asks for, including mutating operations; the approval flow of each panel
  (ADR 0008) and the arming switch of interface input (ADR 0014) still apply behind it.
- Events come from panels only; a bare MCP server has no event bus to fan in.
- The page has not been rendered in a browser by any check yet: its client was driven against the live
  hub from node, and its pure functions are unit-tested; the DOM components are not.
