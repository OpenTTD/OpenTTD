/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"

#include <squirrel.h>
#include "sqpcheader.h"
#include <math.h>
#include "sqopcodes.h"
#include "sqfuncproto.h"
#include "sqvm.h"
#include "sqclosure.h"
#include "sqstring.h"
#include "sqtable.h"
#include "squserdata.h"
#include "sqarray.h"
#include "sqclass.h"

#include "../../../string_func.h"

#include "../../../safeguards.h"

#define TOP() (_stack._vals[_top-1])

#define CLEARSTACK(_last_top) { if((_last_top) >= _top) ClearStack(_last_top); }
void SQVM::ClearStack(SQInteger last_top)
{
	SQObjectType tOldType;
	SQObjectValue unOldVal;
	while (last_top >= _top) {
		SQObjectPtr &o = _stack._vals[last_top--];
		tOldType = o._type;
		unOldVal = o._unVal;
		o._type = OT_NULL;
		o._unVal.pUserPointer = NULL;
		__Release(tOldType,unOldVal);
	}
}

bool SQVM::BW_OP(SQUnsignedInteger op,SQObjectPtr &trg,const SQObjectPtr &o1,const SQObjectPtr &o2)
{
	SQInteger res;
	SQInteger i1 = _integer(o1), i2 = _integer(o2);
	if((type(o1)==OT_INTEGER) && (type(o2)==OT_INTEGER))
	{
		switch(op) {
			case BW_AND:	res = i1 & i2; break;
			case BW_OR:		res = i1 | i2; break;
			case BW_XOR:	res = i1 ^ i2; break;
			case BW_SHIFTL:	res = i1 << i2; break;
			case BW_SHIFTR:	res = i1 >> i2; break;
			case BW_USHIFTR:res = (SQInteger)(*((SQUnsignedInteger*)&i1) >> i2); break;
			default: { Raise_Error("internal vm error bitwise op failed"); return false; }
		}
	}
	else { Raise_Error("bitwise op between '%s' and '%s'",GetTypeName(o1),GetTypeName(o2)); return false;}
	trg = res;
	return true;
}

bool SQVM::ARITH_OP(SQUnsignedInteger op,SQObjectPtr &trg,const SQObjectPtr &o1,const SQObjectPtr &o2)
{
	if(sq_isnumeric(o1) && sq_isnumeric(o2)) {
			if((type(o1)==OT_INTEGER) && (type(o2)==OT_INTEGER)) {
				SQInteger res, i1 = _integer(o1), i2 = _integer(o2);
				switch(op) {
				case '+': res = i1 + i2; break;
				case '-': res = i1 - i2; break;
				case '/': if(i2 == 0) { Raise_Error("division by zero"); return false; }
					res = i1 / i2;
					break;
				case '*': res = i1 * i2; break;
				case '%': if(i2 == 0) { Raise_Error("modulo by zero"); return false; }
					res = i1 % i2;
					break;
				default: res = 0xDEADBEEF;
				}
				trg = res;
			}else{
				SQFloat res, f1 = tofloat(o1), f2 = tofloat(o2);
				switch(op) {
				case '+': res = f1 + f2; break;
				case '-': res = f1 - f2; break;
				case '/': res = f1 / f2; break;
				case '*': res = f1 * f2; break;
				case '%': res = SQFloat(fmod((double)f1,(double)f2)); break;
				default: res = 0x0f;
				}
				trg = res;
			}
		} else {
			if(op == '+' &&	(type(o1) == OT_STRING || type(o2) == OT_STRING)){
					if(!StringCat(o1, o2, trg)) return false;
			}
			else if(!ArithMetaMethod(op,o1,o2,trg)) {
				Raise_Error("arith op %c on between '%s' and '%s'",op,GetTypeName(o1),GetTypeName(o2)); return false;
			}
		}
		return true;
}

SQVM::SQVM(SQSharedState *ss)
{
	_sharedstate=ss;
	_suspended = SQFalse;
	_suspended_target=-1;
	_suspended_root = SQFalse;
	_suspended_traps=0;
	_foreignptr=NULL;
	_nnativecalls=0;
	_lasterror = _null_;
	_errorhandler = _null_;
	_debughook = _null_;
	_can_suspend = false;
	_in_stackoverflow = false;
	_ops_till_suspend = 0;
	_callsstack = NULL;
	_callsstacksize = 0;
	_alloccallsstacksize = 0;
	_top = 0;
	_stackbase = 0;
	ci = NULL;
	INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);
}

void SQVM::Finalize()
{
	_roottable = _null_;
	_lasterror = _null_;
	_errorhandler = _null_;
	_debughook = _null_;
	temp_reg = _null_;
	_callstackdata.resize(0);
	SQInteger size=_stack.size();
	for(SQInteger i=size - 1;i>=0;i--)
		_stack[i]=_null_;
}

SQVM::~SQVM()
{
	Finalize();
	//sq_free(_callsstack,_alloccallsstacksize*sizeof(CallInfo));
	REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
}

bool SQVM::ArithMetaMethod(SQInteger op,const SQObjectPtr &o1,const SQObjectPtr &o2,SQObjectPtr &dest)
{
	SQMetaMethod mm;
	switch(op){
		case '+': mm=MT_ADD; break;
		case '-': mm=MT_SUB; break;
		case '/': mm=MT_DIV; break;
		case '*': mm=MT_MUL; break;
		case '%': mm=MT_MODULO; break;
		default: mm = MT_ADD; assert(0); break; //shutup compiler
	}
	if(is_delegable(o1) && _delegable(o1)->_delegate) {
		Push(o1);Push(o2);
		return CallMetaMethod(_delegable(o1),mm,2,dest);
	}
	return false;
}

bool SQVM::NEG_OP(SQObjectPtr &trg,const SQObjectPtr &o)
{

	switch(type(o)) {
	case OT_INTEGER:
		trg = -_integer(o);
		return true;
	case OT_FLOAT:
		trg = -_float(o);
		return true;
	case OT_TABLE:
	case OT_USERDATA:
	case OT_INSTANCE:
		if(_delegable(o)->_delegate) {
			Push(o);
			if(CallMetaMethod(_delegable(o), MT_UNM, 1, temp_reg)) {
				trg = temp_reg;
				return true;
			}
		}
	default:break; //shutup compiler
	}
	Raise_Error("attempt to negate a %s", GetTypeName(o));
	return false;
}

#define _RET_SUCCEED(exp) { result = (exp); return true; }
bool SQVM::ObjCmp(const SQObjectPtr &o1,const SQObjectPtr &o2,SQInteger &result)
{
	if(type(o1)==type(o2)){
		if(_rawval(o1)==_rawval(o2))_RET_SUCCEED(0);
		SQObjectPtr res;
		switch(type(o1)){
		case OT_STRING:
			_RET_SUCCEED(strcmp(_stringval(o1),_stringval(o2)));
		case OT_INTEGER:
			/* FS#3954: wrong integer comparison */
			_RET_SUCCEED((_integer(o1)<_integer(o2))?-1:(_integer(o1)==_integer(o2))?0:1);
		case OT_FLOAT:
			_RET_SUCCEED((_float(o1)<_float(o2))?-1:1);
		case OT_TABLE:
		case OT_USERDATA:
		case OT_INSTANCE:
			if(_delegable(o1)->_delegate) {
				Push(o1);Push(o2);
				if(CallMetaMethod(_delegable(o1),MT_CMP,2,res)) {
					if(type(res) != OT_INTEGER) {
						Raise_Error("_cmp must return an integer");
						return false;
					}
					_RET_SUCCEED(_integer(res))
				}
			}
			FALLTHROUGH;
		default:
			_RET_SUCCEED( _userpointer(o1) < _userpointer(o2)?-1:1 );
		}
		assert(0);

	}
	else{
		if(sq_isnumeric(o1) && sq_isnumeric(o2)){
			if((type(o1)==OT_INTEGER) && (type(o2)==OT_FLOAT)) {
				if( _integer(o1)==_float(o2) ) { _RET_SUCCEED(0); }
				else if( _integer(o1)<_float(o2) ) { _RET_SUCCEED(-1); }
				_RET_SUCCEED(1);
			}
			else{
				if( _float(o1)==_integer(o2) ) { _RET_SUCCEED(0); }
				else if( _float(o1)<_integer(o2) ) { _RET_SUCCEED(-1); }
				_RET_SUCCEED(1);
			}
		}
		else if(type(o1)==OT_NULL) {_RET_SUCCEED(-1);}
		else if(type(o2)==OT_NULL) {_RET_SUCCEED(1);}
		else { Raise_CompareError(o1,o2); return false; }

	}
	assert(0);
	_RET_SUCCEED(0); //cannot happen
}

