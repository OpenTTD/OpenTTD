/* $Id$ */

#ifndef STDAFX_H
#define STDAFX_H

/* It seems that we need to include stdint.h before anything else
 * We need INT64_MAX, which for most systems comes from stdint.h. However, MSVC
 * does not have stdint.h and apparently neither does MorphOS, so define
 * INT64_MAX for them ourselves. */
#if !defined(_MSC_VER) && !defined( __MORPHOS__)
# if defined (SUNOS)
/* SunOS/Solaris does not have stdint.h, but inttypes.h defines everything
 * stdint.h defines and we need. */
#  include <inttypes.h>
# else
#  define __STDC_LIMIT_MACROS
#  include <stdint.h>
# endif
#else
# define INT64_MAX 9223372036854775807LL
#endif

#include <limits>

#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <climits>

/* MacOS X will use an NSAlert to display failed assertaions since they're lost unless running from a terminal
 * strgen always runs from terminal and don't need a window for asserts */
#if !defined(__APPLE__) || defined(STRGEN)
# include <cassert>
#else
# include "os/macosx/macos.h"
#endif

#if defined(UNIX) || defined(__MINGW32__)
# include <sys/types.h>
#endif

#if defined(__OS2__)
# include <types.h>
# define strcasecmp stricmp
#endif

#if defined(PSP)
# include <psptypes.h>
# include <pspdebug.h>
# include <pspthreadman.h>
#endif /* PSP */

#ifdef __BEOS__
# include <SupportDefs.h>
#endif

#ifdef SUNOS
# include <alloca.h>
#endif

#ifdef __MORPHOS__
/* MorphOS defines certain Amiga defines per default, we undefine them
 * here to make the rest of source less messy and more clear what is
 * required for morphos and what for AmigaOS */
# ifdef amigaos
#  undef amigaos
# endif
# ifdef __amigaos__
#  undef __amigaos__
# endif
# ifdef __AMIGA__
#  undef __AMIGA__
# endif
# ifdef AMIGA
#  undef AMIGA
# endif
# ifdef amiga
#  undef amiga
# endif
#endif /* __MORPHOS__ */

#ifdef __APPLE__
# include "os/macosx/osx_stdafx.h"
/* Make endian swapping use Apple's macros to increase speed (since it will use hardware swapping if available)
 * Even though they should return uint16 and uint32, we get warnings if we don't cast those (why?) */
# define BSWAP32(x) ((uint32)Endian32_Swap(x))
# define BSWAP16(x) ((uint16)Endian16_Swap(x))
#else
# define BSWAP32(x) ((((x) >> 24) & 0xFF) | (((x) >> 8) & 0xFF00) | (((x) << 8) & 0xFF0000) | (((x) << 24) & 0xFF000000))
# define BSWAP16(x) ((x) >> 8 | (x) << 8)
#endif /* __APPLE__ */

#if defined(PSP)
/* PSP can only have 10 file-descriptors open at any given time, but this
 *  switch only limits reads via the Fio system. So keep 2 fds free for things
 *  like saving a game. */
# define LIMITED_FDS 8
# define printf pspDebugScreenPrintf
#endif /* PSP */

/* by default we use [] var arrays */
#define VARARRAY_SIZE


/* Stuff for GCC */
#if defined(__GNUC__)
# define NORETURN __attribute__ ((noreturn))
# define FORCEINLINE inline
# define CDECL
# define __int64 long long
# define NOT_REACHED() assert(0)
# define GCC_PACK __attribute__((packed))

# if (__GNUC__ == 2)
#  undef VARARRAY_SIZE
#  define VARARRAY_SIZE 0
# endif
#endif /* __GNUC__ */

#if defined(__WATCOMC__)
# define NORETURN
# define FORCEINLINE inline
# define CDECL
# define NOT_REACHED() assert(0)
# define GCC_PACK
# include <malloc.h>
#endif /* __WATCOMC__ */

#if defined(__MINGW32__) || defined(__CYGWIN__)
# include <malloc.h> // alloca()
#endif

