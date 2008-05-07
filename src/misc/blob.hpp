/* $Id$ */

/** @file blob.hpp Support for storing random binary data. */

#ifndef BLOB_HPP
#define BLOB_HPP

#include "../core/alloc_func.hpp"

/** Type-safe version of memcpy().
 * @param d destination buffer
 * @param s source buffer
 * @param num_items number of items to be copied (!not number of bytes!) */
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
*    - no thread synchronization at all
*
*  Internal member layout:
*  1. The only class member is pointer to the first item (see union ptr_u).
*  2. Allocated block contains the blob header (see CHdr) followed by the raw byte data.
*     Always, when it allocates memory the allocated size is:
*                                                      sizeof(CHdr) + <data capacity>
*  3. Two 'virtual' members (m_size and m_max_size) are stored in the CHdr at beginning
*     of the alloated block.
*  4. The pointter (in ptr_u) pobsize_ts behind the header (to the first data byte).
*     When memory block is allocated, the sizeof(CHdr) it added to it.
*  5. Benefits of this layout:
*     - items are accessed in the simplest possible way - just dereferencing the pointer,
*       which is good for performance (assuming that data are accessed most often).
*     - sizeof(blob) is the same as the size of any other pointer
*  6. Drawbacks of this layout:
*     - the fact, that pointer to the alocated block is adjusted by sizeof(CHdr) before
*       it is stored can lead to several confusions:
*         - it is not common pattern so the implementation code is bit harder to read
*         - valgrind can generate warning that allocated block is lost (not accessible)
* */
class CBlobBaseSimple {
public:
	typedef ::ptrdiff_t bsize_t;
	typedef ::byte      bitem_t;

protected:
	/** header of the allocated memory block */
	struct CHdr {
		bsize_t    m_size;      ///< actual blob size in bytes
		bsize_t    m_max_size;  ///< maximum (allocated) size in bytes
	};

	/** type used as class member */
	union {
		bitem_t    *m_pData;    ///< ptr to the first byte of data
#if defined(HAS_WCHAR)
		wchar_t    *m_pwData;   ///< ptr to the first byte of data
#endif /* HAS_WCHAR */
		CHdr       *m_pHdr_1;   ///< ptr just after the CHdr holding m_size and m_max_size
	} ptr_u;

public:
	static const bsize_t Ttail_reserve = 4; ///< four extra bytes will be always allocated and zeroed at the end

	/** default constructor - initializes empty blob */
	FORCEINLINE CBlobBaseSimple() { InitEmpty(); }
	/** constructor - create blob with data */
	FORCEINLINE CBlobBaseSimple(const bitem_t *p, bsize_t num_bytes)
	{
		InitEmpty();
		AppendRaw(p, num_bytes);
	}

	/** copy constructor */
	FORCEINLINE CBlobBaseSimple(const CBlobBaseSimple& src)
	{
		InitEmpty();
		AppendRaw(src);
	}

	/** move constructor - take ownership of blob data */
	FORCEINLINE CBlobBaseSimple(CHdr * const & pHdr_1)
	{
		assert(pHdr_1 != NULL);
		ptr_u.m_pHdr_1 = pHdr_1;
		*(CHdr**)&pHdr_1 = NULL;
	}

	/** destructor */
	FORCEINLINE ~CBlobBaseSimple()
	{
		Free();
	}

protected:
	/** initialize the empty blob by setting the ptr_u.m_pHdr_1 pointer to the static CHdr with
	*  both m_size and m_max_size containing zero */
	FORCEINLINE void InitEmpty()
	{
		static CHdr hdrEmpty[] = {{0, 0}, {0, 0}};
		ptr_u.m_pHdr_1 = &hdrEmpty[1];
	}

	/** initialize blob by attaching it to the given header followed by data */
	FORCEINLINE void Init(CHdr* hdr)
	{
		ptr_u.m_pHdr_1 = &hdr[1];
	}

	/** blob header accessor - use it rather than using the pointer arithmetics directly - non-const version */
	FORCEINLINE CHdr& Hdr()
	{
		return ptr_u.m_pHdr_1[-1];
	}

	/** blob header accessor - use it rather than using the pointer arithmetics directly - const version */
	FORCEINLINE const CHdr& Hdr() const
	{
		return ptr_u.m_pHdr_1[-1];
	}

