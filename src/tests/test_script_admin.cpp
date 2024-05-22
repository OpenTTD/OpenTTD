/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_admin_json.cpp Tests for the Squirrel -> JSON conversion in ScriptAdmin. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../game/game_instance.hpp"
#include "../script/api/script_admin.hpp"
#include "../script/api/script_event_types.hpp"
#include "../script/script_instance.hpp"
#include "../script/squirrel.hpp"

#include "../3rdparty/fmt/format.h"
#include "../3rdparty/nlohmann/json.hpp"

#include <squirrel.h>

/**
 * A controller to start enough so we can use Squirrel for testing.
 *
 * To run Squirrel, we need an Allocator, so malloc/free works.
 * For functions that log, we need an ActiveInstance, so the logger knows where
 * to send the logs to.
 *
 * By instantiating this class, both are set correctly. After that you can
 * use Squirrel without issues.
 */
class TestScriptController {
public:
	GameInstance game{};
	ScriptObject::ActiveInstance active{&game};

	Squirrel engine{"test"};
	ScriptAllocatorScope scope{&engine};
};

extern bool ScriptAdminMakeJSON(nlohmann::json &json, HSQUIRRELVM vm, SQInteger index, int depth = 0);

/**
 * Small wrapper around ScriptAdmin's MakeJSON that prepares the Squirrel
 * engine if it was called from actual scripting..
 */
static std::optional<std::string> TestScriptAdminMakeJSON(std::string_view squirrel)
{
	auto vm = sq_open(1024);
	/* sq_compile creates a closure with our snipper, which is a table.
	 * Add "return " to get the table on the stack. */
	std::string buffer = fmt::format("return {}", squirrel);

	/* Insert an (empty) class for testing. */
	sq_pushroottable(vm);
	sq_pushstring(vm, "DummyClass", -1);
	sq_newclass(vm, SQFalse);
	sq_newslot(vm, -3, SQFalse);
	sq_pop(vm, 1);

	/* Compile the snippet. */
	REQUIRE(sq_compilebuffer(vm, buffer.c_str(), buffer.size(), "test", SQTrue) == SQ_OK);
	/* Execute the snippet, capturing the return value. */
	sq_pushroottable(vm);
	REQUIRE(sq_call(vm, 1, SQTrue, SQTrue) == SQ_OK);
	/* Ensure the snippet pushed a table on the stack. */
	REQUIRE(sq_gettype(vm, -1) == OT_TABLE);

	/* Feed the snippet into the MakeJSON function. */
	nlohmann::json json;
	if (!ScriptAdminMakeJSON(json, vm, -1)) {
		sq_close(vm);
		return std::nullopt;
	}

	sq_close(vm);
	return json.dump();
}

/**
 * Validate ScriptEventAdminPort can convert JSON to Squirrel.
 *
 * This function is not actually part of ScriptAdmin, but we will use MakeJSON,
 * and as such need to be inside this class.
 *
 * The easiest way to do validate, is to first use ScriptEventAdminPort (the function
 * we are testing) to convert the JSON to a Squirrel table. Then to use MakeJSON
 * to convert it back to JSON.
 *
 * Sadly, Squirrel has no way to easily compare if two tables are identical, so we
 * use the JSON -> Squirrel -> JSON method to validate the conversion. But mind you,
 * a failure in the final JSON might also mean a bug in MakeJSON.
 *
 * @param json The JSON-string to convert to Squirrel
 * @return The Squirrel table converted to a JSON-string.
 */
static std::optional<std::string> TestScriptEventAdminPort(const std::string &json)
{
	auto vm = sq_open(1024);

	/* Run the conversion JSON -> Squirrel (this will now be on top of the stack). */
	ScriptEventAdminPort(json).GetObject(vm);
	if (sq_gettype(vm, -1) == OT_NULL) {
		sq_close(vm);
		return std::nullopt;
	}
	REQUIRE(sq_gettype(vm, -1) == OT_TABLE);

	nlohmann::json squirrel_json;
	REQUIRE(ScriptAdminMakeJSON(squirrel_json, vm, -1) == true);

	sq_close(vm);
	return squirrel_json.dump();
}

