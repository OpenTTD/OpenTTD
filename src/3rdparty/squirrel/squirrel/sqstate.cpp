/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"

#include "sqpcheader.h"
#include "sqopcodes.h"
#include "sqvm.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqarray.h"
#include "squserdata.h"
#include "sqclass.h"

#include "../../../safeguards.h"

SQObjectPtr _null_;
SQObjectPtr _true_(true);
SQObjectPtr _false_(false);
SQObjectPtr _one_((SQInteger)1);
SQObjectPtr _minusone_((SQInteger)-1);

#define newsysstring(s) {	\
	_systemstrings->push_back(SQString::Create(this,s));	\
	}

#define newmetamethod(s) {	\
	_metamethods->push_back(SQString::Create(this,s));	\
	_table(_metamethodsmap)->NewSlot(_metamethods->back(),(SQInteger)(_metamethods->size()-1)); \
	}

bool CompileTypemask(SQIntVec &res,const SQChar *typemask)
{
	SQInteger i = 0;

	SQInteger mask = 0;
	while(typemask[i] != 0) {

		switch(typemask[i]){
				case 'o': mask |= _RT_NULL; break;
				case 'i': mask |= _RT_INTEGER; break;
				case 'f': mask |= _RT_FLOAT; break;
				case 'n': mask |= (_RT_FLOAT | _RT_INTEGER); break;
				case 's': mask |= _RT_STRING; break;
				case 't': mask |= _RT_TABLE; break;
				case 'a': mask |= _RT_ARRAY; break;
				case 'u': mask |= _RT_USERDATA; break;
				case 'c': mask |= (_RT_CLOSURE | _RT_NATIVECLOSURE); break;
				case 'b': mask |= _RT_BOOL; break;
				case 'g': mask |= _RT_GENERATOR; break;
				case 'p': mask |= _RT_USERPOINTER; break;
				case 'v': mask |= _RT_THREAD; break;
				case 'x': mask |= _RT_INSTANCE; break;
				case 'y': mask |= _RT_CLASS; break;
				case 'r': mask |= _RT_WEAKREF; break;
				case '.': mask = -1; res.push_back(mask); i++; mask = 0; continue;
				case ' ': i++; continue; //ignores spaces
				default:
					return false;
		}
		i++;
		if(typemask[i] == '|') {
			i++;
			if(typemask[i] == 0)
				return false;
			continue;
		}
		res.push_back(mask);
		mask = 0;

	}
	return true;
}

SQTable *CreateDefaultDelegate(SQSharedState *ss,SQRegFunction *funcz)
{
	SQInteger i=0;
	SQTable *t=SQTable::Create(ss,0);
	while(funcz[i].name!=0){
		SQNativeClosure *nc = SQNativeClosure::Create(ss,funcz[i].f);
		nc->_nparamscheck = funcz[i].nparamscheck;
		nc->_name = SQString::Create(ss,funcz[i].name);
		if(funcz[i].typemask && !CompileTypemask(nc->_typecheck,funcz[i].typemask))
			return NULL;
		t->NewSlot(SQString::Create(ss,funcz[i].name),nc);
		i++;
	}
	return t;
}

