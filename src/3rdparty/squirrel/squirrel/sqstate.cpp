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
	while(funcz[i].name!=nullptr){
		SQNativeClosure *nc = SQNativeClosure::Create(ss,funcz[i].f);
		nc->_nparamscheck = funcz[i].nparamscheck;
		nc->_name = SQString::Create(ss,funcz[i].name);
		if(funcz[i].typemask && !CompileTypemask(nc->_typecheck,funcz[i].typemask))
			return nullptr;
		t->NewSlot(SQString::Create(ss,funcz[i].name),nc);
		i++;
	}
	return t;
}

SQSharedState::SQSharedState()
{
	_compilererrorhandler = nullptr;
	_printfunc = nullptr;
	_debuginfo = false;
	_notifyallexceptions = false;
	_scratchpad=nullptr;
	_scratchpadsize=0;
	_collectable_free_processing = false;
#ifndef NO_GARBAGE_COLLECTOR
	_gc_chain=nullptr;
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
	SQCollectable *nx = nullptr;
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
//	assert(_gc_chain==nullptr); //just to proove a theory
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

/**
 * Helper function that is to be used instead of calling FinalFree directly on the instance,
 * so the frees can happen iteratively. This as in the FinalFree the references to any other
 * objects are released, which can cause those object to be freed yielding a potentially
 * very deep stack in case of for example a link list.
 *
 * This is done internally by a vector onto which the to be freed instances are pushed. When
 * this is called when not already processing, this method will actually call the FinalFree
 * function which might cause more elements to end up in the queue which this method then
 * picks up continueing until it has processed all instances in that queue.
 * @param collectable The collectable to (eventually) free.
 */
void SQSharedState::DelayFinalFree(SQCollectable *collectable)
{
	this->_collectable_free_queue.push_back(collectable);

	if (!this->_collectable_free_processing) {
		this->_collectable_free_processing = true;
		while (!this->_collectable_free_queue.empty()) {
			SQCollectable *collectable_to_free = this->_collectable_free_queue.back();
			this->_collectable_free_queue.pop_back();
			collectable_to_free->FinalFree();
		}
		this->_collectable_free_processing = false;
	}
}


#ifndef NO_GARBAGE_COLLECTOR

void SQSharedState::EnqueueMarkObject(SQObjectPtr &o,SQGCMarkerQueue &queue)
{
	switch(type(o)){
	case OT_TABLE:queue.Enqueue(_table(o));break;
	case OT_ARRAY:queue.Enqueue(_array(o));break;
	case OT_USERDATA:queue.Enqueue(_userdata(o));break;
	case OT_CLOSURE:queue.Enqueue(_closure(o));break;
	case OT_NATIVECLOSURE:queue.Enqueue(_nativeclosure(o));break;
	case OT_GENERATOR:queue.Enqueue(_generator(o));break;
	case OT_THREAD:queue.Enqueue(_thread(o));break;
	case OT_CLASS:queue.Enqueue(_class(o));break;
	case OT_INSTANCE:queue.Enqueue(_instance(o));break;
	default: break; //shutup compiler
	}
}


SQInteger SQSharedState::CollectGarbage(SQVM *)
{
	SQInteger n=0;
	SQVM *vms = _thread(_root_vm);

	SQGCMarkerQueue queue;
	queue.Enqueue(vms);
#ifdef WITH_ASSERT
	SQInteger x = _table(_thread(_root_vm)->_roottable)->CountUsed();
#endif
	_refs_table.EnqueueMarkObject(queue);
	EnqueueMarkObject(_registry,queue);
	EnqueueMarkObject(_consts,queue);
	EnqueueMarkObject(_metamethodsmap,queue);
	EnqueueMarkObject(_table_default_delegate,queue);
	EnqueueMarkObject(_array_default_delegate,queue);
	EnqueueMarkObject(_string_default_delegate,queue);
	EnqueueMarkObject(_number_default_delegate,queue);
	EnqueueMarkObject(_generator_default_delegate,queue);
	EnqueueMarkObject(_thread_default_delegate,queue);
	EnqueueMarkObject(_closure_default_delegate,queue);
	EnqueueMarkObject(_class_default_delegate,queue);
	EnqueueMarkObject(_instance_default_delegate,queue);
	EnqueueMarkObject(_weakref_default_delegate,queue);

	SQCollectable *tchain=nullptr;

	while (!queue.IsEmpty()) {
		SQCollectable *q = queue.Pop();
		q->EnqueueMarkObjectForChildren(queue);
		SQCollectable::RemoveFromChain(&_gc_chain, q);
		SQCollectable::AddToChain(&tchain, q);
	}

	SQCollectable *t = _gc_chain;
	SQCollectable *nx = nullptr;
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
    c->_prev = nullptr;
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
	c->_next = nullptr;
	c->_prev = nullptr;
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
void RefTable::EnqueueMarkObject(SQGCMarkerQueue &queue)
{
	RefNode *nodes = (RefNode *)_nodes;
	for(SQUnsignedInteger n = 0; n < _numofslots; n++) {
		if(type(nodes->obj) != OT_NULL) {
			SQSharedState::EnqueueMarkObject(nodes->obj,queue);
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
	[[maybe_unused]] SQUnsignedInteger nfound = 0;
	for(SQUnsignedInteger n = 0; n < oldnumofslots; n++) {
		if(type(t->obj) != OT_NULL) {
			//add back
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
	*prev = nullptr;
	for (ref = _buckets[mainpos]; ref; ) {
		if(_rawval(ref->obj) == _rawval(obj) && type(ref->obj) == type(obj))
			break;
		*prev = ref;
		ref = ref->next;
	}
	if(ref == nullptr && add) {
		if(_numofslots == _slotused) {
			assert(_freelist == nullptr);
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
		bucks[n] = nullptr;
		temp->refs = 0;
		new (&temp->obj) SQObjectPtr;
		temp->next = &temp[1];
		temp++;
	}
	bucks[n] = nullptr;
	temp->refs = 0;
	new (&temp->obj) SQObjectPtr;
	temp->next = nullptr;
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
	_strings = nullptr;
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
	_next = nullptr;
	_sharedstate = nullptr;
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
	SQString *prev=nullptr;
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
