/*
 * Copyright (c) 2003-2011 Alberto Demichelis
 *
 * This software is provided 'as-is', without any
 * express or implied warranty. In no event will the
 * authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software
 * for any purpose, including commercial applications,
 * and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 *   1. The origin of this software must not be
 *   misrepresented; you must not claim that
 *   you wrote the original software. If you
 *   use this software in a product, an
 *   acknowledgment in the product
 *   documentation would be appreciated but is
 *   not required.
 *
 *   2. Altered source versions must be plainly
 *   marked as such, and must not be
 *   misrepresented as being the original
 *   software.
 *
 *   3. This notice may not be removed or
 *   altered from any source distribution.
 */
#ifndef _SQUIRREL_H_
#define _SQUIRREL_H_

#include "../../../string_type.h"

typedef int64_t SQInteger;
typedef uint64_t SQUnsignedInteger;
typedef uint64_t SQHash; /*should be the same size of a pointer*/
typedef int SQInt32;


#ifdef SQUSEDOUBLE
typedef double SQFloat;
#else
typedef float SQFloat;
#endif

typedef int64_t SQRawObjectVal; //must be 64bits
#define SQ_OBJECT_RAWINIT() { _unVal.raw = 0; }

typedef void* SQUserPointer;
typedef SQUnsignedInteger SQBool;
typedef SQInteger SQRESULT;

#define SQTrue	(1)
#define SQFalse	(0)

struct SQVM;
struct SQTable;
struct SQArray;
struct SQString;
struct SQClosure;
struct SQGenerator;
struct SQNativeClosure;
struct SQUserData;
struct SQFunctionProto;
struct SQRefCounted;
struct SQClass;
struct SQInstance;
struct SQDelegable;

typedef char SQChar;
#define MAX_CHAR 0xFFFF

#define SQUIRREL_VERSION	"Squirrel 2.2.5 stable - With custom OpenTTD modifications"
#define SQUIRREL_COPYRIGHT	"Copyright (C) 2003-2010 Alberto Demichelis"
#define SQUIRREL_AUTHOR		"Alberto Demichelis"
#define SQUIRREL_VERSION_NUMBER	225

#define SQ_VMSTATE_IDLE			0
#define SQ_VMSTATE_RUNNING		1
#define SQ_VMSTATE_SUSPENDED	2

#define SQUIRREL_EOB 0
#define SQ_BYTECODE_STREAM_TAG	0xFAFA

#define SQOBJECT_REF_COUNTED	0x08000000
#define SQOBJECT_NUMERIC		0x04000000
#define SQOBJECT_DELEGABLE		0x02000000
#define SQOBJECT_CANBEFALSE		0x01000000

#define SQ_MATCHTYPEMASKSTRING (-99999)

#define _RT_MASK 0x00FFFFFF
#define _RAW_TYPE(type) (type&_RT_MASK)

#define _RT_NULL			0x00000001
#define _RT_INTEGER			0x00000002
#define _RT_FLOAT			0x00000004
#define _RT_BOOL			0x00000008
#define _RT_STRING			0x00000010
#define _RT_TABLE			0x00000020
#define _RT_ARRAY			0x00000040
#define _RT_USERDATA		0x00000080
#define _RT_CLOSURE			0x00000100
#define _RT_NATIVECLOSURE	0x00000200
#define _RT_GENERATOR		0x00000400
#define _RT_USERPOINTER		0x00000800
#define _RT_THREAD			0x00001000
#define _RT_FUNCPROTO		0x00002000
#define _RT_CLASS			0x00004000
#define _RT_INSTANCE		0x00008000
#define _RT_WEAKREF			0x00010000