	/** return reference to the actual blob size - used when the size needs to be modified */
	FORCEINLINE bsize_t& RawSizeRef()
	{
		return Hdr().m_size;
	};

public:
	/** return true if blob doesn't contain valid data */
	FORCEINLINE bool IsEmpty() const
	{
		return RawSize() == 0;
	}

	/** return the number of valid data bytes in the blob */
	FORCEINLINE bsize_t RawSize() const
	{
		return Hdr().m_size;
	};

	/** return the current blob capacity in bytes */
	FORCEINLINE bsize_t MaxRawSize() const
	{
		return Hdr().m_max_size;
	};

	/** return pointer to the first byte of data - non-const version */
	FORCEINLINE bitem_t* RawData()
	{
		return ptr_u.m_pData;
	}

	/** return pointer to the first byte of data - const version */
	FORCEINLINE const bitem_t* RawData() const
	{
		return ptr_u.m_pData;
	}

	/** return the 32 bit CRC of valid data in the blob */
	//FORCEINLINE bsize_t Crc32() const
	//{
	//	return CCrc32::Calc(RawData(), RawSize());
	//}

	/** invalidate blob's data - doesn't free buffer */
	FORCEINLINE void Clear()
	{
		RawSizeRef() = 0;
	}

	/** free the blob's memory */
	FORCEINLINE void Free()
	{
		if (MaxRawSize() > 0) {
			RawFree(&Hdr());
			InitEmpty();
		}
	}

	/** copy data from another blob - replaces any existing blob's data */
	FORCEINLINE void CopyFrom(const CBlobBaseSimple& src)
	{
		Clear();
		AppendRaw(src);
	}

	/** overtake ownership of data buffer from the source blob - source blob will become empty */
	FORCEINLINE void MoveFrom(CBlobBaseSimple& src)
	{
		Free();
		ptr_u.m_pData = src.ptr_u.m_pData;
		src.InitEmpty();
	}

	/** swap buffers (with data) between two blobs (this and source blob) */
	FORCEINLINE void Swap(CBlobBaseSimple& src)
	{
		bitem_t *tmp = ptr_u.m_pData; ptr_u.m_pData = src.ptr_u.m_pData;
		src.ptr_u.m_pData = tmp;
	}

	/** append new bytes at the end of existing data bytes - reallocates if necessary */
	FORCEINLINE void AppendRaw(const void *p, bsize_t num_bytes)
	{
		assert(p != NULL);
		if (num_bytes > 0) {
			memcpy(GrowRawSize(num_bytes), p, num_bytes);
		} else {
			assert(num_bytes >= 0);
		}
	}

	/** append bytes from given source blob to the end of existing data bytes - reallocates if necessary */
	FORCEINLINE void AppendRaw(const CBlobBaseSimple& src)
	{
		if (!src.IsEmpty())
			memcpy(GrowRawSize(src.RawSize()), src.RawData(), src.RawSize());
	}

	/** Reallocate if there is no free space for num_bytes bytes.
	*  @return pointer to the new data to be added */
	FORCEINLINE bitem_t* MakeRawFreeSpace(bsize_t num_bytes)
	{
		assert(num_bytes >= 0);
		bsize_t new_size = RawSize() + num_bytes;
		if (new_size > MaxRawSize()) SmartAlloc(new_size);
		return ptr_u.m_pData + RawSize();
	}

	/** Increase RawSize() by num_bytes.
	*  @return pointer to the new data added */
	FORCEINLINE bitem_t* GrowRawSize(bsize_t num_bytes)
	{
		bitem_t* pNewData = MakeRawFreeSpace(num_bytes);
		RawSizeRef() += num_bytes;
		return pNewData;
	}

	/** Decrease RawSize() by num_bytes. */
	FORCEINLINE void ReduceRawSize(bsize_t num_bytes)
	{
		if (MaxRawSize() > 0 && num_bytes > 0) {
			assert(num_bytes <= RawSize());
			if (num_bytes < RawSize()) RawSizeRef() -= num_bytes;
			else RawSizeRef() = 0;
		}
	}

