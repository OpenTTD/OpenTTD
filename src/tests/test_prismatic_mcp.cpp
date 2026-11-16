/*
 * Part of korczis's PRIVATE RESEARCH FORK of OpenTTD. RESEARCH-ONLY.
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 */

/** @file test_prismatic_mcp.cpp Unit tests for the pure MCP security/logic helpers. */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/catch2/catch.hpp"
#include "../prismatic_mcp/prismatic_mcp_detail.h"
#include "../prismatic_mcp/prismatic_mcp_menu.h"
#include "../prismatic_mcp/prismatic_mcp_ui.h"
#include "../prismatic_mcp/prismatic_mcp_console.h"
#include "../prismatic_mcp/prismatic_mcp_view.h"
#include "../network/network.h"
#include "../prismatic_mcp/prismatic_mcp_world.h"
#include "../prismatic_mcp/prismatic_mcp_entities.h"
#include "../prismatic_mcp/prismatic_mcp_graph.h"
#include "../economy_type.h"
#include "../map_func.h"
#include "../tile_map.h"

#include <set>
#include "../prismatic_mcp/prismatic_mcp_params.hpp"
#include "../tile_type.h"

#include "../safeguards.h"

using namespace PrismaticMCP::detail;
using json = nlohmann::json;

TEST_CASE("ArgDigest is deterministic and argument-binding", "[prismatic_mcp]")
{
	/* Identical input -> identical digest (approval can match a re-submitted call). */
	CHECK(ArgDigestHex("game.set_pause", "{\"pause\":true}") == ArgDigestHex("game.set_pause", "{\"pause\":true}"));

	/* Different arguments -> different digest: this is what defeats the
	 * "change arguments after approval" abuse case. */
	CHECK(ArgDigestHex("game.set_pause", "{\"pause\":true}") != ArgDigestHex("game.set_pause", "{\"pause\":false}"));

	/* Different tool -> different digest. */
	CHECK(ArgDigestHex("game.set_pause", "{}") != ArgDigestHex("game.save", "{}"));

	/* 16 lowercase hex chars. */
	std::string d = ArgDigestHex("x", "y");
	CHECK(d.size() == 16);
	CHECK(d.find_first_not_of("0123456789abcdef") == std::string::npos);
}

TEST_CASE("IsKnownActionType gates the typed catalogue", "[prismatic_mcp]")
{
	CHECK(IsKnownActionType("game.set_pause"));
	CHECK_FALSE(IsKnownActionType("game.delete_everything"));
	CHECK_FALSE(IsKnownActionType(""));
	CHECK_FALSE(IsKnownActionType("rcon"));
	for (std::string_view t : ACTION_TYPES) CHECK(IsKnownActionType(t)); // the table is the catalogue
	/* A game command by its Cmd* name; existence is checked against the command table later. */
	CHECK(IsKnownActionType("command.StartStopVehicle"));
	CHECK_FALSE(IsKnownActionType("command."));
	CHECK_FALSE(IsKnownActionType("command.Start Stop"));
	CHECK_FALSE(IsKnownActionType("command.../x"));
}

namespace {
enum class TestEnum : uint8_t { A = 0, B = 7 };
struct NotConvertible { int a; int b; NotConvertible() = delete; };
}

TEST_CASE("Command parameters convert from JSON by their declared type", "[prismatic_mcp]")
{
	using namespace PrismaticMCP::params;
	std::string err;

	bool b = false;
	CHECK(FromJson(json(true), b, err));
	CHECK(b);
	CHECK_FALSE(FromJson(json(1), b, err));

	uint8_t u8 = 0;
	CHECK(FromJson(json(255), u8, err));
	CHECK(u8 == 255);
	CHECK_FALSE(FromJson(json(256), u8, err));
	CHECK_FALSE(FromJson(json(-1), u8, err));
	CHECK_FALSE(FromJson(json("3"), u8, err));

	int16_t i16 = 0;
	CHECK(FromJson(json(-32768), i16, err));
	CHECK_FALSE(FromJson(json(40000), i16, err));

	TestEnum e = TestEnum::A;
	CHECK(FromJson(json(7), e, err));
	CHECK(e == TestEnum::B);

	std::string str;
	CHECK(FromJson(json("depot"), str, err));
	CHECK(str == "depot");

	/* A strong type is carried as its base integer. */
	TileIndex tile{};
	CHECK(FromJson(json(1234), tile, err));
	CHECK(tile.base() == 1234);
	CHECK(Schema<TileIndex>()["type"] == "integer");

	/* An unsupported type is reported, never guessed at. */
	CHECK_FALSE(IsSupported<NotConvertible>());
	CHECK(IsSupported<TileIndex>());
	CHECK(IsSupported<TestEnum>());
}