typedef enum tagSQObjectType{
	OT_NULL =			(_RT_NULL|SQOBJECT_CANBEFALSE),
	OT_INTEGER =		(_RT_INTEGER|SQOBJECT_NUMERIC|SQOBJECT_CANBEFALSE),
	OT_FLOAT =			(_RT_FLOAT|SQOBJECT_NUMERIC|SQOBJECT_CANBEFALSE),
	OT_BOOL =			(_RT_BOOL|SQOBJECT_CANBEFALSE),
	OT_STRING =			(_RT_STRING|SQOBJECT_REF_COUNTED),
	OT_TABLE =			(_RT_TABLE|SQOBJECT_REF_COUNTED|SQOBJECT_DELEGABLE),
	OT_ARRAY =			(_RT_ARRAY|SQOBJECT_REF_COUNTED),
	OT_USERDATA =		(_RT_USERDATA|SQOBJECT_REF_COUNTED|SQOBJECT_DELEGABLE),
	OT_CLOSURE =		(_RT_CLOSURE|SQOBJECT_REF_COUNTED),
	OT_NATIVECLOSURE =	(_RT_NATIVECLOSURE|SQOBJECT_REF_COUNTED),
	OT_GENERATOR =		(_RT_GENERATOR|SQOBJECT_REF_COUNTED),
	OT_USERPOINTER =	_RT_USERPOINTER,
	OT_THREAD =			(_RT_THREAD|SQOBJECT_REF_COUNTED) ,
	OT_FUNCPROTO =		(_RT_FUNCPROTO|SQOBJECT_REF_COUNTED), //internal usage only
	OT_CLASS =			(_RT_CLASS|SQOBJECT_REF_COUNTED),
	OT_INSTANCE =		(_RT_INSTANCE|SQOBJECT_REF_COUNTED|SQOBJECT_DELEGABLE),
	OT_WEAKREF =		(_RT_WEAKREF|SQOBJECT_REF_COUNTED)
}SQObjectType;

#define ISREFCOUNTED(t) (t&SQOBJECT_REF_COUNTED)


typedef union tagSQObjectValue
{
	struct SQTable *pTable;
	struct SQArray *pArray;
	struct SQClosure *pClosure;
	struct SQGenerator *pGenerator;
	struct SQNativeClosure *pNativeClosure;
	struct SQString *pString;
	struct SQUserData *pUserData;
	SQInteger nInteger;
	SQFloat fFloat;
	SQUserPointer pUserPointer;
	struct SQFunctionProto *pFunctionProto;
	struct SQRefCounted *pRefCounted;
	struct SQDelegable *pDelegable;
	struct SQVM *pThread;
	struct SQClass *pClass;
	struct SQInstance *pInstance;
	struct SQWeakRef *pWeakRef;
	SQRawObjectVal raw;
}SQObjectValue;


typedef struct tagSQObject
{
	SQObjectType _type;
	SQObjectValue _unVal;
}SQObject;

typedef struct tagSQStackInfos{
	const SQChar* funcname;
	const SQChar* source;
	SQInteger line;
}SQStackInfos;

typedef struct SQVM* HSQUIRRELVM;
typedef SQObject HSQOBJECT;
typedef SQInteger (*SQFUNCTION)(HSQUIRRELVM);
typedef SQInteger (*SQRELEASEHOOK)(SQUserPointer,SQInteger size);
typedef void (*SQCOMPILERERROR)(HSQUIRRELVM,const SQChar * /*desc*/,const SQChar * /*source*/,SQInteger /*line*/,SQInteger /*column*/);
typedef void (*SQPRINTFUNCTION)(HSQUIRRELVM,const std::string &);

typedef SQInteger (*SQWRITEFUNC)(SQUserPointer,SQUserPointer,SQInteger);
typedef SQInteger (*SQREADFUNC)(SQUserPointer,SQUserPointer,SQInteger);

typedef char32_t (*SQLEXREADFUNC)(SQUserPointer);

typedef struct tagSQRegFunction{
	const SQChar *name;
	SQFUNCTION f;
	SQInteger nparamscheck;
	const SQChar *typemask;
}SQRegFunction;

typedef struct tagSQFunctionInfo {
	SQUserPointer funcid;
	const SQChar *name;
	const SQChar *source;
}SQFunctionInfo;


