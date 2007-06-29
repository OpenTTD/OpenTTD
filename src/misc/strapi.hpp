/* $Id$ */

/** @file strapi.hpp */

#ifndef  STRAPI_HPP
#define  STRAPI_HPP

#include <string.h>
#include <wchar.h>

#if !defined(_MSC_VER)
#define _stricmp strcmp
#define _wcsicmp wcscmp
#endif //!_MSC_VER

/** String API mapper base - just mapping by character type, not by case sensitivity yet.
	* Class template CStrApiBaseT declaration is general, but following inline method
	* definitions are specialized by character type. Class is not used directly, but only
	* as a base class for template class CStrApiT */
template <typename Tchar>
class CStrApiBaseT
{
public:
	/** ::strlen wrapper */
	static size_t StrLen(const Tchar *s);
	static int SPrintFL(Tchar *buf, size_t count, const Tchar *fmt, va_list args);
};

/** ::strlen wrapper specialization for char */
template <> /*static*/ inline size_t CStrApiBaseT<char>::StrLen(const char *s)
{
	return ::strlen(s);
}

/** ::strlen wrapper specialization for wchar_t */
template <> /*static*/ inline size_t CStrApiBaseT<wchar_t>::StrLen(const wchar_t *s)
{
	return ::wcslen(s);
}

/** ::vsprintf wrapper specialization for char */
template <> /*static*/ inline int CStrApiBaseT<char>::SPrintFL(char *buf, size_t count, const char *fmt, va_list args)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400) // VC 8.0 and above
	return ::vsnprintf_s(buf, count, count - 1, fmt, args);
#else // ! VC 8.0 and above
	return ::vsnprintf(buf, count, fmt, args);
#endif
}

/** ::vsprintf wrapper specialization for wchar_t */
template <> /*static*/ inline int CStrApiBaseT<wchar_t>::SPrintFL(wchar_t *buf, size_t count, const wchar_t *fmt, va_list args)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400) // VC 8.0 and above
	return ::_vsnwprintf_s(buf, count, count - 1, fmt, args);
#else // ! VC 8.0 and above
# if defined(_WIN32)
	 return ::_vsnwprintf(buf, count, fmt, args);
# else // !_WIN32
	 return ::vswprintf(buf, count, fmt, args);
# endif // !_WIN32
#endif
}



template <typename Tchar, bool TcaseInsensitive>
class CStrApiT : public CStrApiBaseT<Tchar>
{
public:
	static int StrCmp(const Tchar *s1, const Tchar *s2);
};

template <> /*static*/ inline int CStrApiT<char, false>::StrCmp(const char *s1, const char *s2)
{
	return ::strcmp(s1, s2);
}

template <> /*static*/ inline int CStrApiT<char, true>::StrCmp(const char *s1, const char *s2)
{
	return ::_stricmp(s1, s2);
}

template <> /*static*/ inline int CStrApiT<wchar_t, false>::StrCmp(const wchar_t *s1, const wchar_t *s2)
{
	return ::wcscmp(s1, s2);
}

template <> /*static*/ inline int CStrApiT<wchar_t, true>::StrCmp(const wchar_t *s1, const wchar_t *s2)
{
	return ::_wcsicmp(s1, s2);
}

#endif /* STRAPI_HPP */
