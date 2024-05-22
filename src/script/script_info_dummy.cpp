/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_info_dummy.cpp Implementation of a dummy Script. */

#include "../stdafx.h"
#include <squirrel.h>

#include "../string_func.h"
#include "../strings_func.h"
#include "../3rdparty/fmt/format.h"

#include "../safeguards.h"

/* The reason this exists in C++, is that a user can trash their ai/ or game/ dir,
 *  leaving no Scripts available. The complexity to solve this is insane, and
 *  therefore the alternative is used, and make sure there is always a Script
 *  available, no matter what the situation is. By defining it in C++, there
 *  is simply no way a user can delete it, and therefore safe to use. It has
 *  to be noted that this Script is complete invisible for the user, and impossible
 *  to select manual. It is a fail-over in case no Scripts are available.
 */

/** Run the dummy info.nut. */
void Script_CreateDummyInfo(HSQUIRRELVM vm, const char *type, const char *dir)
{
	std::string dummy_script = fmt::format(
			"class Dummy{0} extends {0}Info {{\n"
			"function GetAuthor()      {{ return \"OpenTTD Developers Team\"; }}\n"
			"function GetName()        {{ return \"Dummy{0}\"; }}\n"
			"function GetShortName()   {{ return \"DUMM\"; }}\n"
			"function GetDescription() {{ return \"A Dummy {0} that is loaded when your {1}/ dir is empty\"; }}\n"
			"function GetVersion()     {{ return 1; }}\n"
			"function GetDate()        {{ return \"2008-07-26\"; }}\n"
			"function CreateInstance() {{ return \"Dummy{0}\"; }}\n"
			"}} RegisterDummy{0}(Dummy{0}());\n", type, dir);

	sq_pushroottable(vm);

	/* Load and run the script */
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, dummy_script.c_str(), dummy_script.size(), "dummy", SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
			sq_pop(vm, 1);
			return;
		}
	}
	NOT_REACHED();
}

/**
 * Split the given message on newlines ('\n') and escape quotes and (back)slashes,
 * so they can be properly interpreted as string constants by the Squirrel compiler.
 * @param message The message that we want to sanitize for use in Squirrel code.
 * @return Vector with sanitized strings to use as string constant in Squirrel code.
 */
static std::vector<std::string> EscapeQuotesAndSlashesAndSplitOnNewLines(const std::string &message)
{
	std::vector<std::string> messages;

	std::string safe_message;
	for (auto c : message) {
		if (c == '\n') {
			messages.emplace_back(std::move(safe_message));
			continue;
		}

		if (c == '"' || c == '\\') safe_message.push_back('\\');
		safe_message.push_back(c);
	}
	messages.emplace_back(std::move(safe_message));
	return messages;
}

/** Run the dummy AI and let it generate an error message. */
void Script_CreateDummy(HSQUIRRELVM vm, StringID string, const char *type)
{
	/* We want to translate the error message.
	 * We do this in three steps:
	 * 1) We get the error message, escape quotes and slashes, and split on
	 *    newlines because Log.Error terminates passed strings at newlines.
	 */
	std::string error_message = GetString(string);
	std::vector<std::string> messages = EscapeQuotesAndSlashesAndSplitOnNewLines(error_message);

	/* 2) We construct the AI's code. This is done by merging a header, body and footer */
	std::string dummy_script;
	auto back_inserter = std::back_inserter(dummy_script);
	/* Just a rough ballpark estimate. */
	dummy_script.reserve(error_message.size() + 128 + 64 * messages.size());

	fmt::format_to(back_inserter, "class Dummy{0} extends {0}Controller {{\n  function Start()\n  {{\n", type);
	for (std::string &message : messages) {
		fmt::format_to(back_inserter, "    {}Log.Error(\"{}\");\n", type, message);
	}
	dummy_script += "  }\n}\n";

	/* 3) Finally we load and run the script */
	sq_pushroottable(vm);
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, dummy_script.c_str(), dummy_script.size(), "dummy", SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
			sq_pop(vm, 1);
			return;
		}
	}
	NOT_REACHED();
}
