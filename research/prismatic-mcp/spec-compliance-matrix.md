# MCP Spec Compliance Matrix (stable 2025-11-25)

Pinned version: **2025-11-25** (`PrismaticMCP::PROTOCOL_VERSION`). Draft/RC behavior is not
implemented. Status: `DONE` / `PARTIAL` / `STAGED`.

| Requirement | Spec section | Implementation | Status |
|---|---|---|---|
| Protocol version pinned & negotiated | basic/lifecycle | `initialize` returns `protocolVersion: "2025-11-25"` | DONE |
| `initialize` request/response | basic/lifecycle | `DispatchOne` → serverInfo/capabilities/instructions | DONE |
| `notifications/initialized` | basic/lifecycle | accepted, no reply | DONE |
| `ping` | basic/utilities | returns empty result | DONE |
| JSON-RPC 2.0 framing & errors | basic | `-32700/-32600/-32601/-32602` mapped; batch rejected | DONE |
| Streamable HTTP: POST | basic/transports | POST → JSON-RPC; 202 for notifications | DONE |
| Streamable HTTP: DELETE (session end) | basic/transports | 200; stateless build has no session table | PARTIAL |
| Streamable HTTP: GET/SSE | basic/transports | not implemented | STAGED |
| `MCP-Protocol-Version` / `MCP-Session-Id` headers | basic/transports | protocol pinned; session id not issued yet | PARTIAL |
| Origin validation | basic/security_best_practices | any Origin rejected (403) | DONE |
| Auth (bearer) | basic/security_best_practices | 256-bit token, constant-time, per-start | DONE |
| Loopback-only bind | basic/security_best_practices | 127.0.0.1 + ::1 only | DONE |
| `resources/list` | server/resources | every entry of `RESOURCES` in `prismatic_mcp_server.cpp` (list them: `resources/list`) | DONE |
| `resources/read` | server/resources | envelope + JSON text content; unknown-uri error | DONE |
| `resources/templates/list` | server/resources | not implemented | STAGED |
| `resources/list_changed` notification | server/resources | needs SSE | STAGED |
| `tools/list` | server/tools | `ToolsList()`: view, UI, menu, decision, AI, Script API (`openttd.api.*`) and command (`openttd.cmd.*`) tools with input schemas and annotations; see `08-tool-catalog.md` | DONE |
| `tools/call` | server/tools | `HandleToolsCall()`; a mutating tool goes through the approval gate (`16-decision-contract.md`); unknown tool is `-32602` | DONE |
| `prompts/list` / `prompts/get` | server/prompts | `list` returns empty; `get` not implemented | STAGED |
| logging notifications | server/utilities/logging | not implemented (needs SSE) | STAGED |
| progress / cancellation | basic/utilities | not implemented | STAGED |

Status update (2026-11-17): `tools/list` and `tools/call` are served (`ToolsList()` and the
`tools/call` branch of `DispatchOne()` in `src/prismatic_mcp/prismatic_mcp_server.cpp`), and
`tools/research/mcp-live-check` calls tools over loopback HTTP.

Conformance suite + MCP Inspector run: **STAGED** (see `09-testing.md`). The read-only methods above
were exercised over real HTTP+JSON-RPC in the in-game vertical (`10-in-game-runbook.md`).