SQSharedState::SQSharedState()
{
	_compilererrorhandler = NULL;
	_printfunc = NULL;
	_debuginfo = false;
	_notifyallexceptions = false;
	_scratchpad=NULL;
	_scratchpadsize=0;
#ifndef NO_GARBAGE_COLLECTOR
	_gc_chain=NULL;
#endif
	sq_new(_stringtable,SQStringTable);
	sq_new(_metamethods,SQObjectPtrVec);
	sq_new(_systemstrings,SQObjectPtrVec);
	sq_new(_types,SQObjectPtrVec);
	_metamethodsmap = SQTable::Create(this,MT_LAST-1);
	//adding type strings to avoid memory trashing
	//types names
	newsysstring("null");
	newsysstring("table");
	newsysstring("array");
	newsysstring("closure");
	newsysstring("string");
	newsysstring("userdata");
	newsysstring("integer");
	newsysstring("float");
	newsysstring("userpointer");
	newsysstring("function");
	newsysstring("generator");
	newsysstring("thread");
	newsysstring("class");
	newsysstring("instance");
	newsysstring("bool");
	//meta methods
	newmetamethod(MM_ADD);
	newmetamethod(MM_SUB);
	newmetamethod(MM_MUL);
	newmetamethod(MM_DIV);
	newmetamethod(MM_UNM);
	newmetamethod(MM_MODULO);
	newmetamethod(MM_SET);
	newmetamethod(MM_GET);
	newmetamethod(MM_TYPEOF);
	newmetamethod(MM_NEXTI);
	newmetamethod(MM_CMP);
	newmetamethod(MM_CALL);
	newmetamethod(MM_CLONED);
	newmetamethod(MM_NEWSLOT);
	newmetamethod(MM_DELSLOT);
	newmetamethod(MM_TOSTRING);
	newmetamethod(MM_NEWMEMBER);
	newmetamethod(MM_INHERITED);

	_constructoridx = SQString::Create(this,"constructor");
	_registry = SQTable::Create(this,0);
	_consts = SQTable::Create(this,0);
	_table_default_delegate = CreateDefaultDelegate(this,_table_default_delegate_funcz);
	_array_default_delegate = CreateDefaultDelegate(this,_array_default_delegate_funcz);
	_string_default_delegate = CreateDefaultDelegate(this,_string_default_delegate_funcz);
	_number_default_delegate = CreateDefaultDelegate(this,_number_default_delegate_funcz);
	_closure_default_delegate = CreateDefaultDelegate(this,_closure_default_delegate_funcz);
	_generator_default_delegate = CreateDefaultDelegate(this,_generator_default_delegate_funcz);
	_thread_default_delegate = CreateDefaultDelegate(this,_thread_default_delegate_funcz);
	_class_default_delegate = CreateDefaultDelegate(this,_class_default_delegate_funcz);
	_instance_default_delegate = CreateDefaultDelegate(this,_instance_default_delegate_funcz);
	_weakref_default_delegate = CreateDefaultDelegate(this,_weakref_default_delegate_funcz);

}

SQSharedState::~SQSharedState()
{
	_constructoridx = _null_;
	_table(_registry)->Finalize();
	_table(_consts)->Finalize();
	_table(_metamethodsmap)->Finalize();
	_registry = _null_;
	_consts = _null_;
	_metamethodsmap = _null_;
	while(!_systemstrings->empty()) {
		_systemstrings->back()=_null_;
		_systemstrings->pop_back();
	}
	_thread(_root_vm)->Finalize();
	_root_vm = _null_;
	_table_default_delegate = _null_;
	_array_default_delegate = _null_;
	_string_default_delegate = _null_;
	_number_default_delegate = _null_;
	_closure_default_delegate = _null_;
	_generator_default_delegate = _null_;
	_thread_default_delegate = _null_;
	_class_default_delegate = _null_;
	_instance_default_delegate = _null_;
	_weakref_default_delegate = _null_;
	_refs_table.Finalize();
#ifndef NO_GARBAGE_COLLECTOR
	SQCollectable *t = _gc_chain;
	SQCollectable *nx = NULL;
	if(t) {
		t->_uiRef++;
		while(t) {
			t->Finalize();
			nx = t->_next;
			if(nx) nx->_uiRef++;
			if(--t->_uiRef == 0)
				t->Release();
			t = nx;
		}
	}
//	assert(_gc_chain==NULL); //just to proove a theory
	while(_gc_chain){
		_gc_chain->_uiRef--;
		_gc_chain->Release();
	}
#endif

	sq_delete(_types,SQObjectPtrVec);
	sq_delete(_systemstrings,SQObjectPtrVec);
	sq_delete(_metamethods,SQObjectPtrVec);
	sq_delete(_stringtable,SQStringTable);
	if(_scratchpad)SQ_FREE(_scratchpad,_scratchpadsize);
}


