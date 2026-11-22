/*
 * Part of korczis's PRIVATE RESEARCH FORK of OpenTTD. RESEARCH-ONLY / UNSTABLE.
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 */

/**
 * @file prismatic_mcp_commands.cpp The command catalogue, derived from the command table.
 *
 * One table entry per value of the Commands enum, generated at compile time the same way
 * network_command.cpp generates its dispatch table, so a command added to the game appears
 * here without an edit. Nothing about a command is written by hand: its name, category and
 * flags come from its CommandTraits, its parameters' schemas from the types of its Cmd*
 * function. Posting goes through Command<>::Post, the same network-safe, deterministic path
 * a player's click takes; testing goes through Command<>::Do without DoCommandFlag::Execute.
 */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_commands.h"
#include "prismatic_mcp_params.hpp"
#include "prismatic_mcp_text.h"

#include "../core/backup_type.hpp"
#include "../command_func.h"
#include "../company_func.h"
#include "../strings_func.h"

/* Every header that declares command traits, as network_command.cpp includes them. */
#include "../airport_cmd.h"
#include "../aircraft_cmd.h"
#include "../autoreplace_cmd.h"
#include "../company_cmd.h"
#include "../depot_cmd.h"
#include "../dock_cmd.h"
#include "../economy_cmd.h"
#include "../engine_cmd.h"
#include "../goal_cmd.h"
#include "../group_cmd.h"
#include "../industry_cmd.h"
#include "../landscape_cmd.h"
#include "../league_cmd.h"
#include "../misc_cmd.h"
#include "../news_cmd.h"
#include "../object_cmd.h"
#include "../order_cmd.h"
#include "../rail_cmd.h"
#include "../road_cmd.h"
#include "../roadveh_cmd.h"
#include "../settings_cmd.h"
#include "../signs_cmd.h"
#include "../station_cmd.h"
#include "../story_cmd.h"
#include "../subsidy_cmd.h"
#include "../terraform_cmd.h"
#include "../timetable_cmd.h"
#include "../town_cmd.h"
#include "../train_cmd.h"
#include "../tree_cmd.h"
#include "../tunnelbridge_cmd.h"
#include "../vehicle_cmd.h"
#include "../viewport_cmd.h"
#include "../water_cmd.h"
#include "../waypoint_cmd.h"
#include "../script/script_cmd.h"

#include <array>
#include <optional>

#include "../safeguards.h"

