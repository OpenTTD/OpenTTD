/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 *
 * Coverage of the MCP surface, measured where it cannot be faked: the dispatch counts every
 * tools/call and resources/read it receives, and the report compares those counts with the
 * catalogue the server publishes at that moment. An end-to-end run that leaves an entry at zero
 * did not exercise it, whatever its scripts claim. Observer effect: one map increment per call.
 */

/** @file prismatic_mcp_coverage.cpp Call counters of the MCP dispatch and the coverage report (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_coverage.h"

#include <map>

#include "../safeguards.h"

namespace PrismaticMCP::Coverage {

using json = nlohmann::json;

namespace {

/** Calls per tool and reads per resource since this process started its MCP server. Main thread only. */
std::map<std::string, uint64_t> _tools, _resources;

} // namespace

void Count(const char *kind, const std::string &name)
{
	(std::string_view(kind) == "tool" ? _tools : _resources)[name]++;
}

json Report(const json &tools, const json &resources)
{
	json t = json::object(), r = json::object(), ut = json::array(), ur = json::array();
	size_t covered = 0, total = 0;
	for (const auto &e : tools.value("tools", json::array())) {
		std::string name = e.value("name", "");
		auto it = _tools.find(name);
		uint64_t n = it == _tools.end() ? 0 : it->second;
		t[name] = n;
		total++;
		if (n > 0) covered++; else ut.push_back(name);
	}
	for (const auto &e : resources.value("resources", json::array())) {
		std::string uri = e.value("uri", "");
		auto it = _resources.find(uri);
		uint64_t n = it == _resources.end() ? 0 : it->second;
		r[uri] = n;
		total++;
		if (n > 0) covered++; else ur.push_back(uri);
	}
	return json{{"tools", t}, {"resources", r}, {"uncovered", {{"tools", ut}, {"resources", ur}}},
		{"covered", covered}, {"total", total}};
}

} // namespace PrismaticMCP::Coverage

#endif /* OTTD_PRISMATIC_MCP_SERVER */
