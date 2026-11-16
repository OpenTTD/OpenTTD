/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE / DEBUG-ONLY.
 *
 * Embedded, loopback-only MCP server (stable spec 2025-11-25, Streamable HTTP).
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 *
 * Design (see research/prismatic-mcp/):
 *  - The listener binds 127.0.0.1 and ::1 only, never a public interface.
 *  - Every byte is serviced on the OpenTTD main thread from GameLoop()->Poll(),
 *    so request handlers touch pools/globals with no locking, exactly like the
 *    admin network. No worker thread ever dereferences game state.
 *  - Auth is a 256-bit random bearer token generated at Start(), compared in
 *    constant time. The token is never logged and never written to openttd.cfg.
 *  - This first vertical implements: initialize, notifications/initialized, ping,
 *    resources/list, resources/read (read-only, live game state). Tools, the
 *    approval workflow and mutations are staged behind this transport; see the
 *    implementation plan. It deliberately does NOT expose any mutation yet.
 */

/** @file prismatic_mcp_server.cpp Loopback MCP transport + read-only resources (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp.h"
#include "prismatic_mcp_detail.h"
#include "prismatic_mcp_console.h"
#include "prismatic_mcp_coverage.h"
#include "prismatic_mcp_entities.h"
#include "prismatic_mcp_graph.h"
#include "prismatic_mcp_menu.h"
#include "prismatic_mcp_registry.h"
#include "prismatic_mcp_ui.h"
#include "prismatic_mcp_view.h"
#include "prismatic_mcp_world.h"
#include "prismatic_mcp_commands.h"
#include "prismatic_mcp_ai.h"

#include "../3rdparty/nlohmann/json.hpp"
#include "../core/random_func.hpp"
#include "../debug.h"
#include "../rev.h"
#include "../openttd.h"
#include "../company_base.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../vehicle_base.h"
#include "../town.h"
#include "../map_func.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_tick.h"
#include "../network/core/os_abstraction.h"
#include "../network/core/address.h"
#include "../command_func.h"
#include "../command_type.h"
#include "../network/network.h"
#include "../misc_cmd.h"

#include <array>
#include <deque>
#include <functional>
#include <charconv>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../safeguards.h"

namespace PrismaticMCP {

using json = nlohmann::json;

namespace {

/* ---- HTTP/transport limits (mission §11.6, conservative defaults) ---------- */
constexpr size_t MAX_REQUEST_BODY = 1u << 20;   ///< 1 MiB request body cap.
constexpr size_t MAX_HEADER_BYTES = 32u << 10;  ///< 32 KiB header section cap.
constexpr size_t MAX_CONNECTIONS = 16;          ///< Simultaneous TCP connections.

/** One in-flight loopback HTTP connection. Raw byte buffer; no OpenTTD Packet framing. */
struct Connection {
	SOCKET sock = INVALID_SOCKET;
	std::string inbuf;             ///< Accumulated request bytes until headers+body complete.
	bool headers_done = false;
	size_t content_length = 0;
	size_t header_end = 0;         ///< Offset of body start (end of "\r\n\r\n").
};

/** Process-local server state. Never serialized into a savegame; ephemeral. */
struct ServerState {
	bool running = false;
	uint16_t port = 0;
	SocketList listeners;          ///< Loopback listening sockets (v4 + v6).
	std::vector<Connection> conns;
	std::array<uint8_t, 32> token{}; ///< 256-bit bearer token (raw bytes).
	bool token_valid = false;
	uint64_t requests_total = 0;
	uint64_t requests_denied = 0;
	std::string last_error;
};

ServerState _mcp;

/* ---- small helpers -------------------------------------------------------- */

std::string ToHex(std::span<const uint8_t> bytes)
{
	static const char *digits = "0123456789abcdef";
	std::string out;
	out.reserve(bytes.size() * 2);
	for (uint8_t b : bytes) {
		out.push_back(digits[b >> 4]);
		out.push_back(digits[b & 0xF]);
	}
	return out;
}

/** Constant-time compare of the presented token (hex) against the live token. */
bool TokenMatches(std::string_view presented_hex)
{
	if (!_mcp.token_valid) return false;
	return detail::ConstantTimeEquals(presented_hex, ToHex(_mcp.token));
}

std::string_view TrimOWS(std::string_view s)
{
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
	return s;
}

/* ---- live game-state serializers (main thread only) ----------------------- */

/** The common response envelope (mission §15), stamped with the live snapshot. */
json MakeEnvelope()
{
	json env;
	env["schema_version"] = "1.0";
	env["source"] = "OpenTTD";
	env["source_revision"] = _openttd_revision;
	env["protocol_version"] = std::string(PROTOCOL_VERSION);
	env["game_tick"] = (uint64_t)TimerGameTick::counter;

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	env["game_date"] = fmt::format("{:04}-{:02}-{:02}", (int)ymd.year.base(), (int)ymd.month + 1, (int)ymd.day);

	const char *mode = "menu";
	switch (_game_mode) {
		case GameMode::Normal: mode = "normal"; break;
		case GameMode::Editor: mode = "editor"; break;
		case GameMode::Bootstrap: mode = "bootstrap"; break;
		default: mode = "menu"; break;
	}
	env["game_mode"] = mode;
	env["availability"] = "available";
	env["redactions"] = json::array();
	env["truncated"] = false;
	env["next_cursor"] = nullptr;
	return env;
}

json ResourceMcpStatus()
{
	json env = MakeEnvelope();
	json d;
	d["running"] = _mcp.running;
	d["endpoint"] = GetEndpoint();
	d["bind"] = "127.0.0.1,::1";
	d["port"] = _mcp.port;
	d["read_only"] = true;
	d["requests_total"] = _mcp.requests_total;
	d["requests_denied"] = _mcp.requests_denied;
	d["active_connections"] = (uint64_t)_mcp.conns.size();
	env["data"] = d;
	return env;
}

json ResourceMetaBuild()
{
	json env = MakeEnvelope();
	json d;
	d["openttd_version"] = _openttd_revision;
	d["revision_hash"] = std::string(_openttd_revision_hash);
	d["newgrf_version"] = _openttd_newgrf_version;
	d["research_instrumentation"] = true;
	d["mcp_server"] = true;
#ifdef DEDICATED
	d["dedicated"] = true;
#else
	d["dedicated"] = false;
#endif
	env["data"] = d;
	return env;
}

json ResourceGameState()
{
	json env = MakeEnvelope();
	json d;
	d["paused"] = _pause_mode.Any();
	d["map_width"] = Map::SizeX();
	d["map_height"] = Map::SizeY();
	d["num_companies"] = (uint64_t)Company::GetNumItems();
	d["num_vehicles"] = (uint64_t)Vehicle::GetNumItems();
	d["num_towns"] = (uint64_t)Town::GetNumItems();
	env["data"] = d;
	return env;
}

json ResourceCompanies()
{
	json env = MakeEnvelope();
	json list = json::array();
	/* Deterministic order: pools iterate by ascending index. */
	for (const Company *c : Company::Iterate()) {
		json jc;
		jc["id"] = c->index.base();
		jc["name"] = GetString(STR_COMPANY_NAME, c->index); // the displayed name, generated or renamed
		jc["custom_name"] = c->name;       // empty if the player never renamed it
		jc["is_ai"] = c->is_ai;
		jc["money"] = (int64_t)c->money;
		jc["loan"] = (int64_t)c->current_loan;
		jc["max_loan"] = (int64_t)c->GetMaxLoan();
		jc["loan_step"] = LOAN_INTERVAL; // one press of Borrow or Repay (misc_cmd.cpp); Repay takes min(loan, step)
		jc["company_value"] = (int64_t)CalculateCompanyValue(c);
		jc["money_unit"] = "internal_currency";
		jc["inaugurated_year"] = (int)c->inaugurated_year_calendar.base();
		list.push_back(std::move(jc));
	}
	env["data"] = json{{"companies", std::move(list)}};
	return env;
}

