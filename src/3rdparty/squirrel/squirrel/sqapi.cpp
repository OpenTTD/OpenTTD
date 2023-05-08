/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"
#include "../../fmt/format.h"

#include <squirrel.h>
#include "sqpcheader.h"
#include "sqvm.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqarray.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include "squserdata.h"
#include "sqcompiler.h"
#include "sqfuncstate.h"
#include "sqclass.h"

#include "../../../string_func.h"

#include "../../../safeguards.h"

bool sq_aux_gettypedarg(HSQUIRRELVM v,SQInteger idx,SQObjectType type,SQObjectPtr **o)
{
	*o = &stack_get(v,idx);
	if(type(**o) != type){
		SQObjectPtr oval = v->PrintObjVal(**o);
		v->Raise_Error(fmt::format("wrong argument type, expected '{}' got '{:.50s}'",IdType2Name(type),_stringval(oval)));
		return false;
	}
	return true;
}

#define _GETSAFE_OBJ(v,idx,type,o) { if(!sq_aux_gettypedarg(v,idx,type,&o)) return SQ_ERROR; }

#define sq_aux_paramscheck(v,count) \
{ \
	if(sq_gettop(v) < count){ v->Raise_Error("not enough params in the stack"); return SQ_ERROR; }\
}

SQInteger sq_aux_throwobject(HSQUIRRELVM v,SQObjectPtr &e)
{
	v->_lasterror = e;
	return SQ_ERROR;
}

SQInteger sq_aux_invalidtype(HSQUIRRELVM v,SQObjectType type)
{
	return sq_throwerror(v, fmt::format("unexpected type {}", IdType2Name(type)));
}

HSQUIRRELVM sq_open(SQInteger initialstacksize)
{
	SQSharedState *ss;
	SQVM *v;
	sq_new(ss, SQSharedState);
	v = (SQVM *)SQ_MALLOC(sizeof(SQVM));
	new (v) SQVM(ss);
	ss->_root_vm = v;
	if(v->Init(nullptr, initialstacksize)) {
		return v;
	} else {
		sq_delete(v, SQVM);
		return nullptr;
	}
	return v;
}

HSQUIRRELVM sq_newthread(HSQUIRRELVM friendvm, SQInteger initialstacksize)
{
	SQSharedState *ss;
	SQVM *v;
	ss=_ss(friendvm);

	v= (SQVM *)SQ_MALLOC(sizeof(SQVM));
	new (v) SQVM(ss);

	if(v->Init(friendvm, initialstacksize)) {
		friendvm->Push(v);
		return v;
	} else {
		sq_delete(v, SQVM);
		return nullptr;
	}
}

SQInteger sq_getvmstate(HSQUIRRELVM v)
{
	if(v->_suspended)
		return SQ_VMSTATE_SUSPENDED;
	else {
		if(v->_callsstacksize != 0) return SQ_VMSTATE_RUNNING;
		else return SQ_VMSTATE_IDLE;
	}
}

void sq_decreaseops(HSQUIRRELVM v, int amount)
{
	v->DecreaseOps(amount);
}

bool sq_can_suspend(HSQUIRRELVM v)
{
	return v->_nnativecalls <= 2;
}

void sq_seterrorhandler(HSQUIRRELVM v)
{
	SQObject o = stack_get(v, -1);
	if(sq_isclosure(o) || sq_isnativeclosure(o) || sq_isnull(o)) {
		v->_errorhandler = o;
		v->Pop();
	}
}

void sq_setdebughook(HSQUIRRELVM v)
{
	SQObject o = stack_get(v,-1);
	if(sq_isclosure(o) || sq_isnativeclosure(o) || sq_isnull(o)) {
		v->_debughook = o;
		v->Pop();
	}
}

void sq_close(HSQUIRRELVM v)
{
	SQSharedState *ss = _ss(v);
	_thread(ss->_root_vm)->Finalize();
	sq_delete(ss, SQSharedState);
}

SQRESULT sq_compile(HSQUIRRELVM v,SQLEXREADFUNC read,SQUserPointer p,const SQChar *sourcename,SQBool raiseerror)
{
	SQObjectPtr o;
	if(Compile(v, read, p, sourcename, o, raiseerror != 0, _ss(v)->_debuginfo)) {
		v->Push(SQClosure::Create(_ss(v), _funcproto(o)));
		return SQ_OK;
	}
	return SQ_ERROR;
}

void sq_enabledebuginfo(HSQUIRRELVM v, SQBool enable)
{
	_ss(v)->_debuginfo = enable != 0;
}

void sq_notifyallexceptions(HSQUIRRELVM v, SQBool enable)
{
	_ss(v)->_notifyallexceptions = enable != 0;
}

void sq_addref(HSQUIRRELVM v,HSQOBJECT *po)
{
	if(!ISREFCOUNTED(type(*po))) return;
#ifdef NO_GARBAGE_COLLECTOR
	__AddRef(po->_type,po->_unVal);
#else
	_ss(v)->_refs_table.AddRef(*po);
#endif
}