/*vm*/
bool sq_can_suspend(HSQUIRRELVM v);
HSQUIRRELVM sq_open(SQInteger initialstacksize);
HSQUIRRELVM sq_newthread(HSQUIRRELVM friendvm, SQInteger initialstacksize);
void sq_seterrorhandler(HSQUIRRELVM v);
void sq_close(HSQUIRRELVM v);
void sq_setforeignptr(HSQUIRRELVM v,SQUserPointer p);
SQUserPointer sq_getforeignptr(HSQUIRRELVM v);
void sq_setprintfunc(HSQUIRRELVM v, SQPRINTFUNCTION printfunc);
SQPRINTFUNCTION sq_getprintfunc(HSQUIRRELVM v);
SQRESULT sq_suspendvm(HSQUIRRELVM v);
bool sq_resumecatch(HSQUIRRELVM v, int suspend = -1);
bool sq_resumeerror(HSQUIRRELVM v);
SQRESULT sq_wakeupvm(HSQUIRRELVM v,SQBool resumedret,SQBool retval,SQBool raiseerror,SQBool throwerror);
SQInteger sq_getvmstate(HSQUIRRELVM v);
void sq_decreaseops(HSQUIRRELVM v, int amount);

/*compiler*/
SQRESULT sq_compile(HSQUIRRELVM v,SQLEXREADFUNC read,SQUserPointer p,const SQChar *sourcename,SQBool raiseerror);
SQRESULT sq_compilebuffer(HSQUIRRELVM v,const SQChar *s,SQInteger size,const SQChar *sourcename,SQBool raiseerror);
void sq_enabledebuginfo(HSQUIRRELVM v, SQBool enable);
void sq_notifyallexceptions(HSQUIRRELVM v, SQBool enable);
void sq_setcompilererrorhandler(HSQUIRRELVM v,SQCOMPILERERROR f);

/*stack operations*/
void sq_push(HSQUIRRELVM v,SQInteger idx);
void sq_pop(HSQUIRRELVM v,SQInteger nelemstopop);
void sq_poptop(HSQUIRRELVM v);
void sq_remove(HSQUIRRELVM v,SQInteger idx);
SQInteger sq_gettop(HSQUIRRELVM v);
void sq_settop(HSQUIRRELVM v,SQInteger newtop);
void sq_reservestack(HSQUIRRELVM v,SQInteger nsize);
SQInteger sq_cmp(HSQUIRRELVM v);
void sq_move(HSQUIRRELVM dest,HSQUIRRELVM src,SQInteger idx);