/** openttd://commands -- the command catalogue, derived from the command table. */
json ResourceCommands()
{
	json env = MakeEnvelope();
	json list = json::array();
	size_t exposable = 0;
	for (size_t i = 0; i < cmds::Count(); i++) {
		Commands c = static_cast<Commands>(i);
		if (cmds::IsExposable(c)) exposable++;
		list.push_back(cmds::Describe(c));
	}
	env["data"] = json{{"count", cmds::Count()}, {"exposable", exposable}, {"commands", std::move(list)},
		{"usage", "tools/call openttd.cmd.<name> {company, params[, location]}; every call is approval-gated"}};
	return env;
}

json ResourceDecisions(); // defined below, in the decision/approval section

/** Static resource catalogue. Deterministic order; each entry is read-only here. */
struct ResourceEntry {
	const char *uri;
	const char *name;
	const char *description;
	json (*build)();
};
json ResourceTowns(); // defined below, next to the view tools that scroll to a town
json ResourceMap();   // the whole map (prismatic_mcp_world.cpp)
json ResourceStations();
json ResourceDepots();
json ResourceTerrain();
json ResourcePrices();
json ResourceBridges();
json ResourceRailTypes();
json ResourceMenu();
json ResourceRequests();
json ResourceEngines();  // every engine on offer now (prismatic_mcp_world.cpp)
json ResourceVehicles(); // every primary vehicle with its orders (prismatic_mcp_entities.cpp)
json ResourceCoverage(); // what clients exercised, against the published catalogue (prismatic_mcp_coverage.cpp)

json ResourceWays();  // road ways and rail tracks per tile (prismatic_mcp_graph.cpp)
json ResourceWorldGraph(); // the world as one typed graph (prismatic_mcp_graph.cpp)

json ResourceAi() { json env = MakeEnvelope(); env["data"] = ai::Resource(); return env; }

/** Every typed action type this build can execute: the fixed table, then one "command.<Name>" per
 * game command the catalogue exposes. openttd.decision.list_action_types and the decision tools'
 * input schemas read this, so a type is offered exactly when the validator accepts it. */
json ActionTypes()
{
	json types = json::array();
	for (std::string_view t : detail::ACTION_TYPES) types.push_back(t);
	for (size_t i = 0; i < cmds::Count(); i++) {
		Commands c = static_cast<Commands>(i);
		if (cmds::IsExposable(c)) types.push_back("command." + std::string(cmds::ShortName(c)));
	}
	return types;
}

/* The size is counted by the compiler, so adding a resource is one row. */
const auto RESOURCES = std::to_array<ResourceEntry>({
	{"openttd://mcp/status", "MCP server status", "Embedded MCP server runtime status", &ResourceMcpStatus},
	{"openttd://meta/build", "Build metadata", "OpenTTD version, revision and feature gates", &ResourceMetaBuild},
	{"openttd://game/state", "Game state", "Mode, pause, map size and object counts", &ResourceGameState},
	{"openttd://companies", "Companies", "All companies with basic finances (server-visible)", &ResourceCompanies},
	{"openttd://commands", "Commands", "Every game command, derived from the command table: category, flags, parameter schemas", &ResourceCommands},
	{"openttd://ai", "AIs", "Every company's AI with its settings as declared by its info.nut and their live values; the AIs that can be started", &ResourceAi},
	{"openttd://decisions", "Decisions", "Provenance trace: decision -> approval -> command -> observed verdict", &ResourceDecisions},
	{"openttd://towns", "Towns", "Every town: id, name, tile, x/y, population (scroll_to a town by id)", &ResourceTowns},
	{"openttd://map", "Map", "The whole map: tile types, heights and owners, one string per row, encoding included", &ResourceMap},
	{"openttd://stations", "Stations", "Every station: id, name, tile, x/y, owner, facilities, town", &ResourceStations},
	{"openttd://vehicles", "Vehicles", "Every primary vehicle: id, type, owner, engine, tile, state, cargo, profits, orders", &ResourceVehicles},
	{"openttd://map/terrain", "Map terrain", "Per tile, as rows: slope bits, ground (grass, rough, rocks, fields, snow, desert, trees, sea, canal, river, coast), the largest height step to a neighbour, what building there needs, the vehicles standing there, and a digest per row so a planner re-reads only what changed; encoding included", &ResourceTerrain},
	{"openttd://economy/prices", "Prices", "Every base price of the game by name (build_rail, build_road, build_bridge, build_tunnel, build_foundation, terraform, clear_*, build_station_*, build_depot_*, infrastructure_* ...) as it is now: inflated, scaled by the construction cost setting and NewGRF multipliers; with each start value", &ResourcePrices},
	{"openttd://railtypes", "Rail types", "Every rail type in use: label, name, build cost now (RailBuildCost), cost and maintenance multipliers, speeds, which types power and run on it, and the companies that may build it", &ResourceRailTypes},
	{"openttd://bridges", "Bridges", "Every bridge type: name, price multiplier, speed, min and max length, whether it can be built now; the game's bridge and tunnel length limits and the cost formula", &ResourceBridges},
	{"openttd://map/ways", "Map ways", "Per tile: where a road vehicle can leave it (road bits, stops, depots, crossings, bridges, tunnels) its rail tracks and foundation; every bridge and tunnel with both ends; vehicles per tile; encoding included", &ResourceWays},
	{"openttd://world/graph", "World graph", "The world as one typed graph (world-graph/v1): towns, industries, stations, vehicles; near_town, serves_town, order and cargo_link edges", &ResourceWorldGraph},
	{"openttd://depots", "Depots", "Every depot: id, tile, x/y, owner, type, town, entrance tile; and the game's service reach in tiles", &ResourceDepots},
	{"openttd://ui/menu", "Toolbar menu", "The fork's own main-toolbar menu: every item with its action (console, browse, ask) and target, and the hub URL", &ResourceMenu},
	{"openttd://ui/requests", "Requests", "Questions the player asked in the game through the toolbar menu, each with its answer once an MCP client gave one", &ResourceRequests},
	{"openttd://mcp/coverage", "MCP coverage", "Every published tool and resource with its call count since start, and those never called", &ResourceCoverage},
	{"openttd://engines", "Engines", "Every engine some company can buy now: type, speed (also effective for aircraft), capacity, cargo, price, running cost, plane type, buyers", &ResourceEngines},
});

json ResourceMap()
{
	json env = MakeEnvelope();
	env["data"] = World::Map();
	return env;
}

json ResourceStations()
{
	json env = MakeEnvelope();
	env["data"] = World::Stations();
	return env;
}

json ResourceTerrain()
{
	json env = MakeEnvelope();
	env["data"] = World::Terrain();
	return env;
}

json ResourcePrices()
{
	json env = MakeEnvelope();
	env["data"] = World::Prices();
	return env;
}

json ResourceRailTypes()
{
	json env = MakeEnvelope();
	env["data"] = World::RailTypes();
	return env;
}

json ResourceBridges()
{
	json env = MakeEnvelope();
	env["data"] = World::Bridges();
	return env;
}

json ResourceDepots()
{
	json env = MakeEnvelope();
	env["data"] = World::Depots();
	return env;
}

json ResourceMenu()
{
	json env = MakeEnvelope();
	env["data"] = Menu::Describe();
	return env;
}

json ResourceRequests()
{
	json env = MakeEnvelope();
	env["data"] = Menu::Requests();
	return env;
}

json ResourceEngines()
{
	json env = MakeEnvelope();
	env["data"] = World::Engines();
	return env;
}

json ResourceVehicles()
{
	json env = MakeEnvelope();
	env["data"] = Entities::Vehicles();
	return env;
}

json ResourceWays()
{
	json env = MakeEnvelope();
	env["data"] = Graph::Ways();
	return env;
}

json ResourceWorldGraph()
{
	json env = MakeEnvelope();
	env["data"] = Graph::World();
	return env;
}

json ToolsList();
json ResourcesList();

json ResourceCoverage()
{
	json env = MakeEnvelope();
	env["data"] = Coverage::Report(ToolsList(), ResourcesList());
	return env;
}

