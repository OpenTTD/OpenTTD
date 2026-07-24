/*
 * Part of korczis's PRIVATE RESEARCH FORK of OpenTTD. RESEARCH-ONLY.
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 */

/** @file test_prismatic_mcp.cpp Unit tests for the pure MCP security/logic helpers. */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/catch2/catch.hpp"
#include "../prismatic_mcp/prismatic_mcp_detail.h"

#include "../safeguards.h"

using namespace PrismaticMCP::detail;

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

#endif /* OTTD_PRISMATIC_MCP_SERVER */
