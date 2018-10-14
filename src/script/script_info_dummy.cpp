/* $Id$ */

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

#include "../safeguards.h"

/* The reason this exists in C++, is that a user can trash his ai/ or game/ dir,
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
	char dummy_script[4096];
	char *dp = dummy_script;
	dp += seprintf(dp, lastof(dummy_script), "class Dummy%s extends %sInfo {\n", type, type);
	dp += seprintf(dp, lastof(dummy_script), "function GetAuthor()      { return \"OpenTTD Developers Team\"; }\n");
	dp += seprintf(dp, lastof(dummy_script), "function GetName()        { return \"Dummy%s\"; }\n", type);
	dp += seprintf(dp, lastof(dummy_script), "function GetShortName()   { return \"DUMM\"; }\n");
	dp += seprintf(dp, lastof(dummy_script), "function GetDescription() { return \"A Dummy %s that is loaded when your %s/ dir is empty\"; }\n", type, dir);
	dp += seprintf(dp, lastof(dummy_script), "function GetVersion()     { return 1; }\n");
	dp += seprintf(dp, lastof(dummy_script), "function GetDate()        { return \"2008-07-26\"; }\n");
	dp += seprintf(dp, lastof(dummy_script), "function CreateInstance() { return \"Dummy%s\"; }\n", type);
	dp += seprintf(dp, lastof(dummy_script), "} RegisterDummy%s(Dummy%s());\n", type, type);

	const SQChar *sq_dummy_script = dummy_script;

	sq_pushroottable(vm);

	/* Load and run the script */
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, sq_dummy_script, strlen(sq_dummy_script), "dummy", SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
			sq_pop(vm, 1);
			return;
		}
	}
	NOT_REACHED();
}

/** Run the dummy AI and let it generate an error message. */
void Script_CreateDummy(HSQUIRRELVM vm, StringID string, const char *type)
{
	/* We want to translate the error message.
	 * We do this in three steps:
	 * 1) We get the error message
	 */
	char error_message[1024];
	GetString(error_message, string, lastof(error_message));

	/* Make escapes for all quotes and slashes. */
	char safe_error_message[1024];
	char *q = safe_error_message;
	for (const char *p = error_message; *p != '\0' && q < lastof(safe_error_message) - 2; p++, q++) {
		if (*p == '"' || *p == '\\') *q++ = '\\';
		*q = *p;
	}
	*q = '\0';

	/* 2) We construct the AI's code. This is done by merging a header, body and footer */
	char dummy_script[4096];
	char *dp = dummy_script;
	dp += seprintf(dp, lastof(dummy_script), "class Dummy%s extends %sController {\n  function Start()\n  {\n", type, type);

	/* As special trick we need to split the error message on newlines and
	 * emit each newline as a separate error printing string. */
	char *newline;
	char *p = safe_error_message;
	do {
		newline = strchr(p, '\n');
		if (newline != NULL) *newline = '\0';

		dp += seprintf(dp, lastof(dummy_script), "    %sLog.Error(\"%s\");\n", type, p);
		p = newline + 1;
	} while (newline != NULL);

	strecpy(dp, "  }\n}\n", lastof(dummy_script));

	/* 3) We translate the error message in the character format that Squirrel wants.
	 *    We can use the fact that the wchar string printing also uses %s to print
	 *    old style char strings, which is what was generated during the script generation. */
	const SQChar *sq_dummy_script = dummy_script;

	/* And finally we load and run the script */
	sq_pushroottable(vm);
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, sq_dummy_script, strlen(sq_dummy_script), "dummy", SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
			sq_pop(vm, 1);
			return;
		}
	}
	NOT_REACHED();
}
