/* $Id$ */

#ifndef COUNTEDPTR_HPP
#define COUNTEDPTR_HPP

#if 0 // reenable when needed
/** @file CCountedPtr - smart pointer implementation */

/** CCountedPtr - simple reference counting smart pointer.
 *
 *     One of the standard ways how to maintain object's lifetime.
 *
 *     See http://ootips.org/yonat/4dev/smart-pointers.html for more
 *   general info about smart pointers.
 *
 *     This class implements ref-counted pointer for objects/interfaces that
 *   support AddRef() and Release() methods.
 */
template <class Tcls_>
class CCountedPtr {
	/** redefine the template argument to make it visible for derived classes */
public:
	typedef Tcls_ Tcls;

protected:
	/** here we hold our pointer to the target */
	Tcls* m_pT;

public:
	/** default (NULL) construct or construct from a raw pointer */
	FORCEINLINE CCountedPtr(Tcls* pObj = NULL) : m_pT(pObj) {AddRef();};

	/** copy constructor (invoked also when initializing from another smart ptr) */
	FORCEINLINE CCountedPtr(const CCountedPtr& src) : m_pT(src.m_pT) {AddRef();};

	/** destructor releasing the reference */
	FORCEINLINE ~CCountedPtr() {Release();};

protected:
	/** add one ref to the underlaying object */
	FORCEINLINE void AddRef() {if (m_pT != NULL) m_pT->AddRef();}

public:
	/** release smart pointer (and decrement ref count) if not null */
	FORCEINLINE void Release() {if (m_pT != NULL) {m_pT->Release(); m_pT = NULL;}}

	/** dereference of smart pointer - const way */
	FORCEINLINE const Tcls* operator -> () const {assert(m_pT != NULL); return m_pT;};

	/** dereference of smart pointer - non const way */
	FORCEINLINE Tcls* operator -> () {assert(m_pT != NULL); return m_pT;};

	/** raw pointer casting operator - const way */
	FORCEINLINE operator const Tcls*() const {assert(m_pT == NULL); return m_pT;}

	/** raw pointer casting operator - non-const way */
	FORCEINLINE operator Tcls*() {assert(m_pT == NULL); return m_pT;}

	/** operator & to support output arguments */
	FORCEINLINE Tcls** operator &() {assert(m_pT == NULL); return &m_pT;}

	/** assignment operator from raw ptr */
	FORCEINLINE CCountedPtr& operator = (Tcls* pT) {Assign(pT); return *this;}

	/** assignment operator from another smart ptr */
	FORCEINLINE CCountedPtr& operator = (CCountedPtr& src) {Assign(src.m_pT); return *this;}

	/** assignment operator helper */
	FORCEINLINE void Assign(Tcls* pT);

	/** one way how to test for NULL value */
	FORCEINLINE bool IsNull() const {return m_pT == NULL;}

	/** another way how to test for NULL value */
	FORCEINLINE bool operator == (const CCountedPtr& sp) const {return m_pT == sp.m_pT;}

	/** yet another way how to test for NULL value */
	FORCEINLINE bool operator != (const CCountedPtr& sp) const {return m_pT != sp.m_pT;}

	/** assign pointer w/o incrementing ref count */
	FORCEINLINE void Attach(Tcls* pT) {Release(); m_pT = pT;}

	/** detach pointer w/o decrementing ref count */
	FORCEINLINE Tcls* Detach() {Tcls* pT = m_pT; m_pT = NULL; return pT;}
};

template <class Tcls_>
FORCEINLINE void CCountedPtr<Tcls_>::Assign(Tcls* pT)
{
	// if they are the same, we do nothing
	if (pT != m_pT) {
		if (pT) pT->AddRef();        // AddRef new pointer if any
		Tcls* pTold = m_pT;          // save original ptr
		m_pT = pT;                   // update m_pT to new value
		if (pTold) pTold->Release(); // release old ptr if any
	}
}

#endif /* 0 */
#endif /* COUNTEDPTR_HPP */
