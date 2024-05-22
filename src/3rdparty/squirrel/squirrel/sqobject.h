/*	see copyright notice in squirrel.h */
#ifndef _SQOBJECT_H_
#define _SQOBJECT_H_

#include <vector>
#include "squtils.h"

#define SQ_CLOSURESTREAM_HEAD (('S'<<24)|('Q'<<16)|('I'<<8)|('R'))
#define SQ_CLOSURESTREAM_PART (('P'<<24)|('A'<<16)|('R'<<8)|('T'))
#define SQ_CLOSURESTREAM_TAIL (('T'<<24)|('A'<<16)|('I'<<8)|('L'))

struct SQSharedState;

enum SQMetaMethod{
	MT_ADD=0,
	MT_SUB=1,
	MT_MUL=2,
	MT_DIV=3,
	MT_UNM=4,
	MT_MODULO=5,
	MT_SET=6,
	MT_GET=7,
	MT_TYPEOF=8,
	MT_NEXTI=9,
	MT_CMP=10,
	MT_CALL=11,
	MT_CLONED=12,
	MT_NEWSLOT=13,
	MT_DELSLOT=14,
	MT_TOSTRING=15,
	MT_NEWMEMBER=16,
	MT_INHERITED=17,
	MT_LAST = 18
};

#define MM_ADD		"_add"
#define MM_SUB		"_sub"
#define MM_MUL		"_mul"
#define MM_DIV		"_div"
#define MM_UNM		"_unm"
#define MM_MODULO	"_modulo"
#define MM_SET		"_set"
#define MM_GET		"_get"
#define MM_TYPEOF	"_typeof"
#define MM_NEXTI	"_nexti"
#define MM_CMP		"_cmp"
#define MM_CALL		"_call"
#define MM_CLONED	"_cloned"
#define MM_NEWSLOT	"_newslot"
#define MM_DELSLOT	"_delslot"
#define MM_TOSTRING	"_tostring"
#define MM_NEWMEMBER "_newmember"
#define MM_INHERITED "_inherited"

#define MINPOWER2 4

struct SQRefCounted
{
	SQRefCounted() { _uiRef = 0; _weakref = nullptr; }
	virtual ~SQRefCounted();
	SQWeakRef *GetWeakRef(SQObjectType type);
	SQUnsignedInteger _uiRef;
	struct SQWeakRef *_weakref;
	virtual void Release()=0;

	/* Placement new/delete to prevent memory leaks if constructor throws an exception. */
	inline void *operator new(size_t size, SQRefCounted *place)
	{
		place->size = size;
		return place;
	}

	inline void operator delete(void *ptr, SQRefCounted *place)
	{
		SQ_FREE(ptr, place->size);
	}

	/* Never used but required. */
	inline void operator delete(void *) { NOT_REACHED(); }

private:
	size_t size;
};

struct SQWeakRef : SQRefCounted
{
	void Release() override;
	SQObject _obj;
};

#define _realval(o) (type((o)) != OT_WEAKREF?(SQObject)o:_weakref(o)->_obj)

struct SQObjectPtr;

#define __AddRef(type,unval) if(ISREFCOUNTED(type))	\
		{ \
			unval.pRefCounted->_uiRef++; \
		}

#define __Release(type,unval) if(ISREFCOUNTED(type) && ((--unval.pRefCounted->_uiRef)<=0))	\
		{	\
			unval.pRefCounted->Release();	\
		}

#define __ObjRelease(obj) { \
	if((obj)) {	\
		(obj)->_uiRef--; \
		if((obj)->_uiRef == 0) \
			(obj)->Release(); \
		(obj) = nullptr;	\
	} \
}

#define __ObjAddRef(obj) { \
	(obj)->_uiRef++; \
}

#define type(obj) ((obj)._type)
#define is_delegable(t) (type(t)&SQOBJECT_DELEGABLE)
#define raw_type(obj) _RAW_TYPE((obj)._type)

#define _integer(obj) ((obj)._unVal.nInteger)
#define _float(obj) ((obj)._unVal.fFloat)
#define _string(obj) ((obj)._unVal.pString)
#define _table(obj) ((obj)._unVal.pTable)
#define _array(obj) ((obj)._unVal.pArray)
#define _closure(obj) ((obj)._unVal.pClosure)
#define _generator(obj) ((obj)._unVal.pGenerator)
#define _nativeclosure(obj) ((obj)._unVal.pNativeClosure)
#define _userdata(obj) ((obj)._unVal.pUserData)
#define _userpointer(obj) ((obj)._unVal.pUserPointer)
#define _thread(obj) ((obj)._unVal.pThread)
#define _funcproto(obj) ((obj)._unVal.pFunctionProto)
#define _class(obj) ((obj)._unVal.pClass)
#define _instance(obj) ((obj)._unVal.pInstance)
#define _delegable(obj) ((SQDelegable *)(obj)._unVal.pDelegable)
#define _weakref(obj) ((obj)._unVal.pWeakRef)
#define _refcounted(obj) ((obj)._unVal.pRefCounted)
#define _rawval(obj) ((obj)._unVal.raw)