SQBool sq_release(HSQUIRRELVM v,HSQOBJECT *po)
{
	if(!ISREFCOUNTED(type(*po))) return SQTrue;
#ifdef NO_GARBAGE_COLLECTOR
	__Release(po->_type,po->_unVal);
	return SQFalse; //the ret val doesn't work(and cannot be fixed)
#else
	return _ss(v)->_refs_table.Release(*po);
#endif
}

const SQChar *sq_objtostring(HSQOBJECT *o)
{
	if(sq_type(*o) == OT_STRING) {
		return _stringval(*o);
	}
	return nullptr;
}

SQInteger sq_objtointeger(HSQOBJECT *o)
{
	if(sq_isnumeric(*o)) {
		return tointeger(*o);
	}
	return 0;
}

SQFloat sq_objtofloat(HSQOBJECT *o)
{
	if(sq_isnumeric(*o)) {
		return tofloat(*o);
	}
	return 0;
}

SQBool sq_objtobool(HSQOBJECT *o)
{
	if(sq_isbool(*o)) {
		return _integer(*o);
	}
	return SQFalse;
}

void sq_pushnull(HSQUIRRELVM v)
{
	v->Push(_null_);
}

void sq_pushstring(HSQUIRRELVM v,const SQChar *s,SQInteger len)
{
	if(s)
		v->Push(SQObjectPtr(SQString::Create(_ss(v), s, len)));
	else v->Push(_null_);
}

void sq_pushinteger(HSQUIRRELVM v,SQInteger n)
{
	v->Push(n);
}

void sq_pushbool(HSQUIRRELVM v,SQBool b)
{
	v->Push(b != 0);
}

void sq_pushfloat(HSQUIRRELVM v,SQFloat n)
{
	v->Push(n);
}

void sq_pushuserpointer(HSQUIRRELVM v,SQUserPointer p)
{
	v->Push(p);
}

SQUserPointer sq_newuserdata(HSQUIRRELVM v,SQUnsignedInteger size)
{
	SQUserData *ud = SQUserData::Create(_ss(v), size);
	v->Push(ud);
	return ud->_val;
}

void sq_newtable(HSQUIRRELVM v)
{
	v->Push(SQTable::Create(_ss(v), 0));
}

void sq_newarray(HSQUIRRELVM v,SQInteger size)
{
	v->Push(SQArray::Create(_ss(v), size));
}

SQRESULT sq_newclass(HSQUIRRELVM v,SQBool hasbase)
{
	SQClass *baseclass = nullptr;
	if(hasbase) {
		SQObjectPtr &base = stack_get(v,-1);
		if(type(base) != OT_CLASS)
			return sq_throwerror(v,"invalid base type");
		baseclass = _class(base);
	}
	SQClass *newclass = SQClass::Create(_ss(v), baseclass);
	if(baseclass) v->Pop();
	v->Push(newclass);
	return SQ_OK;
}

SQBool sq_instanceof(HSQUIRRELVM v)
{
	SQObjectPtr &inst = stack_get(v,-1);
	SQObjectPtr &cl = stack_get(v,-2);
	if(type(inst) != OT_INSTANCE || type(cl) != OT_CLASS)
		return sq_throwerror(v,"invalid param type");
	return _instance(inst)->InstanceOf(_class(cl))?SQTrue:SQFalse;
}

SQRESULT sq_arrayappend(HSQUIRRELVM v,SQInteger idx)
{
	sq_aux_paramscheck(v,2);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	_array(*arr)->Append(v->GetUp(-1));
	v->Pop(1);
	return SQ_OK;
}

SQRESULT sq_arraypop(HSQUIRRELVM v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	if(_array(*arr)->Size() > 0) {
        if(pushval != 0){ v->Push(_array(*arr)->Top()); }
		_array(*arr)->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, "empty array");
}

SQRESULT sq_arrayresize(HSQUIRRELVM v,SQInteger idx,SQInteger newsize)
{
	sq_aux_paramscheck(v,1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	if(newsize >= 0) {
		_array(*arr)->Resize(newsize);
		return SQ_OK;
	}
	return sq_throwerror(v,"negative size");
}


SQRESULT sq_arrayreverse(HSQUIRRELVM v,SQInteger idx)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *o;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,o);
	SQArray *arr = _array(*o);
	if(arr->Size() > 0) {
		SQObjectPtr t;
		SQInteger size = arr->Size();
		SQInteger n = size >> 1; size -= 1;
		for(SQInteger i = 0; i < n; i++) {
			t = arr->_values[i];
			arr->_values[i] = arr->_values[size-i];
			arr->_values[size-i] = t;
		}
		return SQ_OK;
	}
	return SQ_OK;
}

SQRESULT sq_arrayremove(HSQUIRRELVM v,SQInteger idx,SQInteger itemidx)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	return _array(*arr)->Remove(itemidx) ? SQ_OK : sq_throwerror(v,"index out of range");
}

SQRESULT sq_arrayinsert(HSQUIRRELVM v,SQInteger idx,SQInteger destpos)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	SQRESULT ret = _array(*arr)->Insert(destpos, v->GetUp(-1)) ? SQ_OK : sq_throwerror(v,"index out of range");
	v->Pop();
	return ret;
}