/** Every town, so a controller can find where a bot builds and scroll the view there. */
json ResourceTowns()
{
	json env = MakeEnvelope();
	json list = json::array();
	for (const Town *t : Town::Iterate()) {
		list.push_back({{"id", t->index.base()}, {"name", t->GetCachedName()}, {"tile", t->xy.base()},
			{"x", TileX(t->xy)}, {"y", TileY(t->xy)}, {"population", t->cache.population}});
	}
	env["data"] = json{{"towns", std::move(list)}};
	return env;
}

/* ---- decision / approval / provenance (main thread) ----------------------- *
 * The mission's core continuation vertical: a typed DecisionEnvelope is validated
 * (non-mutating), then a mutation tool is *gated* behind a two-phase, digest-bound,
 * operator approval before it ever touches the game via Command<>::Post. Because
 * tool handlers run inside Poll() on the main thread, the command executes on the
 * correct thread through the normal deterministic command path -- never a raw pool
 * write, never a mutation off the main thread. Approval is one-time (replay of an
 * approved call requires a fresh approval), and the digest binds the approval to
 * the exact arguments so post-approval argument changes are rejected. */

enum class ApprovalState : uint8_t { Pending, Approved, Denied, Consumed };

struct PendingApproval {
	uint64_t id = 0;
	std::string tool;
	std::string arg_digest;      ///< FNV-1a of tool + canonical args; binds approval to exact args.
	std::string args_summary;    ///< Human-readable, shown in the console approval prompt.
	uint64_t created_tick = 0;
	uint64_t expiry_tick = 0;
	ApprovalState state = ApprovalState::Pending;
};

/** Bounded provenance record: decision -> approval -> command -> observed verdict. */
struct ProvenanceRecord {
	uint64_t decision_id = 0;
	uint64_t approval_id = 0;
	std::string action_type;
	std::string arg_digest;
	std::string command_result;   ///< "queued", then "succeeded"/"failed"; "rejected" when never queued
	std::string verdict;          ///< PENDING_OBSERVATION until the command has run, then VERIFIED_* / EXECUTED_UNVERIFIED
	std::string verification;     ///< what the verdict rests on: "observed_state" or "command_result"
	std::string error;            ///< the command's error text, when it failed
	int64_t cost = 0;             ///< the executed command's cost (internal currency)
	std::string actor;            ///< "server", "deity" or the company id
	Commands cmd = Commands::End; ///< the command posted, matched against its executed result
	int callbacks_left = 0;       ///< executed results still to arrive
	bool failed = false;          ///< one of its commands failed
	std::function<bool()> observe; ///< asked after the last result: is the intended effect there?
	uint64_t requested_tick = 0;
	uint64_t observed_tick = 0;
};

std::vector<PendingApproval> _approvals;
std::vector<ProvenanceRecord> _provenance;
std::deque<uint64_t> _awaiting;   ///< decision ids posted and not yet executed, in posting order
uint64_t _next_approval_id = 1;
uint64_t _next_decision_id = 1;
constexpr size_t MAX_APPROVALS = 64;
constexpr size_t MAX_PROVENANCE = 128;
constexpr uint64_t APPROVAL_TTL_TICKS = 100000; ///< generous; the operator approves interactively

/** FNV-1a 64-bit digest of tool+args, via the shared pure helper. */
std::string ArgDigest(std::string_view tool, const json &args)
{
	return detail::ArgDigestHex(tool, args.dump());
}

ProvenanceRecord *FindDecision(uint64_t id)
{
	for (auto &p : _provenance) if (p.decision_id == id) return &p;
	return nullptr;
}

PendingApproval *FindApproval(uint64_t id)
{
	for (auto &a : _approvals) if (a.id == id) return &a;
	return nullptr;
}

using detail::IsKnownActionType;

/* ---- JSON-RPC dispatch (main thread) -------------------------------------- */