SQInteger SQSharedState::GetMetaMethodIdxByName(const SQObjectPtr &name)
{
	if(type(name) != OT_STRING)
		return -1;
	SQObjectPtr ret;
	if(_table(_metamethodsmap)->Get(name,ret)) {
		return _integer(ret);
	}
	return -1;
}

#ifndef NO_GARBAGE_COLLECTOR

void SQSharedState::MarkObject(SQObjectPtr &o,SQCollectable **chain)
{
	switch(type(o)){
	case OT_TABLE:_table(o)->Mark(chain);break;
	case OT_ARRAY:_array(o)->Mark(chain);break;
	case OT_USERDATA:_userdata(o)->Mark(chain);break;
	case OT_CLOSURE:_closure(o)->Mark(chain);break;
	case OT_NATIVECLOSURE:_nativeclosure(o)->Mark(chain);break;
	case OT_GENERATOR:_generator(o)->Mark(chain);break;
	case OT_THREAD:_thread(o)->Mark(chain);break;
	case OT_CLASS:_class(o)->Mark(chain);break;
	case OT_INSTANCE:_instance(o)->Mark(chain);break;
	default: break; //shutup compiler
	}
}


SQInteger SQSharedState::CollectGarbage(SQVM *vm)
{
	SQInteger n=0;
	SQCollectable *tchain=NULL;
	SQVM *vms = _thread(_root_vm);

	vms->Mark(&tchain);
#ifdef WITH_ASSERT
	SQInteger x = _table(_thread(_root_vm)->_roottable)->CountUsed();
#endif
	_refs_table.Mark(&tchain);
	MarkObject(_registry,&tchain);
	MarkObject(_consts,&tchain);
	MarkObject(_metamethodsmap,&tchain);
	MarkObject(_table_default_delegate,&tchain);
	MarkObject(_array_default_delegate,&tchain);
	MarkObject(_string_default_delegate,&tchain);
	MarkObject(_number_default_delegate,&tchain);
	MarkObject(_generator_default_delegate,&tchain);
	MarkObject(_thread_default_delegate,&tchain);
	MarkObject(_closure_default_delegate,&tchain);
	MarkObject(_class_default_delegate,&tchain);
	MarkObject(_instance_default_delegate,&tchain);
	MarkObject(_weakref_default_delegate,&tchain);

	SQCollectable *t = _gc_chain;
	SQCollectable *nx = NULL;
	if(t) {
		t->_uiRef++;
		while(t) {
			t->Finalize();
			nx = t->_next;
			if(nx) nx->_uiRef++;
			if(--t->_uiRef == 0)
				t->Release();
			t = nx;
			n++;
		}
	}

	t = tchain;
	while(t) {
		t->UnMark();
		t = t->_next;
	}
	_gc_chain = tchain;
#ifdef WITH_ASSERT
	SQInteger z = _table(_thread(_root_vm)->_roottable)->CountUsed();
	assert(z == x);
#endif
	return n;
}
#endif

#ifndef NO_GARBAGE_COLLECTOR
void SQCollectable::AddToChain(SQCollectable **chain,SQCollectable *c)
{
    c->_prev = NULL;
	c->_next = *chain;
	if(*chain) (*chain)->_prev = c;
	*chain = c;
}

void SQCollectable::RemoveFromChain(SQCollectable **chain,SQCollectable *c)
{
	if(c->_prev) c->_prev->_next = c->_next;
	else *chain = c->_next;
	if(c->_next)
		c->_next->_prev = c->_prev;
	c->_next = NULL;
	c->_prev = NULL;
}
#endif

SQChar* SQSharedState::GetScratchPad(SQInteger size)
{
	SQInteger newsize;
	if(size>0) {
		if(_scratchpadsize < size) {
			newsize = size + (size>>1);
			_scratchpad = (SQChar *)SQ_REALLOC(_scratchpad,_scratchpadsize,newsize);
			_scratchpadsize = newsize;

		}else if(_scratchpadsize >= (size<<5)) {
			newsize = _scratchpadsize >> 1;
			_scratchpad = (SQChar *)SQ_REALLOC(_scratchpad,_scratchpadsize,newsize);
			_scratchpadsize = newsize;
		}
	}
	return _scratchpad;
}