void sq_newclosure(HSQUIRRELVM v,SQFUNCTION func,SQUnsignedInteger nfreevars)
{
	SQNativeClosure *nc = SQNativeClosure::Create(_ss(v), func);
	for(SQUnsignedInteger i = 0; i < nfreevars; i++) {
		nc->_outervalues.push_back(v->Top());
		v->Pop();
	}
	v->Push(SQObjectPtr(nc));
}

SQRESULT sq_getclosureinfo(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger *nparams,SQUnsignedInteger *nfreevars)
{
	SQObject o = stack_get(v, idx);
	if(sq_isclosure(o)) {
		SQClosure *c = _closure(o);
		SQFunctionProto *proto = _funcproto(c->_function);
		*nparams = (SQUnsignedInteger)proto->_nparameters;
        *nfreevars = (SQUnsignedInteger)c->_outervalues.size();
		return SQ_OK;
	}
	return sq_throwerror(v,"the object is not a closure");
}

SQRESULT sq_setnativeclosurename(HSQUIRRELVM v,SQInteger idx,const SQChar *name)
{
	SQObject o = stack_get(v, idx);
	if(sq_isnativeclosure(o)) {
		SQNativeClosure *nc = _nativeclosure(o);
		nc->_name = SQString::Create(_ss(v),name);
		return SQ_OK;
	}
	return sq_throwerror(v,"the object is not a nativeclosure");
}

SQRESULT sq_setparamscheck(HSQUIRRELVM v,SQInteger nparamscheck,const SQChar *typemask)
{
	SQObject o = stack_get(v, -1);
	if(!sq_isnativeclosure(o))
		return sq_throwerror(v, "native closure expected");
	SQNativeClosure *nc = _nativeclosure(o);
	nc->_nparamscheck = nparamscheck;
	if(typemask) {
		SQIntVec res;
		if(!CompileTypemask(res, typemask))
			return sq_throwerror(v, "invalid typemask");
		nc->_typecheck.copy(res);
	}
	else {
		nc->_typecheck.resize(0);
	}
	if(nparamscheck == SQ_MATCHTYPEMASKSTRING) {
		nc->_nparamscheck = nc->_typecheck.size();
	}
	return SQ_OK;
}

SQRESULT sq_bindenv(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(!sq_isnativeclosure(o) &&
		!sq_isclosure(o))
		return sq_throwerror(v,"the target is not a closure");
    SQObjectPtr &env = stack_get(v,-1);
	if(!sq_istable(env) &&
		!sq_isclass(env) &&
		!sq_isinstance(env))
		return sq_throwerror(v,"invalid environment");
	SQObjectPtr w = _refcounted(env)->GetWeakRef(type(env));
	SQObjectPtr ret;
	if(sq_isclosure(o)) {
		SQClosure *c = _closure(o)->Clone();
		c->_env = w;
		ret = c;
	}
	else { //then must be a native closure
		SQNativeClosure *c = _nativeclosure(o)->Clone();
		c->_env = w;
		ret = c;
	}
	v->Pop();
	v->Push(ret);
	return SQ_OK;
}

SQRESULT sq_clear(HSQUIRRELVM v,SQInteger idx)
{
	SQObject &o=stack_get(v,idx);
	switch(type(o)) {
		case OT_TABLE: _table(o)->Clear();	break;
		case OT_ARRAY: _array(o)->Resize(0); break;
		default:
			return sq_throwerror(v, "clear only works on table and array");
		break;

	}
	return SQ_OK;
}

void sq_pushroottable(HSQUIRRELVM v)
{
	v->Push(v->_roottable);
}

void sq_pushregistrytable(HSQUIRRELVM v)
{
	v->Push(_ss(v)->_registry);
}

void sq_pushconsttable(HSQUIRRELVM v)
{
	v->Push(_ss(v)->_consts);
}

SQRESULT sq_setroottable(HSQUIRRELVM v)
{
	SQObject o = stack_get(v, -1);
	if(sq_istable(o) || sq_isnull(o)) {
		v->_roottable = o;
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, "ivalid type");
}

SQRESULT sq_setconsttable(HSQUIRRELVM v)
{
	SQObject o = stack_get(v, -1);
	if(sq_istable(o)) {
		_ss(v)->_consts = o;
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, "ivalid type, expected table");
}

void sq_setforeignptr(HSQUIRRELVM v,SQUserPointer p)
{
	v->_foreignptr = p;
}

SQUserPointer sq_getforeignptr(HSQUIRRELVM v)
{
	return v->_foreignptr;
}

void sq_push(HSQUIRRELVM v,SQInteger idx)
{
	v->Push(stack_get(v, idx));
}

SQObjectType sq_gettype(HSQUIRRELVM v,SQInteger idx)
{
	return type(stack_get(v, idx));
}


void sq_tostring(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v, idx);
	SQObjectPtr res;
	v->ToString(o,res);
	v->Push(res);
}

void sq_tobool(HSQUIRRELVM v, SQInteger idx, SQBool *b)
{
	SQObjectPtr &o = stack_get(v, idx);
	*b = v->IsFalse(o)?SQFalse:SQTrue;
}

