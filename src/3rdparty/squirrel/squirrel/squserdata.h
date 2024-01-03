/*	see copyright notice in squirrel.h */
#ifndef _SQUSERDATA_H_
#define _SQUSERDATA_H_

struct SQUserData : SQDelegable
{
	SQUserData(SQSharedState *ss, SQInteger size){ _delegate = nullptr; _hook = nullptr; INIT_CHAIN(); ADD_TO_CHAIN(&_ss(this)->_gc_chain, this); _size = size; _typetag = nullptr;
}
	~SQUserData()
	{
		REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain, this);
		SetDelegate(nullptr);
	}
	static SQUserData* Create(SQSharedState *ss, SQInteger size)
	{
		SQUserData* ud = (SQUserData*)SQ_MALLOC(sizeof(SQUserData)+(size-1));
		new (ud) SQUserData(ss, size);
		return ud;
	}
#ifndef NO_GARBAGE_COLLECTOR
	void EnqueueMarkObjectForChildren(SQGCMarkerQueue &queue) override;
	void Finalize() override {SetDelegate(nullptr);}
#endif
	void Release() override {
		if (_hook) _hook(_val,_size);
		SQInteger tsize = _size - 1;
		this->~SQUserData();
		SQ_FREE(this, sizeof(SQUserData) + tsize);
	}

	SQInteger _size;
	SQRELEASEHOOK _hook;
	SQUserPointer _typetag;
	SQChar _val[1];
};

#endif //_SQUSERDATA_H_