bool SQVM::CMP_OP(CmpOP op, const SQObjectPtr &o1,const SQObjectPtr &o2,SQObjectPtr &res)
{
	SQInteger r;
	if(ObjCmp(o1,o2,r)) {
		switch(op) {
			case CMP_G: res = (r > 0)?_true_:_false_; return true;
			case CMP_GE: res = (r >= 0)?_true_:_false_; return true;
			case CMP_L: res = (r < 0)?_true_:_false_; return true;
			case CMP_LE: res = (r <= 0)?_true_:_false_; return true;

		}
		assert(0);
	}
	return false;
}

void SQVM::ToString(const SQObjectPtr &o,SQObjectPtr &res)
{
	char buf[64];
	switch(type(o)) {
	case OT_STRING:
		res = o;
		return;
	case OT_FLOAT:
		seprintf(buf, lastof(buf),"%g",_float(o));
		break;
	case OT_INTEGER:
		seprintf(buf, lastof(buf),OTTD_PRINTF64,_integer(o));
		break;
	case OT_BOOL:
		seprintf(buf, lastof(buf),_integer(o)?"true":"false");
		break;
	case OT_TABLE:
	case OT_USERDATA:
	case OT_INSTANCE:
		if(_delegable(o)->_delegate) {
			Push(o);
			if(CallMetaMethod(_delegable(o),MT_TOSTRING,1,res)) {
				if(type(res) == OT_STRING)
					return;
				//else keeps going to the default
			}
		}
		FALLTHROUGH;
	default:
		seprintf(buf, lastof(buf),"(%s : 0x%p)",GetTypeName(o),(void*)_rawval(o));
	}
	res = SQString::Create(_ss(this),buf);
}


bool SQVM::StringCat(const SQObjectPtr &str,const SQObjectPtr &obj,SQObjectPtr &dest)
{
	SQObjectPtr a, b;
	ToString(str, a);
	ToString(obj, b);
	SQInteger l = _string(a)->_len , ol = _string(b)->_len;
	SQChar *s = _sp(l + ol + 1);
	memcpy(s, _stringval(a), (size_t)l);
	memcpy(s + l, _stringval(b), (size_t)ol);
	dest = SQString::Create(_ss(this), _spval, l + ol);
	return true;
}

void SQVM::TypeOf(const SQObjectPtr &obj1,SQObjectPtr &dest)
{
	if(is_delegable(obj1) && _delegable(obj1)->_delegate) {
		Push(obj1);
		if(CallMetaMethod(_delegable(obj1),MT_TYPEOF,1,dest))
			return;
	}
	dest = SQString::Create(_ss(this),GetTypeName(obj1));
}

bool SQVM::Init(SQVM *friendvm, SQInteger stacksize)
{
	_stack.resize(stacksize);
	_alloccallsstacksize = 4;
	_callstackdata.resize(_alloccallsstacksize);
	_callsstacksize = 0;
	_callsstack = &_callstackdata[0];
	_stackbase = 0;
	_top = 0;
	if(!friendvm)
		_roottable = SQTable::Create(_ss(this), 0);
	else {
		_roottable = friendvm->_roottable;
		_errorhandler = friendvm->_errorhandler;
		_debughook = friendvm->_debughook;
	}

	sq_base_register(this);
	return true;
}

extern SQInstructionDesc g_InstrDesc[];

bool SQVM::StartCall(SQClosure *closure,SQInteger target,SQInteger args,SQInteger stackbase,bool tailcall)
{
	SQFunctionProto *func = _funcproto(closure->_function);

	const SQInteger paramssize = func->_nparameters;
	const SQInteger newtop = stackbase + func->_stacksize;
	SQInteger nargs = args;
	if (paramssize != nargs) {
		SQInteger ndef = func->_ndefaultparams;
		SQInteger diff;
		if(ndef && nargs < paramssize && (diff = paramssize - nargs) <= ndef) {
			for(SQInteger n = ndef - diff; n < ndef; n++) {
				_stack._vals[stackbase + (nargs++)] = closure->_defaultparams[n];
			}
		}
		else if(func->_varparams)
		{
			if (nargs < paramssize) {
				Raise_Error("wrong number of parameters");
				return false;
			}
			for(SQInteger n = 0; n < nargs - paramssize; n++) {
				_vargsstack.push_back(_stack._vals[stackbase+paramssize+n]);
				_stack._vals[stackbase+paramssize+n] = _null_;
			}
		}
		else {
			Raise_Error("wrong number of parameters");
			return false;
		}
	}

	if(type(closure->_env) == OT_WEAKREF) {
		_stack._vals[stackbase] = _weakref(closure->_env)->_obj;
	}

	if (!tailcall) {
		CallInfo lc;
		memset(&lc, 0, sizeof(lc));
		lc._generator = NULL;
		lc._etraps = 0;
		lc._prevstkbase = (SQInt32) ( stackbase - _stackbase );
		lc._target = (SQInt32) target;
		lc._prevtop = (SQInt32) (_top - _stackbase);
		lc._ncalls = 1;
		lc._root = SQFalse;
		PUSH_CALLINFO(this, lc);
	}
	else {
		ci->_ncalls++;
	}
	ci->_vargs.size = (SQInt32)(nargs - paramssize);
	ci->_vargs.base = (SQInt32)(_vargsstack.size()-(ci->_vargs.size));
	ci->_closure = closure;
	ci->_literals = func->_literals;
	ci->_ip = func->_instructions;
	//grows the stack if needed
	if (((SQUnsignedInteger)newtop + (func->_stacksize<<1)) > _stack.size()) {
		_stack.resize(_stack.size() + (func->_stacksize<<1));
	}

	_top = newtop;
	_stackbase = stackbase;
	if (type(_debughook) != OT_NULL && _rawval(_debughook) != _rawval(ci->_closure))
		CallDebugHook('c');
	return true;
}

bool SQVM::Return(SQInteger _arg0, SQInteger _arg1, SQObjectPtr &retval)
{
	if (type(_debughook) != OT_NULL && _rawval(_debughook) != _rawval(ci->_closure))
		for(SQInteger i=0;i<ci->_ncalls;i++)
			CallDebugHook('r');

	SQBool broot = ci->_root;
	SQInteger last_top = _top;
	SQInteger target = ci->_target;
	SQInteger oldstackbase = _stackbase;
	_stackbase -= ci->_prevstkbase;
	_top = _stackbase + ci->_prevtop;
	if(ci->_vargs.size) PopVarArgs(ci->_vargs);
	POP_CALLINFO(this);
	if (broot) {
		if (_arg0 != MAX_FUNC_STACKSIZE) retval = _stack._vals[oldstackbase+_arg1];
		else retval = _null_;
	}
	else {
		if(target != -1) { //-1 is when a class contructor ret value has to be ignored
			if (_arg0 != MAX_FUNC_STACKSIZE)
				STK(target) = _stack._vals[oldstackbase+_arg1];
			else
				STK(target) = _null_;
		}
	}

	while (last_top > oldstackbase) _stack._vals[last_top--].Null();
	assert(oldstackbase >= _stackbase);
	return broot?true:false;
}