	/** reallocate blob data if needed */
	void SmartAlloc(bsize_t new_size)
	{
		bsize_t old_max_size = MaxRawSize();
		if (old_max_size >= new_size) return;
		// calculate minimum block size we need to allocate
		bsize_t min_alloc_size = sizeof(CHdr) + new_size + Ttail_reserve;
		// ask allocation policy for some reasonable block size
		bsize_t alloc_size = AllocPolicy(min_alloc_size);
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
	FORCEINLINE static bsize_t AllocPolicy(bsize_t min_alloc)
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
	static FORCEINLINE CHdr* RawAlloc(bsize_t num_bytes)
	{
		return (CHdr*)MallocT<byte>(num_bytes);
	}

	/** all deallocations should happen here */
	static FORCEINLINE void RawFree(CHdr* p)
	{
		free(p);
	}
	/** fixing the four bytes at the end of blob data - useful when blob is used to hold string */
	FORCEINLINE void FixTail() const
	{
		if (MaxRawSize() > 0) {
			bitem_t *p = &ptr_u.m_pData[RawSize()];
			for (bsize_t i = 0; i < Ttail_reserve; i++) {
				p[i] = 0;
			}
		}
	}
};

/** Blob - simple dynamic Titem_ array. Titem_ (template argument) is a placeholder for any type.
*  Titem_ can be any integral type, pointer, or structure. Using Blob instead of just plain C array
*  simplifies the resource management in several ways:
*  1. When adding new item(s) it automatically grows capacity if needed.
*  2. When variable of type Blob comes out of scope it automatically frees the data buffer.
*  3. Takes care about the actual data size (number of used items).
*  4. Dynamically constructs only used items (as opposite of static array which constructs all items) */
template <class Titem_, class Tbase_ = CBlobBaseSimple>
class CBlobT : public Tbase_ {
	// make template arguments public:
public:
	typedef Titem_ Titem;
	typedef Tbase_ Tbase;
	typedef typename Tbase::bsize_t bsize_t;

	static const bsize_t Titem_size = sizeof(Titem);

	struct OnTransfer {
		typename Tbase_::CHdr *m_pHdr_1;
		OnTransfer(const OnTransfer& src) : m_pHdr_1(src.m_pHdr_1) {assert(src.m_pHdr_1 != NULL); *(typename Tbase_::CHdr**)&src.m_pHdr_1 = NULL;}
		OnTransfer(CBlobT& src) : m_pHdr_1(src.ptr_u.m_pHdr_1) {src.InitEmpty();}
		~OnTransfer() {assert(m_pHdr_1 == NULL);}
	};

	/** Default constructor - makes new Blob ready to accept any data */
	FORCEINLINE CBlobT()
		: Tbase()
	{}

	/** Constructor - makes new Blob with data */
	FORCEINLINE CBlobT(const Titem_ *p, bsize_t num_items)
		: Tbase((typename Tbase_::bitem_t*)p, num_items * Titem_size)
	{}

	/** Copy constructor - make new blob to become copy of the original (source) blob */
	FORCEINLINE CBlobT(const Tbase& src)
		: Tbase(src)
	{
		assert((Tbase::RawSize() % Titem_size) == 0);
	}

	/** Take ownership constructor */
	FORCEINLINE CBlobT(const OnTransfer& ot)
		: Tbase(ot.m_pHdr_1)
	{}

	/** Destructor - ensures that allocated memory (if any) is freed */
	FORCEINLINE ~CBlobT()
	{
		Free();
	}

	/** Check the validity of item index (only in debug mode) */
	FORCEINLINE void CheckIdx(bsize_t idx) const
	{
		assert(idx >= 0); assert(idx < Size());
	}

	/** Return pointer to the first data item - non-const version */
	FORCEINLINE Titem* Data()
	{
		return (Titem*)Tbase::RawData();
	}

	/** Return pointer to the first data item - const version */
	FORCEINLINE const Titem* Data() const
	{
		return (const Titem*)Tbase::RawData();
	}

	/** Return pointer to the idx-th data item - non-const version */
	FORCEINLINE Titem* Data(bsize_t idx)
	{
		CheckIdx(idx);
		return (Data() + idx);
	}

	/** Return pointer to the idx-th data item - const version */
	FORCEINLINE const Titem* Data(bsize_t idx) const
	{
		CheckIdx(idx);
		return (Data() + idx);
	}