SQRESULT sq_getinteger(HSQUIRRELVM v,SQInteger idx,SQInteger *i)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(sq_isnumeric(o)) {
		*i = tointeger(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

SQRESULT sq_getfloat(HSQUIRRELVM v,SQInteger idx,SQFloat *f)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(sq_isnumeric(o)) {
		*f = tofloat(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

SQRESULT sq_getbool(HSQUIRRELVM v,SQInteger idx,SQBool *b)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(sq_isbool(o)) {
		*b = _integer(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

SQRESULT sq_getstring(HSQUIRRELVM v,SQInteger idx,const SQChar **c)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_STRING,o);
	*c = _stringval(*o);
	return SQ_OK;
}

SQRESULT sq_getthread(HSQUIRRELVM v,SQInteger idx,HSQUIRRELVM *thread)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_THREAD,o);
	*thread = _thread(*o);
	return SQ_OK;
}

SQRESULT sq_clone(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v,idx);
	v->Push(_null_);
	if(!v->Clone(o, stack_get(v, -1))){
		v->Pop();
		return sq_aux_invalidtype(v, type(o));
	}
	return SQ_OK;
}

SQInteger sq_getsize(HSQUIRRELVM v, SQInteger idx)
{
	SQObjectPtr &o = stack_get(v, idx);
	SQObjectType type = type(o);
	switch(type) {
	case OT_STRING:		return _string(o)->_len;
	case OT_TABLE:		return _table(o)->CountUsed();
	case OT_ARRAY:		return _array(o)->Size();
	case OT_USERDATA:	return _userdata(o)->_size;
	case OT_INSTANCE:	return _instance(o)->_class->_udsize;
	case OT_CLASS:		return _class(o)->_udsize;
	default:
		return sq_aux_invalidtype(v, type);
	}
}

SQRESULT sq_getuserdata(HSQUIRRELVM v,SQInteger idx,SQUserPointer *p,SQUserPointer *typetag)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_USERDATA,o);
	(*p) = _userdataval(*o);
	if(typetag) *typetag = _userdata(*o)->_typetag;
	return SQ_OK;
}

SQRESULT sq_settypetag(HSQUIRRELVM v,SQInteger idx,SQUserPointer typetag)
{
	SQObjectPtr &o = stack_get(v,idx);
	switch(type(o)) {
		case OT_USERDATA:	_userdata(o)->_typetag = typetag;	break;
		case OT_CLASS:		_class(o)->_typetag = typetag;		break;
		default:			return sq_throwerror(v,"invalid object type");
	}
	return SQ_OK;
}

SQRESULT sq_getobjtypetag(HSQOBJECT *o,SQUserPointer * typetag)
{
  switch(type(*o)) {
    case OT_INSTANCE: *typetag = _instance(*o)->_class->_typetag; break;
    case OT_USERDATA: *typetag = _userdata(*o)->_typetag; break;
    case OT_CLASS:    *typetag = _class(*o)->_typetag; break;
    default: return SQ_ERROR;
  }
  return SQ_OK;
}

SQRESULT sq_gettypetag(HSQUIRRELVM v,SQInteger idx,SQUserPointer *typetag)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(SQ_FAILED(sq_getobjtypetag(&o,typetag)))
		return sq_throwerror(v,"invalid object type");
	return SQ_OK;
}

SQRESULT sq_getuserpointer(HSQUIRRELVM v, SQInteger idx, SQUserPointer *p)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_USERPOINTER,o);
	(*p) = _userpointer(*o);
	return SQ_OK;
}

SQRESULT sq_setinstanceup(HSQUIRRELVM v, SQInteger idx, SQUserPointer p)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_INSTANCE) return sq_throwerror(v,"the object is not a class instance");
	_instance(o)->_userpointer = p;
	return SQ_OK;
}

SQRESULT sq_setclassudsize(HSQUIRRELVM v, SQInteger idx, SQInteger udsize)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_CLASS) return sq_throwerror(v,"the object is not a class");
	if(_class(o)->_locked) return sq_throwerror(v,"the class is locked");
	_class(o)->_udsize = udsize;
	return SQ_OK;
}


SQRESULT sq_getinstanceup(HSQUIRRELVM v, SQInteger idx, SQUserPointer *p,SQUserPointer typetag)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_INSTANCE) return sq_throwerror(v,"the object is not a class instance");
	(*p) = _instance(o)->_userpointer;
	if(typetag != nullptr) {
		SQClass *cl = _instance(o)->_class;
		do{
			if(cl->_typetag == typetag)
				return SQ_OK;
			cl = cl->_base;
		}while(cl != nullptr);
		return sq_throwerror(v,"invalid type tag");
	}
	return SQ_OK;
}

SQInteger sq_gettop(HSQUIRRELVM v)
{
	return (v->_top) - v->_stackbase;
}

void sq_settop(HSQUIRRELVM v, SQInteger newtop)
{
	SQInteger top = sq_gettop(v);
	if(top > newtop)
		sq_pop(v, top - newtop);
	else
		while(top++ < newtop) sq_pushnull(v);
}

void sq_pop(HSQUIRRELVM v, SQInteger nelemstopop)
{
	assert(v->_top >= nelemstopop);
	v->Pop(nelemstopop);
}

void sq_poptop(HSQUIRRELVM v)
{
	assert(v->_top >= 1);
    v->Pop();
}


