/* $Id$ */

/** @file str.hpp String formating? */

#ifndef  STR_HPP
#define  STR_HPP

#include <errno.h>
#include <stdarg.h>
#include "strapi.hpp"

/** Blob based string. */
template <typename Tchar, bool TcaseInsensitive>
struct CStrT : public CBlobT<Tchar>
{
	typedef CBlobT<Tchar> base;                    ///< base class
	typedef CStrApiT<Tchar, TcaseInsensitive> Api; ///< string API abstraction layer
	typedef typename base::bsize_t bsize_t;        ///< size type inherited from blob
	typedef typename base::OnTransfer OnTransfer;  ///< temporary 'transfer ownership' object type

	/** Construction from C zero ended string. */
	FORCEINLINE CStrT(const Tchar* str = NULL)
	{
		AppendStr(str);
	}

	/** Construction from C string and given number of characters. */
	FORCEINLINE CStrT(const Tchar* str, bsize_t num_chars) : base(str, num_chars)
	{
		base::FixTail();
	}

	/** Construction from C string determined by 'begin' and 'end' pointers. */
	FORCEINLINE CStrT(const Tchar* str, const Tchar* end)
		: base(str, end - str)
	{
		base::FixTail();
	}

	/** Construction from blob contents. */
	FORCEINLINE CStrT(const CBlobBaseSimple& src)
		: base(src)
	{
		base::FixTail();
	}

	/** Copy constructor. */
	FORCEINLINE CStrT(const CStrT& src)
		: base(src)
	{
		base::FixTail();
	}

	/** Take over ownership constructor */
	FORCEINLINE CStrT(const OnTransfer& ot)
		: base(ot)
	{
	}

	/** Grow the actual buffer and fix the trailing zero at the end. */
	FORCEINLINE Tchar* GrowSizeNC(bsize_t count)
	{
		Tchar* ret = base::GrowSizeNC(count);
		base::FixTail();
		return ret;
	}

	/** Append zero-ended C string. */
	FORCEINLINE void AppendStr(const Tchar* str)
	{
		if (str != NULL && str[0] != '\0') {
			base::Append(str, (bsize_t)Api::StrLen(str));
			base::FixTail();
		}
	}

	/** Append another CStrT or blob. */
	FORCEINLINE void Append(const CBlobBaseSimple& src)
	{
		if (src.RawSize() > 0) {
			base::AppendRaw(src);
			base::FixTail();
		}
	}

	/** Assignment from C string. */
	FORCEINLINE CStrT& operator = (const Tchar* src)
	{
		base::Clear();
		AppendStr(src);
		return *this;
	}

	/** Assignment from another CStrT or blob. */
	FORCEINLINE CStrT& operator = (const CBlobBaseSimple& src)
	{
		base::Clear();
		base::AppendRaw(src);
		base::FixTail();
		return *this;
	}

	/** Assignment from another CStrT or blob. */
	FORCEINLINE CStrT& operator = (const CStrT& src)
	{
		base::Clear();
		base::AppendRaw(src);
		base::FixTail();
		return *this;
	}

	/** Lower-than operator (to support stl collections) */
	FORCEINLINE bool operator < (const CStrT &other) const
	{
		return (Api::StrCmp(base::Data(), other.Data()) < 0);
	}

	/** Add formated string (like vsprintf) at the end of existing contents. */
	int AddFormatL(const Tchar *format, va_list args)
	{
		bsize_t addSize = Api::StrLen(format);
		if (addSize < 16) addSize = 16;
		addSize += addSize / 2;
		int ret;
		int err = 0;
		for (;;) {
			Tchar *buf = MakeFreeSpace(addSize);
			ret = Api::SPrintFL(buf, base::GetReserve(), format, args);
			if (ret >= base::GetReserve()) {
				/* Greater return than given count means needed buffer size. */
				addSize = ret + 1;
				continue;
			}
			if (ret >= 0) {
				/* success */
				break;
			}
			err = errno;
			if (err != ERANGE && err != ENOENT && err != 0) {
				/* some strange failure */
				break;
			}
			/* small buffer (M$ implementation) */
			addSize *= 2;
		}
		if (ret > 0) {
			GrowSizeNC(ret);
		} else {
			base::FixTail();
		}
		return ret;
	}

	/** Add formated string (like sprintf) at the end of existing contents. */
	int AddFormat(const Tchar *format, ...)
	{
		va_list args;
		va_start(args, format);
		int ret = AddFormatL(format, args);
		va_end(args);
		return ret;
	}

	/** Assign formated string (like vsprintf). */
	int FormatL(const Tchar *format, va_list args)
	{
		base::Free();
		int ret = AddFormatL(format, args);
		return ret;
	}

	/** Assign formated string (like sprintf). */
	int Format(const Tchar *format, ...)
	{
		base::Free();
		va_list args;
		va_start(args, format);
		int ret = AddFormatL(format, args);
		va_end(args);
		return ret;
	}
};

typedef CStrT<char   , false> CStrA;   ///< Case sensitive ANSI/UTF-8 string
typedef CStrT<char   , true > CStrCiA; ///< Case insensitive ANSI/UTF-8 string
#if defined(HAS_WCHAR)
typedef CStrT<wchar_t, false> CStrW;   ///< Case sensitive unicode string
typedef CStrT<wchar_t, true > CStrCiW; ///< Case insensitive unicode string
#endif /* HAS_WCHAR */

#endif /* STR_HPP */
