/* $Id$ */

/** @file stdafx.h */

#ifndef STDAFX_H
#define STDAFX_H

/* It seems that we need to include stdint.h before anything else
 * We need INT64_MAX, which for most systems comes from stdint.h. However, MSVC
 * does not have stdint.h and apparently neither does MorphOS, so define
 * INT64_MAX for them ourselves.
 * Sometimes OSX headers manages to include stdint.h before this but without
 * __STDC_LIMIT_MACROS so it will be without INT64_*. We need to define those
 * too if this is the case. */
#if !defined(_MSC_VER) && !defined( __MORPHOS__) && !defined(_STDINT_H_)
	#if defined(SUNOS)
		/* SunOS/Solaris does not have stdint.h, but inttypes.h defines everything
		 * stdint.h defines and we need. */
		#include <inttypes.h>
	# else
		#define __STDC_LIMIT_MACROS
		#include <stdint.h>
	#endif
#else
	#define INT64_MAX 9223372036854775807LL
	#define INT64_MIN (-INT64_MAX - 1)
#endif

#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <climits>

/* MacOS X will use an NSAlert to display failed assertaions since they're lost unless running from a terminal
 * strgen always runs from terminal and don't need a window for asserts */
#if !defined(__APPLE__) || defined(STRGEN)
	#include <cassert>
#else
	#include "os/macosx/macos.h"
#endif

#if defined(UNIX) || defined(__MINGW32__)
	#include <sys/types.h>
#endif

#if defined(__OS2__)
	#include <types.h>
	#define strcasecmp stricmp
#endif

#if defined(PSP)
	#include <psptypes.h>
	#include <pspdebug.h>
	#include <pspthreadman.h>
#endif

#if defined(__BEOS__)
	#include <SupportDefs.h>
#endif

#if defined(SUNOS)
	#include <alloca.h>
#endif

#if defined(__MORPHOS__)
	/* MorphOS defines certain Amiga defines per default, we undefine them
	 * here to make the rest of source less messy and more clear what is
	 * required for morphos and what for AmigaOS */
	#if defined(amigaos)
		#undef amigaos
	#endif
	#if defined(__amigaos__)
		#undef __amigaos__
	# endif
	#if defined(__AMIGA__)
		#undef __AMIGA__
	#endif
	#if defined(AMIGA)
		#undef AMIGA
	#endif
	#if defined(amiga)
		#undef amiga
	#endif
	/* Act like we already included this file, as it somehow gives linkage problems
	 *  (mismatch linkage of C++ and C between this include and unistd.h). */
	#define CLIB_USERGROUP_PROTOS_H
#endif /* __MORPHOS__ */

#if defined(__APPLE__)
	#include "os/macosx/osx_stdafx.h"
	/* Make endian swapping use Apple's macros to increase speed (since it will use hardware swapping if available)
	 * Even though they should return uint16 and uint32, we get warnings if we don't cast those (why?) */
	#define BSWAP32(x) ((uint32)Endian32_Swap(x))
	#define BSWAP16(x) ((uint16)Endian16_Swap(x))
#else
	#define BSWAP32(x) ((((x) >> 24) & 0xFF) | (((x) >> 8) & 0xFF00) | (((x) << 8) & 0xFF0000) | (((x) << 24) & 0xFF000000))
	#define BSWAP16(x) ((x) >> 8 | (x) << 8)
#endif /* __APPLE__ */

#if defined(PSP)
	/* PSP can only have 10 file-descriptors open at any given time, but this
	 *  switch only limits reads via the Fio system. So keep 2 fds free for things
	 *  like saving a game. */
	#define LIMITED_FDS 8
	#define printf pspDebugScreenPrintf
#endif /* PSP */

/* by default we use [] var arrays */
#define VARARRAY_SIZE

/* Stuff for GCC */
#if defined(__GNUC__)
	#define NORETURN __attribute__ ((noreturn))
	#define FORCEINLINE inline
	#define CDECL
	#define __int64 long long
	#define GCC_PACK __attribute__((packed))

	#if (__GNUC__ == 2)
		#undef VARARRAY_SIZE
		#define VARARRAY_SIZE 0
	#endif
#endif /* __GNUC__ */

#if defined(__WATCOMC__)
	#define NORETURN
	#define FORCEINLINE inline
	#define CDECL
	#define GCC_PACK
	#include <malloc.h>
#endif /* __WATCOMC__ */

#if defined(__MINGW32__) || defined(__CYGWIN__)
	#include <malloc.h> // alloca()
#endif