TEST_CASE("A text, a vehicle list and a cargo source convert from their JSON spelling", "[prismatic_mcp]")
{
	using namespace PrismaticMCP::params;
	std::string err;

	/* A text is a plain string, carried as the game's raw-string message, bounded. */
	CHECK(IsSupported<EncodedString>());
	CHECK(Schema<EncodedString>()["type"] == "string");
	EncodedString text;
	CHECK(FromJson(json("Deliver 500 t of coal"), text, err));
	CHECK_FALSE(text.empty());
	CHECK_FALSE(FromJson(json(5), text, err));
	CHECK_FALSE(FromJson(json(std::string(MAX_TEXT_BYTES + 1, 'x')), text, err));

	/* A vehicle list: every field, each in its range, nothing else. */
	CHECK(IsSupported<VehicleListIdentifier>());
	CHECK(Schema<VehicleListIdentifier>()["required"].size() == 4);
	VehicleListIdentifier vli;
	CHECK(FromJson(json{{"type", 4}, {"vtype", 1}, {"company", 0}, {"index", 7}}, vli, err));
	CHECK(vli.type == VehicleListType::Group);
	CHECK(vli.vtype == VehicleType::Road);
	CHECK(vli.index == 7);
	CHECK_FALSE(FromJson(json{{"type", 5}, {"vtype", 1}, {"company", 0}, {"index", 7}}, vli, err)); // past End
	CHECK_FALSE(FromJson(json{{"type", 0}, {"vtype", 4}, {"company", 0}, {"index", 0}}, vli, err)); // an effect vehicle
	CHECK_FALSE(FromJson(json{{"type", 0}, {"vtype", 0}, {"company", 0}}, vli, err));             // no index
	CHECK_FALSE(FromJson(json{{"type", 0}, {"vtype", 0}, {"company", 0}, {"index", 0}, {"x", 1}}, vli, err));
	CHECK_FALSE(FromJson(json(3), vli, err));

	/* A cargo source: an id and its kind. */
	CHECK(IsSupported<Source>());
	Source src;
	CHECK(FromJson(json{{"id", 12}, {"type", 1}}, src, err));
	CHECK(src.id == 12);
	CHECK(src.type == SourceType::Town);
	CHECK_FALSE(FromJson(json{{"id", 12}, {"type", 3}}, src, err));
	CHECK_FALSE(FromJson(json{{"id", 70000}, {"type", 0}}, src, err));
	CHECK_FALSE(FromJson(json{{"id", 1}}, src, err));
}

TEST_CASE("A parameter tuple is converted positionally and checked for arity", "[prismatic_mcp]")
{
	using namespace PrismaticMCP::params;
	std::string err;
	std::tuple<TileIndex, bool, uint8_t> t{};
	CHECK(TupleFromJson(json::array({10, true, 3}), t, err));
	CHECK(std::get<0>(t).base() == 10);
	CHECK(std::get<1>(t));
	CHECK(std::get<2>(t) == 3);
	CHECK_FALSE(TupleFromJson(json::array({10, true}), t, err));
	CHECK(err.find("expected 3 params") != std::string::npos);
	CHECK_FALSE(TupleFromJson(json::array({10, 1, 3}), t, err));
	CHECK(err.find("params[1]") != std::string::npos);
	CHECK(TupleSchemas<decltype(t)>().size() == 3);
	CHECK((TupleSupported<std::tuple<TileIndex, bool>>()));
}

