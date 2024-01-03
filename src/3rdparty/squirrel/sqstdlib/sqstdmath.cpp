/* see copyright notice in squirrel.h */

#include "../../../stdafx.h"

#include <squirrel.h>
#include <math.h>
#include <sqstdmath.h>

#include "../../../safeguards.h"

#define SINGLE_ARG_FUNC(_funcname, num_ops) static SQInteger math_##_funcname(HSQUIRRELVM v){ \
	SQFloat f; \
	sq_decreaseops(v,num_ops); \
	sq_getfloat(v,2,&f); \
	sq_pushfloat(v,(SQFloat)_funcname(f)); \
	return 1; \
}

#define TWO_ARGS_FUNC(_funcname, num_ops) static SQInteger math_##_funcname(HSQUIRRELVM v){ \
	SQFloat p1,p2; \
	sq_decreaseops(v,num_ops); \
	sq_getfloat(v,2,&p1); \
	sq_getfloat(v,3,&p2); \
	sq_pushfloat(v,(SQFloat)_funcname(p1,p2)); \
	return 1; \
}

#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
static SQInteger math_srand(HSQUIRRELVM v)
{
	SQInteger i;
	if(SQ_FAILED(sq_getinteger(v,2,&i)))
		return sq_throwerror(v,"invalid param");
	srand((unsigned int)i);
	return 0;
}

static SQInteger math_rand(HSQUIRRELVM v)
{
	sq_pushinteger(v,rand());
	return 1;
}
#endif /* EXPORT_DEFAULT_SQUIRREL_FUNCTIONS */

static SQInteger math_abs(HSQUIRRELVM v)
{
	SQInteger n;
	sq_getinteger(v,2,&n);
	sq_pushinteger(v,(SQInteger)abs((int)n));
	return 1;
}

SINGLE_ARG_FUNC(sqrt, 100)
SINGLE_ARG_FUNC(fabs, 1)
SINGLE_ARG_FUNC(sin, 100)
SINGLE_ARG_FUNC(cos, 100)
SINGLE_ARG_FUNC(asin, 100)
SINGLE_ARG_FUNC(acos, 100)
SINGLE_ARG_FUNC(log, 100)
SINGLE_ARG_FUNC(log10, 100)
SINGLE_ARG_FUNC(tan, 100)
SINGLE_ARG_FUNC(atan, 100)
TWO_ARGS_FUNC(atan2, 100)
TWO_ARGS_FUNC(pow, 100)
SINGLE_ARG_FUNC(floor, 1)
SINGLE_ARG_FUNC(ceil, 1)
SINGLE_ARG_FUNC(exp, 100)

#define _DECL_FUNC(name,nparams,tycheck) {#name,math_##name,nparams,tycheck}
static SQRegFunction mathlib_funcs[] = {
	_DECL_FUNC(sqrt,2,".n"),
	_DECL_FUNC(sin,2,".n"),
	_DECL_FUNC(cos,2,".n"),
	_DECL_FUNC(asin,2,".n"),
	_DECL_FUNC(acos,2,".n"),
	_DECL_FUNC(log,2,".n"),
	_DECL_FUNC(log10,2,".n"),
	_DECL_FUNC(tan,2,".n"),
	_DECL_FUNC(atan,2,".n"),
	_DECL_FUNC(atan2,3,".nn"),
	_DECL_FUNC(pow,3,".nn"),
	_DECL_FUNC(floor,2,".n"),
	_DECL_FUNC(ceil,2,".n"),
	_DECL_FUNC(exp,2,".n"),
#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
	_DECL_FUNC(srand,2,".n"),
	_DECL_FUNC(rand,1,nullptr),
#endif /* EXPORT_DEFAULT_SQUIRREL_FUNCTIONS */
	_DECL_FUNC(fabs,2,".n"),
	_DECL_FUNC(abs,2,".n"),
	{nullptr,nullptr,0,nullptr},
};

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

SQRESULT sqstd_register_mathlib(HSQUIRRELVM v)
{
	SQInteger i=0;
	while(mathlib_funcs[i].name!=nullptr)	{
		sq_pushstring(v,mathlib_funcs[i].name,-1);
		sq_newclosure(v,mathlib_funcs[i].f,0);
		sq_setparamscheck(v,mathlib_funcs[i].nparamscheck,mathlib_funcs[i].typemask);
		sq_setnativeclosurename(v,-1,mathlib_funcs[i].name);
		sq_createslot(v,-3);
		i++;
	}
#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
	sq_pushstring(v,"RAND_MAX",-1);
	sq_pushinteger(v,RAND_MAX);
	sq_createslot(v,-3);
#endif /* EXPORT_DEFAULT_SQUIRREL_FUNCTIONS */
	sq_pushstring(v,"PI",-1);
	sq_pushfloat(v,(SQFloat)M_PI);
	sq_createslot(v,-3);
	return SQ_OK;
}