/* Stuff for MSVC */
#if defined(_MSC_VER)
	#pragma once
	/* Define a win32 target platform, to override defaults of the SDK
	 * We need to define NTDDI version for Vista SDK, but win2k is minimum */
	#define NTDDI_VERSION NTDDI_WIN2K // Windows 2000
	#define _WIN32_WINNT 0x0500       // Windows 2000
	#define _WIN32_WINDOWS 0x400      // Windows 95
	#if !defined(WINCE)
		#define WINVER 0x0400     // Windows NT 4.0 / Windows 95
	#endif
	#define _WIN32_IE_ 0x0401         // 4.01 (win98 and NT4SP5+)

	#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
	#pragma warning(disable: 4244)  // 'conversion' conversion from 'type1' to 'type2', possible loss of data
	#pragma warning(disable: 4761)  // integral size mismatch in argument : conversion supplied
	#pragma warning(disable: 4200)  // nonstandard extension used : zero-sized array in struct/union

	#if (_MSC_VER >= 1400)                   // MSVC 2005 safety checks
		#pragma warning(disable: 4996)   // 'strdup' was declared deprecated
		#define _CRT_SECURE_NO_DEPRECATE // all deprecated 'unsafe string functions
		#pragma warning(disable: 6308)   // code analyzer: 'realloc' might return null pointer: assigning null pointer to 't_ptr', which is passed as an argument to 'realloc', will cause the original memory block to be leaked
		#pragma warning(disable: 6011)   // code analyzer: Dereferencing NULL pointer 'pfGetAddrInfo': Lines: 995, 996, 998, 999, 1001
		#pragma warning(disable: 6326)   // code analyzer: potential comparison of a constant with another constant
		#pragma warning(disable: 6031)   // code analyzer: Return value ignored: 'ReadFile'
		#pragma warning(disable: 6255)   // code analyzer: _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead
		#pragma warning(disable: 6246)   // code analyzer: Local declaration of 'statspec' hides declaration of the same name in outer scope. For additional information, see previous declaration at ...
	#else /* _MSC_VER >= 1400  ( <1400 for MSVC2003) */
		#pragma warning(disable: 4288)   // nonstandard extension used : 'y' : loop control variable declared in the for-loop is used outside the for-loop scope; it conflicts with the declaration in the outer scope
		#pragma warning(disable: 4292)   // compiler limit : terminating debug information emission for enum 'StringIdEnum' with member 'STR_801D_COAL_CAR'
	#endif /* _MSC_VER >= 1400 */

	#include <malloc.h> // alloca()
	#define NORETURN __declspec(noreturn)
	#define FORCEINLINE __forceinline
	#define inline _inline

	#if !defined(WINCE)
		#define CDECL _cdecl
	#endif

	int CDECL snprintf(char *str, size_t size, const char *format, ...);
	#if (_MSC_VER < 1400) || defined(WINCE)
		int CDECL vsnprintf(char *str, size_t size, const char *format, va_list ap);
	#endif

	#if defined(WIN32) && !defined(_WIN64) && !defined(WIN64)
		#if !defined(_W64)
			#define _W64
		#endif

		typedef _W64 int INT_PTR, *PINT_PTR;
		typedef _W64 unsigned int UINT_PTR, *PUINT_PTR;
	#endif /* WIN32 && !_WIN64 && !WIN64 */

	#define GCC_PACK

	/* This is needed to zlib uses the stdcall calling convention on visual studio */
	#if defined(WITH_ZLIB) || defined(WITH_PNG)
		#if !defined(ZLIB_WINAPI)
			#define ZLIB_WINAPI
		#endif
	#endif

	#if defined(WINCE)
		#define strcasecmp _stricmp
		#define strncasecmp _strnicmp
		#undef DEBUG
	#else
		#define strcasecmp stricmp
		#define strncasecmp strnicmp
	#endif

	void SetExceptionString(const char* s, ...);

	#if defined(NDEBUG) && defined(WITH_ASSERT)
		#undef assert
		#define assert(expression) if (!(expression)) { SetExceptionString("Assertion failed at %s:%d: %s", __FILE__, __LINE__, #expression); *(byte*)0 = 0; }
	#endif
#endif /* defined(_MSC_VER) */

#if defined(WINCE)
	#define strdup _strdup
#endif /* WINCE */

/* NOTE: the string returned by these functions is only valid until the next
 * call to the same function and is not thread- or reentrancy-safe */
#if !defined(STRGEN)
	#if defined(WIN32) || defined(WIN64)
		char *getcwd(char *buf, size_t size);
		#include <tchar.h>

		/* XXX - WinCE without MSVCRT doesn't support wfopen, so it seems */
		#if !defined(WINCE)
			#define fopen(file, mode) _tfopen(OTTD2FS(file), _T(mode))
		#endif /* WINCE */

		const char *FS2OTTD(const TCHAR *name);
		const TCHAR *OTTD2FS(const char *name);
	#else
		#define fopen(file, mode) fopen(OTTD2FS(file), mode)
		const char *FS2OTTD(const char *name);
		const char *OTTD2FS(const char *name);
	#endif /* WIN32 */
#endif /* STRGEN */

/* Windows has always LITTLE_ENDIAN */
#if defined(WIN32) || defined(__OS2__) || defined(WIN64)
	#define TTD_LITTLE_ENDIAN
#elif defined(TESTING)
	/* Do noting  */
