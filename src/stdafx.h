/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file stdafx.h Definition of base types and functions in a cross-platform compatible way. */

#ifndef STDAFX_H
#define STDAFX_H

#if defined(_WIN32)
	/* MinGW defaults to Windows 7 if none of these are set, and they must be set before any MinGW header is included */
#	define NTDDI_VERSION NTDDI_WINXP // Windows XP
#	define _WIN32_WINNT 0x501        // Windows XP
#	define _WIN32_WINDOWS 0x501      // Windows XP
#	define WINVER 0x0501             // Windows XP
#	define _WIN32_IE_ 0x0600         // 6.0 (XP+)
#endif

#ifdef _MSC_VER
	/* Stop Microsoft (and clang-cl) compilers from complaining about potentially-unsafe/potentially-non-standard functions */
#	define _CRT_SECURE_NO_DEPRECATE
#	define _CRT_SECURE_NO_WARNINGS
#	define _CRT_NONSTDC_NO_WARNINGS
#endif

#if defined(__APPLE__)
#	include "os/macosx/osx_stdafx.h"
#endif /* __APPLE__ */

#if defined(__HAIKU__)
#	include <SupportDefs.h>
#	include <unistd.h>
#	define _DEFAULT_SOURCE
#	define _GNU_SOURCE
#endif

/* It seems that we need to include stdint.h before anything else
 * We need INT64_MAX, which for most systems comes from stdint.h. However, MSVC
 * does not have stdint.h.
 * For OSX the inclusion is already done in osx_stdafx.h. */
#if !defined(__APPLE__) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
#	define __STDC_LIMIT_MACROS
#	include <stdint.h>
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <deque>
#include <exception>
#include <functional>
#include <iterator>
#include <list>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#if defined(UNIX) || defined(__MINGW32__)
#	include <sys/types.h>
#endif

/* Stuff for GCC */
#if defined(__GNUC__) || (defined(__clang__) && !defined(_MSC_VER))
#	define NORETURN __attribute__ ((noreturn))
#	define CDECL
#	if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
#		define FINAL final
#	else
#		define FINAL
#	endif

	/* Use fallthrough attribute where supported */
#	if __GNUC__ >= 7
#		if __cplusplus > 201402L // C++17
#			define FALLTHROUGH [[fallthrough]]
#		else
#			define FALLTHROUGH __attribute__((fallthrough))
#		endif
#	else
#		define FALLTHROUGH
#	endif
#endif /* __GNUC__ || __clang__ */

#if __GNUC__ > 11 || (__GNUC__ == 11 && __GNUC_MINOR__ >= 1)
#      define NOACCESS(args) __attribute__ ((access (none, args)))
#else
#      define NOACCESS(args)
#endif

/* [[nodiscard]] on constructors doesn't work in GCC older than 10.1. */
#if __GNUC__ < 10 || (__GNUC__ == 10 && __GNUC_MINOR__ < 1)
#      define NODISCARD
#else
#      define NODISCARD [[nodiscard]]
#endif

#if defined(_WIN32)
#	define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
#endif

#if defined(_MSC_VER)
	// See https://learn.microsoft.com/en-us/cpp/cpp/empty-bases?view=msvc-170
#	define EMPTY_BASES __declspec(empty_bases)
#else
#	define EMPTY_BASES
#endif

/* Stuff for MSVC */
#if defined(_MSC_VER)
#	pragma once
#	define NOMINMAX                // Disable min/max macros in windows.h.

#	pragma warning(disable: 4244)  // 'conversion' conversion from 'type1' to 'type2', possible loss of data
#	pragma warning(disable: 4761)  // integral size mismatch in argument : conversion supplied
#	pragma warning(disable: 4200)  // nonstandard extension used : zero-sized array in struct/union
#	pragma warning(disable: 4355)  // 'this' : used in base member initializer list

