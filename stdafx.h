#if !defined(_STDAFX_H)
#define _STDAFX_H

#if defined(_MSC_VER)
#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#pragma warning(disable: 4100) // parameter not used
#pragma warning(disable: 4244) // conversion
#pragma warning(disable: 4245) // conversion
#pragma warning(disable: 4201) // nameless union
#pragma warning(disable: 4514) // removed unref inline
#pragma warning(disable: 4127) // constant conditional expression
#pragma warning(disable: 4276) // MSVC BUG??? Complains about function body not declared when using function pointers
#pragma warning(disable: 4761) // warning C4761: integral size mismatch in argument; conversion supplied
#endif

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#if defined(UNIX)
#	include <sys/types.h>
#endif

#if defined(__OS2__)
#	include <types.h>
#endif

#ifdef __BEOS__
#include <SupportDefs.h>
#endif

#ifdef SUNOS
#include <alloca.h>
#endif

#ifdef __MORPHOS__
// morphos defines certain amiga defines per default, we undefine them 
// here to make the rest of source less messy and more clear what is
// required for morphos and what for amigaos
#	ifdef amigaos
#		undef amigaos
#	endif
#	ifdef __amigaos__
#		undef __amigaos__
#	endif
#	ifdef __AMIGA__
#		undef __AMIGA__
#	endif
#	ifdef AMIGA
#		undef AMIGA
#	endif
#	ifdef amiga
#   	undef amiga
#	endif
#endif /* __MORPHOS__ */

#define BSWAP32(x) ((((x) >> 24) & 0xFF) | (((x) >> 8) & 0xFF00) | (((x) << 8) & 0xFF0000) | (((x) << 24) & 0xFF000000))
#define BSWAP16(x) ((x) >> 8 | (x) << 8)

// by default we use [] var arrays
#define VARARRAY_SIZE


// Stuff for GCC
#if defined(__GNUC__)
# define NORETURN
# define FORCEINLINE inline
# define CDECL
//#include <alloca.h>
//#include <malloc.h>
# define __int64 long long
# define NOT_REACHED()
# define GCC_PACK __attribute__((packed))

#	if (__GNUC__ == 2)
#  undef VARARRAY_SIZE
#  define VARARRAY_SIZE 0
# endif
#endif

#if defined(__WATCOMC__)
#       define NORETURN
#       define FORCEINLINE inline
#       define CDECL
#       define NOT_REACHED()
#       define GCC_PACK
#       undef TTD_ALIGNMENT_4
#       undef TTD_ALIGNMENT_2

#       include <malloc.h>
#endif

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <malloc.h> // alloca()
#endif

// Stuff for MSVC
#if defined(_MSC_VER)
#	include <malloc.h> // alloca()
# define NORETURN __declspec(noreturn)
#	define FORCEINLINE __forceinline
#	define inline _inline
#	define CDECL _cdecl
# define NOT_REACHED() _assume(0)
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
# undef TTD_ALIGNMENT_4
# undef TTD_ALIGNMENT_2
# define GCC_PACK
#endif

// Windows has always LITTLE_ENDIAN
#if defined(WIN32) || defined(__OS2__)
  #define TTD_LITTLE_ENDIAN
#else
// Else include endian.h, which has the endian-type, autodetected by the Makefile
  #include "endian.h"
#endif

#if defined(UNIX)
#define PATHSEP "/"
#else
#define PATHSEP "\\"
#endif


typedef unsigned char byte;
#ifndef __BEOS__ // already defined
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
#endif

// This is already defined in unix
#if !defined(UNIX) && !defined(__CYGWIN__)
	typedef unsigned int uint;
#endif
// Not defined in QNX Neutrino (6.x)
#if defined(__QNXNTO__)
	typedef unsigned int uint;
#endif

#ifndef __BEOS__
typedef signed char int8;
typedef signed short int16;
typedef signed int int32;
#ifndef __cplusplus
typedef unsigned char bool;
#endif
typedef signed __int64 int64;
typedef unsigned __int64 uint64;
#endif

// Setup alignment and conversion macros
#if defined(TTD_BIG_ENDIAN)

# define TTD_ALIGNMENT_2
# define TTD_ALIGNMENT_4

static inline uint32 TO_LE32(uint32 x) { return BSWAP32(x); }
static inline uint16 TO_LE16(uint16 x) { return BSWAP16(x); }
static inline uint32 FROM_LE32(uint32 x) { return BSWAP32(x); }
static inline uint16 FROM_LE16(uint16 x) { return BSWAP16(x); }
#define TO_BE32(x) x
#define TO_BE16(x) x
#define FROM_BE32(x) x
#define FROM_BE16(x) x
#define TO_BE32X(x) x

#else

static inline uint32 TO_BE32(uint32 x) { return BSWAP32(x); }
static inline uint16 TO_BE16(uint16 x) { return BSWAP16(x); }
static inline uint32 FROM_BE32(uint32 x) { return BSWAP32(x); }
static inline uint16 FROM_BE16(uint16 x) { return BSWAP16(x); }
#define TO_LE32(x) x
#define TO_LE16(x) x
#define TO_BE32X(x) BSWAP32(x)
#define FROM_LE32(x) x
#define FROM_LE16(x) x

#endif

#if !defined(GAME_DATA_DIR)
#define GAME_DATA_DIR ""
#endif

#if !defined(PERSONAL_DIR)
#define PERSONAL_DIR ""
#endif

#ifndef __cplusplus
#ifndef __BEOS__
enum {
	false = 0,
	true = 1,
};
#endif
#endif

// Compile time assertions
#ifdef __OS2__
#       define assert_compile(expr)
#else
#       define assert_compile(expr) void __ct_assert__(int a[1 - 2 * !(expr)])
#endif

assert_compile(sizeof(uint32) == 4);
assert_compile(sizeof(uint16) == 2);
assert_compile(sizeof(uint8)	== 1);

#define lengthof(x) (sizeof(x)/sizeof(x[0]))
#define endof(x) (&x[lengthof(x)])
#ifndef offsetof
#define offsetof(s,m)   (size_t)&(((s *)0)->m)
#endif


// take care of some name clashes on macos
#if defined(__APPLE__)
#define GetString OTTD_GetString
#define DrawString OTTD_DrawString
#define Random OTTD_Random
#define CloseConnection OTTD_CloseConnection
#endif

#ifdef __AMIGA__
// it seems AmigaOS already have a Point declared
#define Point OTTD_AMIGA_POINT
#endif

#endif // !defined(_STDAFX_H)