TEST_CASE("A reset drops approvals always and waiting decisions only with the world", "[prismatic_mcp]")
{
	/* Every reset drops every approval: consent binds one session of one world. */
	for (auto r : {ResetReason::ServerStopped, ResetReason::TokenRotated, ResetReason::GameModeChanged, ResetReason::WorldReplaced}) {
		CHECK(EffectOf(r).clear_approvals);
		CHECK_FALSE(ReasonText(r).empty());
	}
	/* Posted commands still run in the same world after a stop or a token rotation: keep waiting. */
	CHECK_FALSE(EffectOf(ResetReason::ServerStopped).invalidate_awaiting);
	CHECK_FALSE(EffectOf(ResetReason::TokenRotated).invalidate_awaiting);
	/* The world is gone: their results will never come. */
	CHECK(EffectOf(ResetReason::GameModeChanged).invalidate_awaiting);
	CHECK(EffectOf(ResetReason::WorldReplaced).invalidate_awaiting);
	/* Only a decision still waiting is closed; a verdict already taken stands. */
	CHECK(ClosedByReset("PENDING_OBSERVATION"));
	CHECK_FALSE(ClosedByReset("VERIFIED_SUCCESS"));
	CHECK_FALSE(ClosedByReset("VERIFIED_FAILURE"));
	CHECK_FALSE(ClosedByReset("INVALIDATED"));
}

TEST_CASE("ParseContentLength is case-insensitive and bounded", "[prismatic_mcp]")
{
	CHECK(ParseContentLength("POST /mcp HTTP/1.1\r\nContent-Length: 42\r\n\r\n") == 42u);
	CHECK(ParseContentLength("content-length:7\r\n") == 7u);          // lowercase, no space
	CHECK(ParseContentLength("CONTENT-LENGTH:  128\r\n") == 128u);    // uppercase, extra spaces
	CHECK_FALSE(ParseContentLength("Host: localhost\r\n").has_value()); // absent
	CHECK_FALSE(ParseContentLength("Content-Length: \r\n").has_value()); // present but empty
	CHECK(ParseContentLength("Content-Length: 0\r\n") == 0u);
}

TEST_CASE("ExtractBearer only accepts the Bearer scheme", "[prismatic_mcp]")
{
	CHECK(ExtractBearer("Bearer abc123") == "abc123");
	CHECK(ExtractBearer("bearer abc123") == "abc123");   // case-insensitive scheme
	CHECK(ExtractBearer("Bearer   spaced  ") == "spaced"); // OWS trimmed
	CHECK(ExtractBearer("Basic abc123").empty());         // wrong scheme
	CHECK(ExtractBearer("abc123").empty());               // no scheme
	CHECK(ExtractBearer("").empty());
}

TEST_CASE("ConstantTimeEquals matches semantics of ==", "[prismatic_mcp]")
{
	CHECK(ConstantTimeEquals("deadbeef", "deadbeef"));
	CHECK_FALSE(ConstantTimeEquals("deadbeef", "deadbee0"));
	CHECK_FALSE(ConstantTimeEquals("short", "longer"));    // length mismatch
	CHECK(ConstantTimeEquals("", ""));
}

TEST_CASE("Only a tick counter that went back means another world", "[prismatic_mcp]")
{
	using PrismaticMCP::detail::WorldChanged;
	CHECK_FALSE(WorldChanged(0, 0));
	CHECK_FALSE(WorldChanged(100, 100));
	CHECK_FALSE(WorldChanged(100, 250));
	CHECK(WorldChanged(250, 3));   // a new game or a load restarted the counter
}

TEST_CASE("The toolbar menu is one table, described item for item", "[prismatic_mcp]")
{
	namespace M = PrismaticMCP::Menu;
	auto d = M::Describe();
	REQUIRE(d["items"].size() == M::Items().size());
	for (size_t i = 0; i < M::Items().size(); i++) {
		CHECK(d["items"][i]["index"] == i);
		CHECK(d["items"][i]["label"] == std::string{M::Items()[i].label});
		CHECK(!M::Items()[i].label.empty());
		if (M::Items()[i].action == M::Action::Toggle) CHECK(!M::Items()[i].key.empty());
		if (M::Items()[i].action == M::Action::Request) CHECK(!M::Items()[i].arg.empty());
	}
	CHECK(d["hub"].get<std::string>().rfind("http", 0) == 0);
}