json RpcError(const json &id, int code, std::string_view message)
{
	return json{{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", std::string(message)}}}};
}

json RpcResult(const json &id, json result)
{
	return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

/* ---- decision tools ------------------------------------------------------- */

/** openttd://decisions -- the provenance trace (execution vs verified-outcome). */
json ResourceDecisions()
{
	json env = MakeEnvelope();
	json arr = json::array();
	for (const auto &p : _provenance) {
		arr.push_back({
			{"decision_id", p.decision_id}, {"approval_id", p.approval_id},
			{"action_type", p.action_type}, {"arg_digest", p.arg_digest},
			{"command_result", p.command_result}, {"verdict", p.verdict},
			{"verification", p.verification}, {"error", p.error}, {"cost", p.cost}, {"actor", p.actor},
			{"requested_tick", p.requested_tick}, {"observed_tick", p.observed_tick},
		});
	}
	env["data"] = json{{"decisions", std::move(arr)}};
	return env;
}

/** The command a "command.<Name>" action type names, if any. */
std::optional<Commands> CommandOf(std::string_view type)
{
	if (!type.starts_with("command.")) return std::nullopt;
	return cmds::Find(type.substr(8));
}

/** Who a command action is posted as: "server" (default), "deity" or a company id. */
bool ParseActor(const json &args, cmds::Actor &actor, std::string &label, std::string &err)
{
	if (_networking && !_network_server) {
		/* Client mode: this process is a player's client; it can only act as its own company,
		 * and its commands go to the server like the player's clicks. */
		if (args.contains("company") && !(args["company"].is_string() && args["company"] == "server")
				&& !(args["company"].is_number_integer() && args["company"].get<int64_t>() == _local_company.base())) {
			err = fmt::format("this process is a network client; it acts only as its own company {}", _local_company.base());
			return false;
		}
		actor.keep_current = true;
		label = fmt::format("client:{}", _local_company.base());
		return true;
	}
	if (!args.contains("company") || (args["company"].is_string() && args["company"] == "server")) {
		actor.keep_current = true;
		label = "server";
		return true;
	}
	const json &c = args["company"];
	if (c.is_string() && c == "deity") {
		actor.keep_current = false;
		actor.company = OWNER_DEITY;
		label = "deity";
		return true;
	}
	if (c.is_number_integer()) {
		int64_t v = c.get<int64_t>();
		if (v < 0 || v >= MAX_COMPANIES || !Company::IsValidID(CompanyID(static_cast<uint8_t>(v)))) {
			err = fmt::format("no company {}", v);
			return false;
		}
		actor.keep_current = false;
		actor.company = CompanyID(static_cast<uint8_t>(v));
		label = fmt::format("{}", v);
		return true;
	}
	err = "company must be a company id, \"deity\" or \"server\"";
	return false;
}

/** The location of a command action, required when the command takes one implicitly. */
bool ParseLocation(Commands cmd, const json &args, TileIndex &tile, std::string &err)
{
	tile = TileIndex{};
	if (!args.contains("location")) {
		if (cmds::NeedsLocation(cmd)) { err = "this command requires 'location' (a tile index)"; return false; }
		return true;
	}
	if (!args["location"].is_number_unsigned() || args["location"].get<uint64_t>() >= Map::Size()) {
		err = "location must be a tile index below " + fmt::format("{}", Map::Size());
		return false;
	}
	tile = TileIndex{static_cast<uint32_t>(args["location"].get<uint64_t>())};
	return true;
}

/**
 * Whether this process may post the command at all. A network client that posts a
 * CommandFlag::Server command (Pause, CompanyControl, ...) is kicked by the server, so a
 * client refuses it here, before anything is sent.
 */
bool ClientMayPost(Commands cmd, std::string &err)
{
	if (_networking && !_network_server && GetCommandFlags(cmd).Test(CommandFlag::Server)) {
		err = "this is a server-only command and this process is a network client (the server would kick it)";
		return false;
	}
	return true;
}

/** Non-mutating validation of a typed action + args (mission §10/§11). */
json ValidateAction(std::string_view type, const json &args, json &errors)
{
	json result;
	bool valid = true;
	if (!IsKnownActionType(type)) { errors.push_back("unknown action type"); valid = false; }
	if (type == "game.set_pause") {
		if (!args.contains("pause") || !args["pause"].is_boolean()) {
			errors.push_back("game.set_pause requires boolean 'pause'");
			valid = false;
		}
		if (std::string err; !ClientMayPost(Commands::Pause, err)) { errors.push_back(err); valid = false; }
		result["required_scope"] = "SERVER_ADMIN";
	} else if (ai::IsAiAction(type)) {
		json details = ai::Validate(type, args, errors);
		if (!errors.empty()) valid = false;
		result["details"] = details;
		result["required_scope"] = "AI_CONTROL";
	} else if (auto cmd = CommandOf(type)) {
		cmds::Actor actor;
		std::string label, err;
		TileIndex tile;
		if (!cmds::IsExposable(*cmd)) { errors.push_back("this command has parameters that cannot be expressed in JSON"); valid = false; }
		if (valid && !ClientMayPost(*cmd, err)) { errors.push_back(err); valid = false; }
		if (valid && !ParseActor(args, actor, label, err)) { errors.push_back(err); valid = false; }
		if (valid && !ParseLocation(*cmd, args, tile, err)) { errors.push_back(err); valid = false; }
		if (valid) {
			cmds::TestResult t = cmds::Test(*cmd, actor, args.value("params", json::array()));
			if (!t.params_ok) { errors.push_back(t.error); valid = false; }
			result["would_succeed"] = t.would_succeed;
			result["estimated_cost"] = t.cost;
			if (!t.would_succeed && !t.error.empty()) result["test_error"] = t.error;
			result["actor"] = label;
		}
		result["command"] = cmds::Describe(*cmd);
		result["required_scope"] = "GAME_MUTATION";
	} else if (valid) {
		errors.push_back("unknown command");
		valid = false;
	}
	result["valid"] = valid;
	result["errors"] = errors;
	result["current_snapshot_id"] = fmt::format("tick-{}", (uint64_t)TimerGameTick::counter);
	result["approval_required"] = true;   // no mutation is ever auto-approved in this build
	result["mutating"] = true;
	return result;
}

/** Take the verdict of a decision whose commands have all run (or that ran none). */
void Conclude(ProvenanceRecord &p)
{
	p.observed_tick = (uint64_t)TimerGameTick::counter;
	p.command_result = p.failed ? "failed" : "succeeded";
	if (p.observe) {
		/* The effect, not the report: asked of the game after the last command ran. */
		p.verification = "observed_state";
		p.verdict = p.failed ? "VERIFIED_FAILURE" : (p.observe() ? "VERIFIED_SUCCESS" : "EXECUTED_UNVERIFIED");
	} else {
		p.verification = "command_result";
		p.verdict = p.failed ? "VERIFIED_FAILURE" : "VERIFIED_SUCCESS";
	}
	Debug(Facility::Net, Severity::Error, "[prismatic-mcp] decision #{} {} {} verdict={}", p.decision_id, p.action_type, p.command_result, p.verdict);
}

/** Register one executed result to come for a decision; called before each post. */
void Expect(ProvenanceRecord &pr)
{
	pr.callbacks_left++;
	_awaiting.push_back(pr.decision_id);
}

/** Withdraw the expectations of a decision whose post was refused before it was queued. */
void Unexpect(uint64_t decision_id)
{
	ProvenanceRecord *pr = FindDecision(decision_id);
	for (auto it = _awaiting.begin(); it != _awaiting.end();) {
		if (*it == decision_id) {
			it = _awaiting.erase(it);
			if (pr != nullptr && pr->callbacks_left > 0) pr->callbacks_left--;
		} else {
			++it;
		}
	}
}

/** Record a decision refused before anything was posted. @return false */
bool ExecuteRefused(uint64_t decision_id, const std::string &err)
{
	if (ProvenanceRecord *r = FindDecision(decision_id)) {
		r->failed = true;
		r->error = err;
		Conclude(*r);
		r->command_result = "rejected";
	}
	return false;
}

/**
 * Carry out an approved, digest-bound action. Main thread.
 * Commands go through the normal command path; on a network server they run on a later tick,
 * so the verdict is taken by OnCommandResult when the last of them has actually executed.
 * @return false when the action was refused before anything was queued; the record says why.
 */
bool ExecuteAction(std::string_view type, const json &args, ProvenanceRecord &pr)
{
	std::string err;
	bool ok = false;
	uint64_t did = pr.decision_id;
	if (type == "game.set_pause") {
		bool want = args["pause"].get<bool>();
		pr.cmd = Commands::Pause;
		pr.actor = "server";
		pr.observe = [want]() { return _pause_mode.Any() == want; };
		if (!ClientMayPost(Commands::Pause, err)) return ExecuteRefused(did, err);
		Expect(pr);
		/* SERVER_ADMIN synchronized command -- the deterministic path, not a raw write. */
		ok = Command<Commands::Pause>::Post(StringID{}, &CcPrismaticMcp, PauseMode::Normal, want);
		if (!ok) err = "the pause command was refused";
	} else if (ai::IsAiAction(type)) {
		pr.cmd = Commands::CompanyControl;
		pr.actor = "server";
		ai::Posted posted = ai::Execute(type, args, [did]() { if (ProvenanceRecord *r = FindDecision(did)) Expect(*r); });
		ok = posted.ok;
		err = posted.error;
		if (ProvenanceRecord *r = FindDecision(did)) {
			r->observe = posted.observe;
			/* A partly posted action keeps the expectations of what was posted. */
			while (r->callbacks_left > posted.callbacks) {
				auto it = std::find(_awaiting.rbegin(), _awaiting.rend(), did);
				if (it == _awaiting.rend()) break;
				_awaiting.erase(std::next(it).base());
				r->callbacks_left--;
			}
			if (ok && posted.callbacks == 0 && r->verdict == "PENDING_OBSERVATION") Conclude(*r);
		}
	} else if (auto cmd = CommandOf(type)) {
		cmds::Actor actor;
		TileIndex tile;
		pr.cmd = *cmd;
		if (ClientMayPost(*cmd, err) && ParseActor(args, actor, pr.actor, err) && ParseLocation(*cmd, args, tile, err)) {
			Expect(pr);
			ok = cmds::Post(*cmd, actor, args.value("params", json::array()), tile, err);
			if (!ok && err.empty()) {
				cmds::TestResult t = cmds::Test(*cmd, actor, args.value("params", json::array()));
				err = t.error.empty() ? "the command was refused before it was queued" : t.error;
			}
		}
	} else {
		err = "unknown action type";
	}
	ProvenanceRecord *r = FindDecision(did);
	if (!ok && r != nullptr) {
		/* Refused before it was queued: no result will come. A single-player post that failed
		 * has already been answered by its result, which keeps that verdict. */
		Unexpect(did);
		if (r->verdict == "PENDING_OBSERVATION") {
			r->failed = true;
			r->error = err;
			Conclude(*r);
			r->command_result = "rejected";
		}
	}
	return ok;
}

/** tools/call for the typed decision + mutation tools. */
json HandleToolsCall(const json &id, const json &params)
{
	if (!params.contains("name") || !params["name"].is_string()) {
		return RpcError(id, -32602, "Invalid params: missing tool name");
	}
	std::string name = params["name"].get<std::string>();
	json args = params.contains("arguments") ? params["arguments"] : json::object();

	/* The fork's toolbar menu and the questions asked through it (prismatic_mcp_menu.cpp); before the
	 * UI tools, whose names share the openttd.ui. prefix. */
	if (auto menu = Menu::Call(name, args); menu.has_value()) {
		bool ok = menu->value("ok", false);
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", menu->dump()}}})}, {"isError", !ok}});
	}

	/* UI tools: the interface as data, and armed input through the driver's path (prismatic_mcp_ui.cpp). */
	if (auto ui = UI::Call(name, args); ui.has_value()) {
		bool ok = ui->value("ok", false);
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", ui->dump()}}})}, {"isError", !ok}});
	}

	/* View tools: local viewport only, no approval, no command (prismatic_mcp_view.cpp). */
	if (auto view = View::Call(name, args); view.has_value()) {
		bool ok = view->value("ok", false);
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", view->dump()}}})}, {"isError", !ok}});
	}

	/* Non-mutating validation tool. */
	if (name == "openttd.decision.validate") {
		std::string type = args.value("action_type", args.contains("action") ? args["action"].value("type", "") : "");
		json aargs = args.contains("action") ? args["action"].value("arguments", json::object()) : args.value("arguments", json::object());
		json errs = json::array();
		json v = ValidateAction(type, aargs, errs);
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", v.dump()}}})}, {"isError", false}});
	}

	if (name == "openttd.decision.list_action_types") {
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", json{{"action_types", ActionTypes()}}.dump()}}})}, {"isError", false}});
	}

	if (name == "openttd.ai.list") {
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", ai::Resource().dump()}}})}});
	}

	if (name == "openttd.decision.get") {
		if (!args.contains("decision_id") || !args["decision_id"].is_number_unsigned()) {
			return RpcError(id, -32602, "Invalid params: decision_id (integer) required");
		}
		const ProvenanceRecord *p = FindDecision(args["decision_id"].get<uint64_t>());
		if (p == nullptr) return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", "{\"error\":\"no such decision\"}"}}})}, {"isError", true}});
		json d = {{"decision_id", p->decision_id}, {"approval_id", p->approval_id}, {"action_type", p->action_type},
			{"command_result", p->command_result}, {"verdict", p->verdict}, {"verification", p->verification},
			{"error", p->error}, {"cost", p->cost}, {"actor", p->actor},
			{"requested_tick", p->requested_tick}, {"observed_tick", p->observed_tick}};
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", d.dump()}}})}});
	}

	/* The gated mutation tools. Two-phase: pending_approval -> (operator approves) -> executed. */
	bool is_cmd_tool = name.starts_with("openttd.cmd.");
	bool is_ai_tool = name.starts_with("openttd.ai.") && ai::IsAiAction(name.substr(8));
	if (name == "openttd.game.set_pause" || name == "openttd.decision.submit" || is_cmd_tool || is_ai_tool) {
		std::string type = "game.set_pause";
		json aargs = args;
		if (is_ai_tool) {
			type = name.substr(8);
		} else if (is_cmd_tool) {
			type = "command." + name.substr(12);
		} else if (name == "openttd.decision.submit") {
			type = args.value("action", json::object()).value("type", "");
			aargs = args.value("action", json::object()).value("arguments", json::object());
		}
		json errs = json::array();
		json v = ValidateAction(type, aargs, errs);
		if (!v["valid"].get<bool>()) {
			return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", v.dump()}}})}, {"isError", true}});
		}

		std::string digest = ArgDigest(type, aargs);
		uint64_t now = (uint64_t)TimerGameTick::counter;

		/* Is there an APPROVED, non-expired, non-consumed approval bound to this exact digest? */
		for (auto &a : _approvals) {
			if (a.state == ApprovalState::Approved && a.arg_digest == digest && now <= a.expiry_tick) {
				a.state = ApprovalState::Consumed;   // one-time: replay needs a fresh approval
				if (_provenance.size() >= MAX_PROVENANCE) _provenance.erase(_provenance.begin());
				ProvenanceRecord &pr = _provenance.emplace_back();
				pr.decision_id = _next_decision_id++;
				pr.approval_id = a.id;
				pr.action_type = type;
				pr.arg_digest = digest;
				pr.command_result = "queued";
				pr.verdict = "PENDING_OBSERVATION";
				pr.requested_tick = now;
				uint64_t decision_id = pr.decision_id;
				bool queued = ExecuteAction(type, aargs, pr);
				const ProvenanceRecord *rec = FindDecision(decision_id);
				json out = {{"status", queued ? "executed" : "rejected"}, {"approval_id", a.id}, {"decision_id", decision_id},
					{"command_result", rec->command_result}, {"verdict", rec->verdict},
					{"note", "the verdict is taken when the command has run; read it with openttd.decision.get"}};
				if (!rec->error.empty()) out["error"] = rec->error;
				return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", out.dump()}}})}, {"isError", !queued}});
			}
		}

		/* Otherwise: reuse an existing pending approval for this digest, or create one. */
		for (auto &a : _approvals) {
			if (a.state == ApprovalState::Pending && a.arg_digest == digest && now <= a.expiry_tick) {
				json out = {{"status", "pending_approval"}, {"approval_id", a.id}, {"argument_digest", digest},
					{"message", "Approval required in OpenTTD: prismatic_mcp approve " + fmt::format("{}", a.id)}};
				return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", out.dump()}}})}});
			}
		}
		if (_approvals.size() >= MAX_APPROVALS) _approvals.erase(_approvals.begin());
		PendingApproval pa;
		pa.id = _next_approval_id++;
		pa.tool = type;
		pa.arg_digest = digest;
		pa.args_summary = aargs.dump();
		pa.created_tick = now;
		pa.expiry_tick = now + APPROVAL_TTL_TICKS;
		pa.state = ApprovalState::Pending;
		_approvals.push_back(pa);
		Debug(Facility::Net, Severity::Error, "[prismatic-mcp] pending approval #{} for {} args={} (approve in console: prismatic_mcp approve {})",
			pa.id, type, pa.args_summary, pa.id);
		json out = {{"status", "pending_approval"}, {"approval_id", pa.id}, {"argument_digest", digest},
			{"expires_at_tick", pa.expiry_tick},
			{"message", "Approval required in OpenTTD: prismatic_mcp approve " + fmt::format("{}", pa.id)}};
		return RpcResult(id, json{{"content", json::array({{{"type", "text"}, {"text", out.dump()}}})}});
	}

	return RpcError(id, -32602, "Unknown tool");
}

