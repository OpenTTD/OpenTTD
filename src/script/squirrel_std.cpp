/* $Id$ */

#include <squirrel.h>
#include "../stdafx.h"
#include "../debug.h"
#include "squirrel.hpp"
#include "squirrel_std.hpp"
#include "../core/alloc_func.hpp"
#include "../core/math_func.hpp"

/* abs() is normally defined to myabs(), which we don't want in this file */
#undef abs

SQInteger SquirrelStd::abs(HSQUIRRELVM vm)
{
	SQInteger tmp;

	sq_getinteger(vm, 2, &tmp);
	sq_pushinteger(vm, ::abs(tmp));
	return 1;
}

SQInteger SquirrelStd::min(HSQUIRRELVM vm)
{
	SQInteger tmp1, tmp2;

	sq_getinteger(vm, 2, &tmp1);
	sq_getinteger(vm, 3, &tmp2);
	sq_pushinteger(vm, ::min(tmp1, tmp2));
	return 1;
}

SQInteger SquirrelStd::max(HSQUIRRELVM vm)
{
	SQInteger tmp1, tmp2;

	sq_getinteger(vm, 2, &tmp1);
	sq_getinteger(vm, 3, &tmp2);
	sq_pushinteger(vm, ::max(tmp1, tmp2));
	return 1;
}

SQInteger SquirrelStd::require(HSQUIRRELVM vm)
{
	SQInteger top = sq_gettop(vm);
	const SQChar *filename;
	SQChar *real_filename;

	sq_getstring(vm, 2, &filename);

	/* Get the script-name of the current file, so we can work relative from it */
	SQStackInfos si;
	sq_stackinfos(vm, 1, &si);
	if (si.source == NULL) {
		DEBUG(misc, 0, "[squirrel] Couldn't detect the script-name of the 'require'-caller; this should never happen!");
		return SQ_ERROR;
	}
	real_filename = scstrdup(si.source);
	/* Keep the dir, remove the rest */
	SQChar *s = scstrrchr(real_filename, PATHSEPCHAR);
	if (s != NULL) {
		/* Keep the PATHSEPCHAR there, remove the rest */
		*s++;
		*s = '\0';
	}
	/* And now we concat, so we are relative from the current script
	 * First, we have to make sure we have enough space for the full path */
	real_filename = ReallocT(real_filename, scstrlen(real_filename) + scstrlen(filename) + 1);
	scstrcat(real_filename, filename);
	/* Tars dislike opening files with '/' on Windows.. so convert it to '\\' ;) */
	char *filen = strdup(FS2OTTD(real_filename));
#if (PATHSEPCHAR != '/')
	for (char *n = filen; *n != '\0'; n++) if (*n == '/') *n = PATHSEPCHAR;
#endif

	bool ret = Squirrel::LoadScript(vm, filen);

	/* Reset the top, so the stack stays correct */
	sq_settop(vm, top);
	free(real_filename);
	free(filen);

	return ret ? 0 : SQ_ERROR;
}

SQInteger SquirrelStd::notifyallexceptions(HSQUIRRELVM vm)
{
	SQBool b;

	if (sq_gettop(vm) >= 1) {
		if (SQ_SUCCEEDED(sq_getbool(vm, -1, &b))) {
			sq_notifyallexceptions(vm, b);
			return 0;
		}
	}

	return SQ_ERROR;
}

void squirrel_register_global_std(Squirrel *engine)
{
	/* We don't use squirrel_helper here, as we want to register to the global
	 *  scope and not to a class. */
	engine->AddMethod("require",             &SquirrelStd::require,             2, ".s");
	engine->AddMethod("notifyallexceptions", &SquirrelStd::notifyallexceptions, 2, ".b");
}

void squirrel_register_std(Squirrel *engine)
{
	/* We don't use squirrel_helper here, as we want to register to the global
	 *  scope and not to a class. */
	engine->AddMethod("abs", &SquirrelStd::abs, 2, ".i");
	engine->AddMethod("min", &SquirrelStd::min, 3, ".ii");
	engine->AddMethod("max", &SquirrelStd::max, 3, ".ii");
}
