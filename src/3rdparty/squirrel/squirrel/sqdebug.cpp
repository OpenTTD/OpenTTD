/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"

#include <squirrel.h>
#include "sqpcheader.h"
#include "sqvm.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include "sqstring.h"

#include "../../../core/alloc_func.hpp"
#include "../../../string_func.h"

#include "../../../safeguards.h"

SQRESULT sq_getfunctioninfo(HSQUIRRELVM v,SQInteger level,SQFunctionInfo *fi)
{
	SQInteger cssize = v->_callsstacksize;
	if (cssize > level) {
		SQVM::CallInfo &ci = v->_callsstack[cssize-level-1];
		if(sq_isclosure(ci._closure)) {
			SQClosure *c = _closure(ci._closure);
			SQFunctionProto *proto = _funcproto(c->_function);
			fi->funcid = proto;
			fi->name = type(proto->_name) == OT_STRING?_stringval(proto->_name):"unknown";
			fi->source = type(proto->_name) == OT_STRING?_stringval(proto->_sourcename):"unknown";
			return SQ_OK;
		}
	}
	return sq_throwerror(v,"the object is not a closure");
}

SQRESULT sq_stackinfos(HSQUIRRELVM v, SQInteger level, SQStackInfos *si)
{
	SQInteger cssize = v->_callsstacksize;
	if (cssize > level) {
		memset(si, 0, sizeof(SQStackInfos));
		SQVM::CallInfo &ci = v->_callsstack[cssize-level-1];
		switch (type(ci._closure)) {
		case OT_CLOSURE:{
			SQFunctionProto *func = _funcproto(_closure(ci._closure)->_function);
			if (type(func->_name) == OT_STRING)
				si->funcname = _stringval(func->_name);
			if (type(func->_sourcename) == OT_STRING)
				si->source = _stringval(func->_sourcename);
			si->line = func->GetLine(ci._ip);
						}
			break;
		case OT_NATIVECLOSURE:
			si->source = "NATIVE";
			si->funcname = "unknown";
			if(type(_nativeclosure(ci._closure)->_name) == OT_STRING)
				si->funcname = _stringval(_nativeclosure(ci._closure)->_name);
			si->line = -1;
			break;
		default: break; //shutup compiler
		}
		return SQ_OK;
	}
	return SQ_ERROR;
}

void SQVM::Raise_Error(const SQChar *s, ...)
{
	va_list vl;
	va_start(vl, s);
	size_t len = strlen(s)+(NUMBER_MAX_CHAR*2);
	char *buffer = MallocT<char>(len + 1);
	vseprintf(buffer, buffer + len, s, vl);
	va_end(vl);
	_lasterror = SQString::Create(_ss(this),buffer,-1);
	free(buffer);
}

void SQVM::Raise_Error(SQObjectPtr &desc)
{
	_lasterror = desc;
}

SQString *SQVM::PrintObjVal(const SQObject &o)
{
	char buf[NUMBER_MAX_CHAR+1];
	switch(type(o)) {
	case OT_STRING: return _string(o);
	case OT_INTEGER:
		seprintf(buf, lastof(buf), OTTD_PRINTF64, _integer(o));
		return SQString::Create(_ss(this), buf);
	case OT_FLOAT:
		seprintf(buf, lastof(buf), "%.14g", _float(o));
		return SQString::Create(_ss(this), buf);
	default:
		return SQString::Create(_ss(this), GetTypeName(o));
	}
}

void SQVM::Raise_IdxError(const SQObject &o)
{
	SQObjectPtr oval = PrintObjVal(o);
	Raise_Error("the index '%.50s' does not exist", _stringval(oval));
}

void SQVM::Raise_CompareError(const SQObject &o1, const SQObject &o2)
{
	SQObjectPtr oval1 = PrintObjVal(o1), oval2 = PrintObjVal(o2);
	Raise_Error("comparison between '%.50s' and '%.50s'", _stringval(oval1), _stringval(oval2));
}


void SQVM::Raise_ParamTypeError(SQInteger nparam,SQInteger typemask,SQInteger type)
{
	SQObjectPtr exptypes = SQString::Create(_ss(this), "", -1);
	SQInteger found = 0;
	for(SQInteger i=0; i<16; i++)
	{
		SQInteger mask = 0x00000001LL << i;
		if(typemask & (mask)) {
			if(found>0) StringCat(exptypes,SQString::Create(_ss(this), "|", -1), exptypes);
			found ++;
			StringCat(exptypes,SQString::Create(_ss(this), IdType2Name((SQObjectType)mask), -1), exptypes);
		}
	}
	Raise_Error("parameter %d has an invalid type '%s' ; expected: '%s'", nparam, IdType2Name((SQObjectType)type), _stringval(exptypes));
}