/** Static tool catalogue for tools/list. */
json ToolsList()
{
	json arr = json::array();
	/* View tools first: they change only the local viewport and need no approval. */
	for (auto &t : View::Tools()) arr.push_back(t);
	for (auto &t : UI::Tools()) arr.push_back(t);
	for (auto &t : Menu::Tools()) arr.push_back(t);
	/* The decision tools. Their action types come from ActionTypes(); their annotations say
	 * what a client (and the coverage probe, which calls only readOnlyHint tools) may assume. */
	const json types = ActionTypes();
	const json action = {{"type", "object"}, {"required", json::array({"type"})},
		{"properties", {{"type", {{"type", "string"}, {"enum", types}}}, {"arguments", {{"type", "object"}}}}}};
	const json reads = {{"readOnlyHint", true}, {"idempotentHint", true}, {"openWorldHint", false}};
	const json gated = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", false}};
	arr.push_back({{"name", "openttd.decision.validate"}, {"description", "Validate a typed action/DecisionEnvelope without mutating game state: {action: {type, arguments}} or {action_type, arguments}"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"action", action}, {"action_type", {{"type", "string"}, {"enum", types}}}, {"arguments", {{"type", "object"}}}}}}},
		{"annotations", reads}});
	arr.push_back({{"name", "openttd.decision.list_action_types"}, {"description", "List typed action types this build can execute"},
		{"inputSchema", {{"type", "object"}}}, {"annotations", reads}});
	arr.push_back({{"name", "openttd.decision.submit"}, {"description", "Submit a DecisionEnvelope; returns a pending approval until an operator approves, then executes via the normal command path"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"action"})}, {"properties", {{"action", action}}}}},
		{"annotations", gated}});
	arr.push_back({{"name", "openttd.game.set_pause"}, {"description", "Approval-gated: pause/unpause via Commands::Pause. Requires operator approval (two-phase, digest-bound)"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"pause", {{"type", "boolean"}}}}}, {"required", json::array({"pause"})}}},
		{"annotations", gated}});
	arr.push_back({{"name", "openttd.decision.get"}, {"description", "Read one decision's provenance: command result and the verdict taken after the command ran"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"decision_id", {{"type", "integer"}, {"minimum", 1}}}}}, {"required", json::array({"decision_id"})}}}});
	for (auto &t : ai::Tools()) arr.push_back(t);
	/* One tool per game command whose parameters convert from JSON, derived from the command table. */
	for (size_t i = 0; i < cmds::Count(); i++) {
		Commands c = static_cast<Commands>(i);
		if (!cmds::IsExposable(c)) continue;
		json d = cmds::Describe(c);
		std::string desc = fmt::format("Approval-gated game command {} ({}; flags: {}). Validates by a side-effect-free test run, "
			"executes through the network-safe command path; verdict via openttd.decision.get.",
			d["function"].get<std::string>(), d["category"].get<std::string>(), d["flags"].dump());
		arr.push_back({{"name", "openttd.cmd." + std::string(cmds::ShortName(c))}, {"description", desc},
			{"inputSchema", cmds::ToolInputSchema(c)}});
	}
	/* MCP annotations for the decision, AI and derived command tools, from how each is handled:
	 * a tool that goes through the approval path mutates the game; the rest only read. */
	for (auto &t : arr) {
		std::string n = t["name"].get<std::string>();
		bool mine = n.starts_with("openttd.decision.") || n == "openttd.game.set_pause" || n.starts_with("openttd.cmd.") || n.starts_with("openttd.ai.");
		if (!mine || t.contains("annotations")) continue;
		bool gated = n == "openttd.decision.submit" || n == "openttd.game.set_pause" || n.starts_with("openttd.cmd.")
			|| (n.starts_with("openttd.ai.") && ai::IsAiAction(n.substr(8)));
		t["annotations"] = gated
			? json{{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}
			: json{{"readOnlyHint", true}, {"openWorldHint", false}};
	}
	return json{{"tools", arr}};
}