#	if (_MSC_VER < 1400)                   // MSVC 2005 safety checks
#		error "Only MSVC 2005 or higher are supported. MSVC 2003 and earlier are not! Upgrade your compiler."
#	endif /* (_MSC_VER < 1400) */
#	pragma warning(disable: 4291)   // no matching operator delete found; memory will not be freed if initialization throws an exception (reason: our overloaded functions never throw an exception)
#	pragma warning(disable: 4996)   // 'function': was declared deprecated
#	pragma warning(disable: 6308)   // code analyzer: 'realloc' might return null pointer: assigning null pointer to 't_ptr', which is passed as an argument to 'realloc', will cause the original memory block to be leaked
#	pragma warning(disable: 6011)   // code analyzer: Dereferencing NULL pointer 'pfGetAddrInfo': Lines: 995, 996, 998, 999, 1001
#	pragma warning(disable: 6326)   // code analyzer: potential comparison of a constant with another constant
#	pragma warning(disable: 6031)   // code analyzer: Return value ignored: 'ReadFile'
#	pragma warning(disable: 6246)   // code analyzer: Local declaration of 'statspec' hides declaration of the same name in outer scope. For additional information, see previous declaration at ...

#	if (_MSC_VER == 1500)           // Addresses item #13 on http://blogs.msdn.com/b/vcblog/archive/2008/08/11/tr1-fixes-in-vc9-sp1.aspx, for Visual Studio 2008
#		define _DO_NOT_DECLARE_INTERLOCKED_INTRINSICS_IN_MEMORY
#		include <intrin.h>
#	endif

#	define NORETURN __declspec(noreturn)
#	if (_MSC_VER < 1900)
#		define inline __forceinline
#	endif

#	define CDECL _cdecl
#	define FINAL final

	/* fallthrough attribute, VS 2017 */
#	if (_MSC_VER >= 1910) || defined(__clang__)
#		define FALLTHROUGH [[fallthrough]]
#	else
#		define FALLTHROUGH
#	endif

#	if defined(_WIN32) && !defined(_WIN64)
#		if !defined(_W64)
#			define _W64
#		endif

		typedef _W64 int INT_PTR, *PINT_PTR;
		typedef _W64 unsigned int UINT_PTR, *PUINT_PTR;
#	endif /* _WIN32 && !_WIN64 */

#	if defined(_WIN64)
#		define fseek _fseeki64
#	endif /* _WIN64 */

	/* zlib from vcpkg use cdecl calling convention without enforcing it in the headers */
#	if defined(WITH_ZLIB)
#		if !defined(ZEXPORT)
#			define ZEXPORT CDECL
#		endif
#	endif

	/* freetype from vcpkg use cdecl calling convention without enforcing it in the headers */
#	if defined(WITH_FREETYPE)
#		if !defined(FT_EXPORT)
#			define FT_EXPORT( x )  extern "C"  x CDECL
#		endif
#	endif

	/* liblzma from vcpkg (before 5.2.4-2) used to patch lzma.h to define LZMA_API_STATIC for static builds */
#	if defined(WITH_LIBLZMA)
#		if !defined(LZMA_API_STATIC)
#			define LZMA_API_STATIC
#		endif
#	endif

	/* MSVC doesn't have these :( */
#	define S_ISDIR(mode) (mode & S_IFDIR)
#	define S_ISREG(mode) (mode & S_IFREG)

#endif /* defined(_MSC_VER) */

#if !defined(STRGEN) && !defined(SETTINGSGEN)
#	if defined(_WIN32)
		char *getcwd(char *buf, size_t size);
#		include <io.h>
#		include <tchar.h>

#		define fopen(file, mode) _wfopen(OTTD2FS(file).c_str(), _T(mode))
#		define unlink(file) _wunlink(OTTD2FS(file).c_str())

		std::string FS2OTTD(const std::wstring &name);
		std::wstring OTTD2FS(const std::string &name);
#	elif defined(WITH_ICONV)
#		define fopen(file, mode) fopen(OTTD2FS(file).c_str(), mode)
		std::string FS2OTTD(const std::string &name);
		std::string OTTD2FS(const std::string &name);
#	else
		// no override of fopen() since no transformation is required of the filename
		template <typename T> std::string FS2OTTD(T name) { return name; }
		template <typename T> std::string OTTD2FS(T name) { return name; }
