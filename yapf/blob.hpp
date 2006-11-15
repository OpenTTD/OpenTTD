/* $Id$ */

#ifndef  BLOB_HPP
#define  BLOB_HPP

template <class Titem_>
FORCEINLINE void MemCpyT(Titem_* d, const Titem_* s, int num_items = 1)
{
	memcpy(d, s, num_items * sizeof(Titem_));
}


/** Base class for simple binary blobs.
 *  Item is byte.
 *  The word 'simple' means:
 *    - no configurable allocator type (always made from heap)
 *    - no smart deallocation - deallocation must be called from the same
 *        module (DLL) where the blob was allocated
 *    - no configurable allocation policy (how big blocks should be allocated)
 *    - no extra ownership policy (i.e. 'copy on write') when blob is copied
 *    - no thread synchronization at all */
class CBlobBaseSimple {
protected:
	struct CHdr {
		int    m_size;      // actual blob size in bytes
		int    m_max_size;  // maximum (allocated) size in bytes
	};

	union {
		int8   *m_pData;
		CHdr   *m_pHdr_1;
	} ptr_u;

public:
	static const int Ttail_reserve = 4; // four extra bytes will be always allocated and zeroed at the end

	FORCEINLINE CBlobBaseSimple() { InitEmpty(); }
	FORCEINLINE CBlobBaseSimple(const CBlobBaseSimple& src)
	{
		InitEmpty();
		AppendRaw(src);
	}
	FORCEINLINE ~CBlobBaseSimple() { Free(); }
protected:
	FORCEINLINE void InitEmpty() { static CHdr hdrEmpty[] = {{0, 0}, {0, 0}}; ptr_u.m_pHdr_1 = &hdrEmpty[1]; }
	FORCEINLINE void Init(CHdr* hdr) { ptr_u.m_pHdr_1 = &hdr[1]; }
	FORCEINLINE CHdr& Hdr() { return ptr_u.m_pHdr_1[-1]; }
	FORCEINLINE const CHdr& Hdr() const { return ptr_u.m_pHdr_1[-1]; }
	FORCEINLINE int& RawSizeRef() { return Hdr().m_size; };

public:
	FORCEINLINE bool IsEmpty() const { return RawSize() == 0; }
	FORCEINLINE int RawSize() const { return Hdr().m_size; };
	FORCEINLINE int MaxRawSize() const { return Hdr().m_max_size; };
	FORCEINLINE int8* RawData() { return ptr_u.m_pData; }
	FORCEINLINE const int8* RawData() const { return ptr_u.m_pData; }
#if 0 // reenable when needed
	FORCEINLINE uint32 Crc32() const {return CCrc32::Calc(RawData(), RawSize());}
#endif //0
	FORCEINLINE void Clear() { RawSizeRef() = 0; }
	FORCEINLINE void Free() { if (MaxRawSize() > 0) {RawFree(&Hdr()); InitEmpty();} }
	FORCEINLINE void CopyFrom(const CBlobBaseSimple& src) { Clear(); AppendRaw(src); }
	FORCEINLINE void MoveFrom(CBlobBaseSimple& src) { Free(); ptr_u.m_pData = src.ptr_u.m_pData; src.InitEmpty(); }
	FORCEINLINE void Swap(CBlobBaseSimple& src) { int8 *tmp = ptr_u.m_pData; ptr_u.m_pData = src.ptr_u.m_pData; src.ptr_u.m_pData = tmp; }

	FORCEINLINE void AppendRaw(int8 *p, int num_bytes)
	{
		assert(p != NULL);
		if (num_bytes > 0) {
			memcpy(GrowRawSize(num_bytes), p, num_bytes);
		} else {
			assert(num_bytes >= 0);
		}
	}

	FORCEINLINE void AppendRaw(const CBlobBaseSimple& src)
	{
		if (!src.IsEmpty())
			memcpy(GrowRawSize(src.RawSize()), src.RawData(), src.RawSize());
	}