/* Stuff for MSVC */
#if defined(_MSC_VER)
# pragma once
/* Define a win32 target platform, to override defaults of the SDK
 * We need to define NTDDI version for Vista SDK, but win2k is minimum */
# define NTDDI_VERSION NTDDI_WIN2K // Windows 2000
# define _WIN32_WINNT 0x0500       // Windows 2000

# define _WIN32_WINDOWS 0x400      // Windows 95
# define WINVER 0x0400             // Windows NT 4.0 / Windows 95
# define _WIN32_IE_ 0x0401         // 4.01 (win98 and NT4SP5+)

# define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
# pragma warning(disable: 4244)  // 'conversion' conversion from 'type1' to 'type2', possible loss of data
# pragma warning(disable: 4761)  // integral size mismatch in argument : conversion supplied
# pragma warning(disable: 4200)  // nonstandard extension used : zero-sized array in struct/union

# if _MSC_VER >= 1400              // MSVC 2005 safety checks
#  pragma warning(disable: 4996)   // 'strdup' was declared deprecated
#  define _CRT_SECURE_NO_DEPRECATE // all deprecated 'unsafe string functions
# else /* _MSC_VER >= 1400  ( <1400 for MSVC2003) */
#  pragma warning(disable: 4292)   // compiler limit : terminating debug information emission for enum 'StringIdEnum' with member 'STR_801D_COAL_CAR'
# endif /* _MSC_VER >= 1400 */

# include <malloc.h> // alloca()
# define NORETURN __declspec(noreturn)
# define FORCEINLINE __forceinline
# define inline _inline
# define CDECL _cdecl
# if defined(_DEBUG)
#  define NOT_REACHED() assert(0)
# else
#  define NOT_REACHED() _assume(0)
# endif /* _DEBUG */
  int CDECL snprintf(char *str, size_t size, const char *format, ...);
# if _MSC_VER < 1400
   int CDECL vsnprintf(char *str, size_t size, const char *format, va_list ap);
# endif

# if defined(WIN32) && !defined(_WIN64) && !defined(WIN64)
#  ifndef _W64
#   define _W64
#  endif
   typedef _W64 int INT_PTR, *PINT_PTR;
   typedef _W64 unsigned int UINT_PTR, *PUINT_PTR;
# endif /* WIN32 && !_WIN64 && !WIN64 */

# define GCC_PACK

/* This is needed to zlib uses the stdcall calling convention on visual studio */
# if defined(WITH_ZLIB) || defined(WITH_PNG)
#  ifndef ZLIB_WINAPI
#   define ZLIB_WINAPI
#  endif
# endif

# define strcasecmp stricmp
# define strncasecmp strnicmp
/* suppress: warning C4005: 'offsetof' : macro redefinition (VC8) */
#endif /* defined(_MSC_VER) */

#if defined(WINCE)
# define strdup _strdup
#endif /* WINCE */

/* NOTE: the string returned by these functions is only valid until the next
 * call to the same function and is not thread- or reentrancy-safe */
#if !defined(STRGEN)
# if defined(WIN32) || defined(WIN64)
#  include <tchar.h>
/* XXX - WinCE without MSVCRT doesn't support wfopen, so it seems */
#  if !defined(WINCE)
#   define fopen(file, mode) _tfopen(OTTD2FS(file), _T(mode))
#  endif /* WINCE */

   const char *FS2OTTD(const TCHAR *name);
   const TCHAR *OTTD2FS(const char *name);
# else

#  define fopen(file, mode) fopen(OTTD2FS(file), mode)
   const char *FS2OTTD(const char *name);
   const char *OTTD2FS(const char *name);
# endif /* WIN32 */
#endif /* STRGEN */

/* Windows has always LITTLE_ENDIAN */
#if defined(WIN32) || defined(__OS2__) || defined(WIN64)
# define TTD_LITTLE_ENDIAN
#else
/* Else include endian[target/host].h, which has the endian-type, autodetected by the Makefile */
# if defined(STRGEN)
#  include "endian_host.h"
# else
#  include "endian_target.h"
# endif
#endif /* WIN32 || __OS2__ || WIN64 */

