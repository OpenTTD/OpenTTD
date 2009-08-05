/* $Id$ */

/** @file ai_info_dummy.cpp Implementation of a dummy AI. */

#include <squirrel.h>
#include "../stdafx.h"

#include "../string_func.h"
#include "../strings_func.h"
#include "table/strings.h"

/* The reason this exists in C++, is that a user can trash his ai/ dir,
 *  leaving no AIs available. The complexity to solve this is insane, and
 *  therefor the alternative is used, and make sure there is always an AI
 *  available, no matter what the situation is. By defining it in C++, there
 *  is simply now way a user can delete it, and therefor safe to use. It has
 *  to be noted that this AI is complete invisible for the user, and impossible
 *  to select manual. It is a fail-over in case no AIs are available.
 */

const SQChar _dummy_script_info[] = _SC("                                                       \n\
class DummyAI extends AIInfo {                                                                  \n\
  function GetAuthor()      { return \"OpenTTD NoAI Developers Team\"; }                        \n\
  function GetName()        { return \"DummyAI\"; }                                             \n\
  function GetShortName()   { return \"DUMM\"; }                                                \n\
  function GetDescription() { return \"A Dummy AI that is loaded when your ai/ dir is empty\"; }\n\
  function GetVersion()     { return 1; }                                                       \n\
  function GetDate()        { return \"2008-07-26\"; }                                          \n\
  function CreateInstance() { return \"DummyAI\"; }                                             \n\
}                                                                                               \n\
                                                                                                \n\
RegisterDummyAI(DummyAI());                                                                     \n\
");

void AI_CreateAIInfoDummy(HSQUIRRELVM vm)
{
	sq_pushroottable(vm);

	/* Load and run the script */
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, _dummy_script_info, scstrlen(_dummy_script_info), _SC("dummy"), SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
			sq_pop(vm, 1);
			return;
		}
	}
	NOT_REACHED();
}

void AI_CreateAIDummy(HSQUIRRELVM vm)
{
	/* We want to translate the error message.
	 * We do this in three steps:
	 * 1) We get the error message
	 */
	char error_message[1024];
	GetString(error_message, STR_ERROR_AI_NO_AI_FOUND, lastof(error_message));

	/* 2) We construct the AI's code. This is done by merging a header, body and footer */
	char dummy_script[4096];
	char *dp = dummy_script;
	dp = strecpy(dp, "class DummyAI extends AIController {\n  function Start() {\n", lastof(dummy_script));

	/* As special trick we need to split the error message on newlines and
	 * emit each newline as a separate error printing string. */
	char *newline;
	char *p = error_message;
	do {
		newline = strchr(p, '\n');
		if (newline != NULL) *newline = '\0';

		dp += seprintf(dp, lastof(dummy_script), "    AILog.Error(\"%s\");\n", p);
		p = newline + 1;
	} while (newline != NULL);

	dp = strecpy(dp, "  }\n}\n", lastof(dummy_script));

	/* 3) We translate the error message in the character format that Squirrel wants.
	 *    We can use the fact that the wchar string printing also uses %s to print
	 *    old style char strings, which is what was generated during the script generation. */
	const SQChar *sq_dummy_script = OTTD2FS(dummy_script);

	/* And finally we load and run the script */
	sq_pushroottable(vm);
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, sq_dummy_script, scstrlen(sq_dummy_script), _SC("dummy"), SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
			sq_pop(vm, 1);
			return;
		}
	}
	NOT_REACHED();
}