TEST_CASE("A request is queued, listed with its kind and answered once", "[prismatic_mcp]")
{
	namespace M = PrismaticMCP::Menu;
	size_t before = M::Requests()["requests"].size();
	uint64_t id = M::Queue("serve_here", "");
	auto list = M::Requests()["requests"];
	REQUIRE(list.size() == before + 1);
	CHECK(list.back()["id"] == id);
	CHECK(list.back()["kind"] == "serve_here");
	CHECK(list.back()["answered"] == false);
	CHECK(list.back()["tile"].is_null()); // no main window in a unit test
	CHECK(M::Call("openttd.ui.requests.answer", {{"id", id}})->value("ok", true) == false); // no answer text
	CHECK(M::Call("openttd.ui.requests.answer", {{"id", id + 1000}, {"answer", "x"}})->value("ok", true) == false);
	CHECK(M::Call("openttd.ui.menu.run", {{"index", -1}})->value("ok", true) == false);
	CHECK_FALSE(M::Call("openttd.ui.something.else", json::object()).has_value());
	for (const auto &t : M::Tools()) CHECK(t["annotations"]["readOnlyHint"] == false); // a coverage probe must not run them
}

/* ---- the catalogue as console commands (prismatic_mcp_console.cpp) ---------------------------- */

namespace {
using njson = nlohmann::json;
namespace mc = PrismaticMCP::Console::detail;

const std::vector<std::string> NAMES = {"openttd.view.zoom", "openttd.view.state", "openttd.ui.windows",
	"openttd.input.pointer", "openttd.decision.submit", "openttd.game.set_pause"};

njson ZoomSchema()
{
	return {{"type", "object"}, {"properties", {{"level", {{"type", "integer"}}}}}, {"required", {"level"}}};
}

njson Args(const njson &schema, std::vector<std::string_view> tokens, std::vector<std::string> &errors)
{
	return mc::ParseArgs(schema, tokens, errors);
}
} // namespace

TEST_CASE("Console names resolve exactly, without the prefix or by a unique suffix", "[prismatic_mcp]")
{
	CHECK(mc::Resolve(NAMES, "openttd.view.zoom") == std::vector<std::string>{"openttd.view.zoom"});
	CHECK(mc::Resolve(NAMES, "view.zoom") == std::vector<std::string>{"openttd.view.zoom"});
	CHECK(mc::Resolve(NAMES, "zoom") == std::vector<std::string>{"openttd.view.zoom"});
	CHECK(mc::Resolve(NAMES, "set_pause") == std::vector<std::string>{"openttd.game.set_pause"});
	CHECK(mc::Resolve({"openttd://company/finances", "openttd://map"}, "map") == std::vector<std::string>{"openttd://map"});
	/* Two suffix matches: candidates, never a guess. */
	CHECK(mc::Resolve({"openttd.a.state", "openttd.b.state"}, "state").size() == 2);
	/* A lone substring match is offered, not run: the second entry is the empty marker. */
	auto sub = mc::Resolve(NAMES, "windo");
	REQUIRE(sub.size() == 2);
	CHECK(sub[0] == "openttd.ui.windows");
	CHECK(sub[1].empty());
	CHECK(mc::Resolve(NAMES, "nothing").empty());
	CHECK(mc::Resolve(NAMES, "").empty());
}

TEST_CASE("Console values are typed by the schema property", "[prismatic_mcp]")
{
	std::string e;
	njson b = {{"type", "boolean"}};
	for (auto t : {"true", "1", "yes", "on"}) { CHECK(mc::ParseValue(b, t, e) == true); CHECK(e.empty()); }
	for (auto f : {"false", "0", "no", "off"}) CHECK(mc::ParseValue(b, f, e) == false);
	mc::ParseValue(b, "maybe", e);
	CHECK_FALSE(e.empty());

	njson i = {{"type", "integer"}};
	CHECK(mc::ParseValue(i, "-42", e) == -42);
	mc::ParseValue(i, "1.5", e);
	CHECK_FALSE(e.empty());
	mc::ParseValue(i, "12abc", e);
	CHECK_FALSE(e.empty());

	njson nullable = {{"type", {"integer", "null"}}};
	CHECK(mc::ParseValue(nullable, "null", e).is_null());
	CHECK(e.empty());
	CHECK(mc::ParseValue(nullable, "7", e) == 7);

	CHECK(mc::ParseValue({{"type", "number"}}, "2.5", e) == 2.5);
	CHECK(mc::ParseValue({{"type", "string"}}, "a b", e) == "a b");
	CHECK(mc::ParseValue({{"type", "array"}, {"items", {{"type", "integer"}}}}, "1,2,3", e) == njson::array({1, 2, 3}));
	mc::ParseValue({{"type", "array"}, {"items", {{"type", "integer"}}}}, "1,x", e);
	CHECK_FALSE(e.empty());
	CHECK(mc::ParseValue({{"type", "array"}}, "[1,\"a\"]", e) == njson::array({1, "a"}));
	CHECK(mc::ParseValue({{"type", "object"}}, "{\"k\":1}", e) == njson{{"k", 1}});

	njson en = {{"type", "string"}, {"enum", {"rail", "road"}}};
	CHECK(mc::ParseValue(en, "rail", e) == "rail");
	mc::ParseValue(en, "air", e);
	CHECK_FALSE(e.empty());

	/* No type: JSON when it parses, else the text. */
	CHECK(mc::ParseValue(njson::object(), "3", e) == 3);
	CHECK(mc::ParseValue(njson::object(), "hello", e) == "hello");
}