TEST_CASE("Squirrel -> JSON conversion")
{
	TestScriptController controller;

	CHECK(TestScriptAdminMakeJSON(R"sq({ test = null })sq") == R"json({"test":null})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = 1 })sq") == R"json({"test":1})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = -1 })sq") == R"json({"test":-1})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = true })sq") == R"json({"test":true})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = "a" })sq") == R"json({"test":"a"})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = [ ] })sq") == R"json({"test":[]})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = [ 1 ] })sq") == R"json({"test":[1]})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = [ 1, "a", true, { test = 1 }, [], null ] })sq") == R"json({"test":[1,"a",true,{"test":1},[],null]})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = { } })sq") == R"json({"test":{}})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = { test = 1 } })sq") == R"json({"test":{"test":1}})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = { test = 1, test = 2 } })sq") == R"json({"test":{"test":2}})json");
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = { test = 1, test2 = [ 2 ] } })sq") == R"json({"test":{"test":1,"test2":[2]}})json");

	/* Cases that should fail, as we cannot convert a class to JSON. */
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = DummyClass })sq") == std::nullopt);
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = [ 1, DummyClass ] })sq") == std::nullopt);
	CHECK(TestScriptAdminMakeJSON(R"sq({ test = { test = 1, test2 = DummyClass } })sq") == std::nullopt);
}

TEST_CASE("JSON -> Squirrel conversion")
{
	TestScriptController controller;

	CHECK(TestScriptEventAdminPort(R"json({ "test": null })json") == R"json({"test":null})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": 1 })json") == R"json({"test":1})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": -1 })json") == R"json({"test":-1})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": true })json") == R"json({"test":true})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": "a" })json") == R"json({"test":"a"})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": [] })json") == R"json({"test":[]})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": [ 1 ] })json") == R"json({"test":[1]})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": [ 1, "a", true, { "test": 1 }, [], null ] })json") == R"json({"test":[1,"a",true,{"test":1},[],null]})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": {} })json") == R"json({"test":{}})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": { "test": 1 } })json") == R"json({"test":{"test":1}})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": { "test": 2 } })json") == R"json({"test":{"test":2}})json");
	CHECK(TestScriptEventAdminPort(R"json({ "test": { "test": 1, "test2": [ 2 ] } })json") == R"json({"test":{"test":1,"test2":[2]}})json");

	/* Check if spaces are properly ignored. */
	CHECK(TestScriptEventAdminPort(R"json({"test":1})json") == R"json({"test":1})json");
	CHECK(TestScriptEventAdminPort(R"json({"test":        1})json") == R"json({"test":1})json");

	/* Valid JSON but invalid Squirrel (read: floats). */
	CHECK(TestScriptEventAdminPort(R"json({ "test": 1.1 })json") == std::nullopt);
	CHECK(TestScriptEventAdminPort(R"json({ "test": [ 1, 3, 1.1 ] })json") == std::nullopt);

	/* Root element has to be an object. */
	CHECK(TestScriptEventAdminPort(R"json( 1 )json") == std::nullopt);
	CHECK(TestScriptEventAdminPort(R"json( "a" )json") == std::nullopt);
	CHECK(TestScriptEventAdminPort(R"json( [ 1 ] )json") == std::nullopt);
	CHECK(TestScriptEventAdminPort(R"json( null )json") == std::nullopt);
	CHECK(TestScriptEventAdminPort(R"json( true )json") == std::nullopt);

	/* Cases that should fail, as it is invalid JSON. */
	CHECK(TestScriptEventAdminPort(R"json({"test":test})json") == std::nullopt);
	CHECK(TestScriptEventAdminPort(R"json({ "test": 1 )json") == std::nullopt); // Missing closing }
	CHECK(TestScriptEventAdminPort(R"json(  "test": 1})json") == std::nullopt); // Missing opening {
	CHECK(TestScriptEventAdminPort(R"json({ "test" = 1})json") == std::nullopt);
	CHECK(TestScriptEventAdminPort(R"json({ "test": [ 1 })json") == std::nullopt); // Missing closing ]
	CHECK(TestScriptEventAdminPort(R"json({ "test": 1 ] })json") == std::nullopt); // Missing opening [
}