#define _RET_ON_FAIL(exp) { if(!exp) return false; }

bool SQVM::LOCAL_INC(SQInteger op,SQObjectPtr &target, SQObjectPtr &a, SQObjectPtr &incr)
{
	_RET_ON_FAIL(ARITH_OP( op , target, a, incr));
	a = target;
	return true;
}

bool SQVM::PLOCAL_INC(SQInteger op,SQObjectPtr &target, SQObjectPtr &a, SQObjectPtr &incr)
{
	SQObjectPtr trg;
	_RET_ON_FAIL(ARITH_OP( op , trg, a, incr));
	target = a;
	a = trg;
	return true;
}

bool SQVM::DerefInc(SQInteger op,SQObjectPtr &target, SQObjectPtr &self, SQObjectPtr &key, SQObjectPtr &incr, bool postfix)
{
	SQObjectPtr tmp, tself = self, tkey = key;
	if (!Get(tself, tkey, tmp, false, true)) { Raise_IdxError(tkey); return false; }
	_RET_ON_FAIL(ARITH_OP( op , target, tmp, incr))
	Set(tself, tkey, target,true);
	if (postfix) target = tmp;
	return true;
}

#define arg0 (_i_._arg0)
#define arg1 (_i_._arg1)
#define sarg1 (*(const_cast<SQInt32 *>(&_i_._arg1)))
#define arg2 (_i_._arg2)
#define arg3 (_i_._arg3)
#define sarg3 ((SQInteger)*((const signed char *)&_i_._arg3))

SQRESULT SQVM::Suspend()
{
	if (_suspended)
		return sq_throwerror(this, "cannot suspend an already suspended vm");
	if (_nnativecalls!=2)
		return sq_throwerror(this, "cannot suspend through native calls/metamethods");
	return SQ_SUSPEND_FLAG;
}

void SQVM::PopVarArgs(VarArgs &vargs)
{
	for(SQInteger n = 0; n< vargs.size; n++)
		_vargsstack.pop_back();
}

#define _FINISH(howmuchtojump) {jump = howmuchtojump; return true; }
bool SQVM::FOREACH_OP(SQObjectPtr &o1,SQObjectPtr &o2,SQObjectPtr
&o3,SQObjectPtr &o4,SQInteger arg_2,int exitpos,int &jump)
{
	SQInteger nrefidx;
	switch(type(o1)) {
	case OT_TABLE:
		if((nrefidx = _table(o1)->Next(false,o4, o2, o3)) == -1) _FINISH(exitpos);
		o4 = (SQInteger)nrefidx; _FINISH(1);
	case OT_ARRAY:
		if((nrefidx = _array(o1)->Next(o4, o2, o3)) == -1) _FINISH(exitpos);
		o4 = (SQInteger) nrefidx; _FINISH(1);
	case OT_STRING:
		if((nrefidx = _string(o1)->Next(o4, o2, o3)) == -1)_FINISH(exitpos);
		o4 = (SQInteger)nrefidx; _FINISH(1);
	case OT_CLASS:
		if((nrefidx = _class(o1)->Next(o4, o2, o3)) == -1)_FINISH(exitpos);
		o4 = (SQInteger)nrefidx; _FINISH(1);
	case OT_USERDATA:
	case OT_INSTANCE:
		if(_delegable(o1)->_delegate) {
			SQObjectPtr itr;
			Push(o1);
			Push(o4);
			if(CallMetaMethod(_delegable(o1), MT_NEXTI, 2, itr)){
				o4 = o2 = itr;
				if(type(itr) == OT_NULL) _FINISH(exitpos);
				if(!Get(o1, itr, o3, false,false)) {
					Raise_Error("_nexti returned an invalid idx");
					return false;
				}
				_FINISH(1);
			}
			Raise_Error("_nexti failed");
			return false;
		}
		break;
	case OT_GENERATOR:
		if(_generator(o1)->_state == SQGenerator::eDead) _FINISH(exitpos);
		if(_generator(o1)->_state == SQGenerator::eSuspended) {
			SQInteger idx = 0;
			if(type(o4) == OT_INTEGER) {
				idx = _integer(o4) + 1;
			}
			o2 = idx;
			o4 = idx;
			_generator(o1)->Resume(this, arg_2+1);
			_FINISH(0);
		}
		FALLTHROUGH;
	default:
		Raise_Error("cannot iterate %s", GetTypeName(o1));
	}
	return false; //cannot be hit(just to avoid warnings)
}

bool SQVM::DELEGATE_OP(SQObjectPtr &trg,SQObjectPtr &o1,SQObjectPtr &o2)
{
	if(type(o1) != OT_TABLE) { Raise_Error("delegating a '%s'", GetTypeName(o1)); return false; }
	switch(type(o2)) {
	case OT_TABLE:
		if(!_table(o1)->SetDelegate(_table(o2))){
			Raise_Error("delegate cycle detected");
			return false;
		}
		break;
	case OT_NULL:
		_table(o1)->SetDelegate(NULL);
		break;
	default:
		Raise_Error("using '%s' as delegate", GetTypeName(o2));
		return false;
		break;
	}
	trg = o1;
	return true;
}
#define COND_LITERAL (arg3!=0?ci->_literals[arg1]:STK(arg1))

#define _GUARD(exp) { if(!exp) { Raise_Error(_lasterror); SQ_THROW();} }

#define SQ_THROW() { goto exception_trap; }

bool SQVM::CLOSURE_OP(SQObjectPtr &target, SQFunctionProto *func)
{
	SQInteger nouters;
	SQClosure *closure = SQClosure::Create(_ss(this), func);
	if((nouters = func->_noutervalues)) {
		closure->_outervalues.reserve(nouters);
		for(SQInteger i = 0; i<nouters; i++) {
			SQOuterVar &v = func->_outervalues[i];
			switch(v._type){
			case otSYMBOL:
				closure->_outervalues.push_back(_null_);
				if(!Get(_stack._vals[_stackbase]/*STK(0)*/, v._src, closure->_outervalues.top(), false,true))
				{Raise_IdxError(v._src); return false; }
				break;
			case otLOCAL:
				closure->_outervalues.push_back(_stack._vals[_stackbase+_integer(v._src)]);
				break;
			case otOUTER:
				closure->_outervalues.push_back(_closure(ci->_closure)->_outervalues[_integer(v._src)]);
				break;
			}
		}
	}
	SQInteger ndefparams;
	if((ndefparams = func->_ndefaultparams)) {
		closure->_defaultparams.reserve(ndefparams);
		for(SQInteger i = 0; i < ndefparams; i++) {
			SQInteger spos = func->_defaultparams[i];
			closure->_defaultparams.push_back(_stack._vals[_stackbase + spos]);
		}
	}
	target = closure;
	return true;

}

bool SQVM::GETVARGV_OP(SQObjectPtr &target,SQObjectPtr &index,CallInfo *ci)
{
	if(ci->_vargs.size == 0) {
		Raise_Error("the function doesn't have var args");
		return false;
	}
	if(!sq_isnumeric(index)){
		Raise_Error("indexing 'vargv' with %s",GetTypeName(index));
		return false;
	}
	SQInteger idx = tointeger(index);
	if(idx < 0 || idx >= ci->_vargs.size){ Raise_Error("vargv index out of range"); return false; }
	target = _vargsstack[ci->_vargs.base+idx];
	return true;
}