#define _stringval(obj) (obj)._unVal.pString->_val
#define _userdataval(obj) (obj)._unVal.pUserData->_val

#define tofloat(num) ((type(num)==OT_INTEGER)?(SQFloat)_integer(num):_float(num))
#define tointeger(num) (	(type(num)==OT_FLOAT)?(SQInteger)_float(num):_integer(num))

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
struct SQObjectPtr : public SQObject
{
	SQObjectPtr()
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_NULL;
		_unVal.pUserPointer=nullptr;
	}
	SQObjectPtr(const SQObjectPtr &o)
	{
		SQ_OBJECT_RAWINIT()
		_type=o._type;
		_unVal=o._unVal;
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(const SQObject &o)
	{
		SQ_OBJECT_RAWINIT()
		_type=o._type;
		_unVal=o._unVal;
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQTable *pTable)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_TABLE;
		_unVal.pTable=pTable;
		assert(_unVal.pTable);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQClass *pClass)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_CLASS;
		_unVal.pClass=pClass;
		assert(_unVal.pClass);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQInstance *pInstance)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_INSTANCE;
		_unVal.pInstance=pInstance;
		assert(_unVal.pInstance);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQArray *pArray)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_ARRAY;
		_unVal.pArray=pArray;
		assert(_unVal.pArray);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQClosure *pClosure)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_CLOSURE;
		_unVal.pClosure=pClosure;
		assert(_unVal.pClosure);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQGenerator *pGenerator)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_GENERATOR;
		_unVal.pGenerator=pGenerator;
		assert(_unVal.pGenerator);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQNativeClosure *pNativeClosure)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_NATIVECLOSURE;
		_unVal.pNativeClosure=pNativeClosure;
		assert(_unVal.pNativeClosure);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQString *pString)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_STRING;
		_unVal.pString=pString;
		assert(_unVal.pString);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQUserData *pUserData)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_USERDATA;
		_unVal.pUserData=pUserData;
		assert(_unVal.pUserData);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQVM *pThread)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_THREAD;
		_unVal.pThread=pThread;
		assert(_unVal.pThread);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQWeakRef *pWeakRef)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_WEAKREF;
		_unVal.pWeakRef=pWeakRef;
		assert(_unVal.pWeakRef);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQFunctionProto *pFunctionProto)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_FUNCPROTO;
		_unVal.pFunctionProto=pFunctionProto;
		assert(_unVal.pFunctionProto);
		__AddRef(_type,_unVal);
	}
	SQObjectPtr(SQInteger nInteger)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_INTEGER;
		_unVal.nInteger=nInteger;
	}
	SQObjectPtr(SQFloat fFloat)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_FLOAT;
		_unVal.fFloat=fFloat;
	}
	SQObjectPtr(bool bBool)
	{
		SQ_OBJECT_RAWINIT()
		_type = OT_BOOL;
		_unVal.nInteger = bBool?1:0;
	}
	SQObjectPtr(SQUserPointer pUserPointer)
	{
		SQ_OBJECT_RAWINIT()
		_type=OT_USERPOINTER;
		_unVal.pUserPointer=pUserPointer;
	}
	~SQObjectPtr()
	{
		__Release(_type,_unVal);
	}
	inline void Null()
	{
		SQObjectType tOldType;
		SQObjectValue unOldVal;
		tOldType = _type;
		unOldVal = _unVal;
		_type = OT_NULL;
		_unVal.pUserPointer = nullptr;
		__Release(tOldType,unOldVal);
	}
	inline SQObjectPtr& operator=(SQInteger i)
	{
		__Release(_type,_unVal);
		SQ_OBJECT_RAWINIT()
		_unVal.nInteger = i;
		_type = OT_INTEGER;
		return *this;
	}
	inline SQObjectPtr& operator=(SQFloat f)
	{
		__Release(_type,_unVal);
		SQ_OBJECT_RAWINIT()
		_unVal.fFloat = f;
		_type = OT_FLOAT;
		return *this;
	}
	inline SQObjectPtr& operator=(const SQObjectPtr& obj)
	{
		SQObjectType tOldType;
		SQObjectValue unOldVal;
		tOldType=_type;
		unOldVal=_unVal;
		_unVal = obj._unVal;
		_type = obj._type;
		__AddRef(_type,_unVal);
		__Release(tOldType,unOldVal);
		return *this;
	}
	inline SQObjectPtr& operator=(const SQObject& obj)
	{
		SQObjectType tOldType;
		SQObjectValue unOldVal;
		tOldType=_type;
		unOldVal=_unVal;
		_unVal = obj._unVal;
		_type = obj._type;
		__AddRef(_type,_unVal);
		__Release(tOldType,unOldVal);
		return *this;
	}
	private:
		SQObjectPtr(const SQChar *){} //safety
};

