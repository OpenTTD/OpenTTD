/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file squirrel_std.cpp Implements the Squirrel Standard Function class */

#include <squirrel.h>
#include <sqstdmath.h>
#include "../stdafx.h"
#include "../debug.h"
#include "squirrel_std.hpp"
#include "../core/alloc_func.hpp"
#include "../core/math_func.hpp"


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
		s++;
		*s = '\0';
	}
	/* And now we concat, so we are relative from the current script
	 * First, we have to make sure we have enough space for the full path */
	real_filename = ReallocT(real_filename, scstrlen(real_filename) + scstrlen(filename) + 1);
	scstrcat(real_filename, filename);
	/* Tars dislike opening files with '/' on Windows.. so convert it to '\\' ;) */
	char *filen = strdup(SQ2OTTD(real_filename));
#if (PATHSEPCHAR != '/')
	for (char *n = filen; *n != '\0'; n++) if (*n == '/') *n = PATHSEPCHAR;
#endif

	Squirrel *engine = (Squirrel *)sq_getforeignptr(vm);
	bool ret = engine->LoadScript(vm, filen);

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
	engine->AddMethod("min", &SquirrelStd::min, 3, ".ii");
	engine->AddMethod("max", &SquirrelStd::max, 3, ".ii");

	sqstd_register_mathlib(engine->GetVM());
}