/** The resources/list answer: every entry of RESOURCES. */
json ResourcesList()
{
	json arr = json::array();
	for (const auto &r : RESOURCES) {
		arr.push_back({{"uri", r.uri}, {"name", r.name}, {"description", r.description}, {"mimeType", "application/json"}});
	}
	return json{{"resources", arr}};
}

/** Dispatch a single JSON-RPC request object. Returns std::nullopt for notifications. */
std::optional<json> DispatchOne(const json &req)
{
	json id = req.contains("id") ? req["id"] : json(nullptr);
	bool is_notification = !req.contains("id");

	if (!req.contains("method") || !req["method"].is_string()) {
		return is_notification ? std::nullopt : std::optional<json>(RpcError(id, -32600, "Invalid Request: missing method"));
	}
	std::string method = req["method"].get<std::string>();

	if (method == "initialize") {
		json caps = {{"resources", json::object()}, {"tools", json::object()}, {"logging", json::object()}};
		json result = {
			{"protocolVersion", std::string(PROTOCOL_VERSION)},
			{"capabilities", caps},
			{"serverInfo", {{"name", "openttd-prismatic-mcp"}, {"version", _openttd_revision}}},
			{"instructions", "RESEARCH-ONLY embedded OpenTTD MCP server. Read-only resources are live. "
			 "Every game command whose parameters convert from JSON is a tool openttd.cmd.<name> (catalogue: "
			 "openttd://commands), plus openttd.game.set_pause. Each mutation is validated by a side-effect-free "
			 "test run (openttd.decision.validate), then requires operator approval ('prismatic_mcp approve <id>' "
			 "in the OpenTTD console) before a resubmitted, argument-identical call posts it through the normal "
			 "command path. The verdict is taken after the command has run: read it with openttd.decision.get."},
		};
		return RpcResult(id, result);
	}
	if (method == "notifications/initialized") return std::nullopt; // notification, no reply
	if (method == "ping") return RpcResult(id, json::object());

	if (method == "resources/list") return RpcResult(id, ResourcesList());
	if (method == "resources/read") {
		if (!req.contains("params") || !req["params"].contains("uri")) {
			return RpcError(id, -32602, "Invalid params: missing uri");
		}
		std::string uri = req["params"]["uri"].get<std::string>();
		Coverage::Count("resource", uri);
		for (const auto &r : RESOURCES) {
			if (uri == r.uri) {
				json contents = json::array();
				contents.push_back({{"uri", r.uri}, {"mimeType", "application/json"}, {"text", r.build().dump()}});
				return RpcResult(id, json{{"contents", contents}});
			}
		}
		return RpcError(id, -32602, "Unknown resource uri");
	}
	if (method == "tools/list") return RpcResult(id, ToolsList());
	if (method == "tools/call") {
		if (req.contains("params") && req["params"].contains("name") && req["params"]["name"].is_string()) Coverage::Count("tool", req["params"]["name"].get<std::string>());
		if (!req.contains("params")) return RpcError(id, -32602, "Invalid params");
		return HandleToolsCall(id, req["params"]);
	}
	if (method == "prompts/list") return RpcResult(id, json{{"prompts", json::array()}});

	return is_notification ? std::nullopt : std::optional<json>(RpcError(id, -32601, "Method not found"));
}

/* ---- HTTP request handling ------------------------------------------------ */

/** Build a minimal HTTP/1.1 response with a JSON body and Connection: close. */
std::string HttpResponse(int status, std::string_view reason, std::string_view body, std::string_view extra_headers = "")
{
	std::string out;
	out += "HTTP/1.1 " + fmt::format("{}", status) + " " + std::string(reason) + "\r\n";
	out += "Content-Type: application/json\r\n";
	out += "Content-Length: " + fmt::format("{}", body.size()) + "\r\n";
	if (!extra_headers.empty()) out += std::string(extra_headers);
	out += "Connection: close\r\n\r\n";
	out += std::string(body);
	return out;
}

/** Parse and answer one fully-received HTTP request. Returns the raw response bytes. */
std::string HandleHttpRequest(std::string_view raw, size_t header_end, size_t content_length)
{
	_mcp.requests_total++;

	/* Request line: METHOD SP path SP HTTP/1.1 */
	size_t line_end = raw.find("\r\n");
	std::string_view request_line = raw.substr(0, line_end);
	size_t sp1 = request_line.find(' ');
	size_t sp2 = request_line.find(' ', sp1 + 1);
	if (sp1 == std::string_view::npos || sp2 == std::string_view::npos) {
		_mcp.requests_denied++;
		return HttpResponse(400, "Bad Request", R"({"error":"malformed request line"})");
	}
	std::string_view http_method = request_line.substr(0, sp1);
	std::string_view path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);

	/* Header scan for Authorization + Origin. Header section is [line_end+2, header_end). */
	std::string_view headers = raw.substr(line_end + 2, header_end - (line_end + 2));
	std::string_view auth, origin;
	size_t pos = 0;
	while (pos < headers.size()) {
		size_t nl = headers.find("\r\n", pos);
		if (nl == std::string_view::npos) nl = headers.size();
		std::string_view h = headers.substr(pos, nl - pos);
		size_t colon = h.find(':');
		if (colon != std::string_view::npos) {
			std::string_view name = TrimOWS(h.substr(0, colon));
			std::string_view value = TrimOWS(h.substr(colon + 1));
			/* case-insensitive header name compare */
			auto ieq = [](std::string_view a, std::string_view b) {
				if (a.size() != b.size()) return false;
				for (size_t i = 0; i < a.size(); i++) if (tolower(a[i]) != tolower(b[i])) return false;
				return true;
			};
			if (ieq(name, "authorization")) auth = value;
			else if (ieq(name, "origin")) origin = value;
		}
		pos = nl + 2;
	}

	/* Loopback-only + Origin policy: reject any browser Origin (mission §12). */
	if (!origin.empty()) {
		_mcp.requests_denied++;
		return HttpResponse(403, "Forbidden", R"({"error":"Origin header not allowed for loopback MCP endpoint"})");
	}

	if (path != "/mcp") {
		return HttpResponse(404, "Not Found", R"({"error":"only /mcp is served"})");
	}

	/* Bearer auth, constant-time. */
	std::string_view bearer = detail::ExtractBearer(auth);
	if (!TokenMatches(bearer)) {
		_mcp.requests_denied++;
		return HttpResponse(401, "Unauthorized", R"({"error":"missing or invalid bearer token"})",
			"WWW-Authenticate: Bearer\r\n");
	}

	if (http_method == "DELETE") {
		/* Explicit session termination: this stateless build has no session table yet. */
		return HttpResponse(200, "OK", R"({"ok":true})");
	}
	if (http_method != "POST") {
		return HttpResponse(405, "Method Not Allowed", R"({"error":"POST required for JSON-RPC"})",
			"Allow: POST, DELETE\r\n");
	}

	std::string_view body = raw.substr(header_end, content_length);
	json parsed;
	try {
		parsed = json::parse(body);
	} catch (const json::exception &) {
		return HttpResponse(200, "OK", RpcError(nullptr, -32700, "Parse error").dump());
	}

	/* Reject JSON-RPC batches explicitly (mission: batch rejected). */
	if (parsed.is_array()) {
		return HttpResponse(200, "OK", RpcError(nullptr, -32600, "Batch requests are not supported").dump());
	}

	std::optional<json> reply = DispatchOne(parsed);
	if (!reply.has_value()) {
		/* Notification: 202 Accepted with empty body per Streamable HTTP. */
		return HttpResponse(202, "Accepted", "");
	}
	return HttpResponse(200, "OK", reply->dump());
}

