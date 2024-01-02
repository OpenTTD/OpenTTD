/*	see copyright notice in squirrel.h */
#ifndef _SQVM_H_
#define _SQVM_H_

#include "sqopcodes.h"
#include "sqobject.h"
#define MAX_NATIVE_CALLS 100
#define MIN_STACK_OVERHEAD 10

#define SQ_SUSPEND_FLAG -666
//base lib
void sq_base_register(HSQUIRRELVM v);

struct SQExceptionTrap{
	SQExceptionTrap(SQInteger ss, SQInteger stackbase,SQInstruction *ip, SQInteger ex_target)
		: _stackbase(stackbase), _stacksize(ss), _ip(ip), _extarget(ex_target) {}
	SQInteger _stackbase;
	SQInteger _stacksize;
	SQInstruction *_ip;
	SQInteger _extarget;
};

#define _INLINE

#define STK(a) _stack._vals[_stackbase+(a)]
#define TARGET _stack._vals[_stackbase+arg0]

typedef sqvector<SQExceptionTrap> ExceptionsTraps;

struct SQVM : public CHAINABLE_OBJ
{
	struct VarArgs {
		VarArgs() { size = 0; base = 0; }
		unsigned short size;
		unsigned short base;
	};

	struct CallInfo{
		SQInstruction *_ip;
		SQObjectPtr *_literals;
		SQObjectPtr _closure;
		SQGenerator *_generator;
		SQInt32 _etraps;
		SQInt32 _prevstkbase;
		SQInt32 _prevtop;
		SQInt32 _target;
		SQInt32 _ncalls;
		SQBool _root;
		VarArgs _vargs;
	};

typedef sqvector<CallInfo> CallInfoVec;
public:
	enum ExecutionType { ET_CALL, ET_RESUME_GENERATOR, ET_RESUME_VM, ET_RESUME_THROW_VM, ET_RESUME_OPENTTD };
	SQVM(SQSharedState *ss);
	~SQVM();
	bool Init(SQVM *friendvm, SQInteger stacksize);
	bool Execute(SQObjectPtr &func, SQInteger target, SQInteger nargs, SQInteger stackbase, SQObjectPtr &outres, SQBool raiseerror, ExecutionType et = ET_CALL);
	//starts a native call return when the NATIVE closure returns
	bool CallNative(SQNativeClosure *nclosure, SQInteger nargs, SQInteger stackbase, SQObjectPtr &retval,bool &suspend);
	//starts a SQUIRREL call in the same "Execution loop"
	bool StartCall(SQClosure *closure, SQInteger target, SQInteger nargs, SQInteger stackbase, bool tailcall);
	bool CreateClassInstance(SQClass *theclass, SQObjectPtr &inst, SQObjectPtr &constructor);
	//call a generic closure pure SQUIRREL or NATIVE
	bool Call(SQObjectPtr &closure, SQInteger nparams, SQInteger stackbase, SQObjectPtr &outres,SQBool raiseerror,SQBool can_suspend);
	SQRESULT Suspend();

	void CallDebugHook(SQInteger type,SQInteger forcedline=0);
	void CallErrorHandler(SQObjectPtr &e);
	bool Get(const SQObjectPtr &self, const SQObjectPtr &key, SQObjectPtr &dest, bool raw, bool fetchroot);
	bool FallBackGet(const SQObjectPtr &self,const SQObjectPtr &key,SQObjectPtr &dest,bool raw);
	bool Set(const SQObjectPtr &self, const SQObjectPtr &key, const SQObjectPtr &val, bool fetchroot);
	bool NewSlot(const SQObjectPtr &self, const SQObjectPtr &key, const SQObjectPtr &val,bool bstatic);
	bool DeleteSlot(const SQObjectPtr &self, const SQObjectPtr &key, SQObjectPtr &res);
	bool Clone(const SQObjectPtr &self, SQObjectPtr &target);
	bool ObjCmp(const SQObjectPtr &o1, const SQObjectPtr &o2,SQInteger &res);
	bool StringCat(const SQObjectPtr &str, const SQObjectPtr &obj, SQObjectPtr &dest);
	bool IsEqual(SQObjectPtr &o1,SQObjectPtr &o2,bool &res);
	void ToString(const SQObjectPtr &o,SQObjectPtr &res);
	SQString *PrintObjVal(const SQObject &o);


	void Raise_Error(const std::string &msg);
	void Raise_Error(SQObjectPtr &desc);
	void Raise_IdxError(const SQObject &o);
	void Raise_CompareError(const SQObject &o1, const SQObject &o2);
	void Raise_ParamTypeError(SQInteger nparam,SQInteger typemask,SQInteger type);

