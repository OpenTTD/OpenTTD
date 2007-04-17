/* $Id$ */

/** @file autocopyptr.hpp */

#ifndef  AUTOCOPYPTR_HPP
#define  AUTOCOPYPTR_HPP

#if 0 // reenable when needed
/** CAutoCopyPtrT - kind of CoW (Copy on Write) pointer.
 *  It is non-invasive smart pointer (reference counter is held outside
 *  of Tdata).
 *  When copied, its new copy shares the same underlaying structure Tdata.
 *  When dereferenced, its behaviour depends on 2 factors:
 *     - whether the data is shared (used by more than one pointer)
 *     - type of access (read/write)
 *    When shared pointer is dereferenced for write, new clone of Tdata
 *  is made first.
 *  Can't be used for polymorphic data types (interfaces).
 */
template <class Tdata_>
class CAutoCopyPtrT {
protected:
	typedef Tdata_ Tdata;

	struct CItem {
		int     m_ref_cnt;  ///< reference counter
		Tdata   m_data;     ///< custom data itself

		FORCEINLINE CItem()                  : m_ref_cnt(1)                     {};
		FORCEINLINE CItem(const Tdata& data) : m_ref_cnt(1), m_data(data)       {};
		FORCEINLINE CItem(const CItem& src)  : m_ref_cnt(1), m_data(src.m_data) {};
	};

	mutable CItem* m_pI; ///< points to the ref-counted data

public:
	FORCEINLINE CAutoCopyPtrT() : m_pI(NULL) {};
	FORCEINLINE CAutoCopyPtrT(const Tdata& data) : m_pI(new CItem(data)) {};
	FORCEINLINE CAutoCopyPtrT(const CAutoCopyPtrT& src) : m_pI(src.m_pI) {if (m_pI != NULL) m_pI->m_ref_cnt++;}
	FORCEINLINE ~CAutoCopyPtrT() {if (m_pI == NULL || (--m_pI->m_ref_cnt) > 0) return; delete m_pI; m_pI = NULL;}

	/** data accessor (read only) */
	FORCEINLINE const Tdata& GetDataRO() const {if (m_pI == NULL) m_pI = new CItem(); return m_pI->m_data;}
	/** data accessor (read / write) */
	FORCEINLINE Tdata& GetDataRW() {CloneIfShared(); if (m_pI == NULL) m_pI = new CItem(); return m_pI->m_data;}

	/** clone data if it is shared */
	FORCEINLINE void CloneIfShared()
	{
		if (m_pI != NULL && m_pI->m_ref_cnt > 1) {
			// we share data item with somebody, clone it to become an exclusive owner
			CItem* pNewI = new CItem(*m_pI);
			m_pI->m_ref_cnt--;
			m_pI = pNewI;
		}
	}

	/** assign pointer from the other one (maintaining ref counts) */
	FORCEINLINE void Assign(const CAutoCopyPtrT& src)
	{
		if (m_pI == src.m_pI) return;
		if (m_pI != NULL && (--m_pI->m_ref_cnt) <= 0) delete m_pI;
		m_pI = src.m_pI;
		if (m_pI != NULL) m_pI->m_ref_cnt++;
	}

	/** dereference operator (read only) */
	FORCEINLINE const Tdata* operator -> () const {return &GetDataRO();}
	/** dereference operator (read / write) */
	FORCEINLINE Tdata* operator -> () {return &GetDataRW();}

	/** assignment operator */
	FORCEINLINE CAutoCopyPtrT& operator = (const CAutoCopyPtrT& src) {Assign(src); return *this;}

	/** forwarding 'lower then' operator to the underlaying items */
	FORCEINLINE bool operator < (const CAutoCopyPtrT& other) const
	{
		assert(m_pI != NULL);
		assert(other.m_pI != NULL);
		return (m_pI->m_data) < (other.m_pI->m_data);
	}
};

#endif /* 0 */
#endif /* AUTOCOPYPTR_HPP */
