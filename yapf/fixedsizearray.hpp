/* $Id$ */

#ifndef  FIXEDSIZEARRAY_HPP
#define  FIXEDSIZEARRAY_HPP


/** fixed size array
 *  Upon construction it preallocates fixed size block of memory
 *  for all items, but doesn't construct them. Item's construction
 *  is delayed. */
template <class Titem_, int Tcapacity_>
struct CFixedSizeArrayT {
	/** the only member of fixed size array is pointer to the block
	 *  of C array of items. Header can be found on the offset -sizeof(CHdr). */
	Titem_ *m_items;

	/** header for fixed size array */
	struct CHdr
	{
		int    m_num_items; ///< number of items in the array
		int    m_ref_cnt;   ///< block reference counter (used by copy constructor and by destructor)
	};

	// make types and constants visible from outside
	typedef Titem_ Titem; // type of array item

	static const int Tcapacity = Tcapacity_;     // the array capacity (maximum size)
	static const int TitemSize = sizeof(Titem_); // size of item
	static const int ThdrSize  = sizeof(CHdr);   // size of header

	/** Default constructor. Preallocate space for items and header, then initialize header. */
	CFixedSizeArrayT()
	{
		// allocate block for header + items (don't construct items)
		m_items = (Titem*)(((int8*)malloc(ThdrSize + Tcapacity * sizeof(Titem))) + ThdrSize);
		SizeRef() = 0; // initial number of items
		RefCnt() = 1; // initial reference counter
	}

	/** Copy constructor. Preallocate space for items and header, then initialize header. */
	CFixedSizeArrayT(const CFixedSizeArrayT<Titem_, Tcapacity_>& src)
	{
		// share block (header + items) with the source array
		m_items = src.m_items;
		RefCnt()++; // now we share block with the source
	}

	/** destroy remaining items and free the memory block */
	~CFixedSizeArrayT()
	{
		// release one reference to the shared block
		if ((--RefCnt()) > 0) return; // and return if there is still some owner

		Clear();
		// free the memory block occupied by items
		free(((int8*)m_items) - ThdrSize);
		m_items = NULL;
	}

	/** Clear (destroy) all items */
	FORCEINLINE void Clear()
	{
		// walk through all allocated items backward and destroy them
		for (Titem* pItem = &m_items[Size() - 1]; pItem >= m_items; pItem--) {
			pItem->~Titem_();
		}
		// number of items become zero
		SizeRef() = 0;
	}

protected:
	/** return reference to the array header (non-const) */
	FORCEINLINE CHdr& Hdr() { return *(CHdr*)(((int8*)m_items) - ThdrSize); }
	/** return reference to the array header (const) */
	FORCEINLINE const CHdr& Hdr() const { return *(CHdr*)(((int8*)m_items) - ThdrSize); }
	/** return reference to the block reference counter */
	FORCEINLINE int& RefCnt() { return Hdr().m_ref_cnt; }
	/** return reference to number of used items */
	FORCEINLINE int& SizeRef() { return Hdr().m_num_items; }
public:
	/** return number of used items */
	FORCEINLINE int Size() const { return Hdr().m_num_items; }
	/** return true if array is full */
	FORCEINLINE bool IsFull() const { return Size() >= Tcapacity; };
	/** return true if array is empty */
	FORCEINLINE bool IsEmpty() const { return Size() <= 0; };
	/** index validation */
	FORCEINLINE void CheckIdx(int idx) const { assert(idx >= 0); assert(idx < Size()); }
	/** add (allocate), but don't construct item */
	FORCEINLINE Titem& AddNC() { assert(!IsFull()); return m_items[SizeRef()++]; }
	/** add and construct item using default constructor */
	FORCEINLINE Titem& Add() { Titem& item = AddNC(); new(&item)Titem; return item; }
	/** return item by index (non-const version) */
	FORCEINLINE Titem& operator [] (int idx) { CheckIdx(idx); return m_items[idx]; }
	/** return item by index (const version) */
	FORCEINLINE const Titem& operator [] (int idx) const { CheckIdx(idx); return m_items[idx]; }
};

#endif /* FIXEDSIZEARRAY_HPP */