void sq_remove(HSQUIRRELVM v, SQInteger idx)
{
	v->Remove(idx);
}

SQInteger sq_cmp(HSQUIRRELVM v)
{
	SQInteger res;
	v->ObjCmp(stack_get(v, -1), stack_get(v, -2),res);
	return res;
}

SQRESULT sq_newslot(HSQUIRRELVM v, SQInteger idx, SQBool bstatic)
{
	sq_aux_paramscheck(v, 3);
	SQObjectPtr &self = stack_get(v, idx);
	if(type(self) == OT_TABLE || type(self) == OT_CLASS) {
		SQObjectPtr &key = v->GetUp(-2);
		if(type(key) == OT_NULL) return sq_throwerror(v, "null is not a valid key");
		v->NewSlot(self, key, v->GetUp(-1),bstatic != 0);
		v->Pop(2);
	}
	return SQ_OK;
}

SQRESULT sq_deleteslot(HSQUIRRELVM v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 2);
	SQObjectPtr *self;
	_GETSAFE_OBJ(v, idx, OT_TABLE,self);
	SQObjectPtr &key = v->GetUp(-1);
	if(type(key) == OT_NULL) return sq_throwerror(v, "null is not a valid key");
	SQObjectPtr res;
	if(!v->DeleteSlot(*self, key, res)){
		return SQ_ERROR;
	}
	if(pushval)	v->GetUp(-1) = res;
	else v->Pop(1);
	return SQ_OK;
}

SQRESULT sq_set(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self = stack_get(v, idx);
	if(v->Set(self, v->GetUp(-2), v->GetUp(-1),false)) {
		v->Pop(2);
		return SQ_OK;
	}
	v->Raise_IdxError(v->GetUp(-2));return SQ_ERROR;
}

SQRESULT sq_rawset(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self = stack_get(v, idx);
	if(type(v->GetUp(-2)) == OT_NULL) return sq_throwerror(v, "null key");
	switch(type(self)) {
	case OT_TABLE:
		_table(self)->NewSlot(v->GetUp(-2), v->GetUp(-1));
		v->Pop(2);
		return SQ_OK;
	break;
	case OT_CLASS:
		_class(self)->NewSlot(_ss(v), v->GetUp(-2), v->GetUp(-1),false);
		v->Pop(2);
		return SQ_OK;
	break;
	case OT_INSTANCE:
		if(_instance(self)->Set(v->GetUp(-2), v->GetUp(-1))) {
			v->Pop(2);
			return SQ_OK;
		}
	break;
	case OT_ARRAY:
		if(v->Set(self, v->GetUp(-2), v->GetUp(-1),false)) {
			v->Pop(2);
			return SQ_OK;
		}
	break;
	default:
		v->Pop(2);
		return sq_throwerror(v, "rawset works only on array/table/class and instance");
	}
	v->Raise_IdxError(v->GetUp(-2));return SQ_ERROR;
}

SQRESULT sq_setdelegate(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self = stack_get(v, idx);
	SQObjectPtr &mt = v->GetUp(-1);
	SQObjectType type = type(self);
	switch(type) {
	case OT_TABLE:
		if(type(mt) == OT_TABLE) {
			if(!_table(self)->SetDelegate(_table(mt))) return sq_throwerror(v, "delagate cycle");
			v->Pop();}
		else if(type(mt)==OT_NULL) {
			_table(self)->SetDelegate(nullptr); v->Pop(); }
		else return sq_aux_invalidtype(v,type);
		break;
	case OT_USERDATA:
		if(type(mt)==OT_TABLE) {
			_userdata(self)->SetDelegate(_table(mt)); v->Pop(); }
		else if(type(mt)==OT_NULL) {
			_userdata(self)->SetDelegate(nullptr); v->Pop(); }
		else return sq_aux_invalidtype(v, type);
		break;
	default:
			return sq_aux_invalidtype(v, type);
		break;
	}
	return SQ_OK;
}

SQRESULT sq_rawdeleteslot(HSQUIRRELVM v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 2);
	SQObjectPtr *self;
	_GETSAFE_OBJ(v, idx, OT_TABLE,self);
	SQObjectPtr &key = v->GetUp(-1);
	SQObjectPtr t;
	if(_table(*self)->Get(key,t)) {
		_table(*self)->Remove(key);
	}
	if(pushval != 0) {
		if(pushval)	v->GetUp(-1) = t;
	} else {
		v->Pop(1);
	}
	return SQ_OK;
}

SQRESULT sq_getdelegate(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self=stack_get(v,idx);
	switch(type(self)){
	case OT_TABLE:
	case OT_USERDATA:
		if(!_delegable(self)->_delegate){
			v->Push(_null_);
			break;
		}
		v->Push(SQObjectPtr(_delegable(self)->_delegate));
		break;
	default: return sq_throwerror(v,"wrong type"); break;
	}
	return SQ_OK;

}

SQRESULT sq_get(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self=stack_get(v,idx);
	if(v->Get(self,v->GetUp(-1),v->GetUp(-1),false,false))
		return SQ_OK;
	v->Pop(1);
	return sq_throwerror(v,"the index doesn't exist");
}

