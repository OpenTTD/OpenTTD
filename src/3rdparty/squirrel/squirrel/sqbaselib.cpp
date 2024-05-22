/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"
#include "../../fmt/format.h"

#include "sqpcheader.h"
#include "sqvm.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqarray.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include "sqclass.h"
#include <ctype.h>

#include "../../../safeguards.h"

bool str2num(const SQChar *s,SQObjectPtr &res)
{
	SQChar *end;
	if(strstr(s,".")){
		SQFloat r = SQFloat(strtod(s,&end));
		if(s == end) return false;
		res = r;
		return true;
	}
	else{
		SQInteger r = SQInteger(strtol(s,&end,10));
		if(s == end) return false;
		res = r;
		return true;
	}
}

#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
static SQInteger base_dummy(HSQUIRRELVM v)
{
	return 0;
}

#ifndef NO_GARBAGE_COLLECTOR
static SQInteger base_collectgarbage(HSQUIRRELVM v)
{
	sq_pushinteger(v, sq_collectgarbage(v));
	return 1;
}
#endif

static SQInteger base_getroottable(HSQUIRRELVM v)
{
	v->Push(v->_roottable);
	return 1;
}

static SQInteger base_getconsttable(HSQUIRRELVM v)
{
	v->Push(_ss(v)->_consts);
	return 1;
}


static SQInteger base_setroottable(HSQUIRRELVM v)
{
	SQObjectPtr &o=stack_get(v,2);
	if(SQ_FAILED(sq_setroottable(v))) return SQ_ERROR;
	v->Push(o);
	return 1;
}

static SQInteger base_setconsttable(HSQUIRRELVM v)
{
	SQObjectPtr &o=stack_get(v,2);
	if(SQ_FAILED(sq_setconsttable(v))) return SQ_ERROR;
	v->Push(o);
	return 1;
}

static SQInteger base_seterrorhandler(HSQUIRRELVM v)
{
	sq_seterrorhandler(v);
	return 0;
}

static SQInteger base_setdebughook(HSQUIRRELVM v)
{
	sq_setdebughook(v);
	return 0;
}

static SQInteger base_enabledebuginfo(HSQUIRRELVM v)
{
	SQObjectPtr &o=stack_get(v,2);
	sq_enabledebuginfo(v,(type(o) != OT_NULL)?1:0);
	return 0;
}

static SQInteger base_getstackinfos(HSQUIRRELVM v)
{
	SQInteger level;
	SQStackInfos si;
	SQInteger seq = 0;
	const SQChar *name = nullptr;
	sq_getinteger(v, -1, &level);
	if (SQ_SUCCEEDED(sq_stackinfos(v, level, &si)))
	{
		const SQChar *fn = "unknown";
		const SQChar *src = "unknown";
		if(si.funcname)fn = si.funcname;
		if(si.source)src = si.source;
		sq_newtable(v);
		sq_pushstring(v, "func", -1);
		sq_pushstring(v, fn, -1);
		sq_createslot(v, -3);
		sq_pushstring(v, "src", -1);
		sq_pushstring(v, src, -1);
		sq_createslot(v, -3);
		sq_pushstring(v, "line", -1);
		sq_pushinteger(v, si.line);
		sq_createslot(v, -3);
		sq_pushstring(v, "locals", -1);
		sq_newtable(v);
		seq=0;
		while ((name = sq_getlocal(v, level, seq))) {
			sq_pushstring(v, name, -1);
			sq_push(v, -2);
			sq_createslot(v, -4);
			sq_pop(v, 1);
			seq++;
		}
		sq_createslot(v, -3);
		return 1;
	}

	return 0;
}
#endif /* EXPORT_DEFAULT_SQUIRREL_FUNCTIONS */

static SQInteger base_assert(HSQUIRRELVM v)
{
	if(v->IsFalse(stack_get(v,2))){
		return sq_throwerror(v,"assertion failed");
	}
	return 0;
}