TEST_CASE("Console arguments: key=value, bare values for required keys, refusals", "[prismatic_mcp]")
{
	std::vector<std::string> errors;
	CHECK(Args(ZoomSchema(), {"level=2"}, errors) == njson{{"level", 2}});
	CHECK(errors.empty());
	CHECK(Args(ZoomSchema(), {"3"}, errors) == njson{{"level", 3}});
	CHECK(errors.empty());

	Args(ZoomSchema(), {}, errors);
	REQUIRE(errors.size() == 1);
	CHECK(errors[0].find("missing required key 'level'") != std::string::npos);

	errors.clear();
	Args(ZoomSchema(), {"level=2", "speed=9"}, errors);
	REQUIRE(errors.size() == 1);
	CHECK(errors[0].find("unknown key 'speed'") != std::string::npos);

	errors.clear();
	Args(ZoomSchema(), {"level=high"}, errors);
	REQUIRE(errors.size() == 2);   // mistyped, and therefore still missing
	CHECK(errors[0].find("level:") == 0);

	errors.clear();
	Args(ZoomSchema(), {"2", "3"}, errors);
	CHECK(errors.size() == 1);   // no required key left for the second bare value

	/* A JSON object token is merged; an open schema accepts any key, typed by JSON. */
	errors.clear();
	njson open = {{"type", "object"}};
	CHECK(Args(open, {"{\"action\":{\"type\":\"game.set_pause\"}}", "note=x"}, errors) ==
		njson{{"action", {{"type", "game.set_pause"}}}, {"note", "x"}});
	CHECK(errors.empty());

	/* anyOf: one complete set of required keys is enough. */
	njson either = {{"type", "object"}, {"properties", {{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}}}},
		{"anyOf", njson::array({njson{{"required", {"tile"}}}, njson{{"required", {"x", "y"}}}})}};
	Args(either, {"x=1", "y=2"}, errors);
	CHECK(errors.empty());
	Args(either, {"tile=5"}, errors);
	CHECK(errors.empty());
	Args(either, {"x=1"}, errors);
	REQUIRE(errors.size() == 1);
	CHECK(errors[0] == "needs tile or x+y");
}

TEST_CASE("Console usage lines and result lines are derived, not written", "[prismatic_mcp]")
{
	CHECK(mc::Usage("openttd.view.zoom", ZoomSchema()) == "mcp openttd.view.zoom level=<integer>");
	njson s = {{"type", "object"}, {"properties", {{"mode", {{"type", "string"}, {"enum", {"a", "b"}}}}, {"pause", {{"type", "boolean"}}}}}, {"required", {"pause"}}};
	CHECK(mc::Usage("t", s) == "mcp t pause=<boolean> [mode=a|b]");
	CHECK(mc::Usage("t", njson{{"type", "object"}}) == "mcp t [{json}]");

	njson result = {{"content", njson::array({{{"type", "text"}, {"text", "{\"ok\":true,\"n\":1}"}}})}};
	CHECK(mc::ResultLines(result, true) == std::vector<std::string>{"{\"n\":1,\"ok\":true}"});
	CHECK(mc::ResultLines(result, false).size() == 4);   // { "n": 1, "ok": true }
	njson plain = {{"contents", njson::array({{{"text", "a\nb"}}})}};
	CHECK(mc::ResultLines(plain, false) == std::vector<std::string>{"a", "b"});
}