/*object creation handling*/
SQUserPointer sq_newuserdata(HSQUIRRELVM v,SQUnsignedInteger size);
void sq_newtable(HSQUIRRELVM v);
void sq_newarray(HSQUIRRELVM v,SQInteger size);
void sq_newclosure(HSQUIRRELVM v,SQFUNCTION func,SQUnsignedInteger nfreevars);
SQRESULT sq_setparamscheck(HSQUIRRELVM v,SQInteger nparamscheck,const SQChar *typemask);
SQRESULT sq_bindenv(HSQUIRRELVM v,SQInteger idx);
void sq_pushstring(HSQUIRRELVM v,const SQChar *s,SQInteger len);
static inline void sq_pushstring(HSQUIRRELVM v, const std::string &str, SQInteger len = -1) { sq_pushstring(v, str.data(), len == -1 ? str.size() : len); }
void sq_pushfloat(HSQUIRRELVM v,SQFloat f);
void sq_pushinteger(HSQUIRRELVM v,SQInteger n);
void sq_pushbool(HSQUIRRELVM v,SQBool b);
void sq_pushuserpointer(HSQUIRRELVM v,SQUserPointer p);
void sq_pushnull(HSQUIRRELVM v);
SQObjectType sq_gettype(HSQUIRRELVM v,SQInteger idx);
SQInteger sq_getsize(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_getbase(HSQUIRRELVM v,SQInteger idx);
SQBool sq_instanceof(HSQUIRRELVM v);
void sq_tostring(HSQUIRRELVM v,SQInteger idx);
void sq_tobool(HSQUIRRELVM v, SQInteger idx, SQBool *b);
SQRESULT sq_getstring(HSQUIRRELVM v,SQInteger idx,const SQChar **c);
SQRESULT sq_getinteger(HSQUIRRELVM v,SQInteger idx,SQInteger *i);
SQRESULT sq_getfloat(HSQUIRRELVM v,SQInteger idx,SQFloat *f);
SQRESULT sq_getbool(HSQUIRRELVM v,SQInteger idx,SQBool *b);
SQRESULT sq_getthread(HSQUIRRELVM v,SQInteger idx,HSQUIRRELVM *thread);
SQRESULT sq_getuserpointer(HSQUIRRELVM v,SQInteger idx,SQUserPointer *p);
SQRESULT sq_getuserdata(HSQUIRRELVM v,SQInteger idx,SQUserPointer *p,SQUserPointer *typetag);
SQRESULT sq_settypetag(HSQUIRRELVM v,SQInteger idx,SQUserPointer typetag);
SQRESULT sq_gettypetag(HSQUIRRELVM v,SQInteger idx,SQUserPointer *typetag);
void sq_setreleasehook(HSQUIRRELVM v,SQInteger idx,SQRELEASEHOOK hook);
SQChar *sq_getscratchpad(HSQUIRRELVM v,SQInteger minsize);
SQRESULT sq_getfunctioninfo(HSQUIRRELVM v,SQInteger idx,SQFunctionInfo *fi);
SQRESULT sq_getclosureinfo(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger *nparams,SQUnsignedInteger *nfreevars);
SQRESULT sq_setnativeclosurename(HSQUIRRELVM v,SQInteger idx,const SQChar *name);
SQRESULT sq_setinstanceup(HSQUIRRELVM v, SQInteger idx, SQUserPointer p);
SQRESULT sq_getinstanceup(HSQUIRRELVM v, SQInteger idx, SQUserPointer *p,SQUserPointer typetag);
SQRESULT sq_setclassudsize(HSQUIRRELVM v, SQInteger idx, SQInteger udsize);
SQRESULT sq_newclass(HSQUIRRELVM v,SQBool hasbase);
SQRESULT sq_createinstance(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_setattributes(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_getattributes(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_getclass(HSQUIRRELVM v,SQInteger idx);
void sq_weakref(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_getdefaultdelegate(HSQUIRRELVM v,SQObjectType t);

/*object manipulation*/
void sq_pushroottable(HSQUIRRELVM v);
void sq_pushregistrytable(HSQUIRRELVM v);
void sq_pushconsttable(HSQUIRRELVM v);
SQRESULT sq_setroottable(HSQUIRRELVM v);
SQRESULT sq_setconsttable(HSQUIRRELVM v);
SQRESULT sq_newslot(HSQUIRRELVM v, SQInteger idx, SQBool bstatic);
SQRESULT sq_deleteslot(HSQUIRRELVM v,SQInteger idx,SQBool pushval);
SQRESULT sq_set(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_get(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_rawget(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_rawset(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_rawdeleteslot(HSQUIRRELVM v,SQInteger idx,SQBool pushval);
SQRESULT sq_arrayappend(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_arraypop(HSQUIRRELVM v,SQInteger idx,SQBool pushval);
SQRESULT sq_arrayresize(HSQUIRRELVM v,SQInteger idx,SQInteger newsize);
SQRESULT sq_arrayreverse(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_arrayremove(HSQUIRRELVM v,SQInteger idx,SQInteger itemidx);
SQRESULT sq_arrayinsert(HSQUIRRELVM v,SQInteger idx,SQInteger destpos);
SQRESULT sq_setdelegate(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_getdelegate(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_clone(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_setfreevariable(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger nval);
SQRESULT sq_next(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_getweakrefval(HSQUIRRELVM v,SQInteger idx);
SQRESULT sq_clear(HSQUIRRELVM v,SQInteger idx);

/*calls*/
SQRESULT sq_call(HSQUIRRELVM v,SQInteger params,SQBool retval,SQBool raiseerror, int suspend = -1);
SQRESULT sq_resume(HSQUIRRELVM v,SQBool retval,SQBool raiseerror);
const SQChar *sq_getlocal(HSQUIRRELVM v,SQUnsignedInteger level,SQUnsignedInteger idx);
const SQChar *sq_getfreevariable(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger nval);
SQRESULT sq_throwerror(HSQUIRRELVM v,const SQChar *err, SQInteger len = -1);
static inline SQRESULT sq_throwerror(HSQUIRRELVM v, const std::string_view err) { return sq_throwerror(v, err.data(), err.size()); }
void sq_reseterror(HSQUIRRELVM v);
void sq_getlasterror(HSQUIRRELVM v);

/*raw object handling*/
SQRESULT sq_getstackobj(HSQUIRRELVM v,SQInteger idx,HSQOBJECT *po);
void sq_pushobject(HSQUIRRELVM v,HSQOBJECT obj);
void sq_addref(HSQUIRRELVM v,HSQOBJECT *po);
SQBool sq_release(HSQUIRRELVM v,HSQOBJECT *po);
void sq_resetobject(HSQOBJECT *po);
const SQChar *sq_objtostring(HSQOBJECT *o);
SQBool sq_objtobool(HSQOBJECT *o);
SQInteger sq_objtointeger(HSQOBJECT *o);
SQFloat sq_objtofloat(HSQOBJECT *o);
SQRESULT sq_getobjtypetag(HSQOBJECT *o,SQUserPointer * typetag);

/*GC*/
SQInteger sq_collectgarbage(HSQUIRRELVM v);

/*serialization*/
SQRESULT sq_writeclosure(HSQUIRRELVM vm,SQWRITEFUNC writef,SQUserPointer up);
SQRESULT sq_readclosure(HSQUIRRELVM vm,SQREADFUNC readf,SQUserPointer up);

/*mem allocation*/
void *sq_malloc(SQUnsignedInteger size);
void *sq_realloc(void* p,SQUnsignedInteger oldsize,SQUnsignedInteger newsize);
void sq_free(void *p,SQUnsignedInteger size);

/*debug*/
SQRESULT sq_stackinfos(HSQUIRRELVM v,SQInteger level,SQStackInfos *si);
void sq_setdebughook(HSQUIRRELVM v);

/*UTILITY MACRO*/
#define sq_isnumeric(o) ((o)._type&SQOBJECT_NUMERIC)
#define sq_istable(o) ((o)._type==OT_TABLE)
#define sq_isarray(o) ((o)._type==OT_ARRAY)
#define sq_isfunction(o) ((o)._type==OT_FUNCPROTO)
#define sq_isclosure(o) ((o)._type==OT_CLOSURE)
#define sq_isgenerator(o) ((o)._type==OT_GENERATOR)
#define sq_isnativeclosure(o) ((o)._type==OT_NATIVECLOSURE)
#define sq_isstring(o) ((o)._type==OT_STRING)
#define sq_isinteger(o) ((o)._type==OT_INTEGER)
#define sq_isfloat(o) ((o)._type==OT_FLOAT)
#define sq_isuserpointer(o) ((o)._type==OT_USERPOINTER)
#define sq_isuserdata(o) ((o)._type==OT_USERDATA)
#define sq_isthread(o) ((o)._type==OT_THREAD)
#define sq_isnull(o) ((o)._type==OT_NULL)
#define sq_isclass(o) ((o)._type==OT_CLASS)
#define sq_isinstance(o) ((o)._type==OT_INSTANCE)
#define sq_isbool(o) ((o)._type==OT_BOOL)
#define sq_isweakref(o) ((o)._type==OT_WEAKREF)
#define sq_type(o) ((o)._type)

/* deprecated */
#define sq_createslot(v,n) sq_newslot(v,n,SQFalse)

#define SQ_OK (0)
#define SQ_ERROR (-1)

#define SQ_FAILED(res) (res<0)
#define SQ_SUCCEEDED(res) (res>=0)

#endif /*_SQUIRREL_H_*/
