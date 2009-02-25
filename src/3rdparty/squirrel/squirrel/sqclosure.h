/*	see copyright notice in squirrel.h */
#ifndef _SQCLOSURE_H_
#define _SQCLOSURE_H_

struct SQFunctionProto;

struct SQClosure : public CHAINABLE_OBJ
{
private:
	SQClosure(SQSharedState *ss,SQFunctionProto *func){_function=func; INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);}
public:
	static SQClosure *Create(SQSharedState *ss,SQFunctionProto *func){
		SQClosure *nc=(SQClosure*)SQ_MALLOC(sizeof(SQClosure));
		new (nc) SQClosure(ss,func);
		return nc;
	}
	void Release(){
		sq_delete(this,SQClosure);
	}
	SQClosure *Clone()
	{
		SQClosure * ret = SQClosure::Create(_opt_ss(this),_funcproto(_function));
		ret->_env = _env;
		ret->_outervalues.copy(_outervalues);
		ret->_defaultparams.copy(_defaultparams);
		return ret;
	}
	~SQClosure()
	{
		REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
	}
	bool Save(SQVM *v,SQUserPointer up,SQWRITEFUNC write);
	static bool Load(SQVM *v,SQUserPointer up,SQREADFUNC read,SQObjectPtr &ret);
#ifndef NO_GARBAGE_COLLECTOR
	void Mark(SQCollectable **chain);
	void Finalize(){_outervalues.resize(0); }
#endif
	SQObjectPtr _env;
	SQObjectPtr _function;
	SQObjectPtrVec _outervalues;
	SQObjectPtrVec _defaultparams;
};
//////////////////////////////////////////////
struct SQGenerator : public CHAINABLE_OBJ
{
	enum SQGeneratorState{eRunning,eSuspended,eDead};
private:
	SQGenerator(SQSharedState *ss,SQClosure *closure){_closure=closure;_state=eRunning;_ci._generator=NULL;INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);}
public:
	static SQGenerator *Create(SQSharedState *ss,SQClosure *closure){
		SQGenerator *nc=(SQGenerator*)SQ_MALLOC(sizeof(SQGenerator));
		new (nc) SQGenerator(ss,closure);
		return nc;
	}
	~SQGenerator()
	{
		REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
	}
    void Kill(){
		_state=eDead;
		_stack.resize(0);
		_closure=_null_;}
	void Release(){
		sq_delete(this,SQGenerator);
	}
	bool Yield(SQVM *v);
	bool Resume(SQVM *v,SQInteger target);
#ifndef NO_GARBAGE_COLLECTOR
	void Mark(SQCollectable **chain);
	void Finalize(){_stack.resize(0);_closure=_null_;}
#endif
	SQObjectPtr _closure;
	SQObjectPtrVec _stack;
	SQObjectPtrVec _vargsstack;
	SQVM::CallInfo _ci;
	ExceptionsTraps _etraps;
	SQGeneratorState _state;
};

struct SQNativeClosure : public CHAINABLE_OBJ
{
private:
	SQNativeClosure(SQSharedState *ss,SQFUNCTION func){_function=func;INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);	}
public:
	static SQNativeClosure *Create(SQSharedState *ss,SQFUNCTION func)
	{
		SQNativeClosure *nc=(SQNativeClosure*)SQ_MALLOC(sizeof(SQNativeClosure));
		new (nc) SQNativeClosure(ss,func);
		return nc;
	}
	SQNativeClosure *Clone()
	{
		SQNativeClosure * ret = SQNativeClosure::Create(_opt_ss(this),_function);
		ret->_env = _env;
		ret->_name = _name;
		ret->_outervalues.copy(_outervalues);
		ret->_typecheck.copy(_typecheck);
		ret->_nparamscheck = _nparamscheck;
		return ret;
	}
	~SQNativeClosure()
	{
		REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
	}
	void Release(){
		sq_delete(this,SQNativeClosure);
	}
#ifndef NO_GARBAGE_COLLECTOR
	void Mark(SQCollectable **chain);
	void Finalize(){_outervalues.resize(0);}
#endif
	SQInteger _nparamscheck;
	SQIntVec _typecheck;
	SQObjectPtrVec _outervalues;
	SQObjectPtr _env;
	SQFUNCTION _function;
	SQObjectPtr _name;
};



#endif //_SQCLOSURE_H_