	/** Reallocate if there is no free space for num_bytes bytes.
	 *  @return pointer to the new data to be added */
	FORCEINLINE int8* MakeRawFreeSpace(int num_bytes)
	{
		assert(num_bytes >= 0);
		int new_size = RawSize() + num_bytes;
		if (new_size > MaxRawSize()) SmartAlloc(new_size);
		FixTail();
		return ptr_u.m_pData + RawSize();
	}

	/** Increase RawSize() by num_bytes.
	 *  @return pointer to the new data added */
	FORCEINLINE int8* GrowRawSize(int num_bytes)
	{
		int8* pNewData = MakeRawFreeSpace(num_bytes);
		RawSizeRef() += num_bytes;
		return pNewData;
	}

	/** Decrease RawSize() by num_bytes. */
	FORCEINLINE void ReduceRawSize(int num_bytes)
	{
		if (MaxRawSize() > 0 && num_bytes > 0) {
			assert(num_bytes <= RawSize());
			if (num_bytes < RawSize()) RawSizeRef() -= num_bytes;
			else RawSizeRef() = 0;
		}
	}
	/** reallocate blob data if needed */
	void SmartAlloc(int new_size)
	{
		int old_max_size = MaxRawSize();
		if (old_max_size >= new_size) return;
		// calculate minimum block size we need to allocate
		int min_alloc_size = sizeof(CHdr) + new_size + Ttail_reserve;
		// ask allocation policy for some reasonable block size
		int alloc_size = AllocPolicy(min_alloc_size);
		// allocate new block
		CHdr* pNewHdr = RawAlloc(alloc_size);
		// setup header
		pNewHdr->m_size = RawSize();
		pNewHdr->m_max_size = alloc_size - (sizeof(CHdr) + Ttail_reserve);
		// copy existing data
		if (RawSize() > 0)
			memcpy(pNewHdr + 1, ptr_u.m_pData, pNewHdr->m_size);
		// replace our block with new one
		CHdr* pOldHdr = &Hdr();
		Init(pNewHdr);
		if (old_max_size > 0)
			RawFree(pOldHdr);
	}
	/** simple allocation policy - can be optimized later */
	FORCEINLINE static int AllocPolicy(int min_alloc)
	{
		if (min_alloc < (1 << 9)) {
			if (min_alloc < (1 << 5)) return (1 << 5);
			return (min_alloc < (1 << 7)) ? (1 << 7) : (1 << 9);
		}
		if (min_alloc < (1 << 15)) {
			if (min_alloc < (1 << 11)) return (1 << 11);
			return (min_alloc < (1 << 13)) ? (1 << 13) : (1 << 15);
		}
		if (min_alloc < (1 << 20)) {
			if (min_alloc < (1 << 17)) return (1 << 17);
			return (min_alloc < (1 << 19)) ? (1 << 19) : (1 << 20);
		}
		min_alloc = (min_alloc | ((1 << 20) - 1)) + 1;
		return min_alloc;
	}

	/** all allocation should happen here */
	static FORCEINLINE CHdr* RawAlloc(int num_bytes) { return (CHdr*)malloc(num_bytes); }
	/** all deallocations should happen here */
	static FORCEINLINE void RawFree(CHdr* p) { free(p); }
	/** fixing the four bytes at the end of blob data - useful when blob is used to hold string */
	FORCEINLINE void FixTail()
	{
		if (MaxRawSize() > 0) {
			int8 *p = &ptr_u.m_pData[RawSize()];
			for (int i = 0; i < Ttail_reserve; i++) p[i] = 0;
		}
	}
};

template <class Titem_, class Tbase_ = CBlobBaseSimple>
class CBlobT : public CBlobBaseSimple {
	// make template arguments public:
public:
	typedef Titem_ Titem;
	typedef Tbase_ Tbase;

	static const int Titem_size = sizeof(Titem);