#	endif /* _WIN32 or WITH_ICONV */
#endif /* STRGEN || SETTINGSGEN */

#if defined(_WIN32)
#	define PATHSEP "\\"
#	define PATHSEPCHAR '\\'
#else
#	define PATHSEP "/"
#	define PATHSEPCHAR '/'
#endif

#if defined(_MSC_VER)
#	define PACK_N(type_dec, n) __pragma(pack(push, n)) type_dec; __pragma(pack(pop))
#elif defined(__MINGW32__)
#	define PRAGMA(x) _Pragma(#x)
#	define PACK_N(type_dec, n) PRAGMA(pack(push, n)) type_dec; PRAGMA(pack(pop))
#else
#	define PACK_N(type_dec, n) type_dec __attribute__((__packed__, aligned(n)))
#endif
#define PACK(type_dec) PACK_N(type_dec, 1)

/*
 * When making a (pure) debug build, the compiler will by default disable
 * inlining of functions. This has a detremental effect on the performance of
 * debug builds, especially when more and more trivial (wrapper) functions get
 * added to the code base.
 * Take for example the savegame called "Wentbourne", when running this game
 * for 100 ticks with the null video driver a number of fairly trivial
 * functions show up on top. The most common one is the implicit conversion
 * operator of TileIndex to unsigned int, which takes up over 5% of the total
 * run time and functionally does absolutely nothing. The remaining functions
 * for the top 5 are GB, GetTileType, Map::Size and IsTileType to a total of
 * about 12.5% of the game's total run time.
 * It is possible to still force inlining in the most commonly used compilers,
 * but that is at the cost of some problems with debugging due to the forced
 * inlining. However, the performance benefit can be enormous; when forcing
 * inlining for the previously mentioned top 5, the debug build ran about 15%
 * quicker.
 * The following debug_inline annotation may be added to functions comply
 * with the following preconditions:
 *  1: the function takes more than 0.5% of a profiled debug runtime
 *  2: the function does not modify the game state
 *  3: the function does not contain selection or iteration statements,
 *     i.e. no if, switch, for, do, while, etcetera.
 *  4: the function is one line of code, excluding assertions.
 *  5: the function is defined in a header file.
 * The debug_inline annotation must be placed in front of the function, i.e.
 * before the optional static or constexpr modifier.
 */
#if !defined(_DEBUG) || defined(NO_DEBUG_INLINE)
/*
 * Do not force inlining when not in debug. This way we do not work against
 * any carefully designed compiler optimizations.
 */
#define debug_inline inline
#elif defined(__clang__) || defined(__GNUC__)
#define debug_inline [[gnu::always_inline]] inline
#else
/*
 * MSVC explicitly disables inlining, even forced inlining, in debug builds
 * so __forceinline makes no difference compared to inline. Other unknown
 * compilers can also just fallback to a normal inline.
 */
#define debug_inline inline
#endif

typedef uint8_t byte;

/* This is already defined in unix, but not in QNX Neutrino (6.x) or Cygwin. */
#if (!defined(UNIX) && !defined(__HAIKU__)) || defined(__QNXNTO__) || defined(__CYGWIN__)
	typedef unsigned int uint;
#endif

#if !defined(WITH_PERSONAL_DIR)
#	define PERSONAL_DIR ""
#endif

/* Define the the platforms that use XDG */
#if defined(WITH_PERSONAL_DIR) && defined(UNIX) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#	define USE_XDG
#endif

/* Check if the types have the bitsizes like we are using them */
static_assert(sizeof(uint64_t) == 8);
static_assert(sizeof(uint32_t) == 4);
static_assert(sizeof(uint16_t) == 2);
static_assert(sizeof(uint8_t)  == 1);
static_assert(SIZE_MAX >= UINT32_MAX);

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#define M_PI   3.14159265358979323846
#endif /* M_PI_2 */

/**
 * Return the length of an fixed size array.
 * Unlike sizeof this function returns the number of elements
 * of the given type.
 *
 * @param x The pointer to the first element of the array
 * @return The number of elements
 */
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/**
 * Get the end element of an fixed size array.
 *
 * @param x The pointer to the first element of the array
 * @return The pointer past to the last element of the array
 */