bool SQVM::CLASS_OP(SQObjectPtr &target,SQInteger baseclass,SQInteger attributes)
{
	SQClass *base = NULL;
	SQObjectPtr attrs;
	if(baseclass != -1) {
		if(type(_stack._vals[_stackbase+baseclass]) != OT_CLASS) { Raise_Error("trying to inherit from a %s",GetTypeName(_stack._vals[_stackbase+baseclass])); return false; }
		base = _class(_stack._vals[_stackbase + baseclass]);
	}
	if(attributes != MAX_FUNC_STACKSIZE) {
		attrs = _stack._vals[_stackbase+attributes];
	}
	target = SQClass::Create(_ss(this),base);
	if(type(_class(target)->_metamethods[MT_INHERITED]) != OT_NULL) {
		int nparams = 2;
		SQObjectPtr ret;
		Push(target); Push(attrs);
		Call(_class(target)->_metamethods[MT_INHERITED],nparams,_top - nparams, ret, false, false);
		Pop(nparams);
	}
	_class(target)->_attributes = attrs;
	return true;
}



bool SQVM::IsEqual(SQObjectPtr &o1,SQObjectPtr &o2,bool &res)
{
	if(type(o1) == type(o2)) {
		res = ((_rawval(o1) == _rawval(o2)?true:false));
	}
	else {
		if(sq_isnumeric(o1) && sq_isnumeric(o2)) {
			SQInteger cmpres;
			if(!ObjCmp(o1, o2,cmpres)) return false;
			res = (cmpres == 0);
		}
		else {
			res = false;
		}
	}
	return true;
}

bool SQVM::IsFalse(SQObjectPtr &o)
{
	if(((type(o) & SQOBJECT_CANBEFALSE) && ( (type(o) == OT_FLOAT) && (_float(o) == SQFloat(0.0)) ))
		|| (_integer(o) == 0) ) { //OT_NULL|OT_INTEGER|OT_BOOL
		return true;
	}
	return false;
}

bool SQVM::GETPARENT_OP(SQObjectPtr &o,SQObjectPtr &target)
{
	switch(type(o)) {
		case OT_TABLE: target = _table(o)->_delegate?SQObjectPtr(_table(o)->_delegate):_null_;
			break;
		case OT_CLASS: target = _class(o)->_base?_class(o)->_base:_null_;
			break;
		default:
			Raise_Error("the %s type doesn't have a parent slot", GetTypeName(o));
			return false;
	}
	return true;
}

