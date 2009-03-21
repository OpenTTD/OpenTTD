/* $Id$ */

#include <squirrel.h>
#include "../stdafx.h"

/* The reason this exists in C++, is that a user can trash his ai/ dir,
 *  leaving no AIs available. The complexity to solve this is insane, and
 *  therefor the alternative is used, and make sure there is always an AI
 *  available, no matter what the situation is. By defining it in C++, there
 *  is simply now way a user can delete it, and therefor safe to use. It has
 *  to be noted that this AI is complete invisible for the user, and impossible
 *  to select manual. It is a fail-over in case no AIs are available.
 */

const SQChar dummy_script_info[] = _SC("                                                        \n\
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

const SQChar dummy_script[] = _SC("                                                             \n\
class DummyAI extends AIController {                                                            \n\
  function Start() {                                                                            \n\
    AILog.Error(\"No suitable AI found to load.\");                                             \n\
    AILog.Error(\"This AI is a dummy AI and won't do anything.\");                              \n\
    AILog.Error(\"You can download several AIs via the 'Online Content' system.\");             \n\
  }                                                                                             \n\
}                                                                                               \n\
");

void AI_CreateAIInfoDummy(HSQUIRRELVM vm)
{
	sq_pushroottable(vm);

	/* Load and run the script */
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, dummy_script_info, scstrlen(dummy_script_info), _SC("dummy"), SQTrue))) {
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
	sq_pushroottable(vm);

	/* Load and run the script */
	if (SQ_SUCCEEDED(sq_compilebuffer(vm, dummy_script, scstrlen(dummy_script), _SC("dummy"), SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
			sq_pop(vm, 1);
			return;
		}
	}
	NOT_REACHED();
}