	void TypeOf(const SQObjectPtr &obj1, SQObjectPtr &dest);
	bool CallMetaMethod(SQDelegable *del, SQMetaMethod mm, SQInteger nparams, SQObjectPtr &outres);
	bool ArithMetaMethod(SQInteger op, const SQObjectPtr &o1, const SQObjectPtr &o2, SQObjectPtr &dest);
	bool Return(SQInteger _arg0, SQInteger _arg1, SQObjectPtr &retval);
	//new stuff
	_INLINE bool ARITH_OP(SQUnsignedInteger op,SQObjectPtr &trg,const SQObjectPtr &o1,const SQObjectPtr &o2);
	_INLINE bool BW_OP(SQUnsignedInteger op,SQObjectPtr &trg,const SQObjectPtr &o1,const SQObjectPtr &o2);
	_INLINE bool NEG_OP(SQObjectPtr &trg,const SQObjectPtr &o1);
	_INLINE bool CMP_OP(CmpOP op, const SQObjectPtr &o1,const SQObjectPtr &o2,SQObjectPtr &res);
	bool CLOSURE_OP(SQObjectPtr &target, SQFunctionProto *func);
	bool GETVARGV_OP(SQObjectPtr &target,SQObjectPtr &idx,CallInfo *ci);
	bool CLASS_OP(SQObjectPtr &target,SQInteger base,SQInteger attrs);
	bool GETPARENT_OP(SQObjectPtr &o,SQObjectPtr &target);
	//return true if the loop is finished
	bool FOREACH_OP(SQObjectPtr &o1,SQObjectPtr &o2,SQObjectPtr &o3,SQObjectPtr &o4,SQInteger arg_2,int exitpos,int &jump);
	bool DELEGATE_OP(SQObjectPtr &trg,SQObjectPtr &o1,SQObjectPtr &o2);
	_INLINE bool LOCAL_INC(SQInteger op,SQObjectPtr &target, SQObjectPtr &a, SQObjectPtr &incr);
	_INLINE bool PLOCAL_INC(SQInteger op,SQObjectPtr &target, SQObjectPtr &a, SQObjectPtr &incr);
	_INLINE bool DerefInc(SQInteger op,SQObjectPtr &target, SQObjectPtr &self, SQObjectPtr &key, SQObjectPtr &incr, bool postfix);
	void PopVarArgs(VarArgs &vargs);
	void ClearStack(SQInteger last_top);
#ifdef _DEBUG_DUMP
	void dumpstack(SQInteger stackbase=-1, bool dumpall = false);
#endif

#ifndef NO_GARBAGE_COLLECTOR
	void EnqueueMarkObjectForChildren(SQGCMarkerQueue &queue) override;
#endif
	void Finalize() override;
	void GrowCallStack() {
		SQInteger newsize = _alloccallsstacksize*2;
		_callstackdata.resize(newsize);
		_callsstack = &_callstackdata[0];
		_alloccallsstacksize = newsize;
	}
	void Release() override { sq_delete(this,SQVM); } //does nothing
////////////////////////////////////////////////////////////////////////////
	//stack functions for the api
	void Remove(SQInteger n);

	bool IsFalse(SQObjectPtr &o);

	void Pop();
	void Pop(SQInteger n);
	void Push(const SQObjectPtr &o);
	SQObjectPtr &Top();
	SQObjectPtr &PopGet();
	SQObjectPtr &GetUp(SQInteger n);
	SQObjectPtr &GetAt(SQInteger n);

	SQObjectPtrVec _stack;
	SQObjectPtrVec _vargsstack;
	SQInteger _top;
	SQInteger _stackbase;
	SQObjectPtr _roottable;
	SQObjectPtr _lasterror;
	SQObjectPtr _errorhandler;
	SQObjectPtr _debughook;

	SQObjectPtr temp_reg;


	CallInfo* _callsstack;
	SQInteger _callsstacksize;
	SQInteger _alloccallsstacksize;
	sqvector<CallInfo>  _callstackdata;

	ExceptionsTraps _etraps;
	CallInfo *ci;
	void *_foreignptr;
	//VMs sharing the same state
	SQSharedState *_sharedstate;
	SQInteger _nnativecalls;
	//suspend infos
	SQBool _suspended;
	SQBool _suspended_root;
	SQInteger _suspended_target;
	SQInteger _suspended_traps;
	VarArgs _suspend_varargs;

	SQBool _can_suspend;
	SQInteger _ops_till_suspend;
	SQInteger _ops_till_suspend_error_threshold;
	const char *_ops_till_suspend_error_label;
	SQBool _in_stackoverflow;

	bool ShouldSuspend()
	{
		return _can_suspend && _ops_till_suspend <= 0;
	}

	bool IsOpsTillSuspendError()
	{
		return _ops_till_suspend < _ops_till_suspend_error_threshold;
	}

	void DecreaseOps(SQInteger amount)
	{
		if (_ops_till_suspend - amount < _ops_till_suspend) _ops_till_suspend -= amount;
	}
};

struct AutoDec{
	AutoDec(SQInteger *n) { _n = n; }
	~AutoDec() { (*_n)--; }
	SQInteger *_n;
};

inline SQObjectPtr &stack_get(HSQUIRRELVM v,SQInteger idx){return ((idx>=0)?(v->GetAt(idx+v->_stackbase-1)):(v->GetUp(idx)));}

#define _ss(_vm_) (_vm_)->_sharedstate

#ifndef NO_GARBAGE_COLLECTOR
#define _opt_ss(_vm_) (_vm_)->_sharedstate
#else
#define _opt_ss(_vm_) nullptr
#endif

#define PUSH_CALLINFO(v,nci){ \
	if(v->_callsstacksize == v->_alloccallsstacksize) { \
		if (v->_callsstacksize > 65535 && !v->_in_stackoverflow) {\
			v->_in_stackoverflow = true; \
			v->Raise_Error("stack overflow");\
			v->CallErrorHandler(v->_lasterror);\
			return false;\
		}\
		v->GrowCallStack(); \
	} \
	v->ci = &v->_callsstack[v->_callsstacksize]; \
	*(v->ci) = nci; \
	v->_callsstacksize++; \
}

#define POP_CALLINFO(v){ \
	v->_callsstacksize--; \
	v->ci->_closure.Null(); \
	if(v->_callsstacksize)	\
		v->ci = &v->_callsstack[v->_callsstacksize-1] ; \
	else	\
		v->ci = nullptr; \
}
#endif //_SQVM_H_