#else
	/* Else include endian[target/host].h, which has the endian-type, autodetected by the Makefile */
	#if defined(STRGEN)
		#include "endian_host.h"
	#else
		#include "endian_target.h"
	#endif
#endif /* WIN32 || __OS2__ || WIN64 */

#if defined(WIN32) || defined(WIN64) || defined(__OS2__) && !defined(__INNOTEK_LIBC__)
	#define PATHSEP "\\"
	#define PATHSEPCHAR '\\'
#else
	#define PATHSEP "/"
	#define PATHSEPCHAR '/'
#endif

typedef unsigned char byte;

/* This is already defined in unix, but not in QNX Neutrino (6.x)*/
#if (!defined(UNIX) && !defined(__CYGWIN__) && !defined(__BEOS__) && !defined(__MORPHOS__)) || defined(__QNXNTO__)
	typedef unsigned int uint;
#endif

#if !defined(__BEOS__) /* Already defined on BEOS */
	typedef unsigned char    uint8;
	typedef   signed char     int8;
	typedef unsigned short   uint16;
	typedef   signed short    int16;
	typedef unsigned int     uint32;
	typedef   signed int      int32;
	typedef unsigned __int64 uint64;
	typedef   signed __int64  int64;
#endif

#if defined(ARM) || defined(__arm__) || defined(__alpha__)
	#define OTTD_ALIGNMENT
#endif

/* Setup alignment and conversion macros */
#if defined(TTD_BIG_ENDIAN)
	#define TO_BE32X(x)  (x)
	#define FROM_BE32(x) (x)
	#define TO_BE32(x)   (x)
	#define FROM_BE16(x) (x)
	#define TO_BE16(x)   (x)
	#define TO_LE32X(x)  BSWAP32(x)
	static inline uint32 FROM_LE32(uint32 x) { return BSWAP32(x); }
	static inline uint32 TO_LE32(uint32 x)   { return BSWAP32(x); }
	static inline uint16 FROM_LE16(uint16 x) { return BSWAP16(x); }
	static inline uint16 TO_LE16(uint16 x)   { return BSWAP16(x); }
#else
	#define TO_BE32X(x)  BSWAP32(x)
	static inline uint32 FROM_BE32(uint32 x) { return BSWAP32(x); }
	static inline uint32 TO_BE32(uint32 x)   { return BSWAP32(x); }
	static inline uint16 FROM_BE16(uint16 x) { return BSWAP16(x); }
	static inline uint16 TO_BE16(uint16 x)   { return BSWAP16(x); }
	#define TO_LE32X(x)  (x)
	#define FROM_LE32(x) (x)
	#define TO_LE32(x)   (x)
	#define FROM_LE16(x) (x)
	#define TO_LE16(x)   (x)
#endif /* TTD_BIG_ENDIAN */

#if !defined(WITH_PERSONAL_DIR)
	#define PERSONAL_DIR ""
#endif

/* Compile time assertions */
#if defined(__OS2__)
	#define assert_compile(expr)
#else
	#define assert_compile(expr) extern "C" void __ct_assert__(int a[1 - 2 * !(expr)])
#endif /* __OS2__ */

assert_compile(sizeof(uint32) == 4);
assert_compile(sizeof(uint16) == 2);
assert_compile(sizeof(uint8)  == 1);

#define lengthof(x) (sizeof(x)/sizeof(x[0]))
#define endof(x) (&x[lengthof(x)])
#define lastof(x) (&x[lengthof(x) - 1])

#define cpp_offsetof(s,m)   (((size_t)&reinterpret_cast<const volatile char&>((((s*)(char*)8)->m))) - 8)
#if !defined(offsetof)
	#define offsetof(s,m) cpp_offsetof(s, m)
#endif /* offsetof */


/* take care of some name clashes on MacOS */
#if defined(__APPLE__)
	#define GetString OTTD_GetString
	#define DrawString OTTD_DrawString
	#define CloseConnection OTTD_CloseConnection
#endif /* __APPLE__ */

#if !defined(STRGEN)
	/* In strgen error is not fatal and returns */
	void NORETURN CDECL error(const char *str, ...);
#else
	void CDECL error(const char *str, ...);
#endif

#define NOT_REACHED() error("NOT_REACHED triggered at line %i of %s", __LINE__, __FILE__)

#if defined(MORPHOS)
	/* MorphOS doesn't have C++ conformant _stricmp... */
	#define _stricmp stricmp
#elif defined(OPENBSD)
	/* OpenBSD uses strcasecmp(3) */
	#define _stricmp strcasecmp
#endif

#if !defined(MORPHOS) && !defined(OPENBSD)
	/* MorphOS & OpenBSD don't know wchars, the rest does :( */
	#define HAS_WCHAR
#endif /* !defined(MORPHOS) && !defined(OPENBSD) */

#if !defined(MAX_PATH)
	#define MAX_PATH 260
#endif

/**
 * The largest value that can be entered in a variable
 * @param type the type of the variable
 */
#define MAX_UVALUE(type) ((type)~(type)0)

#endif /* STDAFX_H */
