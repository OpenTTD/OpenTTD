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
	/** default (nullptr) construct or construct from a raw pointer */
	inline CCountedPtr(Tcls *pObj = nullptr) : m_pT(pObj)
	{
		AddRef();
	}

	/** copy constructor (invoked also when initializing from another smart ptr) */
	inline CCountedPtr(const CCountedPtr &src) : m_pT(src.m_pT)
	{
		AddRef();
	}

	/** destructor releasing the reference */
	inline ~CCountedPtr()
	{
		Release();
	}

protected:
	/** add one ref to the underlying object */
	inline void AddRef()
	{
		if (m_pT != nullptr) m_pT->AddRef();
	}

public:
	/** release smart pointer (and decrement ref count) if not null */
	inline void Release()
	{
		if (m_pT != nullptr) {
			Tcls *pT = m_pT;
			m_pT = nullptr;
			pT->Release();
		}
	}

	/** dereference of smart pointer - const way */
	inline const Tcls *operator->() const
	{
		assert(m_pT != nullptr);
		return m_pT;
	}

	/** dereference of smart pointer - non const way */
	inline Tcls *operator->()
	{
		assert(m_pT != nullptr);
		return m_pT;
	}

	/** raw pointer casting operator - const way */
	inline operator const Tcls*() const
	{
		assert(m_pT == nullptr);
		return m_pT;
	}

	/** raw pointer casting operator - non-const way */
	inline operator Tcls*()
	{
		return m_pT;
	}

	/** operator & to support output arguments */
	inline Tcls** operator&()
	{
		assert(m_pT == nullptr);
		return &m_pT;
	}

	/** assignment operator from raw ptr */
	inline CCountedPtr& operator=(Tcls *pT)
	{
		Assign(pT);
		return *this;
	}

	/** assignment operator from another smart ptr */
	inline CCountedPtr& operator=(const CCountedPtr &src)
	{
		Assign(src.m_pT);
		return *this;
	}

	/** assignment operator helper */
	inline void Assign(Tcls *pT);

	/** one way how to test for nullptr value */
	inline bool IsNull() const
	{
		return m_pT == nullptr;
	}

	/** assign pointer w/o incrementing ref count */
	inline void Attach(Tcls *pT)
	{
		Release();
		m_pT = pT;
	}

	/** detach pointer w/o decrementing ref count */
	inline Tcls *Detach()
	{
		Tcls *pT = m_pT;
		m_pT = nullptr;
		return pT;
	}
};

template <class Tcls_>
inline void CCountedPtr<Tcls_>::Assign(Tcls *pT)
{
	/* if they are the same, we do nothing */
	if (pT != m_pT) {
		if (pT != nullptr) pT->AddRef();        // AddRef new pointer if any
		Tcls *pTold = m_pT;                  // save original ptr
		m_pT = pT;                           // update m_pT to new value
		if (pTold != nullptr) pTold->Release(); // release old ptr if any
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
 *  last reference to it is released (using Release() method). The initial reference
 *  count (when it is created) is zero (don't forget AddRef() at least one time if
 *  not using CCountedPtr<T>.
 *
 *  @see misc/countedobj.cpp for implementation.
 */
struct SimpleCountedObject {
	int32_t m_ref_cnt;

	SimpleCountedObject()
		: m_ref_cnt(0)
	{}

	virtual ~SimpleCountedObject()
	{}

	virtual int32_t AddRef();
	virtual int32_t Release();
	virtual void FinalRelease() {};
};

#endif /* COUNTEDPTR_HPP */