	/** Return number of items in the Blob */
	FORCEINLINE bsize_t Size() const
	{
		return (Tbase::RawSize() / Titem_size);
	}

	/** Return total number of items that can fit in the Blob without buffer reallocation */
	FORCEINLINE bsize_t MaxSize() const
	{
		return (Tbase::MaxRawSize() / Titem_size);
	}
	/** Return number of additional items that can fit in the Blob without buffer reallocation */
	FORCEINLINE bsize_t GetReserve() const
	{
		return ((Tbase::MaxRawSize() - Tbase::RawSize()) / Titem_size);
	}

	/** Free the memory occupied by Blob destroying all items */
	FORCEINLINE void Free()
	{
		assert((Tbase::RawSize() % Titem_size) == 0);
		bsize_t old_size = Size();
		if (old_size > 0) {
			// destroy removed items;
			Titem* pI_last_to_destroy = Data(0);
			for (Titem* pI = Data(old_size - 1); pI >= pI_last_to_destroy; pI--) pI->~Titem_();
		}
		Tbase::Free();
	}

	/** Grow number of data items in Blob by given number - doesn't construct items */
	FORCEINLINE Titem* GrowSizeNC(bsize_t num_items)
	{
		return (Titem*)Tbase::GrowRawSize(num_items * Titem_size);
	}

	/** Grow number of data items in Blob by given number - constructs new items (using Titem_'s default constructor) */
	FORCEINLINE Titem* GrowSizeC(bsize_t num_items)
	{
		Titem* pI = GrowSizeNC(num_items);
		for (bsize_t i = num_items; i > 0; i--, pI++) new (pI) Titem();
	}

	/** Destroy given number of items and reduce the Blob's data size */
	FORCEINLINE void ReduceSize(bsize_t num_items)
	{
		assert((Tbase::RawSize() % Titem_size) == 0);
		bsize_t old_size = Size();
		assert(num_items <= old_size);
		bsize_t new_size = (num_items <= old_size) ? (old_size - num_items) : 0;
		// destroy removed items;
		Titem* pI_last_to_destroy = Data(new_size);
		for (Titem* pI = Data(old_size - 1); pI >= pI_last_to_destroy; pI--) pI->~Titem();
		// remove them
		Tbase::ReduceRawSize(num_items * Titem_size);
	}

	/** Append one data item at the end (calls Titem_'s default constructor) */
	FORCEINLINE Titem* AppendNew()
	{
		Titem& dst = *GrowSizeNC(1); // Grow size by one item
		Titem* pNewItem = new (&dst) Titem(); // construct the new item by calling in-place new operator
		return pNewItem;
	}

	/** Append the copy of given item at the end of Blob (using copy constructor) */
	FORCEINLINE Titem* Append(const Titem& src)
	{
		Titem& dst = *GrowSizeNC(1); // Grow size by one item
		Titem* pNewItem = new (&dst) Titem(src); // construct the new item by calling in-place new operator with copy ctor()
		return pNewItem;
	}

	/** Add given items (ptr + number of items) at the end of blob */
	FORCEINLINE Titem* Append(const Titem* pSrc, bsize_t num_items)
	{
		Titem* pDst = GrowSizeNC(num_items);
		Titem* pDstOrg = pDst;
		Titem* pDstEnd = pDst + num_items;
		while (pDst < pDstEnd) new (pDst++) Titem(*(pSrc++));
		return pDstOrg;
	}

	/** Remove item with the given index by replacing it by the last item and reducing the size by one */
	FORCEINLINE void RemoveBySwap(bsize_t idx)
	{
		CheckIdx(idx);
		// destroy removed item
		Titem* pRemoved = Data(idx);
		RemoveBySwap(pRemoved);
	}

	/** Remove item given by pointer replacing it by the last item and reducing the size by one */
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
		Tbase::ReduceRawSize(Titem_size);
	}

	/** Ensures that given number of items can be added to the end of Blob. Returns pointer to the
	*  first free (unused) item */
	FORCEINLINE Titem* MakeFreeSpace(bsize_t num_items)
	{
		return (Titem*)Tbase::MakeRawFreeSpace(num_items * Titem_size);
	}

	FORCEINLINE OnTransfer Transfer()
	{
		return OnTransfer(*this);
	};
};


#endif /* BLOB_HPP */