bool SQVM::Execute(SQObjectPtr &closure, SQInteger target, SQInteger nargs, SQInteger stackbase,SQObjectPtr &outres, SQBool raiseerror,ExecutionType et)
{
	if ((_nnativecalls + 1) > MAX_NATIVE_CALLS) { Raise_Error("Native stack overflow"); return false; }
	_nnativecalls++;
	AutoDec ad(&_nnativecalls);
	SQInteger traps = 0;
	//temp_reg vars for OP_CALL
	SQInteger ct_target;
	SQInteger ct_stackbase;
	bool ct_tailcall;

	switch(et) {
		case ET_CALL: {
			SQInteger last_top = _top;
			temp_reg = closure;
			if(!StartCall(_closure(temp_reg), _top - nargs, nargs, stackbase, false)) {
				//call the handler if there are no calls in the stack, if not relies on the previous node
				if(ci == NULL) CallErrorHandler(_lasterror);
				return false;
			}
			if (_funcproto(_closure(temp_reg)->_function)->_bgenerator) {
				//SQFunctionProto *f = _funcproto(_closure(temp_reg)->_function);
				SQGenerator *gen = SQGenerator::Create(_ss(this), _closure(temp_reg));
				_GUARD(gen->Yield(this));
				Return(1, ci->_target, temp_reg);
				outres = gen;
				CLEARSTACK(last_top);
				return true;
			}
			ci->_root = SQTrue;
					  }
			break;
		case ET_RESUME_GENERATOR: _generator(closure)->Resume(this, target); ci->_root = SQTrue; traps += ci->_etraps; break;
		case ET_RESUME_VM:
		case ET_RESUME_THROW_VM:
			traps = _suspended_traps;
			ci->_root = _suspended_root;
			ci->_vargs = _suspend_varargs;
			_suspended = SQFalse;
			if(et  == ET_RESUME_THROW_VM) { SQ_THROW(); }
			break;
		case ET_RESUME_OPENTTD:
			traps = _suspended_traps;
			_suspended = SQFalse;
			break;
	}

exception_restore:
	//
	{
		for(;;)
		{
			DecreaseOps(1);
			if (ShouldSuspend()) { _suspended = SQTrue; _suspended_traps = traps; return true; }

			const SQInstruction &_i_ = *ci->_ip++;
			//dumpstack(_stackbase);
			//printf("%s %d %d %d %d\n",g_InstrDesc[_i_.op].name,arg0,arg1,arg2,arg3);
			switch(_i_.op)
			{
			case _OP_LINE:
				if(type(_debughook) != OT_NULL && _rawval(_debughook) != _rawval(ci->_closure))
					CallDebugHook('l',arg1);
				continue;
			case _OP_LOAD: TARGET = ci->_literals[arg1]; continue;
			case _OP_LOADINT: TARGET = (SQInteger)arg1; continue;
			case _OP_LOADFLOAT: TARGET = *((const SQFloat *)&arg1); continue;
			case _OP_DLOAD: TARGET = ci->_literals[arg1]; STK(arg2) = ci->_literals[arg3];continue;
			case _OP_TAILCALL:
				temp_reg = STK(arg1);
				if (type(temp_reg) == OT_CLOSURE && !_funcproto(_closure(temp_reg)->_function)->_bgenerator){
					ct_tailcall = true;
					if(ci->_vargs.size) PopVarArgs(ci->_vargs);
					for (SQInteger i = 0; i < arg3; i++) STK(i) = STK(arg2 + i);
					ct_target = ci->_target;
					ct_stackbase = _stackbase;
					goto common_call;
				}
				FALLTHROUGH;
			case _OP_CALL: {
					ct_tailcall = false;
					ct_target = arg0;
					temp_reg = STK(arg1);
					ct_stackbase = _stackbase+arg2;

common_call:
					SQObjectPtr clo = temp_reg;
					SQInteger last_top = _top;
					switch (type(clo)) {
					case OT_CLOSURE:{
						_GUARD(StartCall(_closure(clo), ct_target, arg3, ct_stackbase, ct_tailcall));
						if (_funcproto(_closure(clo)->_function)->_bgenerator) {
							SQGenerator *gen = SQGenerator::Create(_ss(this), _closure(clo));
							_GUARD(gen->Yield(this));
							Return(1, ct_target, clo);
							STK(ct_target) = gen;
						}
						CLEARSTACK(last_top);
						}
						continue;
					case OT_NATIVECLOSURE: {
						bool suspend;
						_suspended_target = ct_target;
						try {
							_GUARD(CallNative(_nativeclosure(clo), arg3, ct_stackbase, clo,suspend));
						} catch (...) {
							_suspended = SQTrue;
							_suspended_target = ct_target;
							_suspended_root = ci->_root;
							_suspended_traps = traps;
							_suspend_varargs = ci->_vargs;
							throw;
						}
						if(suspend){
							_suspended = SQTrue;
							_suspended_target = ct_target;
							_suspended_root = ci->_root;
							_suspended_traps = traps;
							_suspend_varargs = ci->_vargs;
							outres = clo;
							return true;
						}
						if(ct_target != -1) { //skip return value for constructors
							STK(ct_target) = clo;
						}
										   }
						continue;
					case OT_CLASS:{
						SQObjectPtr inst;
						_GUARD(CreateClassInstance(_class(clo),inst,temp_reg));
						STK(ct_target) = inst;
						ct_target = -1; //fakes return value target so that is not overwritten by the constructor
						if(type(temp_reg) != OT_NULL) {
							_stack._vals[ct_stackbase] = inst;
							goto common_call; //hard core spaghetti code(reissues the OP_CALL to invoke the constructor)
						}
						}
						break;
					case OT_TABLE:
					case OT_USERDATA:
					case OT_INSTANCE:
						{
						Push(clo);
						for (SQInteger i = 0; i < arg3; i++) Push(STK(arg2 + i));
						if (_delegable(clo) && CallMetaMethod(_delegable(clo), MT_CALL, arg3+1, clo)){
							STK(ct_target) = clo;
							break;
						}
						Raise_Error("attempt to call '%s'", GetTypeName(clo));
						SQ_THROW();
					  }
					default:
						Raise_Error("attempt to call '%s'", GetTypeName(clo));
						SQ_THROW();
					}
				}
				  continue;
			case _OP_PREPCALL:
			case _OP_PREPCALLK:
				{
					SQObjectPtr &key = _i_.op == _OP_PREPCALLK?(ci->_literals)[arg1]:STK(arg1);
					SQObjectPtr &o = STK(arg2);
					if (!Get(o, key, temp_reg,false,true)) {
						if(type(o) == OT_CLASS) { //hack?
							if(_class_ddel->Get(key,temp_reg)) {
								STK(arg3) = o;
								TARGET = temp_reg;
								continue;
							}
						}
						{ Raise_IdxError(key); SQ_THROW();}
					}

					STK(arg3) = type(o) == OT_CLASS?STK(0):o;
					TARGET = temp_reg;
				}
				continue;
			case _OP_SCOPE_END:
			{
				SQInteger from = arg0;
				SQInteger count = arg1 - arg0 + 2;
				/* When 'return' is executed, it happens that the stack is already cleaned
				 *  (by Return()), but this OP-code is still executed. So check for this
				 *  situation, and ignore the cleanup */
				if (_stackbase + count + from <= _top) {
					while (--count >= 0) _stack._vals[_stackbase + count + from].Null();
				}
			} continue;
			case _OP_GETK:
				if (!Get(STK(arg2), ci->_literals[arg1], temp_reg, false,true)) { Raise_IdxError(ci->_literals[arg1]); SQ_THROW();}
				TARGET = temp_reg;
				continue;
			case _OP_MOVE: TARGET = STK(arg1); continue;
			case _OP_NEWSLOT:
				_GUARD(NewSlot(STK(arg1), STK(arg2), STK(arg3),false));
				if(arg0 != arg3) TARGET = STK(arg3);
				continue;
			case _OP_DELETE: _GUARD(DeleteSlot(STK(arg1), STK(arg2), TARGET)); continue;
			case _OP_SET:
				if (!Set(STK(arg1), STK(arg2), STK(arg3),true)) { Raise_IdxError(STK(arg2)); SQ_THROW(); }
				if (arg0 != arg3) TARGET = STK(arg3);
				continue;
			case _OP_GET:
				if (!Get(STK(arg1), STK(arg2), temp_reg, false,true)) { Raise_IdxError(STK(arg2)); SQ_THROW(); }
				TARGET = temp_reg;
				continue;
			case _OP_EQ:{
				bool res;
				if(!IsEqual(STK(arg2),COND_LITERAL,res)) { SQ_THROW(); }
				TARGET = res?_true_:_false_;
				}continue;
			case _OP_NE:{
				bool res;
				if(!IsEqual(STK(arg2),COND_LITERAL,res)) { SQ_THROW(); }
				TARGET = (!res)?_true_:_false_;
				} continue;
			case _OP_ARITH: _GUARD(ARITH_OP( arg3 , temp_reg, STK(arg2), STK(arg1))); TARGET = temp_reg; continue;
			case _OP_BITW:	_GUARD(BW_OP( arg3,TARGET,STK(arg2),STK(arg1))); continue;
			case _OP_RETURN:
				if(ci->_generator) {
					ci->_generator->Kill();
				}
				if(Return(arg0, arg1, temp_reg)){
					assert(traps==0);
					outres = temp_reg;
					return true;
				}
				continue;
			case _OP_LOADNULLS:{ for(SQInt32 n=0; n < arg1; n++) STK(arg0+n) = _null_; }continue;
			case _OP_LOADROOTTABLE:	TARGET = _roottable; continue;
			case _OP_LOADBOOL: TARGET = arg1?_true_:_false_; continue;
			case _OP_DMOVE: STK(arg0) = STK(arg1); STK(arg2) = STK(arg3); continue;
			case _OP_JMP: ci->_ip += (sarg1); continue;
			case _OP_JNZ: if(!IsFalse(STK(arg0))) ci->_ip+=(sarg1); continue;
			case _OP_JZ: if(IsFalse(STK(arg0))) ci->_ip+=(sarg1); continue;
			case _OP_LOADFREEVAR: TARGET = _closure(ci->_closure)->_outervalues[arg1]; continue;
			case _OP_VARGC: TARGET = SQInteger(ci->_vargs.size); continue;
			case _OP_GETVARGV:
				if(!GETVARGV_OP(TARGET,STK(arg1),ci)) { SQ_THROW(); }
				continue;
			case _OP_NEWTABLE: TARGET = SQTable::Create(_ss(this), arg1); continue;
			case _OP_NEWARRAY: TARGET = SQArray::Create(_ss(this), 0); _array(TARGET)->Reserve(arg1); continue;
			case _OP_APPENDARRAY: _array(STK(arg0))->Append(COND_LITERAL);	continue;
			case _OP_GETPARENT: _GUARD(GETPARENT_OP(STK(arg1),TARGET)); continue;
			case _OP_COMPARITH: _GUARD(DerefInc(arg3, TARGET, STK((((SQUnsignedInteger)arg1&0xFFFF0000)>>16)), STK(arg2), STK(arg1&0x0000FFFF), false)); continue;
			case _OP_COMPARITHL: _GUARD(LOCAL_INC(arg3, TARGET, STK(arg1), STK(arg2))); continue;
			case _OP_INC: {SQObjectPtr o(sarg3); _GUARD(DerefInc('+',TARGET, STK(arg1), STK(arg2), o, false));} continue;
			case _OP_INCL: {SQObjectPtr o(sarg3); _GUARD(LOCAL_INC('+',TARGET, STK(arg1), o));} continue;
			case _OP_PINC: {SQObjectPtr o(sarg3); _GUARD(DerefInc('+',TARGET, STK(arg1), STK(arg2), o, true));} continue;
			case _OP_PINCL:	{SQObjectPtr o(sarg3); _GUARD(PLOCAL_INC('+',TARGET, STK(arg1), o));} continue;
			case _OP_CMP:	_GUARD(CMP_OP((CmpOP)arg3,STK(arg2),STK(arg1),TARGET))	continue;
			case _OP_EXISTS: TARGET = Get(STK(arg1), STK(arg2), temp_reg, true,false)?_true_:_false_;continue;
			case _OP_INSTANCEOF:
				if(type(STK(arg1)) != OT_CLASS || type(STK(arg2)) != OT_INSTANCE)
				{Raise_Error("cannot apply instanceof between a %s and a %s",GetTypeName(STK(arg1)),GetTypeName(STK(arg2))); SQ_THROW();}
				TARGET = _instance(STK(arg2))->InstanceOf(_class(STK(arg1)))?_true_:_false_;
				continue;
			case _OP_AND:
				if(IsFalse(STK(arg2))) {
					TARGET = STK(arg2);
					ci->_ip += (sarg1);
				}
				continue;
			case _OP_OR:
				if(!IsFalse(STK(arg2))) {
					TARGET = STK(arg2);
					ci->_ip += (sarg1);
				}
				continue;
			case _OP_NEG: _GUARD(NEG_OP(TARGET,STK(arg1))); continue;
			case _OP_NOT: TARGET = (IsFalse(STK(arg1))?_true_:_false_); continue;
			case _OP_BWNOT:
				if(type(STK(arg1)) == OT_INTEGER) {
					SQInteger t = _integer(STK(arg1));
					TARGET = SQInteger(~t);
					continue;
				}
				Raise_Error("attempt to perform a bitwise op on a %s", GetTypeName(STK(arg1)));
				SQ_THROW();
			case _OP_CLOSURE: {
				SQClosure *c = ci->_closure._unVal.pClosure;
				SQFunctionProto *fp = c->_function._unVal.pFunctionProto;
				if(!CLOSURE_OP(TARGET,fp->_functions[arg1]._unVal.pFunctionProto)) { SQ_THROW(); }
				continue;
			}
			case _OP_YIELD:{
				if(ci->_generator) {
					if(sarg1 != MAX_FUNC_STACKSIZE) temp_reg = STK(arg1);
					_GUARD(ci->_generator->Yield(this));
					traps -= ci->_etraps;
					if(sarg1 != MAX_FUNC_STACKSIZE) STK(arg1) = temp_reg;
				}
				else { Raise_Error("trying to yield a '%s',only genenerator can be yielded", GetTypeName(ci->_closure)); SQ_THROW();}
				if(Return(arg0, arg1, temp_reg)){
					assert(traps == 0);
					outres = temp_reg;
					return true;
				}

				}
				continue;
			case _OP_RESUME:
				if(type(STK(arg1)) != OT_GENERATOR){ Raise_Error("trying to resume a '%s',only genenerator can be resumed", GetTypeName(STK(arg1))); SQ_THROW();}
				_GUARD(_generator(STK(arg1))->Resume(this, arg0));
				traps += ci->_etraps;
                continue;
			case _OP_FOREACH:{ int tojump;
				_GUARD(FOREACH_OP(STK(arg0),STK(arg2),STK(arg2+1),STK(arg2+2),arg2,sarg1,tojump));
				ci->_ip += tojump; }
				continue;
			case _OP_POSTFOREACH:
				assert(type(STK(arg0)) == OT_GENERATOR);
				if(_generator(STK(arg0))->_state == SQGenerator::eDead)
					ci->_ip += (sarg1 - 1);
				continue;
			case _OP_DELEGATE: _GUARD(DELEGATE_OP(TARGET,STK(arg1),STK(arg2))); continue;
			case _OP_CLONE:
				if(!Clone(STK(arg1), TARGET))
				{ Raise_Error("cloning a %s", GetTypeName(STK(arg1))); SQ_THROW();}
				continue;
			case _OP_TYPEOF: TypeOf(STK(arg1), TARGET); continue;
			case _OP_PUSHTRAP:{
				SQInstruction *_iv = _funcproto(_closure(ci->_closure)->_function)->_instructions;
				_etraps.push_back(SQExceptionTrap(_top,_stackbase, &_iv[(ci->_ip-_iv)+arg1], arg0)); traps++;
				ci->_etraps++;
							  }
				continue;
			case _OP_POPTRAP: {
				for(SQInteger i = 0; i < arg0; i++) {
					_etraps.pop_back(); traps--;
					ci->_etraps--;
				}
							  }
				continue;
			case _OP_THROW:	Raise_Error(TARGET); SQ_THROW();
			case _OP_CLASS: _GUARD(CLASS_OP(TARGET,arg1,arg2)); continue;
			case _OP_NEWSLOTA:
				bool bstatic = (arg0&NEW_SLOT_STATIC_FLAG)?true:false;
				if(type(STK(arg1)) == OT_CLASS) {
					if(type(_class(STK(arg1))->_metamethods[MT_NEWMEMBER]) != OT_NULL ) {
						Push(STK(arg1)); Push(STK(arg2)); Push(STK(arg3));
						Push((arg0&NEW_SLOT_ATTRIBUTES_FLAG) ? STK(arg2-1) : _null_);
						Push(bstatic);
						int nparams = 5;
						if(Call(_class(STK(arg1))->_metamethods[MT_NEWMEMBER], nparams, _top - nparams, temp_reg,SQFalse,SQFalse)) {
							Pop(nparams);
							continue;
						}
					}
				}
				_GUARD(NewSlot(STK(arg1), STK(arg2), STK(arg3),bstatic));
				if((arg0&NEW_SLOT_ATTRIBUTES_FLAG)) {
					_class(STK(arg1))->SetAttributes(STK(arg2),STK(arg2-1));
				}
				continue;
			}

		}
	}
exception_trap:
	{
		SQObjectPtr currerror = _lasterror;
//		dumpstack(_stackbase);
		SQInteger n = 0;
		SQInteger last_top = _top;
		if(ci) {
			if(_ss(this)->_notifyallexceptions) CallErrorHandler(currerror);

			if(traps) {
				do {
					if(ci->_etraps > 0) {
						SQExceptionTrap &et = _etraps.top();
						ci->_ip = et._ip;
						_top = et._stacksize;
						_stackbase = et._stackbase;
						_stack._vals[_stackbase+et._extarget] = currerror;
						_etraps.pop_back(); traps--; ci->_etraps--;
						CLEARSTACK(last_top);
						goto exception_restore;
					}
					//if is a native closure
					if(type(ci->_closure) != OT_CLOSURE && n)
						break;
					if(ci->_generator) ci->_generator->Kill();
					PopVarArgs(ci->_vargs);
					POP_CALLINFO(this);
					n++;
				} while(_callsstacksize);
			}
			else {
				//call the hook
				if(raiseerror && !_ss(this)->_notifyallexceptions)
					CallErrorHandler(currerror);
			}
			//remove call stack until a C function is found or the cstack is empty
			if(ci) do {
				SQBool exitafterthisone = ci->_root;
				if(ci->_generator) ci->_generator->Kill();
				_stackbase -= ci->_prevstkbase;
				_top = _stackbase + ci->_prevtop;
				PopVarArgs(ci->_vargs);
				POP_CALLINFO(this);
				if( (ci && type(ci->_closure) != OT_CLOSURE) || exitafterthisone) break;
			} while(_callsstacksize);

			CLEARSTACK(last_top);
		}
		_lasterror = currerror;
		return false;
	}
	NOT_REACHED();
}

