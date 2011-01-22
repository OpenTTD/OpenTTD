/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file str.hpp String formating? */

#ifndef STR_HPP
#define STR_HPP

#include <errno.h>
#include <stdarg.h>
#include "blob.hpp"
#include "../core/math_func.hpp"
#include "../string_func.h"

/** Blob based case sensitive ANSI/UTF-8 string */
struct CStrA : public CBlobT<char>
{
	typedef CBlobT<char> base;                    ///< base class

	/** Create an empty CStrT */
	FORCEINLINE CStrA()
	{
	}

	/** Copy constructor */
	FORCEINLINE CStrA(const CStrA &src) : base(src)
	{
		base::FixTail();
	}

	/** Take over ownership constructor */
	FORCEINLINE CStrA(const OnTransfer& ot)
		: base(ot)
	{
	}

	/** Grow the actual buffer and fix the trailing zero at the end. */
	FORCEINLINE char *GrowSizeNC(uint count)
	{
		char *ret = base::GrowSizeNC(count);
		base::FixTail();
		return ret;
	}

	/** Append zero-ended C string. */
	FORCEINLINE void AppendStr(const char *str)
	{
		if (!StrEmpty(str)) {
			base::AppendRaw(str, strlen(str));
			base::FixTail();
		}
	}

	/** Append another CStrA. */
	FORCEINLINE void Append(const CStrA &src)
	{
		if (src.Length() > 0) {
			base::AppendRaw(src);
			base::FixTail();
		}
	}

	/** Assignment from C string. */
	FORCEINLINE CStrA &operator = (const char *src)
	{
		base::Clear();
		AppendStr(src);
		return *this;
	}

	/** Assignment from another CStrA. */
	FORCEINLINE CStrA &operator = (const CStrA &src)
	{
		if (&src != this) {
			base::Clear();
			base::AppendRaw(src.Data(), src.Size());
			base::FixTail();
		}
		return *this;
	}

	/** Lower-than operator (to support stl collections) */
	FORCEINLINE bool operator < (const CStrA &other) const
	{
		return strcmp(base::Data(), other.Data()) < 0;
	}

	/** Add formated string (like vsprintf) at the end of existing contents. */
	int AddFormatL(const char *format, va_list args)
	{
		size_t addSize = max<size_t>(strlen(format), 16);
		addSize += addSize / 2;
		int ret;
		int err = 0;
		for (;;) {
			char *buf = MakeFreeSpace(addSize);
			ret = vsnprintf(buf, base::GetReserve(), format, args);
			if (ret >= (int)base::GetReserve()) {
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
	int CDECL WARN_FORMAT(2, 3) AddFormat(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		int ret = AddFormatL(format, args);
		va_end(args);
		return ret;
	}

	/** Assign formated string (like sprintf). */
	int CDECL WARN_FORMAT(2, 3) Format(const char *format, ...)
	{
		base::Free();
		va_list args;
		va_start(args, format);
		int ret = AddFormatL(format, args);
		va_end(args);
		return ret;
	}
};

#endif /* STR_HPP */