SQRESULT sq_rawget(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self=stack_get(v,idx);
	switch(type(self)) {
	case OT_TABLE:
		if(_table(self)->Get(v->GetUp(-1),v->GetUp(-1)))
			return SQ_OK;
		break;
	case OT_CLASS:
		if(_class(self)->Get(v->GetUp(-1),v->GetUp(-1)))
			return SQ_OK;
		break;
	case OT_INSTANCE:
		if(_instance(self)->Get(v->GetUp(-1),v->GetUp(-1)))
			return SQ_OK;
		break;
	case OT_ARRAY:
		if(v->Get(self,v->GetUp(-1),v->GetUp(-1),false,false))
			return SQ_OK;
		break;
	default:
		v->Pop(1);
		return sq_throwerror(v,"rawget works only on array/table/instance and class");
	}
	v->Pop(1);
	return sq_throwerror(v,"the index doesn't exist");
}

SQRESULT sq_getstackobj(HSQUIRRELVM v,SQInteger idx,HSQOBJECT *po)
{
	*po=stack_get(v,idx);
	return SQ_OK;
}

const SQChar *sq_getlocal(HSQUIRRELVM v,SQUnsignedInteger level,SQUnsignedInteger idx)
{
	SQUnsignedInteger cstksize=v->_callsstacksize;
	SQUnsignedInteger lvl=(cstksize-level)-1;
	SQInteger stackbase=v->_stackbase;
	if(lvl<cstksize){
		for(SQUnsignedInteger i=0;i<level;i++){
			SQVM::CallInfo &ci=v->_callsstack[(cstksize-i)-1];
			stackbase-=ci._prevstkbase;
		}
		SQVM::CallInfo &ci=v->_callsstack[lvl];
		if(type(ci._closure)!=OT_CLOSURE)
			return nullptr;
		SQClosure *c=_closure(ci._closure);
		SQFunctionProto *func=_funcproto(c->_function);
		if(func->_noutervalues > (SQInteger)idx) {
			v->Push(c->_outervalues[idx]);
			return _stringval(func->_outervalues[idx]._name);
		}
		idx -= func->_noutervalues;
		return func->GetLocal(v,stackbase,idx,(SQInteger)(ci._ip-func->_instructions)-1);
	}
	return nullptr;
}

void sq_pushobject(HSQUIRRELVM v,HSQOBJECT obj)
{
	v->Push(SQObjectPtr(obj));
}

void sq_resetobject(HSQOBJECT *po)
{
	po->_unVal.pUserPointer=nullptr;po->_type=OT_NULL;
}

SQRESULT sq_throwerror(HSQUIRRELVM v,const SQChar *err, SQInteger len)
{
	v->_lasterror=SQString::Create(_ss(v),err, len);
	return -1;
}

void sq_reseterror(HSQUIRRELVM v)
{
	v->_lasterror = _null_;
}

void sq_getlasterror(HSQUIRRELVM v)
{
	v->Push(v->_lasterror);
}

void sq_reservestack(HSQUIRRELVM v,SQInteger nsize)
{
	if (((SQUnsignedInteger)v->_top + nsize) > v->_stack.size()) {
		v->_stack.resize(v->_stack.size() + ((v->_top + nsize) - v->_stack.size()));
	}
}

SQRESULT sq_resume(HSQUIRRELVM v,SQBool retval,SQBool raiseerror)
{
	if(type(v->GetUp(-1))==OT_GENERATOR){
		v->Push(_null_); //retval
		v->_can_suspend = false;
		if(!v->Execute(v->GetUp(-2),v->_top,0,v->_top,v->GetUp(-1),raiseerror,SQVM::ET_RESUME_GENERATOR))
		{v->Raise_Error(v->_lasterror); return SQ_ERROR;}
		if(!retval)
			v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v,"only generators can be resumed");
}

SQRESULT sq_call(HSQUIRRELVM v,SQInteger params,SQBool retval,SQBool raiseerror, int suspend)
{
	SQObjectPtr res;
	v->_can_suspend = suspend >= 0;
	if (v->_can_suspend) v->_ops_till_suspend = suspend;

	if(v->Call(v->GetUp(-(params+1)),params,v->_top-params,res,raiseerror != 0,v->_can_suspend)){
		if(!v->_suspended) {
			v->Pop(params);//pop closure and args
		}
		if(retval){
			v->Push(res); return SQ_OK;
		}
		return SQ_OK;
	}
	else {
		v->Pop(params);
		return SQ_ERROR;
	}
}

SQRESULT sq_suspendvm(HSQUIRRELVM v)
{
	return v->Suspend();
}

SQRESULT sq_wakeupvm(HSQUIRRELVM v,SQBool wakeupret,SQBool retval,SQBool raiseerror,SQBool throwerror)
{
	SQObjectPtr ret;
	if(!v->_suspended)
		return sq_throwerror(v,"cannot resume a vm that is not running any code");
	if(wakeupret) {
		v->GetAt(v->_stackbase+v->_suspended_target)=v->GetUp(-1); //retval
		v->Pop();
	} else v->GetAt(v->_stackbase+v->_suspended_target)=_null_;
	v->_can_suspend = false;
	if(!v->Execute(_null_,v->_top,-1,-1,ret,raiseerror,throwerror?SQVM::ET_RESUME_THROW_VM : SQVM::ET_RESUME_VM))
		return SQ_ERROR;
	if(sq_getvmstate(v) == SQ_VMSTATE_IDLE) {
		while (v->_top > 1) v->_stack[--v->_top] = _null_;
	}
	if(retval)
		v->Push(ret);
	return SQ_OK;
}