inline void _Swap(SQObject &a,SQObject &b)
{
	SQObjectType tOldType = a._type;
	SQObjectValue unOldVal = a._unVal;
	a._type = b._type;
	a._unVal = b._unVal;
	b._type = tOldType;
	b._unVal = unOldVal;
}
/////////////////////////////////////////////////////////////////////////////////////
#ifndef NO_GARBAGE_COLLECTOR
#define MARK_FLAG 0x80000000
struct SQCollectable : public SQRefCounted {
	SQCollectable *_next;
	SQCollectable *_prev;
	SQSharedState *_sharedstate;
	void Release() override=0;
	virtual void EnqueueMarkObjectForChildren(class SQGCMarkerQueue &queue)=0;
	void UnMark();
	virtual void Finalize()=0;
	static void AddToChain(SQCollectable **chain,SQCollectable *c);
	static void RemoveFromChain(SQCollectable **chain,SQCollectable *c);

	/**
	 * Helper to perform the final memory freeing of this instance. Since the destructor might
	 * release more objects, this can cause a very deep recursion. As such, the calls to this
	 * are to be done via _sharedstate->DelayFinalFree which ensures the calls to this method
	 * are done in an iterative instead of recursive approach.
	 */
	virtual void FinalFree() {}
};

/**
 * Helper container for state to change the garbage collection from a recursive to an iterative approach.
 * The iterative approach provides effectively a depth first search approach.
 */
class SQGCMarkerQueue {
	std::vector<SQCollectable*> stack; ///< The elements to still process, with the most recent elements at the back.
public:
	/** Whether there are any elements left to process. */
	bool IsEmpty() { return this->stack.empty(); }

	/**
	 * Remove the most recently added element from the queue.
	 * Removal when the queue is empty results in undefined behaviour.
	 */
	SQCollectable *Pop()
	{
		SQCollectable *collectable = this->stack.back();
		this->stack.pop_back();
		return collectable;
	}

	/**
	 * Add a collectable to the queue, but only when it has not been marked yet.
	 * When adding it to the queue, the collectable will be marked, so subsequent calls
	 * will not add it again.
	 */
	void Enqueue(SQCollectable *collectable)
	{
		if ((collectable->_uiRef & MARK_FLAG) == 0) {
			collectable->_uiRef |= MARK_FLAG;
			this->stack.push_back(collectable);
		}
	}
};


#define ADD_TO_CHAIN(chain,obj) AddToChain(chain,obj)
#define REMOVE_FROM_CHAIN(chain,obj) {if(!(_uiRef&MARK_FLAG))RemoveFromChain(chain,obj);}
#define CHAINABLE_OBJ SQCollectable
#define INIT_CHAIN() {_next=nullptr;_prev=nullptr;_sharedstate=ss;}
#else

#define ADD_TO_CHAIN(chain,obj) ((void)0)
#define REMOVE_FROM_CHAIN(chain,obj) ((void)0)
#define CHAINABLE_OBJ SQRefCounted
#define INIT_CHAIN() ((void)0)
#endif

struct SQDelegable : public CHAINABLE_OBJ {
	bool SetDelegate(SQTable *m);
	virtual bool GetMetaMethod(SQVM *v,SQMetaMethod mm,SQObjectPtr &res);
	SQTable *_delegate;
};

SQUnsignedInteger TranslateIndex(const SQObjectPtr &idx);
typedef sqvector<SQObjectPtr> SQObjectPtrVec;
typedef sqvector<SQInteger> SQIntVec;
const SQChar *GetTypeName(const SQObjectPtr &obj1);
const SQChar *IdType2Name(SQObjectType type);



#endif //_SQOBJECT_H_