/* ---- socket servicing (main thread, non-blocking) ------------------------- */

void CloseConnection(Connection &c)
{
	if (c.sock != INVALID_SOCKET) closesocket(c.sock);
	c.sock = INVALID_SOCKET;
}

/** Try to parse headers once enough bytes have arrived; sets content_length. */
bool TryCompleteHeaders(Connection &c)
{
	if (c.headers_done) return true;
	size_t end = c.inbuf.find("\r\n\r\n");
	if (end == std::string::npos) {
		if (c.inbuf.size() > MAX_HEADER_BYTES) { CloseConnection(c); }
		return false;
	}
	c.header_end = end + 4;
	c.headers_done = true;

	/* Content-Length via the shared pure parser (case-insensitive). */
	std::optional<size_t> cl = detail::ParseContentLength(std::string_view(c.inbuf).substr(0, c.header_end));
	if (cl.has_value()) {
		c.content_length = *cl;
		if (c.content_length > MAX_REQUEST_BODY) { CloseConnection(c); return false; }
	}
	return true;
}

void ServiceConnection(Connection &c)
{
	if (c.sock == INVALID_SOCKET) return;

	/* Read whatever is available (non-blocking). */
	char buf[4096];
	for (;;) {
		ssize_t n = recv(c.sock, buf, sizeof(buf), 0);
		if (n > 0) {
			c.inbuf.append(buf, (size_t)n);
			if (c.inbuf.size() > MAX_REQUEST_BODY + MAX_HEADER_BYTES) { CloseConnection(c); return; }
			continue;
		}
		if (n == 0) { CloseConnection(c); return; } // peer closed
		NetworkError err = NetworkError::GetLast();
		if (err.WouldBlock()) break;
		CloseConnection(c);
		return;
	}

	if (!TryCompleteHeaders(c)) return;
	if (c.inbuf.size() < c.header_end + c.content_length) return; // body still arriving

	std::string response = HandleHttpRequest(c.inbuf, c.header_end, c.content_length);
	/* Best-effort blocking-ish send of the whole response, then close. */
	size_t sent = 0;
	while (sent < response.size()) {
		ssize_t n = send(c.sock, response.data() + sent, response.size() - sent, 0);
		if (n > 0) { sent += (size_t)n; continue; }
		NetworkError err = NetworkError::GetLast();
		if (err.WouldBlock()) continue;
		break;
	}
	CloseConnection(c);
}

} // anonymous namespace

json Menu::CallTool(std::string_view name, const json &args)
{
	/* The menu calls a tool the way a client's tools/call does, and gets the call's result. */
	json resp = HandleToolsCall(nullptr, {{"name", name}, {"arguments", args}});
	if (resp.contains("result")) return resp["result"];
	return json{{"isError", true}, {"error", resp.value("error", json::object())}};
}

/* ---- public API ----------------------------------------------------------- */

void Init()
{
	/* Nothing heavy at startup; the listener is created only on explicit Start(). */
	_mcp.running = false;
}

/**
 * Does something already accept TCP connections on loopback `port`? Listen() sets SO_REUSEPORT
 * (os_abstraction.cpp), so on macOS and Linux a second process can listen on the same port and the
 * kernel then spreads connections between the two: a client would reach either server at random
 * (seen 2026-11-16, two OpenTTD MCP servers on 8731). A connect probe before Listen catches the
 * real case; two processes starting in the same instant can still race.
 */
static bool PortHasListener(uint16_t port)
{
	for (const char *host : {"127.0.0.1", "::1"}) {
		NetworkAddress addr(host, port);
		const sockaddr_storage *ss = addr.GetAddress();
		SOCKET s = socket(ss->ss_family, SOCK_STREAM, 0);
		if (s == INVALID_SOCKET) continue;
		bool accepted = connect(s, reinterpret_cast<const sockaddr *>(ss), addr.GetAddressLength()) == 0;
		closesocket(s);
		if (accepted) return true;
	}
	return false;
}

bool Start(uint16_t port, std::string &error)
{
	if (_mcp.running) { error = "already running"; return false; }
	_mcp.port = (port == 0) ? DEFAULT_PORT : port;
	if (PortHasListener(_mcp.port)) {
		error = fmt::format("port {} already has a listener (another OpenTTD with MCP?); pick a free port", _mcp.port);
		_mcp.last_error = error;
		return false;
	}

	/* Bind loopback only: construct explicit v4 + v6 loopback addresses rather
	 * than using the all-interfaces default in GetBindAddresses(). */
	_mcp.listeners.clear();
	NetworkAddress v4("127.0.0.1", _mcp.port);
	v4.Listen(SOCK_STREAM, &_mcp.listeners);
	/* The advertised endpoint is 127.0.0.1: without the IPv4 listener a client would reach
	 * whatever else holds that port, so a failed IPv4 bind is a failed start. */
	if (_mcp.listeners.empty()) {
		error = "could not bind 127.0.0.1:" + fmt::format("{}", _mcp.port) + " (in use by another process?)";
		_mcp.last_error = error;
		return false;
	}
	NetworkAddress v6("::1", _mcp.port);
	v6.Listen(SOCK_STREAM, &_mcp.listeners);

	RandomBytesWithFallback(_mcp.token);
	_mcp.token_valid = true;
	_mcp.running = true;
	_mcp.requests_total = 0;
	_mcp.requests_denied = 0;
	Debug(Facility::Net, Severity::Error, "[prismatic-mcp] RESEARCH-ONLY MCP server listening on {} (loopback only)", GetEndpoint());
	Registry::Announce(GetEndpoint(), _mcp.port); // discoverable by any reader of the instance registry
	return true;
}

/**
 * Forget every approval, granted or pending. An approval binds an action to the operator's consent in
 * one session of one world: it must not outlive the server, its token, or the game it was given in.
 * This is the one point where session- and world-bound decision state is dropped: anything else that
 * waits on the old session or world (results still to be observed) is cleared here too.
 */
static void ClearApprovals(detail::ResetReason reason)
{
	const detail::ResetEffect effect = detail::EffectOf(reason);
	const std::string_view why = detail::ReasonText(reason);
	if (effect.clear_approvals && !_approvals.empty()) {
		Debug(Facility::Net, Severity::Error, "[prismatic-mcp] {} approval(s) cleared: {}", _approvals.size(), why);
		_approvals.clear();
	}
	/* Commands already posted still run in the same world after a stop or a token rotation, and
	 * their results still arrive. Only when the world itself is gone will they never come: the
	 * decisions that wait for them are closed as INVALIDATED, so none waits forever. */
	if (!effect.invalidate_awaiting || _awaiting.empty()) return;
	size_t n = 0;
	for (uint64_t id : _awaiting) {
		ProvenanceRecord *p = FindDecision(id);
		if (p == nullptr || !detail::ClosedByReset(p->verdict)) continue;
		p->verdict = "INVALIDATED";
		p->command_result = "unobserved";
		p->verification = "none";
		p->error = std::string(why);
		p->callbacks_left = 0;
		p->observed_tick = (uint64_t)TimerGameTick::counter;
		n++;
	}
	_awaiting.clear();
	Debug(Facility::Net, Severity::Error, "[prismatic-mcp] {} decision(s) invalidated: {}", n, why);
}