#if defined(WIN32) || defined(WIN64) || defined(__OS2__) && !defined(__INNOTEK_LIBC__)
# define PATHSEP "\\"
# define PATHSEPCHAR '\\'
#else
# define PATHSEP "/"
# define PATHSEPCHAR '/'
#endif

typedef unsigned char byte;
#ifndef __BEOS__ /* already defined */
  typedef unsigned char uint8;
  typedef unsigned short uint16;
  typedef unsigned int uint32;
#endif

/* This is already defined in unix */
#if !defined(UNIX) && !defined(__CYGWIN__) && !defined(__BEOS__) && !defined(__MORPHOS__)
  typedef unsigned int uint;
#endif
/* Not defined in QNX Neutrino (6.x) */
#if defined(__QNXNTO__)
  typedef unsigned int uint;
#endif

#ifndef __BEOS__
	typedef signed char int8;
	typedef signed short int16;
	typedef signed int int32;
	typedef signed __int64 int64;
	typedef unsigned __int64 uint64;
#endif /* !__BEOS__ */

#if defined(ARM) || defined(__arm__) || defined(__alpha__)
# define OTTD_ALIGNMENT
#endif

/* Setup alignment and conversion macros */
#if defined(TTD_BIG_ENDIAN)
  static inline uint32 TO_LE32(uint32 x) { return BSWAP32(x); }
  static inline uint16 TO_LE16(uint16 x) { return BSWAP16(x); }
  static inline uint32 FROM_LE32(uint32 x) { return BSWAP32(x); }
  static inline uint16 FROM_LE16(uint16 x) { return BSWAP16(x); }
# define TO_BE32(x)   (x)
# define TO_BE16(x)   (x)
# define FROM_BE32(x) (x)
# define FROM_BE16(x) (x)
# define TO_LE32X(x)  BSWAP32(x)
# define TO_BE32X(x)  (x)
#else
  static inline uint32 TO_BE32(uint32 x) { return BSWAP32(x); }
  static inline uint16 TO_BE16(uint16 x) { return BSWAP16(x); }
  static inline uint32 FROM_BE32(uint32 x) { return BSWAP32(x); }
  static inline uint16 FROM_BE16(uint16 x) { return BSWAP16(x); }
# define TO_LE32(x)   (x)
# define TO_LE16(x)   (x)
# define FROM_LE32(x) (x)
# define FROM_LE16(x) (x)
# define TO_LE32X(x)  (x)
# define TO_BE32X(x)  BSWAP32(x)
#endif /* TTD_BIG_ENDIAN */

#if !defined(GAME_DATA_DIR)
# define GAME_DATA_DIR ""
#endif

#if !defined(PERSONAL_DIR)
# define PERSONAL_DIR ""
#endif

/* Compile time assertions */
#ifdef __OS2__
# define assert_compile(expr)
#else
# define assert_compile(expr) extern "C" void __ct_assert__(int a[1 - 2 * !(expr)])
#endif /* __OS2__ */

assert_compile(sizeof(uint32) == 4);
assert_compile(sizeof(uint16) == 2);
assert_compile(sizeof(uint8)  == 1);

#define lengthof(x) (sizeof(x)/sizeof(x[0]))
#define endof(x) (&x[lengthof(x)])
#define lastof(x) (&x[lengthof(x) - 1])

#define cpp_offsetof(s,m)   (((size_t)&reinterpret_cast<const volatile char&>((((s*)(char*)8)->m))) - 8)
#ifndef offsetof
# define offsetof(s,m)       cpp_offsetof(s, m)
#endif /* offsetof */


/* take care of some name clashes on MacOS */
#if defined(__APPLE__)
# define GetString OTTD_GetString
# define DrawString OTTD_DrawString
# define Random OTTD_Random
# define CloseConnection OTTD_CloseConnection
#endif /* __APPLE */

#ifdef __AMIGA__
/* it seems AmigaOS already have a Point declared */
# define Point OTTD_AMIGA_POINT
#endif

#endif /* STDAFX_H */