static SQInteger get_slice_params(HSQUIRRELVM v,SQInteger &sidx,SQInteger &eidx,SQObjectPtr &o)
{
	SQInteger top = sq_gettop(v);
	sidx=0;
	eidx=0;
	o=stack_get(v,1);
	SQObjectPtr &start=stack_get(v,2);
	if(type(start)!=OT_NULL && sq_isnumeric(start)){
		sidx=tointeger(start);
	}
	if(top>2){
		SQObjectPtr &end=stack_get(v,3);
		if(sq_isnumeric(end)){
			eidx=tointeger(end);
		}
	}
	else {
		eidx = sq_getsize(v,1);
	}
	return 1;
}

static SQInteger base_print(HSQUIRRELVM v)
{
	const SQChar *str;
	sq_tostring(v,2);
	sq_getstring(v,-1,&str);
	if(_ss(v)->_printfunc) _ss(v)->_printfunc(v,str);
	return 0;
}

#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
static SQInteger base_compilestring(HSQUIRRELVM v)
{
	SQInteger nargs=sq_gettop(v);
	const SQChar *src=nullptr,*name="unnamedbuffer";
	SQInteger size;
	sq_getstring(v,2,&src);
	size=sq_getsize(v,2);
	if(nargs>2){
		sq_getstring(v,3,&name);
	}
	if(SQ_SUCCEEDED(sq_compilebuffer(v,src,size,name,SQFalse)))
		return 1;
	else
		return SQ_ERROR;
}

static SQInteger base_newthread(HSQUIRRELVM v)
{
	SQObjectPtr &func = stack_get(v,2);
	SQInteger stksize = (_funcproto(_closure(func)->_function)->_stacksize << 1) +2;
	HSQUIRRELVM newv = sq_newthread(v, (stksize < MIN_STACK_OVERHEAD + 2)? MIN_STACK_OVERHEAD + 2 : stksize);
	sq_move(newv,v,-2);
	return 1;
}

static SQInteger base_suspend(HSQUIRRELVM v)
{
	return sq_suspendvm(v);
}
#endif /* EXPORT_DEFAULT_SQUIRREL_FUNCTIONS */

static SQInteger base_array(HSQUIRRELVM v)
{
	SQArray *a;
	SQInteger nInitialSize = tointeger(stack_get(v,2));
	SQInteger ret = 1;
	if (nInitialSize < 0) {
		v->Raise_Error(fmt::format("can't create/resize array with/to size {}", nInitialSize));
		nInitialSize = 0;
		ret = -1;
	}
	if(sq_gettop(v) > 2) {
		a = SQArray::Create(_ss(v),0);
		a->Resize(nInitialSize,stack_get(v,3));
	}
	else {
		a = SQArray::Create(_ss(v),nInitialSize);
	}
	v->Push(a);
	return ret;
}

static SQInteger base_type(HSQUIRRELVM v)
{
	SQObjectPtr &o = stack_get(v,2);
	v->Push(SQString::Create(_ss(v),GetTypeName(o),-1));
	return 1;
}

static SQRegFunction base_funcs[]={
	//generic
#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
	{"seterrorhandler",base_seterrorhandler,2, nullptr},
	{"setdebughook",base_setdebughook,2, nullptr},
	{"enabledebuginfo",base_enabledebuginfo,2, nullptr},
	{"getstackinfos",base_getstackinfos,2, ".n"},
	{"getroottable",base_getroottable,1, nullptr},
	{"setroottable",base_setroottable,2, nullptr},
	{"getconsttable",base_getconsttable,1, nullptr},
	{"setconsttable",base_setconsttable,2, nullptr},
#endif
	{"assert",base_assert,2, nullptr},
	{"print",base_print,2, nullptr},
#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
	{"compilestring",base_compilestring,-2, ".ss"},
	{"newthread",base_newthread,2, ".c"},
	{"suspend",base_suspend,-1, nullptr},
#endif
	{"array",base_array,-2, ".n"},
	{"type",base_type,2, nullptr},
#ifdef EXPORT_DEFAULT_SQUIRREL_FUNCTIONS
	{"dummy",base_dummy,0,nullptr},
#ifndef NO_GARBAGE_COLLECTOR
	{"collectgarbage",base_collectgarbage,1, "t"},
#endif
#endif
	{nullptr,nullptr,0,nullptr}
};