bool sq_resumecatch(HSQUIRRELVM v, int suspend)
{
	SQObjectPtr ret;
	v->_can_suspend = suspend >= 0;
	if (v->_can_suspend) v->_ops_till_suspend = suspend;
	return v->Execute(_null_, v->_top, -1, -1, ret, SQTrue, SQVM::ET_RESUME_OPENTTD);
}

bool sq_resumeerror(HSQUIRRELVM v)
{
	SQObjectPtr ret;
	v->_can_suspend = true;
	v->_ops_till_suspend = 1;
	return v->Execute(_null_, v->_top, -1, -1, ret, SQTrue, SQVM::ET_RESUME_THROW_VM);
}

void sq_setreleasehook(HSQUIRRELVM v,SQInteger idx,SQRELEASEHOOK hook)
{
	if(sq_gettop(v) >= 1){
		SQObjectPtr &ud=stack_get(v,idx);
		switch( type(ud) ) {
		case OT_USERDATA:	_userdata(ud)->_hook = hook;	break;
		case OT_INSTANCE:	_instance(ud)->_hook = hook;	break;
		case OT_CLASS:		_class(ud)->_hook = hook;		break;
		default: break; //shutup compiler
		}
	}
}

void sq_setcompilererrorhandler(HSQUIRRELVM v,SQCOMPILERERROR f)
{
	_ss(v)->_compilererrorhandler = f;
}

SQRESULT sq_writeclosure(HSQUIRRELVM v,SQWRITEFUNC w,SQUserPointer up)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, -1, OT_CLOSURE,o);
	unsigned short tag = SQ_BYTECODE_STREAM_TAG;
	if(w(up,&tag,2) != 2)
		return sq_throwerror(v,"io error");
	if(!_closure(*o)->Save(v,up,w))
		return SQ_ERROR;
	return SQ_OK;
}

SQRESULT sq_readclosure(HSQUIRRELVM v,SQREADFUNC r,SQUserPointer up)
{
	SQObjectPtr closure;

	unsigned short tag;
	if(r(up,&tag,2) != 2)
		return sq_throwerror(v,"io error");
	if(tag != SQ_BYTECODE_STREAM_TAG)
		return sq_throwerror(v,"invalid stream");
	if(!SQClosure::Load(v,up,r,closure))
		return SQ_ERROR;
	v->Push(closure);
	return SQ_OK;
}

SQChar *sq_getscratchpad(HSQUIRRELVM v,SQInteger minsize)
{
	return _ss(v)->GetScratchPad(minsize);
}

SQInteger sq_collectgarbage(HSQUIRRELVM v)
{
#ifndef NO_GARBAGE_COLLECTOR
	return _ss(v)->CollectGarbage(v);
#else
	return -1;
#endif
}

const SQChar *sq_getfreevariable(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger nval)
{
	SQObjectPtr &self = stack_get(v,idx);
	const SQChar *name = nullptr;
	if(type(self) == OT_CLOSURE) {
		if(_closure(self)->_outervalues.size()>nval) {
			v->Push(_closure(self)->_outervalues[nval]);
			SQFunctionProto *fp = _funcproto(_closure(self)->_function);
			SQOuterVar &ov = fp->_outervalues[nval];
			name = _stringval(ov._name);
		}
	}
	return name;
}

SQRESULT sq_setfreevariable(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger nval)
{
	SQObjectPtr &self=stack_get(v,idx);
	switch(type(self))
	{
	case OT_CLOSURE:
		if(_closure(self)->_outervalues.size()>nval){
			_closure(self)->_outervalues[nval]=stack_get(v,-1);
		}
		else return sq_throwerror(v,"invalid free var index");
		break;
	case OT_NATIVECLOSURE:
		if(_nativeclosure(self)->_outervalues.size()>nval){
			_nativeclosure(self)->_outervalues[nval]=stack_get(v,-1);
		}
		else return sq_throwerror(v,"invalid free var index");
		break;
	default:
		return sq_aux_invalidtype(v,type(self));
	}
	v->Pop(1);
	return SQ_OK;
}

SQRESULT sq_setattributes(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	SQObjectPtr &key = stack_get(v,-2);
	SQObjectPtr &val = stack_get(v,-1);
	SQObjectPtr attrs;
	if(type(key) == OT_NULL) {
		attrs = _class(*o)->_attributes;
		_class(*o)->_attributes = val;
		v->Pop(2);
		v->Push(attrs);
		return SQ_OK;
	}else if(_class(*o)->GetAttributes(key,attrs)) {
		_class(*o)->SetAttributes(key,val);
		v->Pop(2);
		v->Push(attrs);
		return SQ_OK;
	}
	return sq_throwerror(v,"wrong index");
}