RefTable::RefTable()
{
	AllocNodes(4);
}

void RefTable::Finalize()
{
	RefNode *nodes = _nodes;
	for(SQUnsignedInteger n = 0; n < _numofslots; n++) {
		nodes->obj = _null_;
		nodes++;
	}
}

RefTable::~RefTable()
{
	SQ_FREE(_buckets,(_numofslots * sizeof(RefNode *)) + (_numofslots * sizeof(RefNode)));
}

#ifndef NO_GARBAGE_COLLECTOR
void RefTable::Mark(SQCollectable **chain)
{
	RefNode *nodes = (RefNode *)_nodes;
	for(SQUnsignedInteger n = 0; n < _numofslots; n++) {
		if(type(nodes->obj) != OT_NULL) {
			SQSharedState::MarkObject(nodes->obj,chain);
		}
		nodes++;
	}
}
#endif

void RefTable::AddRef(SQObject &obj)
{
	SQHash mainpos;
	RefNode *prev;
	RefNode *ref = Get(obj,mainpos,&prev,true);
	ref->refs++;
}

SQBool RefTable::Release(SQObject &obj)
{
	SQHash mainpos;
	RefNode *prev;
	RefNode *ref = Get(obj,mainpos,&prev,false);
	if(ref) {
		if(--ref->refs == 0) {
			SQObjectPtr o = ref->obj;
			if(prev) {
				prev->next = ref->next;
			}
			else {
				_buckets[mainpos] = ref->next;
			}
			ref->next = _freelist;
			_freelist = ref;
			_slotused--;
			ref->obj = _null_;
			//<<FIXME>>test for shrink?
			return SQTrue;
		}
	}
	else {
		assert(0);
	}
	return SQFalse;
}

void RefTable::Resize(SQUnsignedInteger size)
{
	RefNode **oldbucks = _buckets;
	RefNode *t = _nodes;
	SQUnsignedInteger oldnumofslots = _numofslots;
	AllocNodes(size);
	//rehash
	SQUnsignedInteger nfound = 0;
	for(SQUnsignedInteger n = 0; n < oldnumofslots; n++) {
		if(type(t->obj) != OT_NULL) {
			//add back;
			assert(t->refs != 0);
			RefNode *nn = Add(::HashObj(t->obj)&(_numofslots-1),t->obj);
			nn->refs = t->refs;
			t->obj = _null_;
			nfound++;
		}
		t++;
	}
	assert(nfound == oldnumofslots);
	SQ_FREE(oldbucks,(oldnumofslots * sizeof(RefNode *)) + (oldnumofslots * sizeof(RefNode)));
}

RefTable::RefNode *RefTable::Add(SQHash mainpos,SQObject &obj)
{
	RefNode *t = _buckets[mainpos];
	RefNode *newnode = _freelist;
	newnode->obj = obj;
	_buckets[mainpos] = newnode;
	_freelist = _freelist->next;
	newnode->next = t;
	assert(newnode->refs == 0);
	_slotused++;
	return newnode;
}

RefTable::RefNode *RefTable::Get(SQObject &obj,SQHash &mainpos,RefNode **prev,bool add)
{
	RefNode *ref;
	mainpos = ::HashObj(obj)&(_numofslots-1);
	*prev = NULL;
	for (ref = _buckets[mainpos]; ref; ) {
		if(_rawval(ref->obj) == _rawval(obj) && type(ref->obj) == type(obj))
			break;
		*prev = ref;
		ref = ref->next;
	}
	if(ref == NULL && add) {
		if(_numofslots == _slotused) {
			assert(_freelist == 0);
			Resize(_numofslots*2);
			mainpos = ::HashObj(obj)&(_numofslots-1);
		}
		ref = Add(mainpos,obj);
	}
	return ref;
}