bool SQVM::CreateClassInstance(SQClass *theclass, SQObjectPtr &inst, SQObjectPtr &constructor)
{
	inst = theclass->CreateInstance();
	if(!theclass->Get(_ss(this)->_constructoridx,constructor)) {
		constructor = _null_;
	}
	return true;
}

void SQVM::CallErrorHandler(SQObjectPtr &error)
{
	if(type(_errorhandler) != OT_NULL) {
		SQObjectPtr out;
		Push(_roottable); Push(error);
		Call(_errorhandler, 2, _top-2, out,SQFalse,SQFalse);
		Pop(2);
	}
}

void SQVM::CallDebugHook(SQInteger type,SQInteger forcedline)
{
	SQObjectPtr temp_reg;
	SQInteger nparams=5;
	SQFunctionProto *func=_funcproto(_closure(ci->_closure)->_function);
	Push(_roottable); Push(type); Push(func->_sourcename); Push(forcedline?forcedline:func->GetLine(ci->_ip)); Push(func->_name);
	Call(_debughook,nparams,_top-nparams,temp_reg,SQFalse,SQFalse);
	Pop(nparams);
}

bool SQVM::CallNative(SQNativeClosure *nclosure,SQInteger nargs,SQInteger stackbase,SQObjectPtr &retval,bool &suspend)
{
	if (_nnativecalls + 1 > MAX_NATIVE_CALLS) { Raise_Error("Native stack overflow"); return false; }
	SQInteger nparamscheck = nclosure->_nparamscheck;
	if(((nparamscheck > 0) && (nparamscheck != nargs))
		|| ((nparamscheck < 0) && (nargs < (-nparamscheck)))) {
		Raise_Error("wrong number of parameters");
		return false;
		}

	SQInteger tcs;
	if((tcs = nclosure->_typecheck.size())) {
		for(SQInteger i = 0; i < nargs && i < tcs; i++)
			if((nclosure->_typecheck._vals[i] != -1) && !(type(_stack._vals[stackbase+i]) & nclosure->_typecheck[i])) {
                Raise_ParamTypeError(i,nclosure->_typecheck._vals[i],type(_stack._vals[stackbase+i]));
				return false;
			}
	}
	_nnativecalls++;
	if ((_top + MIN_STACK_OVERHEAD) > (SQInteger)_stack.size()) {
		_stack.resize(_stack.size() + (MIN_STACK_OVERHEAD<<1));
	}
	SQInteger oldtop = _top;
	SQInteger oldstackbase = _stackbase;
	_top = stackbase + nargs;
	CallInfo lci;
	memset(&lci, 0, sizeof(lci));
	lci._closure = nclosure;
	lci._generator = NULL;
	lci._etraps = 0;
	lci._prevstkbase = (SQInt32) (stackbase - _stackbase);
	lci._ncalls = 1;
	lci._prevtop = (SQInt32) (oldtop - oldstackbase);
	PUSH_CALLINFO(this, lci);
	_stackbase = stackbase;
	//push free variables
	SQInteger outers = nclosure->_outervalues.size();
	for (SQInteger i = 0; i < outers; i++) {
		Push(nclosure->_outervalues[i]);
	}

	if(type(nclosure->_env) == OT_WEAKREF) {
		_stack[stackbase] = _weakref(nclosure->_env)->_obj;
	}


	/* Store the call stack size, so we can restore that */
	SQInteger cstksize = _callsstacksize;
	SQInteger ret;
	try {
		SQBool can_suspend = this->_can_suspend;
		this->_can_suspend = false;
		ret = (nclosure->_function)(this);
		this->_can_suspend = can_suspend;
	} catch (...) {
		_nnativecalls--;
		suspend = false;

		_callsstacksize = cstksize;
		_stackbase = oldstackbase;
		_top = oldtop;

		POP_CALLINFO(this);

		while(oldtop > _stackbase + stackbase) _stack._vals[oldtop--].Null();
		throw;
	}

	_callsstacksize = cstksize;

	_nnativecalls--;
	suspend = false;
	if( ret == SQ_SUSPEND_FLAG) suspend = true;
	else if (ret < 0) {
		_stackbase = oldstackbase;
		_top = oldtop;
		POP_CALLINFO(this);
		while(oldtop > _stackbase + stackbase) _stack._vals[oldtop--].Null();
		Raise_Error(_lasterror);
		return false;
	}

	if (ret != 0){ retval = TOP(); TOP().Null(); }
	else { retval = _null_; }
	_stackbase = oldstackbase;
	_top = oldtop;
	POP_CALLINFO(this);
	while(oldtop > _stackbase + stackbase) _stack._vals[oldtop--].Null();
	return true;
}