void sq_base_register(HSQUIRRELVM v)
{
	SQInteger i=0;
	sq_pushroottable(v);
	while(base_funcs[i].name!=nullptr) {
		sq_pushstring(v,base_funcs[i].name,-1);
		sq_newclosure(v,base_funcs[i].f,0);
		sq_setnativeclosurename(v,-1,base_funcs[i].name);
		sq_setparamscheck(v,base_funcs[i].nparamscheck,base_funcs[i].typemask);
		sq_createslot(v,-3);
		i++;
	}
	sq_pushstring(v,"_version_",-1);
	sq_pushstring(v,SQUIRREL_VERSION,-1);
	sq_createslot(v,-3);
	sq_pushstring(v,"_charsize_",-1);
	sq_pushinteger(v,sizeof(SQChar));
	sq_createslot(v,-3);
	sq_pushstring(v,"_intsize_",-1);
	sq_pushinteger(v,sizeof(SQInteger));
	sq_createslot(v,-3);
	sq_pushstring(v,"_floatsize_",-1);
	sq_pushinteger(v,sizeof(SQFloat));
	sq_createslot(v,-3);
	sq_pop(v,1);
}

static SQInteger default_delegate_len(HSQUIRRELVM v)
{
	v->Push(SQInteger(sq_getsize(v,1)));
	return 1;
}

static SQInteger default_delegate_tofloat(HSQUIRRELVM v)
{
	SQObjectPtr &o=stack_get(v,1);
	switch(type(o)){
	case OT_STRING:{
		SQObjectPtr res;
		if(str2num(_stringval(o),res)){
			v->Push(SQObjectPtr(tofloat(res)));
			break;
		}}
		return sq_throwerror(v, "cannot convert the string");
		break;
	case OT_INTEGER:case OT_FLOAT:
		v->Push(SQObjectPtr(tofloat(o)));
		break;
	case OT_BOOL:
		v->Push(SQObjectPtr((SQFloat)(_integer(o)?1:0)));
		break;
	default:
		v->Push(_null_);
		break;
	}
	return 1;
}

static SQInteger default_delegate_tointeger(HSQUIRRELVM v)
{
	SQObjectPtr &o=stack_get(v,1);
	switch(type(o)){
	case OT_STRING:{
		SQObjectPtr res;
		if(str2num(_stringval(o),res)){
			v->Push(SQObjectPtr(tointeger(res)));
			break;
		}}
		return sq_throwerror(v, "cannot convert the string");
		break;
	case OT_INTEGER:case OT_FLOAT:
		v->Push(SQObjectPtr(tointeger(o)));
		break;
	case OT_BOOL:
		v->Push(SQObjectPtr(_integer(o)?(SQInteger)1:(SQInteger)0));
		break;
	default:
		v->Push(_null_);
		break;
	}
	return 1;
}

static SQInteger default_delegate_tostring(HSQUIRRELVM v)
{
	sq_tostring(v,1);
	return 1;
}

static SQInteger obj_delegate_weakref(HSQUIRRELVM v)
{
	sq_weakref(v,1);
	return 1;
}

static SQInteger obj_clear(HSQUIRRELVM v)
{
	return sq_clear(v,-1);
}


static SQInteger number_delegate_tochar(HSQUIRRELVM v)
{
	SQObject &o=stack_get(v,1);
	SQChar c = (SQChar)tointeger(o);
	v->Push(SQString::Create(_ss(v),(const SQChar *)&c,1));
	return 1;
}


/////////////////////////////////////////////////////////////////
//TABLE DEFAULT DELEGATE

static SQInteger table_rawdelete(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_rawdeleteslot(v,1,SQTrue)))
		return SQ_ERROR;
	return 1;
}


static SQInteger container_rawexists(HSQUIRRELVM v)
{
	if(SQ_SUCCEEDED(sq_rawget(v,-2))) {
		sq_pushbool(v,SQTrue);
		return 1;
	}
	sq_pushbool(v,SQFalse);
	return 1;
}

