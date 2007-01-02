/* $Id$ */

#ifndef  ARRAY_HPP
#define  ARRAY_HPP

#include "fixedsizearray.hpp"

/** Flexible array with size limit. Implemented as fixed size
 *  array of fixed size arrays */
template <class Titem_, int Tblock_size_ = 1024, int Tnum_blocks_ = Tblock_size_>
class CArrayT {
public:
	typedef Titem_ Titem; ///< Titem is now visible from outside
	typedef CFixedSizeArrayT<Titem_, Tblock_size_> CSubArray; ///< inner array
	typedef CFixedSizeArrayT<CSubArray, Tnum_blocks_> CSuperArray; ///< outer array

protected:
	CSuperArray     m_a; ///< array of arrays of items

public:
	static const int Tblock_size = Tblock_size_; ///< block size is now visible from outside
	static const int Tnum_blocks = Tnum_blocks_; ///< number of blocks is now visible from outside
	static const int Tcapacity   = Tblock_size * Tnum_blocks; ///< total max number of items

	/** implicit constructor */
	FORCEINLINE CArrayT() { }
	/** Clear (destroy) all items */
	FORCEINLINE void Clear() {m_a.Clear();}
	/** Return actual number of items */
	FORCEINLINE int Size() const
	{
		int super_size = m_a.Size();
		if (super_size == 0) return 0;
		int sub_size = m_a[super_size - 1].Size();
		return (super_size - 1) * Tblock_size + sub_size;
	}
	/** return true if array is empty */
	FORCEINLINE bool IsEmpty() { return m_a.IsEmpty(); }
	/** return true if array is full */
	FORCEINLINE bool IsFull() { return m_a.IsFull() && m_a[Tnum_blocks - 1].IsFull(); }
	/** return first sub-array with free space for new item */
	FORCEINLINE CSubArray& FirstFreeSubArray()
	{
		int super_size = m_a.Size();
		if (super_size > 0) {
			CSubArray& sa = m_a[super_size - 1];
			if (!sa.IsFull()) return sa;
		}
		return m_a.Add();
	}
	/** allocate but not construct new item */
	FORCEINLINE Titem_& AddNC() { return FirstFreeSubArray().AddNC(); }
	/** allocate and construct new item */
	FORCEINLINE Titem_& Add()   { return FirstFreeSubArray().Add(); }
	/** indexed access (non-const) */
	FORCEINLINE Titem& operator [] (int idx)
	{
		CSubArray& sa = m_a[idx / Tblock_size];
		Titem& item   = sa [idx % Tblock_size];
		return item;
	}
	/** indexed access (const) */
	FORCEINLINE const Titem& operator [] (int idx) const
	{
		CSubArray& sa = m_a[idx / Tblock_size];
		Titem& item   = sa [idx % Tblock_size];
		return item;
	}
};

#endif /* ARRAY_HPP */