bool SQVM::Get(const SQObjectPtr &self,const SQObjectPtr &key,SQObjectPtr &dest,bool raw, bool fetchroot)
{
	switch(type(self)){
	case OT_TABLE:
		if(_table(self)->Get(key,dest))return true;
		break;
	case OT_ARRAY:
		if(sq_isnumeric(key)){
			return _array(self)->Get(tointeger(key),dest);
		}
		break;
	case OT_INSTANCE:
		if(_instance(self)->Get(key,dest)) return true;
		break;
	default:break; //shut up compiler
	}
	if(FallBackGet(self,key,dest,raw)) return true;

	if(fetchroot) {
		if(_rawval(STK(0)) == _rawval(self) &&
			type(STK(0)) == type(self)) {
				return _table(_roottable)->Get(key,dest);
		}
	}
	return false;
}

bool SQVM::FallBackGet(const SQObjectPtr &self,const SQObjectPtr &key,SQObjectPtr &dest,bool raw)
{
	switch(type(self)){
	case OT_CLASS:
		return _class(self)->Get(key,dest);
		break;
	case OT_TABLE:
	case OT_USERDATA:
        //delegation
		if(_delegable(self)->_delegate) {
			if(Get(SQObjectPtr(_delegable(self)->_delegate),key,dest,raw,false))
				return true;
			if(raw)return false;
			Push(self);Push(key);
			if(CallMetaMethod(_delegable(self),MT_GET,2,dest))
				return true;
		}
		if(type(self) == OT_TABLE) {
			if(raw) return false;
			return _table_ddel->Get(key,dest);
		}
		return false;
		break;
	case OT_ARRAY:
		if(raw)return false;
		return _array_ddel->Get(key,dest);
	case OT_STRING:
		if(sq_isnumeric(key)){
			SQInteger n=tointeger(key);
			if(abs((int)n)<_string(self)->_len){
				if(n<0)n=_string(self)->_len-n;
				dest=SQInteger(_stringval(self)[n]);
				return true;
			}
			return false;
		}
		else {
			if(raw)return false;
			return _string_ddel->Get(key,dest);
		}
		break;
	case OT_INSTANCE:
		if(raw)return false;
		Push(self);Push(key);
		if(!CallMetaMethod(_delegable(self),MT_GET,2,dest)) {
			return _instance_ddel->Get(key,dest);
		}
		return true;
	case OT_INTEGER:case OT_FLOAT:case OT_BOOL:
		if(raw)return false;
		return _number_ddel->Get(key,dest);
	case OT_GENERATOR:
		if(raw)return false;
		return _generator_ddel->Get(key,dest);
	case OT_CLOSURE: case OT_NATIVECLOSURE:
		if(raw)return false;
		return _closure_ddel->Get(key,dest);
	case OT_THREAD:
		if(raw)return false;
		return  _thread_ddel->Get(key,dest);
	case OT_WEAKREF:
		if(raw)return false;
		return  _weakref_ddel->Get(key,dest);
	default:return false;
	}
	return false;
}

bool SQVM::Set(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val,bool fetchroot)
{
	switch(type(self)){
	case OT_TABLE:
		if(_table(self)->Set(key,val))
			return true;
		if(_table(self)->_delegate) {
			if(Set(_table(self)->_delegate,key,val,false)) {
				return true;
			}
		}
		FALLTHROUGH;
	case OT_USERDATA:
		if(_delegable(self)->_delegate) {
			SQObjectPtr t;
			Push(self);Push(key);Push(val);
			if(CallMetaMethod(_delegable(self),MT_SET,3,t)) return true;
		}
		break;
	case OT_INSTANCE:{
		if(_instance(self)->Set(key,val))
			return true;
		SQObjectPtr t;
		Push(self);Push(key);Push(val);
		if(CallMetaMethod(_delegable(self),MT_SET,3,t)) return true;
		}
		break;
	case OT_ARRAY:
		if(!sq_isnumeric(key)) {Raise_Error("indexing %s with %s",GetTypeName(self),GetTypeName(key)); return false; }
		return _array(self)->Set(tointeger(key),val);
	default:
		Raise_Error("trying to set '%s'",GetTypeName(self));
		return false;
	}
	if(fetchroot) {
		if(_rawval(STK(0)) == _rawval(self) &&
			type(STK(0)) == type(self)) {
				return _table(_roottable)->Set(key,val);
			}
	}
	return false;
}

bool SQVM::Clone(const SQObjectPtr &self,SQObjectPtr &target)
{
	SQObjectPtr temp_reg;
	SQObjectPtr newobj;
	switch(type(self)){
	case OT_TABLE:
		newobj = _table(self)->Clone();
		goto cloned_mt;
	case OT_INSTANCE:
		newobj = _instance(self)->Clone(_ss(this));
cloned_mt:
		if(_delegable(newobj)->_delegate){
			Push(newobj);
			Push(self);
			CallMetaMethod(_delegable(newobj),MT_CLONED,2,temp_reg);
		}
		target = newobj;
		return true;
	case OT_ARRAY:
		target = _array(self)->Clone();
		return true;
	default: return false;
	}
}