static SQInteger table_rawset(HSQUIRRELVM v)
{
	return sq_rawset(v,-3);
}


static SQInteger table_rawget(HSQUIRRELVM v)
{
	return SQ_SUCCEEDED(sq_rawget(v,-2))?1:SQ_ERROR;
}


SQRegFunction SQSharedState::_table_default_delegate_funcz[]={
	{"len",default_delegate_len,1, "t"},
	{"rawget",table_rawget,2, "t"},
	{"rawset",table_rawset,3, "t"},
	{"rawdelete",table_rawdelete,2, "t"},
	{"rawin",container_rawexists,2, "t"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{"clear",obj_clear,1, "."},
	{nullptr,nullptr,0,nullptr}
};

//ARRAY DEFAULT DELEGATE///////////////////////////////////////

static SQInteger array_append(HSQUIRRELVM v)
{
	return sq_arrayappend(v,-2);
}

static SQInteger array_extend(HSQUIRRELVM v)
{
	_array(stack_get(v,1))->Extend(_array(stack_get(v,2)));
	return 0;
}

static SQInteger array_reverse(HSQUIRRELVM v)
{
	return sq_arrayreverse(v,-1);
}

static SQInteger array_pop(HSQUIRRELVM v)
{
	return SQ_SUCCEEDED(sq_arraypop(v,1,SQTrue))?1:SQ_ERROR;
}

static SQInteger array_top(HSQUIRRELVM v)
{
	SQObject &o=stack_get(v,1);
	if(_array(o)->Size()>0){
		v->Push(_array(o)->Top());
		return 1;
	}
	else return sq_throwerror(v,"top() on a empty array");
}

static SQInteger array_insert(HSQUIRRELVM v)
{
	SQObject &o=stack_get(v,1);
	SQObject &idx=stack_get(v,2);
	SQObject &val=stack_get(v,3);
	if(!_array(o)->Insert(tointeger(idx),val))
		return sq_throwerror(v,"index out of range");
	return 0;
}

static SQInteger array_remove(HSQUIRRELVM v)
{
	SQObject &o = stack_get(v, 1);
	SQObject &idx = stack_get(v, 2);
	if(!sq_isnumeric(idx)) return sq_throwerror(v, "wrong type");
	SQObjectPtr val;
	if(_array(o)->Get(tointeger(idx), val)) {
		_array(o)->Remove(tointeger(idx));
		v->Push(val);
		return 1;
	}
	return sq_throwerror(v, "idx out of range");
}

static SQInteger array_resize(HSQUIRRELVM v)
{
	SQObject &o = stack_get(v, 1);
	SQObject &nsize = stack_get(v, 2);
	SQObjectPtr fill;
	if(sq_isnumeric(nsize)) {
		if(sq_gettop(v) > 2)
			fill = stack_get(v, 3);
		_array(o)->Resize(tointeger(nsize),fill);
		return 0;
	}
	return sq_throwerror(v, "size must be a number");
}


bool _sort_compare(HSQUIRRELVM v,SQObjectPtr &a,SQObjectPtr &b,SQInteger func,SQInteger &ret)
{
	if(func < 0) {
		if(!v->ObjCmp(a,b,ret)) return false;
	}
	else {
		SQInteger top = sq_gettop(v);
		sq_push(v, func);
		sq_pushroottable(v);
		v->Push(a);
		v->Push(b);
		if(SQ_FAILED(sq_call(v, 3, SQTrue, SQFalse))) {
			if(!sq_isstring( v->_lasterror))
				v->Raise_Error("compare func failed");
			return false;
		}
		if(SQ_FAILED(sq_getinteger(v, -1, &ret))) {
			v->Raise_Error("numeric value expected as return value of the compare function");
			return false;
		}
		sq_settop(v, top);
		return true;
	}
	return true;
}

bool _hsort_sift_down(HSQUIRRELVM v,SQArray *arr, SQInteger root, SQInteger bottom, SQInteger func)
{
	SQInteger maxChild;
	SQInteger done = 0;
	SQInteger ret;
	SQInteger root2;
	while (((root2 = root * 2) <= bottom) && (!done))
	{
		if (root2 == bottom) {
			maxChild = root2;
		}
		else {
			if(!_sort_compare(v,arr->_values[root2],arr->_values[root2 + 1],func,ret))
				return false;
			if (ret > 0) {
				maxChild = root2;
			}
			else {
				maxChild = root2 + 1;
			}

		}

		if(!_sort_compare(v,arr->_values[root],arr->_values[maxChild],func,ret))
			return false;
		if (ret < 0) {
			if (root == maxChild) {
				v->Raise_Error("inconsistent compare function");
				return false; // We'd be swapping ourselve. The compare function is incorrect
			}
			_Swap(arr->_values[root],arr->_values[maxChild]);
			root = maxChild;
		}
		else {
			done = 1;
		}
	}
	return true;
}

bool _hsort(HSQUIRRELVM v,SQObjectPtr &arr, SQInteger, SQInteger,SQInteger func)
{
	SQArray *a = _array(arr);
	SQInteger i;
	SQInteger array_size = a->Size();
	for (i = (array_size / 2); i >= 0; i--) {
		if(!_hsort_sift_down(v,a, i, array_size - 1,func)) return false;
	}

	for (i = array_size-1; i >= 1; i--)
	{
		_Swap(a->_values[0],a->_values[i]);
		if(!_hsort_sift_down(v,a, 0, i-1,func)) return false;
	}
	return true;
}

static SQInteger array_sort(HSQUIRRELVM v)
{
	SQInteger func = -1;
	SQObjectPtr &o = stack_get(v,1);
	if(_array(o)->Size() > 1) {
		if(sq_gettop(v) == 2) func = 2;
		if(!_hsort(v, o, 0, _array(o)->Size()-1, func))
			return SQ_ERROR;

	}
	return 0;
}

static SQInteger array_slice(HSQUIRRELVM v)
{
	SQInteger sidx,eidx;
	SQObjectPtr o;
	if(get_slice_params(v,sidx,eidx,o)==-1)return -1;
	SQInteger alen = _array(o)->Size();
	if(sidx < 0)sidx = alen + sidx;
	if(eidx < 0)eidx = alen + eidx;
	if(eidx < sidx)return sq_throwerror(v,"wrong indexes");
	if(eidx > alen)return sq_throwerror(v,"slice out of range");
	SQArray *arr=SQArray::Create(_ss(v),eidx-sidx);
	SQObjectPtr t;
	SQInteger count=0;
	for(SQInteger i=sidx;i<eidx;i++){
		_array(o)->Get(i,t);
		arr->Set(count++,t);
	}
	v->Push(arr);
	return 1;

}

SQRegFunction SQSharedState::_array_default_delegate_funcz[]={
	{"len",default_delegate_len,1, "a"},
	{"append",array_append,2, "a"},
	{"extend",array_extend,2, "aa"},
	{"push",array_append,2, "a"},
	{"pop",array_pop,1, "a"},
	{"top",array_top,1, "a"},
	{"insert",array_insert,3, "an"},
	{"remove",array_remove,2, "an"},
	{"resize",array_resize,-2, "an"},
	{"reverse",array_reverse,1, "a"},
	{"sort",array_sort,-1, "ac"},
	{"slice",array_slice,-1, "ann"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{"clear",obj_clear,1, "."},
	{nullptr,nullptr,0,nullptr}
};

//STRING DEFAULT DELEGATE//////////////////////////
static SQInteger string_slice(HSQUIRRELVM v)
{
	SQInteger sidx,eidx;
	SQObjectPtr o;
	if(SQ_FAILED(get_slice_params(v,sidx,eidx,o)))return -1;
	SQInteger slen = _string(o)->_len;
	if(sidx < 0)sidx = slen + sidx;
	if(eidx < 0)eidx = slen + eidx;
	if(eidx < sidx)	return sq_throwerror(v,"wrong indexes");
	if(eidx > slen)	return sq_throwerror(v,"slice out of range");
	v->Push(SQString::Create(_ss(v),&_stringval(o)[sidx],eidx-sidx));
	return 1;
}

static SQInteger string_find(HSQUIRRELVM v)
{
	SQInteger top,start_idx=0;
	const SQChar *str,*substr,*ret;
	if(((top=sq_gettop(v))>1) && SQ_SUCCEEDED(sq_getstring(v,1,&str)) && SQ_SUCCEEDED(sq_getstring(v,2,&substr))){
		if(top>2)sq_getinteger(v,3,&start_idx);
		if((sq_getsize(v,1)>start_idx) && (start_idx>=0)){
			ret=strstr(&str[start_idx],substr);
			if(ret){
				sq_pushinteger(v,(SQInteger)(ret-str));
				return 1;
			}
		}
		return 0;
	}
	return sq_throwerror(v,"invalid param");
}

#define STRING_TOFUNCZ(func) static SQInteger string_##func(HSQUIRRELVM v) \
{ \
	SQObject str=stack_get(v,1); \
	SQInteger len=_string(str)->_len; \
	const SQChar *sThis=_stringval(str); \
	SQChar *sNew=(_ss(v)->GetScratchPad(len)); \
	for(SQInteger i=0;i<len;i++) sNew[i]=func(sThis[i]); \
	v->Push(SQString::Create(_ss(v),sNew,len)); \
	return 1; \
}


STRING_TOFUNCZ(tolower)
STRING_TOFUNCZ(toupper)

SQRegFunction SQSharedState::_string_default_delegate_funcz[]={
	{"len",default_delegate_len,1, "s"},
	{"tointeger",default_delegate_tointeger,1, "s"},
	{"tofloat",default_delegate_tofloat,1, "s"},
	{"tostring",default_delegate_tostring,1, "."},
	{"slice",string_slice,-1, " s n  n"},
	{"find",string_find,-2, "s s n "},
	{"tolower",string_tolower,1, "s"},
	{"toupper",string_toupper,1, "s"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{nullptr,nullptr,0,nullptr}
};

//INTEGER DEFAULT DELEGATE//////////////////////////
SQRegFunction SQSharedState::_number_default_delegate_funcz[]={
	{"tointeger",default_delegate_tointeger,1, "n|b"},
	{"tofloat",default_delegate_tofloat,1, "n|b"},
	{"tostring",default_delegate_tostring,1, "."},
	{"tochar",number_delegate_tochar,1, "n|b"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{nullptr,nullptr,0,nullptr}
};

//CLOSURE DEFAULT DELEGATE//////////////////////////
static SQInteger closure_pcall(HSQUIRRELVM v)
{
	return SQ_SUCCEEDED(sq_call(v,sq_gettop(v)-1,SQTrue,SQFalse))?1:SQ_ERROR;
}

static SQInteger closure_call(HSQUIRRELVM v)
{
	return SQ_SUCCEEDED(sq_call(v,sq_gettop(v)-1,SQTrue,SQTrue))?1:SQ_ERROR;
}

static SQInteger _closure_acall(HSQUIRRELVM v,SQBool raiseerror)
{
	SQArray *aparams=_array(stack_get(v,2));
	SQInteger nparams=aparams->Size();
	v->Push(stack_get(v,1));
	for(SQInteger i=0;i<nparams;i++)v->Push(aparams->_values[i]);
	return SQ_SUCCEEDED(sq_call(v,nparams,SQTrue,raiseerror))?1:SQ_ERROR;
}

static SQInteger closure_acall(HSQUIRRELVM v)
{
	return _closure_acall(v,SQTrue);
}

static SQInteger closure_pacall(HSQUIRRELVM v)
{
	return _closure_acall(v,SQFalse);
}

static SQInteger closure_bindenv(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_bindenv(v,1)))
		return SQ_ERROR;
	return 1;
}

static SQInteger closure_getinfos(HSQUIRRELVM v) {
	SQObject o = stack_get(v,1);
	SQTable *res = SQTable::Create(_ss(v),4);
	if(type(o) == OT_CLOSURE) {
		SQFunctionProto *f = _funcproto(_closure(o)->_function);
		SQInteger nparams = f->_nparameters + (f->_varparams?1:0);
		SQObjectPtr params = SQArray::Create(_ss(v),nparams);
		for(SQInteger n = 0; n<f->_nparameters; n++) {
			_array(params)->Set((SQInteger)n,f->_parameters[n]);
		}
		if(f->_varparams) {
			_array(params)->Set(nparams-1,SQString::Create(_ss(v),"...",-1));
		}
		res->NewSlot(SQString::Create(_ss(v),"native",-1),false);
		res->NewSlot(SQString::Create(_ss(v),"name",-1),f->_name);
		res->NewSlot(SQString::Create(_ss(v),"src",-1),f->_sourcename);
		res->NewSlot(SQString::Create(_ss(v),"parameters",-1),params);
		res->NewSlot(SQString::Create(_ss(v),"varargs",-1),f->_varparams);
	}
	else { //OT_NATIVECLOSURE
		SQNativeClosure *nc = _nativeclosure(o);
		res->NewSlot(SQString::Create(_ss(v),"native",-1),true);
		res->NewSlot(SQString::Create(_ss(v),"name",-1),nc->_name);
		res->NewSlot(SQString::Create(_ss(v),"paramscheck",-1),nc->_nparamscheck);
		SQObjectPtr typecheck;
		if(!nc->_typecheck.empty()) {
			typecheck =
				SQArray::Create(_ss(v), nc->_typecheck.size());
			for(SQUnsignedInteger n = 0; n<nc->_typecheck.size(); n++) {
					_array(typecheck)->Set((SQInteger)n,nc->_typecheck[n]);
			}
		}
		res->NewSlot(SQString::Create(_ss(v),"typecheck",-1),typecheck);
	}
	v->Push(res);
	return 1;
}


SQRegFunction SQSharedState::_closure_default_delegate_funcz[]={
	{"call",closure_call,-1, "c"},
	{"pcall",closure_pcall,-1, "c"},
	{"acall",closure_acall,2, "ca"},
	{"pacall",closure_pacall,2, "ca"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{"bindenv",closure_bindenv,2, "c x|y|t"},
	{"getinfos",closure_getinfos,1, "c"},
	{nullptr,nullptr,0,nullptr}
};

//GENERATOR DEFAULT DELEGATE
static SQInteger generator_getstatus(HSQUIRRELVM v)
{
	SQObject &o=stack_get(v,1);
	switch(_generator(o)->_state){
		case SQGenerator::eSuspended:v->Push(SQString::Create(_ss(v),"suspended"));break;
		case SQGenerator::eRunning:v->Push(SQString::Create(_ss(v),"running"));break;
		case SQGenerator::eDead:v->Push(SQString::Create(_ss(v),"dead"));break;
	}
	return 1;
}

SQRegFunction SQSharedState::_generator_default_delegate_funcz[]={
	{"getstatus",generator_getstatus,1, "g"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{nullptr,nullptr,0,nullptr}
};

//THREAD DEFAULT DELEGATE

static SQInteger thread_call(HSQUIRRELVM v)
{

	SQObjectPtr o = stack_get(v,1);
	if(type(o) == OT_THREAD) {
		SQInteger nparams = sq_gettop(v);
		_thread(o)->Push(_thread(o)->_roottable);
		for(SQInteger i = 2; i<(nparams+1); i++)
			sq_move(_thread(o),v,i);
		if(SQ_SUCCEEDED(sq_call(_thread(o),nparams,SQTrue,SQFalse))) {
			sq_move(v,_thread(o),-1);
			sq_pop(_thread(o),1);
			return 1;
		}
		v->_lasterror = _thread(o)->_lasterror;
		return SQ_ERROR;
	}
	return sq_throwerror(v,"wrong parameter");
}

static SQInteger thread_wakeup(HSQUIRRELVM v)
{
	SQObjectPtr o = stack_get(v,1);
	if(type(o) == OT_THREAD) {
		SQVM *thread = _thread(o);
		SQInteger state = sq_getvmstate(thread);
		if(state != SQ_VMSTATE_SUSPENDED) {
			switch(state) {
				case SQ_VMSTATE_IDLE:
					return sq_throwerror(v,"cannot wakeup a idle thread");
				break;
				case SQ_VMSTATE_RUNNING:
					return sq_throwerror(v,"cannot wakeup a running thread");
				break;
			}
		}

		SQInteger wakeupret = sq_gettop(v)>1?1:0;
		if(wakeupret) {
			sq_move(thread,v,2);
		}
		if(SQ_SUCCEEDED(sq_wakeupvm(thread,wakeupret,SQTrue,SQTrue,SQFalse))) {
			sq_move(v,thread,-1);
			sq_pop(thread,1); //pop retval
			if(sq_getvmstate(thread) == SQ_VMSTATE_IDLE) {
				sq_settop(thread,1); //pop roottable
			}
			return 1;
		}
		sq_settop(thread,1);
		v->_lasterror = thread->_lasterror;
		return SQ_ERROR;
	}
	return sq_throwerror(v,"wrong parameter");
}

static SQInteger thread_getstatus(HSQUIRRELVM v)
{
	SQObjectPtr &o = stack_get(v,1);
	switch(sq_getvmstate(_thread(o))) {
		case SQ_VMSTATE_IDLE:
			sq_pushstring(v,"idle",-1);
		break;
		case SQ_VMSTATE_RUNNING:
			sq_pushstring(v,"running",-1);
		break;
		case SQ_VMSTATE_SUSPENDED:
			sq_pushstring(v,"suspended",-1);
		break;
		default:
			return sq_throwerror(v,"internal VM error");
	}
	return 1;
}

SQRegFunction SQSharedState::_thread_default_delegate_funcz[] = {
	{"call", thread_call, -1, "v"},
	{"wakeup", thread_wakeup, -1, "v"},
	{"getstatus", thread_getstatus, 1, "v"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{nullptr,nullptr,0,nullptr},
};

static SQInteger class_getattributes(HSQUIRRELVM v)
{
	if(SQ_SUCCEEDED(sq_getattributes(v,-2)))
		return 1;
	return SQ_ERROR;
}

static SQInteger class_setattributes(HSQUIRRELVM v)
{
	if(SQ_SUCCEEDED(sq_setattributes(v,-3)))
		return 1;
	return SQ_ERROR;
}

static SQInteger class_instance(HSQUIRRELVM v)
{
	if(SQ_SUCCEEDED(sq_createinstance(v,-1)))
		return 1;
	return SQ_ERROR;
}

SQRegFunction SQSharedState::_class_default_delegate_funcz[] = {
	{"getattributes", class_getattributes, 2, "y."},
	{"setattributes", class_setattributes, 3, "y.."},
	{"rawin",container_rawexists,2, "y"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{"instance",class_instance,1, "y"},
	{nullptr,nullptr,0,nullptr}
};

static SQInteger instance_getclass(HSQUIRRELVM v)
{
	if(SQ_SUCCEEDED(sq_getclass(v,1)))
		return 1;
	return SQ_ERROR;
}

SQRegFunction SQSharedState::_instance_default_delegate_funcz[] = {
	{"getclass", instance_getclass, 1, "x"},
	{"rawin",container_rawexists,2, "x"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{nullptr,nullptr,0,nullptr}
};

static SQInteger weakref_ref(HSQUIRRELVM v)
{
	if(SQ_FAILED(sq_getweakrefval(v,1)))
		return SQ_ERROR;
	return 1;
}

SQRegFunction SQSharedState::_weakref_default_delegate_funcz[] = {
	{"ref",weakref_ref,1, "r"},
	{"weakref",obj_delegate_weakref,1, nullptr },
	{"tostring",default_delegate_tostring,1, "."},
	{nullptr,nullptr,0,nullptr}
};