void Stop()
{
	if (!_mcp.running && _mcp.listeners.empty()) return;
	Registry::Withdraw();
	for (auto &c : _mcp.conns) CloseConnection(c);
	_mcp.conns.clear();
	for (auto &pair : _mcp.listeners) closesocket(pair.first);
	_mcp.listeners.clear();
	_mcp.token.fill(0);
	_mcp.token_valid = false;
	_mcp.running = false;
	ClearApprovals(detail::ResetReason::ServerStopped);
	Debug(Facility::Net, Severity::Error, "[prismatic-mcp] MCP server stopped");
}

void Shutdown()
{
	Stop();
}

void OnGameModeChanged()
{
	ClearApprovals(detail::ResetReason::GameModeChanged);
}

/** The tick the last poll saw; a smaller one means another world (detail::WorldChanged). */
static uint64_t _last_poll_tick = 0;

void Poll()
{
	if (!_mcp.running || _mcp.listeners.empty()) return;
	const uint64_t now_tick = TimerGameTick::counter;
	if (detail::WorldChanged(_last_poll_tick, now_tick)) ClearApprovals(detail::ResetReason::WorldReplaced);
	_last_poll_tick = now_tick;

	fd_set read_fds;
	FD_ZERO(&read_fds);
	SOCKET max_fd = 0;
	for (auto &pair : _mcp.listeners) {
		FD_SET(pair.first, &read_fds);
		if (pair.first > max_fd) max_fd = pair.first;
	}
	for (auto &c : _mcp.conns) {
		if (c.sock == INVALID_SOCKET) continue;
		FD_SET(c.sock, &read_fds);
		if (c.sock > max_fd) max_fd = c.sock;
	}

	timeval tv = {0, 0}; // never block the game loop
	if (select(max_fd + 1, &read_fds, nullptr, nullptr, &tv) <= 0) {
		/* Still service any connections that already have buffered work. */
	}

	/* Accept new connections. */
	for (auto &pair : _mcp.listeners) {
		if (!FD_ISSET(pair.first, &read_fds)) continue;
		for (;;) {
			sockaddr_storage ss;
			socklen_t slen = sizeof(ss);
			SOCKET s = accept(pair.first, (sockaddr *)&ss, &slen);
			if (s == INVALID_SOCKET) break;
			if (_mcp.conns.size() >= MAX_CONNECTIONS) { closesocket(s); break; }
			SetNonBlocking(s);
			SetNoDelay(s);
			Connection c;
			c.sock = s;
			_mcp.conns.push_back(std::move(c));
		}
	}

	/* Service existing connections. */
	for (auto &c : _mcp.conns) ServiceConnection(c);

	/* Reap closed connections. */
	_mcp.conns.erase(
		std::remove_if(_mcp.conns.begin(), _mcp.conns.end(), [](const Connection &c) { return c.sock == INVALID_SOCKET; }),
		_mcp.conns.end());
}

bool IsRunning() { return _mcp.running; }

std::string GetEndpoint()
{
	if (!_mcp.running) return "";
	return "http://127.0.0.1:" + fmt::format("{}", _mcp.port) + "/mcp";
}

std::string GetToken()
{
	if (!_mcp.token_valid) return "";
	return ToHex(_mcp.token);
}

void RotateToken()
{
	if (!_mcp.running) return;
	RandomBytesWithFallback(_mcp.token);
	_mcp.token_valid = true;
	/* No session table yet; rotation invalidates the old bearer value and what was approved under it. */
	ClearApprovals(detail::ResetReason::TokenRotated);
	Debug(Facility::Net, Severity::Error, "[prismatic-mcp] bearer token rotated");
}

void OnCommandResult(Commands cmd, const CommandCost &result)
{
	for (auto it = _awaiting.begin(); it != _awaiting.end(); ++it) {
		ProvenanceRecord *p = FindDecision(*it);
		if (p == nullptr || p->cmd != cmd) continue;
		_awaiting.erase(it);
		p->cost += static_cast<int64_t>(result.GetCost());
		if (result.Failed()) {
			p->failed = true;
			std::string e = cmds::ErrorText(result);
			p->error = p->error.empty() ? e : p->error + "; " + e;
		}
		if (p->callbacks_left > 0) p->callbacks_left--;
		if (p->callbacks_left == 0) Conclude(*p);
		return;
	}
	Debug(Facility::Net, Severity::Error, "[prismatic-mcp] executed result for command {} that no decision awaits", to_underlying(cmd));
}


std::string ListApprovals()
{
	if (_approvals.empty()) return "no approvals";
	uint64_t now = (uint64_t)TimerGameTick::counter;
	std::string s;
	for (const auto &a : _approvals) {
		const char *st = a.state == ApprovalState::Pending ? "PENDING" :
			a.state == ApprovalState::Approved ? "APPROVED" :
			a.state == ApprovalState::Denied ? "DENIED" : "CONSUMED";
		bool expired = now > a.expiry_tick && a.state == ApprovalState::Pending;
		s += fmt::format("#{} {} {}{} tool={} args={}\n", a.id, st, expired ? "(EXPIRED) " : "",
			"", a.tool, a.args_summary);
	}
	return s;
}

bool ApproveById(uint64_t id, std::string &message)
{
	PendingApproval *a = FindApproval(id);
	if (a == nullptr) { message = fmt::format("no approval #{}", id); return false; }
	uint64_t now = (uint64_t)TimerGameTick::counter;
	if (a->state != ApprovalState::Pending) { message = fmt::format("approval #{} is not pending", id); return false; }
	if (now > a->expiry_tick) { message = fmt::format("approval #{} expired", id); return false; }
	a->state = ApprovalState::Approved;
	message = fmt::format("approved #{} ({}) -- resubmit the tool call to execute", id, a->tool);
	Debug(Facility::Net, Severity::Error, "[prismatic-mcp] operator approved #{} ({})", id, a->tool);
	return true;
}

bool DenyById(uint64_t id, std::string &message)
{
	PendingApproval *a = FindApproval(id);
	if (a == nullptr) { message = fmt::format("no approval #{}", id); return false; }
	a->state = ApprovalState::Denied;
	message = fmt::format("denied #{}", id);
	return true;
}

std::string DecisionTrace(int max_rows)
{
	if (_provenance.empty()) return "no decisions executed";
	std::string s;
	int start = (int)_provenance.size() - max_rows;
	if (start < 0) start = 0;
	for (size_t i = (size_t)start; i < _provenance.size(); i++) {
		const auto &p = _provenance[i];
		s += fmt::format("decision #{} approval #{} {} result={} verdict={} req_tick={} obs_tick={}\n",
			p.decision_id, p.approval_id, p.action_type, p.command_result, p.verdict,
			p.requested_tick, p.observed_tick);
	}
	return s;
}

std::string GetStatusLine()
{
	std::string s = "prismatic_mcp";
	s += _mcp.running ? " running" : " stopped";
	if (_mcp.running) {
		s += " endpoint=" + GetEndpoint();
		s += " read_only=true";
		s += " requests=" + fmt::format("{}", _mcp.requests_total);
		s += " denied=" + fmt::format("{}", _mcp.requests_denied);
		s += " conns=" + fmt::format("{}", _mcp.conns.size());
	}
	return s;
}

json Dispatch(const json &request)
{
	auto reply = DispatchOne(request);
	return reply.has_value() ? *reply : json(nullptr);
}

} // namespace PrismaticMCP

#endif /* OTTD_PRISMATIC_MCP_SERVER */