namespace PrismaticMCP::cmds {

namespace {

/** One catalogue entry, generated per command. */
struct Entry {
	std::string_view function;  ///< The Cmd* function name, from CommandTraits::name.
	CommandType type;
	CommandFlags flags;
	bool exposable;
	bool location;
	json (*schemas)();
	json (*unsupported)();
	TestResult (*test)(const Actor &, const json &);
	bool (*post)(const Actor &, const json &, TileIndex, std::string &);
};

template <typename T>
CommandCost ExtractCost(const T &res)
{
	if constexpr (std::is_same_v<T, CommandCost>) {
		return res;
	} else {
		return std::get<0>(res);
	}
}

template <Commands Tcmd>
TestResult TestImpl(const Actor &actor, const json &params)
{
	using Args = typename CommandTraits<Tcmd>::Args;
	TestResult r;
	if constexpr (!params::TupleSupported<Args>()) {
		r.error = "command has parameters that cannot be expressed in JSON: " + params::UnsupportedTypes<Args>().dump();
		return r;
	} else {
		Args args{};
		if (!params::TupleFromJson(params, args, r.error)) return r;
		r.params_ok = true;
		std::optional<Backup<CompanyID>> cur;
		if (!actor.keep_current) cur.emplace(_current_company, actor.company);
		/* Test phase only: no DoCommandFlag::Execute, so nothing is changed. */
		auto res = std::apply([](auto &...a) {
			return Command<Tcmd>::Do(CommandFlagsToDCFlags(GetCommandFlags<Tcmd>()), a...);
		}, args);
		if (cur) cur->Restore();
		CommandCost cost = ExtractCost(res);
		r.would_succeed = cost.Succeeded();
		r.cost = static_cast<int64_t>(cost.GetCost());
		if (cost.Failed()) r.error = ErrorText(cost);
		return r;
	}
}

template <Commands Tcmd>
bool PostImpl(const Actor &actor, const json &params, TileIndex location, std::string &error)
{
	using Args = typename CommandTraits<Tcmd>::Args;
	if constexpr (!params::TupleSupported<Args>()) {
		error = "command has parameters that cannot be expressed in JSON: " + params::UnsupportedTypes<Args>().dump();
		return false;
	} else {
		Args args{};
		if (!params::TupleFromJson(params, args, error)) return false;
		std::optional<Backup<CompanyID>> cur;
		if (!actor.keep_current) cur.emplace(_current_company, actor.company);
		bool ok;
		if constexpr (GetCommandFlags<Tcmd>().Test(CommandFlag::Location)) {
			ok = std::apply([location](auto &...a) {
				return Command<Tcmd>::Post(StringID{}, &CcPrismaticMcp, location, a...);
			}, args);
		} else {
			ok = std::apply([](auto &...a) {
				return Command<Tcmd>::Post(StringID{}, &CcPrismaticMcp, a...);
			}, args);
		}
		if (cur) cur->Restore();
		return ok;
	}
}

template <Commands Tcmd>
json SchemasImpl()
{
	return params::TupleSchemas<typename CommandTraits<Tcmd>::Args>();
}

template <Commands Tcmd>
json UnsupportedImpl()
{
	return params::UnsupportedTypes<typename CommandTraits<Tcmd>::Args>();
}

template <Commands Tcmd>
constexpr Entry MakeEntry()
{
	using Traits = CommandTraits<Tcmd>;
	return Entry{
		Traits::name, Traits::type, Traits::flags,
		params::TupleSupported<typename Traits::Args>(),
		Traits::flags.Test(CommandFlag::Location),
		&SchemasImpl<Tcmd>, &UnsupportedImpl<Tcmd>, &TestImpl<Tcmd>, &PostImpl<Tcmd>,
	};
}

template <typename T, T... i>
auto MakeTable(std::integer_sequence<T, i...>)
{
	return std::array<Entry, sizeof...(i)>{{ MakeEntry<static_cast<Commands>(i)>()... }};
}

const auto _table = MakeTable(std::make_integer_sequence<std::underlying_type_t<Commands>, to_underlying(Commands::End)>{});

const Entry *Get(Commands cmd)
{
	size_t i = to_underlying(cmd);
	return i < _table.size() ? &_table[i] : nullptr;
}

std::string_view CategoryName(CommandType t)
{
	switch (t) {
		case CommandType::LandscapeConstruction: return "landscape_construction";
		case CommandType::VehicleConstruction: return "vehicle_construction";
		case CommandType::MoneyManagement: return "money_management";
		case CommandType::VehicleManagement: return "vehicle_management";
		case CommandType::RouteManagement: return "route_management";
		case CommandType::OtherManagement: return "other_management";
		case CommandType::CompanySetting: return "company_setting";
		case CommandType::ServerSetting: return "server_setting";
		case CommandType::Cheat: return "cheat";
		default: return "unknown";
	}
}

json FlagNames(CommandFlags f)
{
	static constexpr std::pair<CommandFlag, std::string_view> names[] = {
		{CommandFlag::Server, "server"}, {CommandFlag::Spectator, "spectator"},
		{CommandFlag::Offline, "offline"}, {CommandFlag::Auto, "auto"},
		{CommandFlag::AllTiles, "all_tiles"}, {CommandFlag::NoTest, "no_test"},
		{CommandFlag::NoWater, "no_water"}, {CommandFlag::ClientID, "client_id"},
		{CommandFlag::Deity, "deity"}, {CommandFlag::StrCtrl, "str_ctrl"},
		{CommandFlag::NoEst, "no_estimate"}, {CommandFlag::Location, "location"},
	};
	json arr = json::array();
	for (const auto &[flag, name] : names) {
		if (f.Test(flag)) arr.push_back(std::string(name));
	}
	return arr;
}

} // namespace

size_t Count() { return _table.size(); }

std::string_view ShortName(Commands cmd)
{
	const Entry *e = Get(cmd);
	if (e == nullptr) return {};
	std::string_view n = e->function;
	if (n.starts_with("Cmd")) n.remove_prefix(3);
	return n;
}

std::optional<Commands> Find(std::string_view short_name)
{
	for (size_t i = 0; i < _table.size(); i++) {
		if (ShortName(static_cast<Commands>(i)) == short_name) return static_cast<Commands>(i);
	}
	return std::nullopt;
}

bool IsExposable(Commands cmd)
{
	const Entry *e = Get(cmd);
	return e != nullptr && e->exposable;
}

bool NeedsLocation(Commands cmd)
{
	const Entry *e = Get(cmd);
	return e != nullptr && e->location;
}

json Describe(Commands cmd)
{
	const Entry *e = Get(cmd);
	if (e == nullptr) return json::object();
	json d = {
		{"name", std::string(ShortName(cmd))},
		{"function", std::string(e->function)},
		{"id", to_underlying(cmd)},
		{"category", std::string(CategoryName(e->type))},
		{"flags", FlagNames(e->flags)},
		{"allowed_while_paused", IsCommandAllowedWhilePaused(cmd)},
		{"location_required", e->location},
		{"exposable", e->exposable},
		{"params", e->schemas()},
	};
	if (!e->exposable) d["unsupported_types"] = e->unsupported();
	return d;
}

json ToolInputSchema(Commands cmd)
{
	const Entry *e = Get(cmd);
	if (e == nullptr) return json{{"type", "object"}};
	json params = e->schemas();
	json props = {
		{"company", {
			{"description", "Who the command is posted as: a company id (0-14), \"deity\" for game-script "
			 "authority, or \"server\" (default) for the server's own context (server-only commands)."},
			{"anyOf", json::array({{{"type", "integer"}, {"minimum", 0}, {"maximum", 254}},
				{{"type", "string"}, {"enum", json::array({"server", "deity"})}}})},
		}},
		{"params", {
			{"description", "The command's parameters, positionally, in the order of its Cmd* function."},
			{"type", "array"},
			{"prefixItems", params},
			{"minItems", params.size()},
			{"maxItems", params.size()},
		}},
	};
	json required = json::array({"params"});
	if (e->location) {
		props["location"] = {{"type", "integer"}, {"minimum", 0}, {"description", "tile index for the command's location (y * map_width + x)"}};
		required.push_back("location");
	}
	return json{{"type", "object"}, {"properties", props}, {"required", required}};
}

TestResult Test(Commands cmd, const Actor &actor, const json &params)
{
	const Entry *e = Get(cmd);
	if (e == nullptr) return TestResult{false, false, 0, "unknown command"};
	return e->test(actor, params);
}

bool Post(Commands cmd, const Actor &actor, const json &params, TileIndex location, std::string &error)
{
	const Entry *e = Get(cmd);
	if (e == nullptr) { error = "unknown command"; return false; }
	return e->post(actor, params, location, error);
}

std::string ErrorText(const CommandCost &cost)
{
	CommandCost c = cost;
	try {
		if (!c.GetEncodedMessage().empty()) return PlainText(c.GetEncodedMessage().GetDecodedString());
		StringID id = c.GetErrorMessage();
		if (id == INVALID_STRING_ID) return "the command failed without an error message (CMD_ERROR: invalid parameters)";
		return PlainText(GetString(id));
	} catch (const std::exception &) {
		return fmt::format("string #{}", c.GetErrorMessage());
	}
}

} // namespace PrismaticMCP::cmds

void CcPrismaticMcp(Commands cmd, const CommandCost &result, const CommandDataBuffer &, CommandDataBuffer)
{
	PrismaticMCP::OnCommandResult(cmd, result);
}

#endif /* OTTD_PRISMATIC_MCP_SERVER */
