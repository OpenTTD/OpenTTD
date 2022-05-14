/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"

#include "sqpcheader.h"
#include "sqvm.h"
#include "sqtable.h"
#include "sqclass.h"
#include "sqclosure.h"

#include "../../../safeguards.h"

SQClass::SQClass(SQSharedState *ss,SQClass *base)
{
	_base = base;
	_typetag = nullptr;
	_hook = nullptr;
	_udsize = 0;
	_metamethods.resize(MT_LAST); //size it to max size
	if(_base) {
		_defaultvalues.copy(base->_defaultvalues);
		_methods.copy(base->_methods);
		_metamethods.copy(base->_metamethods);
		__ObjAddRef(_base);
	}
	_members = base?base->_members->Clone() : SQTable::Create(ss,0);
	__ObjAddRef(_members);
	_locked = false;
	INIT_CHAIN();
	ADD_TO_CHAIN(&_sharedstate->_gc_chain, this);
}

void SQClass::Finalize() {
	_attributes = _null_;
	/* SQInstance's Finalize depends on the size of this sqvector, so instead of
	 * resizing, all SQObjectPtrs are set to "null" so it holds no references to
	 * other objects anymore. That way everything gets released properly. */
	for (SQUnsignedInteger i = 0; i < _defaultvalues.size(); i++) {
		_defaultvalues[i].val = _null_;
		_defaultvalues[i].attrs = _null_;
	}
	_methods.resize(0);
	_metamethods.resize(0);
	__ObjRelease(_members);
	if(_base) {
		__ObjRelease(_base);
	}
}

SQClass::~SQClass()
{
	REMOVE_FROM_CHAIN(&_sharedstate->_gc_chain, this);
	Finalize();
}

bool SQClass::NewSlot(SQSharedState *ss,const SQObjectPtr &key,const SQObjectPtr &val,bool bstatic)
{
	SQObjectPtr temp;
	if(_locked)
		return false; //the class already has an instance so cannot be modified
	if(_members->Get(key,temp) && _isfield(temp)) //overrides the default value
	{
		_defaultvalues[_member_idx(temp)].val = val;
		return true;
	}
	if(type(val) == OT_CLOSURE || type(val) == OT_NATIVECLOSURE || bstatic) {
		SQInteger mmidx;
		if((type(val) == OT_CLOSURE || type(val) == OT_NATIVECLOSURE) &&
			(mmidx = ss->GetMetaMethodIdxByName(key)) != -1) {
			_metamethods[mmidx] = val;
		}
		else {
			if(type(temp) == OT_NULL) {
				SQClassMember m;
				m.val = val;
				_members->NewSlot(key,SQObjectPtr(_make_method_idx(_methods.size())));
				_methods.push_back(m);
			}
			else {
				_methods[_member_idx(temp)].val = val;
			}
		}
		return true;
	}
	SQClassMember m;
	m.val = val;
	_members->NewSlot(key,SQObjectPtr(_make_field_idx(_defaultvalues.size())));
	_defaultvalues.push_back(m);
	return true;
}

SQInstance *SQClass::CreateInstance()
{
	if(!_locked) Lock();
	return SQInstance::Create(_opt_ss(this),this);
}

SQInteger SQClass::Next(const SQObjectPtr &refpos, SQObjectPtr &outkey, SQObjectPtr &outval)
{
	SQObjectPtr oval;
	SQInteger idx = _members->Next(false,refpos,outkey,oval);
	if(idx != -1) {
		if(_ismethod(oval)) {
			outval = _methods[_member_idx(oval)].val;
		}
		else {
			SQObjectPtr &o = _defaultvalues[_member_idx(oval)].val;
			outval = _realval(o);
		}
	}
	return idx;
}

bool SQClass::SetAttributes(const SQObjectPtr &key,const SQObjectPtr &val)
{
	SQObjectPtr idx;
	if(_members->Get(key,idx)) {
		if(_isfield(idx))
			_defaultvalues[_member_idx(idx)].attrs = val;
		else
			_methods[_member_idx(idx)].attrs = val;
		return true;
	}
	return false;
}

bool SQClass::GetAttributes(const SQObjectPtr &key,SQObjectPtr &outval)
{
	SQObjectPtr idx;
	if(_members->Get(key,idx)) {
		outval = (_isfield(idx)?_defaultvalues[_member_idx(idx)].attrs:_methods[_member_idx(idx)].attrs);
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////
void SQInstance::Init(SQSharedState *ss)
{
	_userpointer = nullptr;
	_hook = nullptr;
	__ObjAddRef(_class);
	_delegate = _class->_members;
	INIT_CHAIN();
	ADD_TO_CHAIN(&_sharedstate->_gc_chain, this);
}

SQInstance::SQInstance(SQSharedState *ss, SQClass *c, SQInteger memsize)
{
	_memsize = memsize;
	_class = c;
	SQUnsignedInteger nvalues = _class->_defaultvalues.size();
	for(SQUnsignedInteger n = 0; n < nvalues; n++) {
		new (&_values[n]) SQObjectPtr(_class->_defaultvalues[n].val);
	}
	Init(ss);
}

SQInstance::SQInstance(SQSharedState *ss, SQInstance *i, SQInteger memsize)
{
	_memsize = memsize;
	_class = i->_class;
	SQUnsignedInteger nvalues = _class->_defaultvalues.size();
	for(SQUnsignedInteger n = 0; n < nvalues; n++) {
		new (&_values[n]) SQObjectPtr(i->_values[n]);
	}
	Init(ss);
}

void SQInstance::Finalize()
{
	SQUnsignedInteger nvalues = _class->_defaultvalues.size();
	__ObjRelease(_class);
	for(SQUnsignedInteger i = 0; i < nvalues; i++) {
		_values[i] = _null_;
	}
}

SQInstance::~SQInstance()
{
	REMOVE_FROM_CHAIN(&_sharedstate->_gc_chain, this);
	if(_class){ Finalize(); } //if _class is null it was already finalized by the GC
}

bool SQInstance::GetMetaMethod(SQVM *v,SQMetaMethod mm,SQObjectPtr &res)
{
	if(type(_class->_metamethods[mm]) != OT_NULL) {
		res = _class->_metamethods[mm];
		return true;
	}
	return false;
}

bool SQInstance::InstanceOf(SQClass *trg)
{
	SQClass *parent = _class;
	while(parent != nullptr) {
		if(parent == trg)
			return true;
		parent = parent->_base;
	}
	return false;
}