void RefTable::AllocNodes(SQUnsignedInteger size)
{
	RefNode **bucks;
	RefNode *nodes;
	bucks = (RefNode **)SQ_MALLOC((size * sizeof(RefNode *)) + (size * sizeof(RefNode)));
	nodes = (RefNode *)&bucks[size];
	RefNode *temp = nodes;
	SQUnsignedInteger n;
	for(n = 0; n < size - 1; n++) {
		bucks[n] = NULL;
		temp->refs = 0;
		new (&temp->obj) SQObjectPtr;
		temp->next = temp+1;
		temp++;
	}
	bucks[n] = NULL;
	temp->refs = 0;
	new (&temp->obj) SQObjectPtr;
	temp->next = NULL;
	_freelist = nodes;
	_nodes = nodes;
	_buckets = bucks;
	_slotused = 0;
	_numofslots = size;
}
//////////////////////////////////////////////////////////////////////////
//SQStringTable
/*
 * The following code is based on Lua 4.0 (Copyright 1994-2002 Tecgraf, PUC-Rio.)
 * http://www.lua.org/copyright.html#4
 * http://www.lua.org/source/4.0.1/src_lstring.c.html
 */

SQStringTable::SQStringTable()
{
	AllocNodes(4);
	_slotused = 0;
}

SQStringTable::~SQStringTable()
{
	SQ_FREE(_strings,sizeof(SQString*)*_numofslots);
	_strings = NULL;
}

void SQStringTable::AllocNodes(SQInteger size)
{
	_numofslots = size;
	_strings = (SQString**)SQ_MALLOC(sizeof(SQString*)*_numofslots);
	memset(_strings,0,sizeof(SQString*)*(size_t)_numofslots);
}

SQString *SQStringTable::Add(const SQChar *news,SQInteger len)
{
	if(len<0)
		len = (SQInteger)strlen(news);
	SQHash h = ::_hashstr(news,(size_t)len)&(_numofslots-1);
	SQString *s;
	for (s = _strings[h]; s; s = s->_next){
		if(s->_len == len && (!memcmp(news,s->_val,(size_t)len)))
			return s; //found
	}

	SQString *t=(SQString *)SQ_MALLOC(len+sizeof(SQString));
	new (t) SQString(news, len);
	t->_next = _strings[h];
	_strings[h] = t;
	_slotused++;
	if (_slotused > _numofslots)  /* too crowded? */
		Resize(_numofslots*2);
	return t;
}

SQString::SQString(const SQChar *news, SQInteger len)
{
	memcpy(_val,news,(size_t)len);
	_val[len] = '\0';
	_len = len;
	_hash = ::_hashstr(news,(size_t)len);
	_next = NULL;
	_sharedstate = NULL;
}

void SQStringTable::Resize(SQInteger size)
{
	SQInteger oldsize=_numofslots;
	SQString **oldtable=_strings;
	AllocNodes(size);
	for (SQInteger i=0; i<oldsize; i++){
		SQString *p = oldtable[i];
		while(p){
			SQString *next = p->_next;
			SQHash h = p->_hash&(_numofslots-1);
			p->_next = _strings[h];
			_strings[h] = p;
			p = next;
		}
	}
	SQ_FREE(oldtable,oldsize*sizeof(SQString*));
}

void SQStringTable::Remove(SQString *bs)
{
	SQString *s;
	SQString *prev=NULL;
	SQHash h = bs->_hash&(_numofslots - 1);

	for (s = _strings[h]; s; ){
		if(s == bs){
			if(prev)
				prev->_next = s->_next;
			else
				_strings[h] = s->_next;
			_slotused--;
			SQInteger slen = s->_len;
			s->~SQString();
			SQ_FREE(s,sizeof(SQString) + slen);
			return;
		}
		prev = s;
		s = s->_next;
	}
	assert(0);//if this fail something is wrong
}
