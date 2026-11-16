/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 *
 * The `mcp` console command. It names no tool, resource or parameter: it asks the server's own
 * dispatch for tools/list and resources/list and derives every subcommand, help line and
 * argument type from the answer, then calls through that same dispatch. A tool added to the
 * server is a console command the moment it is in the catalogue.
 *
 * Observer effect: a console call is counted by the MCP coverage counters like an HTTP call.
 */

/** @file prismatic_mcp_console.cpp The `mcp` console command family, derived from the MCP catalogue. */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp.h"
#include "prismatic_mcp_console.h"
#include "../console_func.h"
#include "../console_type.h"
#include "../network/network_func.h"
#include <algorithm>
#include <charconv>

#include "../safeguards.h"

using json = nlohmann::json;

namespace PrismaticMCP {

namespace Console::detail {

std::vector<std::string> Resolve(const std::vector<std::string> &names, std::string_view query)
{
	if (query.empty()) return {};
	for (const auto &n : names) if (n == query) return {n};
	const std::string prefixed[] = {"openttd." + std::string(query), "openttd://" + std::string(query)};
	for (const auto &p : prefixed) for (const auto &n : names) if (n == p) return {n};

	std::vector<std::string> suffix;
	for (const auto &n : names) {
		if (n.size() <= query.size() || !n.ends_with(query)) continue;
		char before = n[n.size() - query.size() - 1];
		if (before == '.' || before == '/') suffix.push_back(n);
	}
	if (!suffix.empty()) return suffix;

	std::vector<std::string> contains;
	for (const auto &n : names) if (n.find(query) != std::string::npos) contains.push_back(n);
	/* A single substring match is a suggestion, not a resolution: return it with a marker the
	 * caller cannot mistake for a name, so `mcp foo` never runs a tool nobody named. */
	if (contains.size() == 1) contains.push_back("");
	return contains;
}

/** The schema's type list: a string, an array of strings, or none. */
static std::vector<std::string> Types(const json &prop)
{
	std::vector<std::string> out;
	if (!prop.is_object() || !prop.contains("type")) return out;
	const json &t = prop["type"];
	if (t.is_string()) out.push_back(t.get<std::string>());
	if (t.is_array()) for (const auto &e : t) if (e.is_string()) out.push_back(e.get<std::string>());
	return out;
}

static std::optional<bool> ParseBool(std::string_view s)
{
	if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
	if (s == "false" || s == "0" || s == "no" || s == "off") return false;
	return std::nullopt;
}

static std::optional<int64_t> ParseInt(std::string_view s)
{
	int64_t v = 0;
	auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
	if (ec != std::errc() || p != s.data() + s.size()) return std::nullopt;
	return v;
}

json ParseValue(const json &prop, std::string_view text, std::string &error)
{
	error.clear();
	std::vector<std::string> types = Types(prop);
	auto allows = [&types](std::string_view t) { return std::ranges::find(types, t) != types.end(); };
	json value;
	bool typed = false;

	if (types.empty()) {
		json parsed = json::parse(text, nullptr, false);
		value = parsed.is_discarded() ? json(std::string(text)) : parsed;
		typed = true;
	}
	if (!typed && allows("null") && text == "null") { value = nullptr; typed = true; }
	if (!typed && allows("boolean")) {
		if (auto b = ParseBool(text); b.has_value()) { value = *b; typed = true; }
	}
	if (!typed && allows("integer")) {
		if (auto i = ParseInt(text); i.has_value()) { value = *i; typed = true; }
	}
	if (!typed && allows("number")) {
		json parsed = json::parse(text, nullptr, false);
		if (!parsed.is_discarded() && parsed.is_number()) { value = parsed; typed = true; }
	}
	if (!typed && allows("array")) {
		if (text.starts_with("[")) {
			json parsed = json::parse(text, nullptr, false);
			if (!parsed.is_discarded() && parsed.is_array()) { value = parsed; typed = true; }
		} else {
			json items = prop.contains("items") ? prop["items"] : json::object();
			json arr = json::array();
			bool ok = true;
			std::string_view rest = text;
			while (ok) {
				size_t comma = rest.find(',');
				std::string_view item = rest.substr(0, comma);
				std::string item_error;
				json v = ParseValue(items, item, item_error);
				if (!item_error.empty()) { ok = false; break; }
				arr.push_back(v);
				if (comma == std::string_view::npos) break;
				rest = rest.substr(comma + 1);
			}
			if (ok && !text.empty()) { value = arr; typed = true; }
		}
	}
	if (!typed && allows("object")) {
		json parsed = json::parse(text, nullptr, false);
		if (!parsed.is_discarded() && parsed.is_object()) { value = parsed; typed = true; }
	}
	if (!typed && allows("string")) { value = std::string(text); typed = true; }

	if (!typed) {
		std::string want;
		for (const auto &t : types) want += (want.empty() ? "" : "|") + t;
		error = fmt::format("'{}' is not {}", text, want);
		return json();
	}
	if (prop.contains("enum") && prop["enum"].is_array()) {
		const json &e = prop["enum"];
		if (std::ranges::find(e, value) == e.end()) {
			error = fmt::format("'{}' is not one of {}", text, e.dump());
			return json();
		}
	}
	return value;
}

/** The schema's required keys, in their declared order. */
static std::vector<std::string> Required(const json &schema)
{
	std::vector<std::string> out;
	if (schema.is_object() && schema.contains("required") && schema["required"].is_array()) {
		for (const auto &r : schema["required"]) if (r.is_string()) out.push_back(r.get<std::string>());
	}
	return out;
}

json ParseArgs(const json &schema, std::span<const std::string_view> tokens, std::vector<std::string> &errors)
{
	json props = (schema.is_object() && schema.contains("properties") && schema["properties"].is_object()) ? schema["properties"] : json::object();
	bool open = props.empty() || (schema.contains("additionalProperties") && schema["additionalProperties"] != false);
	std::vector<std::string> required = Required(schema);
	json args = json::object();
	size_t positional = 0;

	for (std::string_view tok : tokens) {
		if (tok.starts_with("{")) {
			json obj = json::parse(tok, nullptr, false);
			if (obj.is_discarded() || !obj.is_object()) { errors.push_back(fmt::format("'{}' is not a JSON object", tok)); continue; }
			for (auto &[k, v] : obj.items()) args[k] = v;
			continue;
		}
		std::string key;
		std::string_view text;
		size_t eq = tok.find('=');
		if (eq == std::string_view::npos) {
			/* A bare token fills the next required key that is not yet given. */
			while (positional < required.size() && args.contains(required[positional])) positional++;
			if (positional >= required.size()) { errors.push_back(fmt::format("'{}' has no key and no required key is left to take it", tok)); continue; }
			key = required[positional++];
			text = tok;
		} else {
			key = std::string(tok.substr(0, eq));
			text = tok.substr(eq + 1);
		}
		if (!props.contains(key) && !open) {
			std::string known;
			for (auto &[k, v] : props.items()) known += (known.empty() ? "" : ", ") + k;
			errors.push_back(fmt::format("unknown key '{}' (keys: {})", key, known));
			continue;
		}
		std::string error;
		json v = ParseValue(props.contains(key) ? props[key] : json::object(), text, error);
		if (!error.empty()) { errors.push_back(fmt::format("{}: {}", key, error)); continue; }
		args[key] = v;
	}

	for (const auto &r : required) {
		if (!args.contains(r)) errors.push_back(fmt::format("missing required key '{}'", r));
	}
	/* anyOf of required sets: at least one set must be complete. */
	if (schema.is_object() && schema.contains("anyOf") && schema["anyOf"].is_array() && !schema["anyOf"].empty()) {
		bool any = false;
		std::string alternatives;
		for (const auto &alt : schema["anyOf"]) {
			std::vector<std::string> need = Required(alt);
			bool all = std::ranges::all_of(need, [&args](const std::string &k) { return args.contains(k); });
			any = any || all;
			std::string set;
			for (const auto &k : need) set += (set.empty() ? "" : "+") + k;
			alternatives += (alternatives.empty() ? "" : " or ") + set;
		}
		if (!any) errors.push_back(fmt::format("needs {}", alternatives));
	}
	return args;
}

/** `<type>` or `a|b|c` for an enum, as the usage line shows a property. */
static std::string Placeholder(const json &prop)
{
	if (prop.contains("enum") && prop["enum"].is_array()) {
		std::string s;
		for (const auto &e : prop["enum"]) s += (s.empty() ? "" : "|") + (e.is_string() ? e.get<std::string>() : e.dump());
		return s;
	}
	std::string s;
	for (const auto &t : Types(prop)) s += (s.empty() ? "" : "|") + t;
	return "<" + (s.empty() ? std::string("json") : s) + ">";
}

std::string Usage(std::string_view name, const json &schema)
{
	std::string line = fmt::format("mcp {}", name);
	json props = (schema.is_object() && schema.contains("properties") && schema["properties"].is_object()) ? schema["properties"] : json::object();
	std::vector<std::string> required = Required(schema);
	for (const auto &r : required) {
		line += fmt::format(" {}={}", r, Placeholder(props.contains(r) ? props[r] : json::object()));
	}
	for (auto &[k, v] : props.items()) {
		if (std::ranges::find(required, k) != required.end()) continue;
		line += fmt::format(" [{}={}]", k, Placeholder(v));
	}
	if (props.empty() && required.empty()) line += " [{json}]";
	return line;
}

/** Append `text`'s lines, pretty-printing it when it is JSON. */
static void AppendText(std::vector<std::string> &out, const std::string &text, bool compact)
{
	json parsed = json::parse(text, nullptr, false);
	std::string body = parsed.is_discarded() ? text : (compact ? parsed.dump() : parsed.dump(2));
	size_t start = 0;
	while (start <= body.size()) {
		size_t nl = body.find('\n', start);
		out.push_back(body.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
		if (nl == std::string::npos) break;
		start = nl + 1;
	}
}

std::vector<std::string> ResultLines(const json &result, bool compact)
{
	std::vector<std::string> out;
	for (const char *key : {"content", "contents"}) {
		if (!result.contains(key) || !result[key].is_array()) continue;
		for (const auto &c : result[key]) {
			if (c.contains("text") && c["text"].is_string()) AppendText(out, c["text"].get<std::string>(), compact);
		}
	}
	if (out.empty()) AppendText(out, result.dump(), compact);
	return out;
}

std::vector<std::string> Complete(const json &tools, const json &resources, std::span<const std::string_view> words, std::string_view query)
{
	std::vector<std::string> out;
	if (words.empty()) return out;
	std::string_view command = words[0];
	if (command.starts_with('/')) command.remove_prefix(1);
	auto offer = [&](std::string_view candidate) {
		if (candidate.starts_with(query)) out.emplace_back(candidate);
	};
	auto names = [](const json &entries, std::string_view key) {
		std::vector<std::string> v;
		for (const auto &e : entries) if (e.contains(key) && e[key].is_string()) v.push_back(e[key].get<std::string>());
		return v;
	};
	auto short_name = [](const std::string &n) { return n.starts_with("openttd.") ? n.substr(8) : n; };

	if (command == "bot" || command == "ai") {
		if (words.size() == 1) for (std::string_view s : BOT_SUBCOMMANDS) offer(s);
		return out;
	}
	if (command != "mcp") return out;

	std::vector<std::string> tool_names = names(tools, "name");
	if (words.size() == 1) {
		for (std::string_view s : MCP_SUBCOMMANDS) offer(s);
		for (const auto &n : tool_names) offer(short_name(n));
		return out;
	}
	std::string_view sub = words[1];
	if (sub == "read") {
		if (words.size() == 2) for (const auto &u : names(resources, "uri")) offer(u);
		return out;
	}
	if (sub == "help") {
		if (words.size() == 2) for (const auto &n : tool_names) offer(short_name(n));
		return out;
	}
	if (sub == "tools" || sub == "resources" || sub == "rpc") return out;

	/* `mcp <tool> ...`: the tool's parameters not given yet, as key=. */
	std::vector<std::string> found = Resolve(tool_names, sub);
	if (found.size() != 1) return out;
	for (const auto &t : tools) {
		if (t.value("name", "") != found[0]) continue;
		const json &props = t.value("inputSchema", json::object()).value("properties", json::object());
		for (const auto &[key, _] : props.items()) {
			std::string given = key + "=";
			bool used = false;
			for (size_t i = 2; i < words.size(); i++) used |= words[i].starts_with(given);
			if (!used) offer(given);
		}
	}
	return out;
}

} // namespace Console::detail

using namespace Console::detail;

namespace {

/** Default number of result lines printed before the rest is summarised. */
constexpr size_t DEFAULT_MAX_LINES = 200;

json Rpc(std::string_view method, json params = json::object())
{
	return Dispatch(json{{"jsonrpc", "2.0"}, {"id", "console"}, {"method", std::string(method)}, {"params", std::move(params)}});
}

/** The catalogue entries of one list call (tools or resources), as returned. */
json Catalogue(std::string_view method, std::string_view field)
{
	json r = Rpc(method);
	if (r.contains("result") && r["result"].contains(field) && r["result"][field].is_array()) return r["result"][field];
	return json::array();
}

std::vector<std::string> Names(const json &entries, std::string_view key)
{
	std::vector<std::string> out;
	for (const auto &e : entries) if (e.contains(key) && e[key].is_string()) out.push_back(e[key].get<std::string>());
	return out;
}

const json *Find(const json &entries, std::string_view key, std::string_view name)
{
	for (const auto &e : entries) if (e.contains(key) && e[key] == name) return &e;
	return nullptr;
}

bool IsRemoteConsole()
{
	return _redirect_console_to_client != ClientID::Invalid;
}

void PrintLines(const std::vector<std::string> &lines, size_t max_lines, TextColour colour)
{
	size_t n = (max_lines == 0) ? lines.size() : std::min(lines.size(), max_lines);
	for (size_t i = 0; i < n; i++) IConsolePrint(colour, "{}", lines[i]);
	if (n < lines.size()) IConsolePrint(CC_HELP, "... {} more line(s); repeat with --all", lines.size() - n);
}

/** Report a name that did not resolve to exactly one entry. Returns true when it was handled. */
bool ReportUnresolved(const std::vector<std::string> &found, std::string_view what, std::string_view query)
{
	if (found.size() == 1 && !found[0].empty()) return false;
	if (found.empty()) {
		IConsolePrint(CC_ERROR, "No {} matches '{}'. Try 'mcp {}'.", what, query, what == "tool" ? "tools" : "resources");
		return true;
	}
	IConsolePrint(CC_ERROR, "'{}' names no single {}; did you mean:", query, what);
	for (const auto &f : found) if (!f.empty()) IConsolePrint(CC_HELP, "  {}", f);
	return true;
}

void PrintHelp()
{
	json tools = Catalogue("tools/list", "tools");
	json resources = Catalogue("resources/list", "resources");
	IConsolePrint(CC_HELP, "RESEARCH-ONLY/UNSTABLE: the MCP catalogue as console commands ({} tool(s), {} resource(s), read from the server's own lists).", tools.size(), resources.size());
	IConsolePrint(CC_HELP, "  mcp tools [filter]            list the tools");
	IConsolePrint(CC_HELP, "  mcp resources [filter]        list the resources");
	IConsolePrint(CC_HELP, "  mcp help <tool>               the tool's description and parameters");
	IConsolePrint(CC_HELP, "  mcp read <uri> [--all|--compact]   read a resource");
	IConsolePrint(CC_HELP, "  mcp <tool> [key=value ...] [{json}] [--all|--compact]   call a tool");
	IConsolePrint(CC_HELP, "  mcp rpc <method> [{json params}]   any JSON-RPC method of the server");
	IConsolePrint(CC_HELP, "A name may drop its 'openttd.' prefix or be a unique suffix (e.g. 'mcp view.zoom 2'). Bare values fill the required keys in order.");
}

void ListEntries(const json &entries, std::string_view key, std::string_view filter)
{
	for (const auto &e : entries) {
		std::string name = e.value(std::string(key), "");
		if (!filter.empty() && name.find(filter) == std::string::npos) continue;
		bool ro = e.contains("annotations") && e["annotations"].value("readOnlyHint", false);
		IConsolePrint(CC_INFO, "{}{}  {}", name, ro ? " (read)" : "", e.value("description", ""));
	}
}

} // anonymous namespace

/**
 * `mcp`: the MCP catalogue as console commands. @copydoc IConsoleCmdProc
 */
bool ConMcp(std::span<std::string_view> argv)
{
	if (argv.size() < 2) {
		PrintHelp();
		return true;
	}

	/* Flags anywhere after the subcommand. */
	size_t max_lines = DEFAULT_MAX_LINES;
	bool compact = false;
	std::vector<std::string_view> rest;
	for (size_t i = 2; i < argv.size(); i++) {
		if (argv[i] == "--all") { max_lines = 0; continue; }
		if (argv[i] == "--compact") { compact = true; continue; }
		rest.push_back(argv[i]);
	}
	std::string_view sub = argv[1];

	if (sub == "tools" || sub == "resources") {
		bool tools = sub == "tools";
		ListEntries(Catalogue(tools ? "tools/list" : "resources/list", tools ? "tools" : "resources"), tools ? "name" : "uri", rest.empty() ? "" : rest[0]);
		return true;
	}

	if (sub == "help") {
		if (rest.empty()) { PrintHelp(); return true; }
		json tools = Catalogue("tools/list", "tools");
		auto found = Resolve(Names(tools, "name"), rest[0]);
		if (ReportUnresolved(found, "tool", rest[0])) return true;
		const json *t = Find(tools, "name", found[0]);
		IConsolePrint(CC_INFO, "{}", t->value("description", ""));
		IConsolePrint(CC_HELP, "{}", Usage(found[0], t->value("inputSchema", json::object())));
		return true;
	}

	if (sub == "read") {
		if (rest.empty()) { IConsolePrint(CC_ERROR, "Usage: mcp read <uri>. Try 'mcp resources'."); return true; }
		json resources = Catalogue("resources/list", "resources");
		auto found = Resolve(Names(resources, "uri"), rest[0]);
		if (ReportUnresolved(found, "resource", rest[0])) return true;
		json r = Rpc("resources/read", json{{"uri", found[0]}});
		if (r.contains("error")) { IConsolePrint(CC_ERROR, "{}", r["error"].value("message", r["error"].dump())); return true; }
		PrintLines(ResultLines(r["result"], compact), max_lines, CC_INFO);
		return true;
	}

	if (sub == "rpc") {
		if (rest.empty()) { IConsolePrint(CC_ERROR, "Usage: mcp rpc <method> [{json params}]"); return true; }
		if (IsRemoteConsole()) { IConsolePrint(CC_ERROR, "Refused: raw JSON-RPC is not offered to a remote console."); return true; }
		json params = json::object();
		if (rest.size() > 1) {
			params = json::parse(rest[1], nullptr, false);
			if (params.is_discarded()) { IConsolePrint(CC_ERROR, "'{}' is not JSON", rest[1]); return true; }
		}
		json r = Rpc(rest[0], params);
		json shown = r.is_null() ? json("(notification: no reply)") : (r.contains("result") ? r["result"] : r);
		std::vector<std::string> lines = ResultLines(json{{"content", json::array({{{"text", shown.dump()}}})}}, compact);
		PrintLines(lines, max_lines, r.contains("error") ? CC_ERROR : CC_INFO);
		return true;
	}

	/* Anything else names a tool. */
	json tools = Catalogue("tools/list", "tools");
	auto found = Resolve(Names(tools, "name"), sub);
	if (ReportUnresolved(found, "tool", sub)) return true;
	const json *t = Find(tools, "name", found[0]);
	json schema = t->value("inputSchema", json::object());
	bool read_only = t->contains("annotations") && (*t)["annotations"].value("readOnlyHint", false);
	if (IsRemoteConsole() && !read_only) {
		IConsolePrint(CC_ERROR, "Refused: '{}' is not annotated read-only, and a remote console runs only read-only tools.", found[0]);
		return true;
	}

	std::vector<std::string> errors;
	json args = ParseArgs(schema, rest, errors);
	if (!errors.empty()) {
		for (const auto &e : errors) IConsolePrint(CC_ERROR, "{}", e);
		IConsolePrint(CC_HELP, "{}", Usage(found[0], schema));
		return true;
	}

	json r = Rpc("tools/call", json{{"name", found[0]}, {"arguments", args}});
	if (r.contains("error")) {
		IConsolePrint(CC_ERROR, "{}", r["error"].value("message", r["error"].dump()));
		return true;
	}
	bool is_error = r["result"].value("isError", false);
	PrintLines(ResultLines(r["result"], compact), max_lines, is_error ? CC_ERROR : CC_INFO);
	return true;
}

std::vector<std::string> ConsoleCompletions(std::string_view prefix, std::string_view query)
{
	std::vector<std::string_view> words;
	size_t start = 0;
	while (start < prefix.size()) {
		size_t end = prefix.find(' ', start);
		if (end == std::string_view::npos) end = prefix.size();
		if (end > start) words.push_back(prefix.substr(start, end - start));
		start = end + 1;
	}
	if (words.empty()) return {};
	std::string_view command = words[0].starts_with('/') ? words[0].substr(1) : words[0];
	if (command != "mcp" && command != "bot" && command != "ai") return {};
	return Complete(Catalogue("tools/list", "tools"), Catalogue("resources/list", "resources"), words, query);
}

} // namespace PrismaticMCP

#endif /* OTTD_PRISMATIC_MCP_SERVER */