SQRESULT sq_getattributes(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	SQObjectPtr &key = stack_get(v,-1);
	SQObjectPtr attrs;
	if(type(key) == OT_NULL) {
		attrs = _class(*o)->_attributes;
		v->Pop();
		v->Push(attrs);
		return SQ_OK;
	}
	else if(_class(*o)->GetAttributes(key,attrs)) {
		v->Pop();
		v->Push(attrs);
		return SQ_OK;
	}
	return sq_throwerror(v,"wrong index");
}

SQRESULT sq_getbase(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	if(_class(*o)->_base)
		v->Push(SQObjectPtr(_class(*o)->_base));
	else
		v->Push(_null_);
	return SQ_OK;
}

SQRESULT sq_getclass(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_INSTANCE,o);
	v->Push(SQObjectPtr(_instance(*o)->_class));
	return SQ_OK;
}

SQRESULT sq_createinstance(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = nullptr;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	v->Push(_class(*o)->CreateInstance());
	return SQ_OK;
}

void sq_weakref(HSQUIRRELVM v,SQInteger idx)
{
	SQObject &o=stack_get(v,idx);
	if(ISREFCOUNTED(type(o))) {
		v->Push(_refcounted(o)->GetWeakRef(type(o)));
		return;
	}
	v->Push(o);
}

SQRESULT sq_getweakrefval(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_WEAKREF) {
		return sq_throwerror(v,"the object must be a weakref");
	}
	v->Push(_weakref(o)->_obj);
	return SQ_OK;
}

SQRESULT sq_getdefaultdelegate(HSQUIRRELVM v,SQObjectType t)
{
	SQSharedState *ss = _ss(v);
	switch(t) {
	case OT_TABLE: v->Push(ss->_table_default_delegate); break;
	case OT_ARRAY: v->Push(ss->_array_default_delegate); break;
	case OT_STRING: v->Push(ss->_string_default_delegate); break;
	case OT_INTEGER: case OT_FLOAT: v->Push(ss->_number_default_delegate); break;
	case OT_GENERATOR: v->Push(ss->_generator_default_delegate); break;
	case OT_CLOSURE: case OT_NATIVECLOSURE: v->Push(ss->_closure_default_delegate); break;
	case OT_THREAD: v->Push(ss->_thread_default_delegate); break;
	case OT_CLASS: v->Push(ss->_class_default_delegate); break;
	case OT_INSTANCE: v->Push(ss->_instance_default_delegate); break;
	case OT_WEAKREF: v->Push(ss->_weakref_default_delegate); break;
	default: return sq_throwerror(v,"the type doesn't have a default delegate");
	}
	return SQ_OK;
}

SQRESULT sq_next(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr o=stack_get(v,idx),&refpos = stack_get(v,-1),realkey,val;
	if(type(o) == OT_GENERATOR) {
		return sq_throwerror(v,"cannot iterate a generator");
	}
	int faketojump;
	if(!v->FOREACH_OP(o,realkey,val,refpos,0,666,faketojump))
		return SQ_ERROR;
	if(faketojump != 666) {
		v->Push(realkey);
		v->Push(val);
		return SQ_OK;
	}
	return SQ_ERROR;
}

struct BufState{
	const SQChar *buf;
	SQInteger ptr;
	SQInteger size;
};

char32_t buf_lexfeed(SQUserPointer file)
{
	/* Convert an UTF-8 character into a char32_t */
	BufState *buf = (BufState *)file;
	const char *p = &buf->buf[buf->ptr];

	if (buf->size < buf->ptr + 1) return 0;

	/* Read the first character, and get the length based on UTF-8 specs. If invalid, bail out. */
	uint len = Utf8EncodedCharLen(*p);
	if (len == 0) {
		buf->ptr++;
		return -1;
	}

	/* Read the remaining bits. */
	if (buf->size < buf->ptr + len) return 0;
	buf->ptr += len;

	/* Convert the character, and when definitely invalid, bail out as well. */
	char32_t c;
	if (Utf8Decode(&c, p) != len) return -1;

	return c;
}

SQRESULT sq_compilebuffer(HSQUIRRELVM v,const SQChar *s,SQInteger size,const SQChar *sourcename,SQBool raiseerror) {
	BufState buf;
	buf.buf = s;
	buf.size = size;
	buf.ptr = 0;
	return sq_compile(v, buf_lexfeed, &buf, sourcename, raiseerror);
}

void sq_move(HSQUIRRELVM dest,HSQUIRRELVM src,SQInteger idx)
{
	dest->Push(stack_get(src,idx));
}

void sq_setprintfunc(HSQUIRRELVM v, SQPRINTFUNCTION printfunc)
{
	_ss(v)->_printfunc = printfunc;
}

SQPRINTFUNCTION sq_getprintfunc(HSQUIRRELVM v)
{
	return _ss(v)->_printfunc;
}

void *sq_malloc(SQUnsignedInteger size)
{
	return SQ_MALLOC(size);
}

void *sq_realloc(void* p,SQUnsignedInteger oldsize,SQUnsignedInteger newsize)
{
	return SQ_REALLOC(p,oldsize,newsize);
}

void sq_free(void *p,SQUnsignedInteger size)
{
	SQ_FREE(p,size);
}