TEST_CASE("Console completion is derived from the catalogue, with or without a leading slash", "[prismatic_mcp]")
{
	const json tools = json::array({
		{{"name", "openttd.view.zoom"}, {"inputSchema", {{"type", "object"}, {"properties", {{"level", {{"type", "integer"}}}}}}}},
		{{"name", "openttd.cmd.Pause"}, {"inputSchema", {{"type", "object"}, {"properties", {{"company", json::object()}, {"params", json::object()}}}}}},
	});
	const json resources = json::array({{{"uri", "openttd://game/state"}}, {{"uri", "openttd://commands"}}});
	auto complete = [&](std::vector<std::string_view> words, std::string_view query) { return mc::Complete(tools, resources, words, query); };

	/* After mcp: its subcommands, then every tool without the openttd. prefix. */
	auto all = complete({"mcp"}, "");
	CHECK(std::find(all.begin(), all.end(), "read") != all.end());
	CHECK(std::find(all.begin(), all.end(), "cmd.Pause") != all.end());
	CHECK(complete({"mcp"}, "vi") == std::vector<std::string>{"view.zoom"});
	CHECK(complete({"/mcp"}, "vi") == std::vector<std::string>{"view.zoom"}); // the slash changes nothing
	/* After mcp read, the resources; after mcp help, the tools. */
	CHECK(complete({"mcp", "read"}, "openttd://c") == std::vector<std::string>{"openttd://commands"});
	CHECK(complete({"mcp", "help"}, "cmd") == std::vector<std::string>{"cmd.Pause"});
	/* After a tool, its parameters not given yet, as key=. */
	CHECK(complete({"mcp", "cmd.Pause"}, "") == std::vector<std::string>{"company=", "params="});
	CHECK(complete({"mcp", "cmd.Pause", "company=server"}, "") == std::vector<std::string>{"params="});
	CHECK(complete({"mcp", "zoom"}, "l") == std::vector<std::string>{"level="});
	CHECK(complete({"mcp", "no_such_tool"}, "").empty());
	/* bot and its alias ai: the subcommands. */
	CHECK(complete({"bot"}, "s") == std::vector<std::string>{"status", "set", "start", "stop"});
	CHECK(complete({"/ai"}, "re") == std::vector<std::string>{"reload"});
	/* Anything else is not the fork's to complete. */
	CHECK(complete({"echo"}, "").empty());
	CHECK(complete({}, "mc").empty());
}

TEST_CASE("A dedicated server has no toolbar, so nothing of the menu", "[prismatic_mcp]")
{
	namespace M = PrismaticMCP::Menu;
	const bool was = _network_dedicated;
	_network_dedicated = true;
	CHECK(M::Tools().empty());
	CHECK(M::Call("openttd.ui.menu.run", {{"index", 0}})->value("ok", true) == false);
	CHECK(M::Call("openttd.ui.requests.answer", {{"id", 1}, {"answer", "x"}})->value("ok", true) == false);
	CHECK(M::Describe()["items"].empty());
	CHECK(M::Describe()["dedicated"] == true);
	CHECK(M::Requests()["requests"].empty());
	_network_dedicated = was;
	CHECK_FALSE(M::Tools().empty());
}