bool SQVM::NewSlot(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val,bool bstatic)
{
	if(type(key) == OT_NULL) { Raise_Error("null cannot be used as index"); return false; }
	switch(type(self)) {
	case OT_TABLE: {
		bool rawcall = true;
		if(_table(self)->_delegate) {
			SQObjectPtr res;
			if(!_table(self)->Get(key,res)) {
				Push(self);Push(key);Push(val);
				rawcall = !CallMetaMethod(_table(self),MT_NEWSLOT,3,res);
			}
		}
		if(rawcall) _table(self)->NewSlot(key,val); //cannot fail

		break;}
	case OT_INSTANCE: {
		SQObjectPtr res;
		Push(self);Push(key);Push(val);
		if(!CallMetaMethod(_instance(self),MT_NEWSLOT,3,res)) {
			Raise_Error("class instances do not support the new slot operator");
			return false;
		}
		break;}
	case OT_CLASS:
		if(!_class(self)->NewSlot(_ss(this),key,val,bstatic)) {
			if(_class(self)->_locked) {
				Raise_Error("trying to modify a class that has already been instantiated");
				return false;
			}
			else {
				SQObjectPtr oval = PrintObjVal(key);
				Raise_Error("the property '%s' already exists",_stringval(oval));
				return false;
			}
		}
		break;
	default:
		Raise_Error("indexing %s with %s",GetTypeName(self),GetTypeName(key));
		return false;
		break;
	}
	return true;
}

bool SQVM::DeleteSlot(const SQObjectPtr &self,const SQObjectPtr &key,SQObjectPtr &res)
{
	switch(type(self)) {
	case OT_TABLE:
	case OT_INSTANCE:
	case OT_USERDATA: {
		SQObjectPtr t;
		bool handled = false;
		if(_delegable(self)->_delegate) {
			Push(self);Push(key);
			handled = CallMetaMethod(_delegable(self),MT_DELSLOT,2,t);
		}

		if(!handled) {
			if(type(self) == OT_TABLE) {
				if(_table(self)->Get(key,t)) {
					_table(self)->Remove(key);
				}
				else {
					Raise_IdxError((const SQObject &)key);
					return false;
				}
			}
			else {
				Raise_Error("cannot delete a slot from %s",GetTypeName(self));
				return false;
			}
		}
		res = t;
				}
		break;
	default:
		Raise_Error("attempt to delete a slot from a %s",GetTypeName(self));
		return false;
	}
	return true;
}

bool SQVM::Call(SQObjectPtr &closure,SQInteger nparams,SQInteger stackbase,SQObjectPtr &outres,SQBool raiseerror,SQBool can_suspend)
{
#ifdef _DEBUG
SQInteger prevstackbase = _stackbase;
#endif
	switch(type(closure)) {
	case OT_CLOSURE: {
		assert(!can_suspend || this->_can_suspend);
		SQBool backup_suspend = this->_can_suspend;
		this->_can_suspend = can_suspend;
		bool ret = Execute(closure, _top - nparams, nparams, stackbase,outres,raiseerror);
		this->_can_suspend = backup_suspend;
		return ret;
					 }
		break;
	case OT_NATIVECLOSURE:{
		bool suspend;
		return CallNative(_nativeclosure(closure), nparams, stackbase, outres,suspend);

						  }
		break;
	case OT_CLASS: {
		SQObjectPtr constr;
		SQObjectPtr temp;
		CreateClassInstance(_class(closure),outres,constr);
		if(type(constr) != OT_NULL) {
			_stack[stackbase] = outres;
			return Call(constr,nparams,stackbase,temp,raiseerror,false);
		}
		return true;
				   }
		break;
	default:
		return false;
	}
#ifdef _DEBUG
	if(!_suspended) {
		assert(_stackbase == prevstackbase);
	}
#endif
	return true;
}

bool SQVM::CallMetaMethod(SQDelegable *del,SQMetaMethod mm,SQInteger nparams,SQObjectPtr &outres)
{
	SQObjectPtr closure;
	if(del->GetMetaMethod(this, mm, closure)) {
		if(Call(closure, nparams, _top - nparams, outres, SQFalse, SQFalse)) {
			Pop(nparams);
			return true;
		}
	}
	Pop(nparams);
	return false;
}

void SQVM::Remove(SQInteger n) {
	n = (n >= 0)?n + _stackbase - 1:_top + n;
	for(SQInteger i = n; i < _top; i++){
		_stack[i] = _stack[i+1];
	}
	_stack[_top] = _null_;
	_top--;
}

void SQVM::Pop() {
	_stack[--_top] = _null_;
}

void SQVM::Pop(SQInteger n) {
	for(SQInteger i = 0; i < n; i++){
		_stack[--_top] = _null_;
	}
}

void SQVM::Push(const SQObjectPtr &o) {
	/* Normally the stack shouldn't get this full, sometimes it might. As of now
	 * all cases have been bugs in "our" (OpenTTD) code. Trigger an assert for
	 * all debug builds and for the release builds just increase the stack size.
	 * This way getting a false positive isn't that bad (releases work fine) and
	 * if there is something fishy it can be caught in RCs/nightlies. */
#ifdef NDEBUG
	if (_top >= (int)_stack.capacity()) _stack.resize(2 * _stack.capacity());
#else
	assert(_top < (int)_stack.capacity());
#endif
	_stack[_top++] = o;
}
SQObjectPtr &SQVM::Top() { return _stack[_top-1]; }
SQObjectPtr &SQVM::PopGet() { return _stack[--_top]; }
SQObjectPtr &SQVM::GetUp(SQInteger n) { return _stack[_top+n]; }
SQObjectPtr &SQVM::GetAt(SQInteger n) { return _stack[n]; }

#ifdef _DEBUG_DUMP
void SQVM::dumpstack(SQInteger stackbase,bool dumpall)
{
	SQInteger size=dumpall?_stack.size():_top;
	SQInteger n=0;
	printf("\n>>>>stack dump<<<<\n");
	CallInfo &ci=_callsstack[_callsstacksize-1];
	printf("IP: %p\n",ci._ip);
	printf("prev stack base: %d\n",ci._prevstkbase);
	printf("prev top: %d\n",ci._prevtop);
	for(SQInteger i=0;i<size;i++){
		SQObjectPtr &obj=_stack[i];
		if(stackbase==i)printf(">");else printf(" ");
		printf("[%d]:",n);
		switch(type(obj)){
		case OT_FLOAT:			printf("FLOAT %.3f",_float(obj));break;
		case OT_INTEGER:		printf("INTEGER %d",_integer(obj));break;
		case OT_BOOL:			printf("BOOL %s",_integer(obj)?"true":"false");break;
		case OT_STRING:			printf("STRING %s",_stringval(obj));break;
		case OT_NULL:			printf("NULL");	break;
		case OT_TABLE:			printf("TABLE %p[%p]",_table(obj),_table(obj)->_delegate);break;
		case OT_ARRAY:			printf("ARRAY %p",_array(obj));break;
		case OT_CLOSURE:		printf("CLOSURE [%p]",_closure(obj));break;
		case OT_NATIVECLOSURE:	printf("NATIVECLOSURE");break;
		case OT_USERDATA:		printf("USERDATA %p[%p]",_userdataval(obj),_userdata(obj)->_delegate);break;
		case OT_GENERATOR:		printf("GENERATOR %p",_generator(obj));break;
		case OT_THREAD:			printf("THREAD [%p]",_thread(obj));break;
		case OT_USERPOINTER:	printf("USERPOINTER %p",_userpointer(obj));break;
		case OT_CLASS:			printf("CLASS %p",_class(obj));break;
		case OT_INSTANCE:		printf("INSTANCE %p",_instance(obj));break;
		case OT_WEAKREF:		printf("WEAKERF %p",_weakref(obj));break;
		default:
			assert(0);
			break;
		};
		printf("\n");
		++n;
	}
}



#endif
