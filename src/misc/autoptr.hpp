/* $Id$ */

/** @file autoptr.hpp */

#ifndef AUTOPTR_HPP
#define AUTOPTR_HPP

/** AutoPtrT - kind of smart pointer that ensures the owned object gets
 *  deleted when its pointer goes out of scope.
 *  It is non-invasive smart pointer (no reference counter).
 *  When copied, the copy takes ownership of underlying object
 *  and original becomes NULL!
 *  Can be used also for polymorphic data types (interfaces).
 */
template <class T>
class AutoPtrT {
public:
	typedef T obj_t;

protected:
	mutable T* m_p; ///< points to the data

public:
	FORCEINLINE AutoPtrT()
	 : m_p(NULL)
	{};

	FORCEINLINE AutoPtrT(const AutoPtrT<T>& src)
	 : m_p(src.m_p)
	{
		if (m_p != NULL) src.m_p = NULL;
	};

	FORCEINLINE AutoPtrT(T *p)
	 : m_p(p)
	{}

	FORCEINLINE ~AutoPtrT()
	{
		if (m_p != NULL) {
			T *p = m_p;
			m_p = NULL;
			delete p;
		}
	}

	/** give-up ownership and NULLify the raw pointer */
	FORCEINLINE T* Release()
	{
		T* p = m_p;
		m_p = NULL;
		return p;
	}

	/** raw-pointer cast operator (read only) */
	FORCEINLINE operator const T* () const
	{
		return m_p;
	}

	/** raw-pointer cast operator */
	FORCEINLINE operator T* ()
	{
		return m_p;
	}

	/** dereference operator (read only) */
	FORCEINLINE const T* operator -> () const
	{
		assert(m_p != NULL);
		return m_p;
	}

	/** dereference operator (read / write) */
	FORCEINLINE T* operator -> ()
	{
		assert(m_p != NULL);
		return m_p;
	}

	/** assignment operator */
	FORCEINLINE AutoPtrT& operator = (const AutoPtrT& src)
	{
		m_p = src.m_p;
		if (m_p != NULL) src.m_p = NULL;
		return *this;
	}

	/** forwarding 'lower than' operator to the underlaying items */
	FORCEINLINE bool operator < (const AutoPtrT& other) const
	{
		assert(m_p != NULL);
		assert(other.m_p != NULL);
		return (*m_p) < (*other.m_p);
	}
};

#endif /* AUTOPTR_HPP */