TEST_CASE("Every price is named once, in the game's order", "[prismatic_mcp]")
{
	auto p = PrismaticMCP::World::Prices();
	REQUIRE(p["order"].size() == to_underlying(Price::End));
	std::set<std::string> seen;
	for (const auto &n : p["order"]) {
		std::string name = n.get<std::string>();
		CHECK(seen.insert(name).second);
		CHECK(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz_") == std::string::npos);
		CHECK(p["prices"].contains(name));
		CHECK(p["base"].contains(name));
	}
	CHECK(p["order"][to_underlying(Price::BuildRail)] == "build_rail");
	CHECK(p["order"][to_underlying(Price::BuildBridge)] == "build_bridge");
	CHECK(p["order"][to_underlying(Price::BuildTunnel)] == "build_tunnel");
	CHECK(p["order"][to_underlying(Price::ClearTrees)] == "clear_trees");
}

TEST_CASE("The terrain shows a raised corner in the two rows it touches, and nowhere else", "[prismatic_mcp]")
{
	Map::Allocate(64, 64);
	auto flat = PrismaticMCP::World::Terrain();
	REQUIRE(flat["slopes"].size() == 64);
	CHECK(flat["slopes"][10].get<std::string>() == std::string(128, '0'));
	CHECK(flat["ground"][10].get<std::string>() == std::string(64, 'b')); // a zeroed clear tile: grass of density 0
	CHECK(flat["buildable"][10].get<std::string>() == std::string(64, '.'));
	CHECK(flat["vehicles"][10].get<std::string>() == std::string(64, '.'));
	CHECK(flat["gradients"][10].get<std::string>() == std::string(64, '0'));

	SetTileHeight(TileXY(10, 10), 1);
	auto raised = PrismaticMCP::World::Terrain();
	/* The height of tile (10,10) is the north corner of (10,10), the west of (9,10)... : rows 9 and 10 change. */
	for (uint y = 0; y < 64; y++) {
		bool changed = raised["row_digests"][y] != flat["row_digests"][y];
		CHECK(changed == (y == 9 || y == 10));
	}
	CHECK(raised["slopes"][10].get<std::string>().substr(20, 2) != "00");
	CHECK(raised["gradients"][10].get<std::string>()[10] == '1');
	CHECK(raised["gradients"][10].get<std::string>()[11] == '1');
	CHECK(raised["gradients"][10].get<std::string>()[12] == '0');
	CHECK(raised["gradients"][9].get<std::string>()[10] == '1');
}


TEST_CASE("The owner switch auto-answers the bot's question with no dialog", "[prismatic_mcp]")
{
	namespace V = PrismaticMCP::View;
	// Off by default: turning it on with "yes" reports the state.
	auto on = V::Call("openttd.view.ask.auto", {{"answer", "yes"}});
	REQUIRE(on.has_value());
	CHECK((*on)["auto"] == true);
	CHECK((*on)["answer"] == "yes");

	// With it on, a question is answered at once and shows no dialog (no ShowQuery, so safe in a test).
	auto asked = V::Call("openttd.view.ask", {{"question", "build here?"}});
	REQUIRE(asked.has_value());
	CHECK((*asked)["ok"] == true);
	CHECK((*asked)["auto_answered"] == true);
	CHECK((*asked)["answer"] == "yes");

	// Spellings normalise: "off" means the answer "no".
	V::Call("openttd.view.ask.auto", {{"answer", "off"}});
	CHECK((*V::Call("openttd.view.ask", {{"question", "again?"}}))["answer"] == "no");

	// Turning it off clears the switch (a later openttd.view.ask would show the dialog again).
	auto off = V::Call("openttd.view.ask.auto", json::object());
	REQUIRE(off.has_value());
	CHECK((*off)["auto"] == false);
	CHECK((*off)["answer"].is_null());

	// The tool is listed, so it reaches the `mcp` console, Swagger and .ai by derivation.
	bool listed = false;
	for (const auto &t : V::Tools()) if (t["name"] == "openttd.view.ask.auto") listed = true;
	CHECK(listed);
}

TEST_CASE("Every map resource is one row per map row, sized to the map", "[prismatic_mcp]")
{
	Map::Allocate(64, 128); // the smallest width, a taller height: rows follow y
	auto m = PrismaticMCP::World::Map();
	CHECK(m["width"] == 64);
	CHECK(m["height"] == 128);
	REQUIRE(m["types"].size() == 128);
	CHECK(m["types"][5].get<std::string>() == std::string(64, 'C')); // a zeroed tile is clear land
	CHECK(m["heights"][5].get<std::string>() == std::string(128, '0')); // two hex digits per tile
	CHECK(m["owners"][5].get<std::string>().size() == 64);
	for (const char *layer : {"types", "heights", "owners"}) CHECK(m["encoding"].contains(layer));

	auto w = PrismaticMCP::Graph::Ways();
	CHECK(w["schema"] == "map-ways/v1");
	REQUIRE(w["roads"].size() == 128);
	CHECK(w["roads"][0].get<std::string>() == std::string(64, '0'));     // no road anywhere
	CHECK(w["rails"][0].get<std::string>() == std::string(128, '0'));    // no track anywhere
	CHECK(w["foundations"][0].get<std::string>().size() == 64);
	CHECK(w["crossings"].empty());
	CHECK(w["occupancy"].empty());

	auto t = PrismaticMCP::World::Terrain();
	for (const char *layer : {"ground", "gradients", "buildable", "vehicles"}) CHECK(t[layer][127].get<std::string>().size() == 64);
	CHECK(t["slopes"][127].get<std::string>().size() == 128);
	CHECK(t["row_digests"].size() == 128);
}

TEST_CASE("The building and vehicle lists are empty when nothing is built", "[prismatic_mcp]")
{
	Map::Allocate(64, 64);
	CHECK(PrismaticMCP::World::Stations()["stations"].empty());
	auto d = PrismaticMCP::World::Depots();
	CHECK(d["depots"].empty());
	CHECK(d["service_reach_tiles"].is_number());
	CHECK(PrismaticMCP::Entities::Vehicles()["vehicles"].empty());
}

TEST_CASE("Every client tool says whether it changes anything", "[prismatic_mcp]")
{
	/* Clients and the coverage probe decide by readOnlyHint: a tool without it is never probed. */
	std::vector<nlohmann::json> tools;
	for (const auto &t : PrismaticMCP::View::Tools()) tools.push_back(t);
	for (const auto &t : PrismaticMCP::UI::Tools()) tools.push_back(t);
	for (const auto &t : PrismaticMCP::Menu::Tools()) tools.push_back(t);
	REQUIRE_FALSE(tools.empty());
	for (const auto &t : tools) {
		INFO(t["name"].get<std::string>());
		REQUIRE(t.contains("annotations"));
		CHECK(t["annotations"].contains("readOnlyHint"));
		CHECK(t["annotations"]["readOnlyHint"].is_boolean());
	}
}

TEST_CASE("The approval state machine binds to canonical args, executes once, refuses replay and changed args", "[prismatic_mcp]")
{
	/* ClassifySubmit is exactly what the submit handler runs to decide pending/reuse/execute. An
	 * approval binds to the submission's EXACT canonical arguments, not the 64-bit FNV-1a digest
	 * (which could collide). States mirror ApprovalState: 0 Pending, 1 Approved, 3 Consumed. */
	const std::string pause5 = R"({"ticks":5})";
	const std::string pause6 = R"({"ticks":6})";
	size_t idx = 999;

	SECTION("submit with nothing live asks for approval; that pending approval is then reused") {
		std::vector<ApprovalView> a;
		CHECK(ClassifySubmit(a, pause5, 10, idx) == SubmitDecision::CreatePending);
		a.push_back({0, pause5, 100});
		idx = 999;
		CHECK(ClassifySubmit(a, pause5, 10, idx) == SubmitDecision::ReusePending);
		CHECK(idx == 0);
	}
	SECTION("an approved, unexpired approval executes once for the exact args") {
		std::vector<ApprovalView> a{{1, pause5, 100}};
		CHECK(ClassifySubmit(a, pause5, 10, idx) == SubmitDecision::Execute);
		CHECK(idx == 0);
	}
	SECTION("changed arguments after approval are refused -- a fresh approval is required") {
		std::vector<ApprovalView> a{{1, pause5, 100}};
		CHECK(ClassifySubmit(a, pause6, 10, idx) == SubmitDecision::CreatePending);
	}
	SECTION("a replay of an already-consumed approval is refused") {
		std::vector<ApprovalView> a{{3, pause5, 100}};
		CHECK(ClassifySubmit(a, pause5, 10, idx) == SubmitDecision::CreatePending);
	}
	SECTION("an expired approval does not execute") {
		std::vector<ApprovalView> a{{1, pause5, 5}};
		CHECK(ClassifySubmit(a, pause5, 10, idx) == SubmitDecision::CreatePending);
	}
	SECTION("the binding is the exact args, so two different argument sets never share one approval") {
		std::vector<ApprovalView> a{{1, pause5, 100}};
		CHECK(ClassifySubmit(a, pause5, 10, idx) == SubmitDecision::Execute);
		CHECK(ClassifySubmit(a, pause6, 10, idx) == SubmitDecision::CreatePending);
	}
}

#endif /* OTTD_PRISMATIC_MCP_SERVER */
