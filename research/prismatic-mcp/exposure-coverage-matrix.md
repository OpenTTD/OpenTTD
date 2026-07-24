# Exposure Coverage Matrix

Honest coverage of internal domains using the mission's status vocabulary. This build exposes a
deliberately small read-only slice; the rest is catalogued as designed-but-not-yet-adapted, not
pretended-covered.

| Domain | Status | Notes |
|---|---|---|
| MCP runtime status (`openttd://mcp/status`) | `EXPOSED_READ_ONLY` | running/endpoint/port/counters. |
| Build & runtime meta (`openttd://meta/build`) | `EXPOSED_READ_ONLY` | version, revision hash, NewGRF version, dedicated flag, gates. |
| Game state (`openttd://game/state`) | `EXPOSED_READ_ONLY` | mode, pause, map size, object counts. |
| Companies (`openttd://companies`) | `EXPOSED_READ_ONLY` | id/name/is_ai/money/inaugurated. Server-visible; see privacy note. |
| Vehicles / towns / industries / stations | `NOT_YET_ADAPTED` | designed; need pagination (Phase E) before exposure. |
| Map tiles / regions | `NOT_YET_ADAPTED` | strict area cap + coordinate convention required (Phase E). |
| AI / GameScript state & logs | `NOT_YET_ADAPTED` | logs must be bounded/redacted (Phase E). |
| Native UI windows / viewport | `NOT_YET_ADAPTED` | semantic metadata only; ships with UI tools (Phase F). |
| Debug / performance / pools | `NOT_YET_ADAPTED` | expensive diagnostics must be opt-in/bounded (Phase E). |
| Settings schema | `NOT_YET_ADAPTED` | secrets omitted/redacted when added (Phase E). |
| Network clients (IP, keys, names) | `WITHHELD_PRIVACY` | redacted-by-default resource; not exposed in this build. |
| Admin password / client_secret_key / X25519 keys | `WITHHELD_SECRET` | never serialized. |
| Bearer token | `WITHHELD_SECRET` | only via explicit `prismatic_mcp token`; never in cfg/logs/snapshots. |
| Raw pointers / process memory / filesystem | `NOT_APPLICABLE` | never serialized by design (forbidden-shortcuts list). |
| Game-state mutations (pause/save/AI/vehicle) | `EXPOSED_APPROVAL_REQUIRED` (target) | **not registered yet**; lands only with the approval workflow (Phase G/H). |
| Server-admin ops | `EXPOSED_SERVER_ONLY` (target) | staged; server-authority + audit required. |
| Determinism-sensitive internals (RNG, tick order) | `WITHHELD_DETERMINISM` | never mutated by reads; token uses OS CSPRNG, not game RNG. |

Invariant honored: *everything discoverable; everything safe observable; only adapted operations
executable; nothing sensitive leaks implicitly* — with the honest caveat that only 4 read-only
domains are observable **today**, and no operation is executable yet.
