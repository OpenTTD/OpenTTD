/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file countedptr.hpp CCountedPtr - smart pointer implementation. */

#ifndef COUNTEDPTR_HPP
#define COUNTEDPTR_HPP

/**
 * CCountedPtr - simple reference counting smart pointer.
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
	Tcls *m_pT;

public:
	/** default (NULL) construct or construct from a raw pointer */
	FORCEINLINE CCountedPtr(Tcls *pObj = NULL) : m_pT(pObj) {AddRef();}

	/** copy constructor (invoked also when initializing from another smart ptr) */
	FORCEINLINE CCountedPtr(const CCountedPtr& src) : m_pT(src.m_pT) {AddRef();}

	/** destructor releasing the reference */
	FORCEINLINE ~CCountedPtr() {Release();}

protected:
	/** add one ref to the underlaying object */
	FORCEINLINE void AddRef() {if (m_pT != NULL) m_pT->AddRef();}

public:
	/** release smart pointer (and decrement ref count) if not null */
	FORCEINLINE void Release() {if (m_pT != NULL) {Tcls *pT = m_pT; m_pT = NULL; pT->Release();}}

	/** dereference of smart pointer - const way */
	FORCEINLINE const Tcls *operator -> () const {assert(m_pT != NULL); return m_pT;}

	/** dereference of smart pointer - non const way */
	FORCEINLINE Tcls *operator -> () {assert(m_pT != NULL); return m_pT;}

	/** raw pointer casting operator - const way */
	FORCEINLINE operator const Tcls*() const {assert(m_pT == NULL); return m_pT;}

	/** raw pointer casting operator - non-const way */
	FORCEINLINE operator Tcls*() {return m_pT;}

	/** operator & to support output arguments */
	FORCEINLINE Tcls** operator &() {assert(m_pT == NULL); return &m_pT;}

	/** assignment operator from raw ptr */
	FORCEINLINE CCountedPtr& operator = (Tcls *pT) {Assign(pT); return *this;}

	/** assignment operator from another smart ptr */
	FORCEINLINE CCountedPtr& operator = (const CCountedPtr& src) {Assign(src.m_pT); return *this;}

	/** assignment operator helper */
	FORCEINLINE void Assign(Tcls *pT);

	/** one way how to test for NULL value */
	FORCEINLINE bool IsNull() const {return m_pT == NULL;}

	/** another way how to test for NULL value */
	//FORCEINLINE bool operator == (const CCountedPtr& sp) const {return m_pT == sp.m_pT;}

	/** yet another way how to test for NULL value */
	//FORCEINLINE bool operator != (const CCountedPtr& sp) const {return m_pT != sp.m_pT;}

	/** assign pointer w/o incrementing ref count */
	FORCEINLINE void Attach(Tcls *pT) {Release(); m_pT = pT;}

	/** detach pointer w/o decrementing ref count */
	FORCEINLINE Tcls *Detach() {Tcls *pT = m_pT; m_pT = NULL; return pT;}
};

template <class Tcls_>
FORCEINLINE void CCountedPtr<Tcls_>::Assign(Tcls *pT)
{
	/* if they are the same, we do nothing */
	if (pT != m_pT) {
		if (pT != NULL) pT->AddRef();        // AddRef new pointer if any
		Tcls *pTold = m_pT;                  // save original ptr
		m_pT = pT;                           // update m_pT to new value
		if (pTold != NULL) pTold->Release(); // release old ptr if any
	}
}

/**
 * Adapter wrapper for CCountedPtr like classes that can't be used directly by stl
 * collections as item type. For example CCountedPtr has overloaded operator & which
 * prevents using CCountedPtr in stl collections (i.e. std::list<CCountedPtr<MyType> >)
 */
template <class T> struct AdaptT {
	T m_t;

	/** construct by wrapping the given object */
	AdaptT(const T &t)
		: m_t(t)
	{}

	/** assignment operator */
	T& operator = (const T &t)
	{
		m_t = t;
		return t;
	}

	/** type-cast operator (used when AdaptT is used instead of T) */
	operator T& ()
	{
		return m_t;
	}

	/** const type-cast operator (used when AdaptT is used instead of const T) */
	operator const T& () const
	{
		return m_t;
	}
};


/**
 * Simple counted object. Use it as base of your struct/class if you want to use
 *  basic reference counting. Your struct/class will destroy and free itself when
 *  last reference to it is released (using Relese() method). The initial reference
 *  count (when it is created) is zero (don't forget AddRef() at least one time if
 *  not using CCountedPtr<T>.
 *
 *  @see misc/countedobj.cpp for implementation.
 */
struct SimpleCountedObject {
	int32 m_ref_cnt;

	SimpleCountedObject()
		: m_ref_cnt(0)
	{}

	virtual ~SimpleCountedObject()
	{}

	virtual int32 AddRef();
	virtual int32 Release();
	virtual void FinalRelease() {};
};




#endif /* COUNTEDPTR_HPP */