	FORCEINLINE CBlobT() : Tbase() {}
	FORCEINLINE CBlobT(const Tbase& src) : Tbase(src) {assert((RawSize() % Titem_size) == 0);}
	FORCEINLINE ~CBlobT() { Free(); }
	FORCEINLINE void CheckIdx(int idx) { assert(idx >= 0); assert(idx < Size()); }
	FORCEINLINE Titem* Data() { return (Titem*)RawData(); }
	FORCEINLINE const Titem* Data() const { return (const Titem*)RawData(); }
	FORCEINLINE Titem* Data(int idx) { CheckIdx(idx); return (Data() + idx); }
	FORCEINLINE const Titem* Data(int idx) const { CheckIdx(idx); return (Data() + idx); }
	FORCEINLINE int Size() const { return (RawSize() / Titem_size); }
	FORCEINLINE void Free()
	{
		assert((RawSize() % Titem_size) == 0);
		int old_size = Size();
		if (old_size > 0) {
			// destroy removed items;
			Titem* pI_last_to_destroy = Data(0);
			for (Titem* pI = Data(old_size - 1); pI >= pI_last_to_destroy; pI--) pI->~Titem_();
		}
		Tbase::Free();
	}
	FORCEINLINE Titem* GrowSizeNC(int num_items) { return (Titem*)GrowRawSize(num_items * Titem_size); }
	FORCEINLINE Titem* GrowSizeC(int num_items)
	{
		Titem* pI = GrowSizeNC(num_items);
		for (int i = num_items; i > 0; i--, pI++) new (pI) Titem();
	}
	FORCEINLINE void ReduceSize(int num_items)
	{
		assert((RawSize() % Titem_size) == 0);
		int old_size = Size();
		assert(num_items <= old_size);
		int new_size = (num_items <= old_size) ? (old_size - num_items) : 0;
		// destroy removed items;
		Titem* pI_last_to_destroy = Data(new_size);
		for (Titem* pI = Data(old_size - 1); pI >= pI_last_to_destroy; pI--) pI->~Titem();
		// remove them
		ReduceRawSize(num_items * Titem_size);
	}
	FORCEINLINE Titem* AppendNew()
	{
		Titem& dst = *GrowSizeNC(1);
		Titem* pNewItem = new (&dst) Titem();
		return pNewItem;
	}
	FORCEINLINE Titem* Append(const Titem& src)
	{
		Titem& dst = *GrowSizeNC(1);
		Titem* pNewItem = new (&dst) Titem(src);
		return pNewItem;
	}
	FORCEINLINE Titem* Append(const Titem* pSrc, int num_items)
	{
		Titem* pDst = GrowSizeNC(num_items);
		Titem* pDstOrg = pDst;
		Titem* pDstEnd = pDst + num_items;
		while (pDst < pDstEnd) new (pDst++) Titem(*(pSrc++));
		return pDstOrg;
	}
	FORCEINLINE void RemoveBySwap(int idx)
	{
		CheckIdx(idx);
		// destroy removed item
		Titem* pRemoved = Data(idx);
		RemoveBySwap(pRemoved);
	}
	FORCEINLINE void RemoveBySwap(Titem* pItem)
	{
		Titem* pLast = Data(Size() - 1);
		assert(pItem >= Data() && pItem <= pLast);
		// move last item to its new place
		if (pItem != pLast) {
			pItem->~Titem_();
			new (pItem) Titem_(*pLast);
		}
		// destroy the last item
		pLast->~Titem_();
		// and reduce the raw blob size
		ReduceRawSize(Titem_size);
	}
	FORCEINLINE Titem* MakeFreeSpace(int num_items) { return (Titem*)MakeRawFreeSpace(num_items * Titem_size); }
};

// simple string implementation
struct CStrA : public CBlobT<char>
{
	typedef CBlobT<char> base;
	CStrA(const char* str = NULL) {Append(str);}
	FORCEINLINE CStrA(const CBlobBaseSimple& src) : base(src) {}
	void Append(const char* str) {if (str != NULL && str[0] != '\0') base::Append(str, (int)strlen(str));}
};

#endif /* BLOB_HPP */