#define endof(x) (&x[lengthof(x)])

/**
 * Get the last element of an fixed size array.
 *
 * @param x The pointer to the first element of the array
 * @return The pointer to the last element of the array
 */
#define lastof(x) (&x[lengthof(x) - 1])

/**
 * Gets the size of a variable within a class.
 * @param base     The class the variable is in.
 * @param variable The variable to get the size of.
 * @return the size of the variable
 */
#define cpp_sizeof(base, variable) (sizeof(std::declval<base>().variable))

/**
 * Gets the length of an array variable within a class.
 * @param base     The class the variable is in.
 * @param variable The array variable to get the size of.
 * @return the length of the array
 */
#define cpp_lengthof(base, variable) (cpp_sizeof(base, variable) / cpp_sizeof(base, variable[0]))


/* take care of some name clashes on MacOS */
#if defined(__APPLE__)
#	define GetString OTTD_GetString
#	define DrawString OTTD_DrawString
#	define CloseConnection OTTD_CloseConnection
#endif /* __APPLE__ */

#if defined(__GNUC__) || defined(__clang__)
#	define likely(x)     __builtin_expect(!!(x), 1)
#	define unlikely(x)   __builtin_expect(!!(x), 0)
#	define GNU_TARGET(x) [[gnu::target(x)]]
#else
#	define likely(x)     (x)
#	define unlikely(x)   (x)
#	define GNU_TARGET(x)
#endif /* __GNUC__ || __clang__ */

/* For the FMT library we only want to use the headers, not link to some library. */
#define FMT_HEADER_ONLY

void NORETURN NotReachedError(int line, const char *file);
void NORETURN AssertFailedError(int line, const char *file, const char *expression);
#define NOT_REACHED() NotReachedError(__LINE__, __FILE__)

/* For non-debug builds with assertions enabled use the special assertion handler. */
#if defined(NDEBUG) && defined(WITH_ASSERT)
#	undef assert
#	define assert(expression) if (unlikely(!(expression))) AssertFailedError(__LINE__, __FILE__, #expression);
#endif

/* Define JSON_ASSERT, which is used by nlohmann-json. Otherwise the header-file
 * will re-include assert.h, and reset the assert macro. */
#define JSON_ASSERT(x) assert(x)

#if defined(MAX_PATH)
	/* It's already defined, no need to override */
#elif defined(PATH_MAX) && PATH_MAX > 0
	/* Use the value from PATH_MAX, if it exists */
#	define MAX_PATH PATH_MAX
#else
	/* If all else fails, hardcode something :( */
#	define MAX_PATH 260
#endif

/**
 * Version of the standard free that accepts const pointers.
 * @param ptr The data to free.
 */
static inline void free(const void *ptr)
{
	free(const_cast<void *>(ptr));
}

/**
 * The largest value that can be entered in a variable
 * @param type the type of the variable
 */
#define MAX_UVALUE(type) ((type)~(type)0)

#if defined(_MSC_VER) && !defined(_DEBUG)
#	define IGNORE_UNINITIALIZED_WARNING_START __pragma(warning(push)) __pragma(warning(disable:4700))
#	define IGNORE_UNINITIALIZED_WARNING_STOP __pragma(warning(pop))
#elif defined(__GNUC__) && !defined(_DEBUG)
#	define HELPER0(x) #x
#	define HELPER1(x) HELPER0(GCC diagnostic ignored x)
#	define HELPER2(y) HELPER1(#y)
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#	define IGNORE_UNINITIALIZED_WARNING_START \
		_Pragma("GCC diagnostic push") \
		_Pragma(HELPER2(-Wuninitialized)) \
		_Pragma(HELPER2(-Wmaybe-uninitialized))
#	define IGNORE_UNINITIALIZED_WARNING_STOP _Pragma("GCC diagnostic pop")
#endif
#endif

#ifndef IGNORE_UNINITIALIZED_WARNING_START
#	define IGNORE_UNINITIALIZED_WARNING_START
#	define IGNORE_UNINITIALIZED_WARNING_STOP
#endif

#endif /* STDAFX_H */
