/* $Id$ */

/**
 * @file minilzo.cpp -- mini subset of the LZO real-time data compression library
 *
 * This file is part of the LZO real-time data compression library.
 *
 * Copyright (C) 2002 Markus Franz Xaver Johannes Oberhumer
 * Copyright (C) 2001 Markus Franz Xaver Johannes Oberhumer
 * Copyright (C) 2000 Markus Franz Xaver Johannes Oberhumer
 * Copyright (C) 1999 Markus Franz Xaver Johannes Oberhumer
 * Copyright (C) 1998 Markus Franz Xaver Johannes Oberhumer
 * Copyright (C) 1997 Markus Franz Xaver Johannes Oberhumer
 * Copyright (C) 1996 Markus Franz Xaver Johannes Oberhumer
 * All Rights Reserved.
 *
 * The LZO library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * The LZO library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the LZO library; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Markus F.X.J. Oberhumer
 * <markus@oberhumer.com>
 * http://www.oberhumer.com/opensource/lzo/
 *
 * NOTE:
 *   the full LZO package can be found at
 *   http://www.oberhumer.com/opensource/lzo/
 */

#define LZO_DISABLE_CHECKS
#define LZO1X

#define __LZO_IN_MINILZO
#define LZO_BUILD

#ifdef MINILZO_HAVE_CONFIG_H
#  include <config.h>
#endif

#undef LZO_HAVE_CONFIG_H
#include "minilzo.h"

#if !defined(MINILZO_VERSION) || (MINILZO_VERSION != 0x1080)
#  error "version mismatch in miniLZO source files"
#endif

#ifdef MINILZO_HAVE_CONFIG_H
#  define LZO_HAVE_CONFIG_H
#endif

#if !defined(LZO_NO_SYS_TYPES_H) && !defined(WINCE)
#  include <sys/types.h>
#endif
#include <stdio.h>

#ifndef __LZO_CONF_H
#define __LZO_CONF_H

#if defined(__BOUNDS_CHECKING_ON)
#  include <unchecked.h>
#else
#  define BOUNDS_CHECKING_OFF_DURING(stmt)	  stmt
#  define BOUNDS_CHECKING_OFF_IN_EXPR(expr)	 (expr)
#endif

#if !defined(LZO_HAVE_CONFIG_H)
#  include <stddef.h>
#  include <string.h>
#  if !defined(NO_STDLIB_H)
#	include <stdlib.h>
#  endif
#  define HAVE_MEMCMP
#  define HAVE_MEMCPY
#  define HAVE_MEMMOVE
#  define HAVE_MEMSET
#else
#  include <sys/types.h>
#  if defined(HAVE_STDDEF_H)
#	include <stddef.h>
#  endif
#  if defined(STDC_HEADERS)
#	include <string.h>
#	include <stdlib.h>
#  endif
#endif

#if defined(__LZO_DOS16) || defined(__LZO_WIN16)
#  define HAVE_MALLOC_H
#  define HAVE_HALLOC
#endif

#undef NDEBUG
#if !defined(LZO_DEBUG)
#  define NDEBUG
#endif
#if defined(LZO_DEBUG) || !defined(NDEBUG)
#  if !defined(NO_STDIO_H)
#	include <stdio.h>
#  endif
#endif
#include <assert.h>

#if !defined(LZO_COMPILE_TIME_ASSERT)
#  define LZO_COMPILE_TIME_ASSERT(expr) \
		{ typedef int __lzo_compile_time_assert_fail[1 - 2 * !(expr)]; }
#endif

#if !defined(LZO_UNUSED)
#  if 1
#	define LZO_UNUSED(var)	 ((void)&var)
#  elif 0
#	define LZO_UNUSED(var)	 { typedef int __lzo_unused[sizeof(var) ? 2 : 1]; }
#  else
#	define LZO_UNUSED(parm)	(parm = parm)
#  endif
#endif

#if !defined(__inline__) && !defined(__GNUC__)
#  if defined(__cplusplus)
#	define __inline__	  inline
#  else
#	define __inline__
#  endif
#endif

#if defined(NO_MEMCMP)
#  undef HAVE_MEMCMP
#endif

#if 0
#  define LZO_BYTE(x)	   ((unsigned char) (x))
#else
#  define LZO_BYTE(x)	   ((unsigned char) ((x) & 0xff))
#endif

#define LZO_MAX(a,b)		((a) >= (b) ? (a) : (b))
#define LZO_MIN(a,b)		((a) <= (b) ? (a) : (b))
#define LZO_MAX3(a,b,c)	 ((a) >= (b) ? LZO_MAX(a,c) : LZO_MAX(b,c))
#define LZO_MIN3(a,b,c)	 ((a) <= (b) ? LZO_MIN(a,c) : LZO_MIN(b,c))

#define lzo_sizeof(type)	((lzo_uint) (sizeof(type)))

#define LZO_HIGH(array)	 ((lzo_uint) (sizeof(array)/sizeof(*(array))))

#define LZO_SIZE(bits)	  (1u << (bits))
#define LZO_MASK(bits)	  (LZO_SIZE(bits) - 1)

#define LZO_LSIZE(bits)	 (1ul << (bits))
#define LZO_LMASK(bits)	 (LZO_LSIZE(bits) - 1)

#define LZO_USIZE(bits)	 ((lzo_uint) 1 << (bits))
#define LZO_UMASK(bits)	 (LZO_USIZE(bits) - 1)

#define LZO_STYPE_MAX(b)	(((1l  << (8*(b)-2)) - 1l)  + (1l  << (8*(b)-2)))
#define LZO_UTYPE_MAX(b)	(((1ul << (8*(b)-1)) - 1ul) + (1ul << (8*(b)-1)))

#if !defined(SIZEOF_UNSIGNED)
#  if (UINT_MAX == 0xffff)
#	define SIZEOF_UNSIGNED		 2
#  elif (UINT_MAX == LZO_0xffffffffL)
#	define SIZEOF_UNSIGNED		 4
#  elif (UINT_MAX >= LZO_0xffffffffL)
#	define SIZEOF_UNSIGNED		 8
#  else
#	error "SIZEOF_UNSIGNED"
#  endif
#endif

#if !defined(SIZEOF_UNSIGNED_LONG)
#  if (ULONG_MAX == LZO_0xffffffffL)
#	define SIZEOF_UNSIGNED_LONG	4
#  elif (ULONG_MAX >= LZO_0xffffffffL)
#	define SIZEOF_UNSIGNED_LONG	8
#  else
#	error "SIZEOF_UNSIGNED_LONG"
#  endif
#endif

#if !defined(SIZEOF_SIZE_T)
#  define SIZEOF_SIZE_T			 SIZEOF_UNSIGNED
#endif
#if !defined(SIZE_T_MAX)
#  define SIZE_T_MAX				LZO_UTYPE_MAX(SIZEOF_SIZE_T)
#endif

#if 1 && defined(__LZO_i386) && (UINT_MAX == LZO_0xffffffffL)
#  if !defined(LZO_UNALIGNED_OK_2) && (USHRT_MAX == 0xffff)
#	define LZO_UNALIGNED_OK_2
#  endif
#  if !defined(LZO_UNALIGNED_OK_4) && (LZO_UINT32_MAX == LZO_0xffffffffL)
#	define LZO_UNALIGNED_OK_4
#  endif
#endif

#if defined(LZO_UNALIGNED_OK_2) || defined(LZO_UNALIGNED_OK_4)
#  if !defined(LZO_UNALIGNED_OK)
#	define LZO_UNALIGNED_OK
#  endif
#endif

#if defined(__LZO_NO_UNALIGNED)
#  undef LZO_UNALIGNED_OK
#  undef LZO_UNALIGNED_OK_2
#  undef LZO_UNALIGNED_OK_4
#endif

#if defined(LZO_UNALIGNED_OK_2) && (USHRT_MAX != 0xffff)
#  error "LZO_UNALIGNED_OK_2 must not be defined on this system"
#endif
#if defined(LZO_UNALIGNED_OK_4) && (LZO_UINT32_MAX != LZO_0xffffffffL)
#  error "LZO_UNALIGNED_OK_4 must not be defined on this system"
#endif

#if defined(__LZO_NO_ALIGNED)
#  undef LZO_ALIGNED_OK_4
#endif

#if defined(LZO_ALIGNED_OK_4) && (LZO_UINT32_MAX != LZO_0xffffffffL)
#  error "LZO_ALIGNED_OK_4 must not be defined on this system"
#endif

#define LZO_LITTLE_ENDIAN 1234
#define LZO_BIG_ENDIAN    4321
#define LZO_PDP_ENDIAN    3412

#if !defined(LZO_BYTE_ORDER)
#  if defined(MFX_BYTE_ORDER)
#	define LZO_BYTE_ORDER	  MFX_BYTE_ORDER
#  elif defined(__LZO_i386)
#	define LZO_BYTE_ORDER	  LZO_LITTLE_ENDIAN
#  elif defined(BYTE_ORDER)
#	define LZO_BYTE_ORDER	  BYTE_ORDER
#  elif defined(__BYTE_ORDER)
#	define LZO_BYTE_ORDER	  __BYTE_ORDER
#  endif
#endif

#if defined(LZO_BYTE_ORDER)
#  if (LZO_BYTE_ORDER != LZO_LITTLE_ENDIAN) && \
	  (LZO_BYTE_ORDER != LZO_BIG_ENDIAN)
#	error "invalid LZO_BYTE_ORDER"
#  endif
#endif

#if defined(LZO_UNALIGNED_OK) && !defined(LZO_BYTE_ORDER)
#  error "LZO_BYTE_ORDER is not defined"
#endif

#define LZO_OPTIMIZE_GNUC_i386_IS_BUGGY

#if defined(NDEBUG) && !defined(LZO_DEBUG) && !defined(__LZO_CHECKER)
#  if defined(__GNUC__) && defined(__i386__)
#	if !defined(LZO_OPTIMIZE_GNUC_i386_IS_BUGGY)
#	  define LZO_OPTIMIZE_GNUC_i386
#	endif
#  endif
#endif

__LZO_EXTERN_C int __lzo_init_done;
__LZO_EXTERN_C const lzo_byte __lzo_copyright[];
LZO_EXTERN(const lzo_byte *) lzo_copyright();
__LZO_EXTERN_C const lzo_uint32 _lzo_crc32_table[256];

#define _LZO_STRINGIZE(x)		   #x
#define _LZO_MEXPAND(x)			 _LZO_STRINGIZE(x)

#define _LZO_CONCAT2(a,b)		   a ## b
#define _LZO_CONCAT3(a,b,c)		 a ## b ## c
#define _LZO_CONCAT4(a,b,c,d)	   a ## b ## c ## d
#define _LZO_CONCAT5(a,b,c,d,e)	 a ## b ## c ## d ## e

#define _LZO_ECONCAT2(a,b)		  _LZO_CONCAT2(a,b)
#define _LZO_ECONCAT3(a,b,c)		_LZO_CONCAT3(a,b,c)
#define _LZO_ECONCAT4(a,b,c,d)	  _LZO_CONCAT4(a,b,c,d)
#define _LZO_ECONCAT5(a,b,c,d,e)	_LZO_CONCAT5(a,b,c,d,e)

#ifndef __LZO_PTR_H
#define __LZO_PTR_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__LZO_DOS16) || defined(__LZO_WIN16)
#  include <dos.h>
#  if 1 && defined(__WATCOMC__)
#	include <i86.h>
	 __LZO_EXTERN_C unsigned char _HShift;
#	define __LZO_HShift	_HShift
#  elif 1 && defined(_MSC_VER)
	 __LZO_EXTERN_C unsigned short __near _AHSHIFT;
#	define __LZO_HShift	((unsigned) &_AHSHIFT)
#  elif defined(__LZO_WIN16)
#	define __LZO_HShift	3
#  else
#	define __LZO_HShift	12
#  endif
#  if !defined(_FP_SEG) && defined(FP_SEG)
#	define _FP_SEG		 FP_SEG
#  endif
#  if !defined(_FP_OFF) && defined(FP_OFF)
#	define _FP_OFF		 FP_OFF
#  endif
#endif

#if !defined(lzo_ptrdiff_t)
#  if (UINT_MAX >= LZO_0xffffffffL)
	 typedef ptrdiff_t		  lzo_ptrdiff_t;
#  else
	 typedef long			   lzo_ptrdiff_t;
#  endif
#endif

#if !defined(__LZO_HAVE_PTR_T)
#  if defined(lzo_ptr_t)
#	define __LZO_HAVE_PTR_T
#  endif
#endif
#if !defined(__LZO_HAVE_PTR_T)
#	if defined(_WIN64)
		typedef unsigned __int64 lzo_ptr_t;
		typedef signed __int64   lzo_sptr_r;
#		define __LZO_HAVE_PTR_T
#	endif
#endif
#if !defined(__LZO_HAVE_PTR_T)
#  if defined(SIZEOF_CHAR_P) && defined(SIZEOF_UNSIGNED_LONG)
#	if (SIZEOF_CHAR_P == SIZEOF_UNSIGNED_LONG)
	   typedef unsigned long	lzo_ptr_t;
	   typedef long			 lzo_sptr_t;
#	  define __LZO_HAVE_PTR_T
#	endif
#  endif
#endif
#if !defined(__LZO_HAVE_PTR_T)
#  if defined(SIZEOF_CHAR_P) && defined(SIZEOF_UNSIGNED)
#	if (SIZEOF_CHAR_P == SIZEOF_UNSIGNED)
	   typedef unsigned int	 lzo_ptr_t;
	   typedef int			  lzo_sptr_t;
#	  define __LZO_HAVE_PTR_T
#	endif
#  endif
#endif
#if !defined(__LZO_HAVE_PTR_T)
#  if defined(SIZEOF_CHAR_P) && defined(SIZEOF_UNSIGNED_SHORT)
#	if (SIZEOF_CHAR_P == SIZEOF_UNSIGNED_SHORT)
	   typedef unsigned short   lzo_ptr_t;
	   typedef short			lzo_sptr_t;
#	  define __LZO_HAVE_PTR_T
#	endif
#  endif
#endif
#if !defined(__LZO_HAVE_PTR_T)
#  if defined(LZO_HAVE_CONFIG_H) || defined(SIZEOF_CHAR_P)
#	error "no suitable type for lzo_ptr_t"
#  else
	 typedef unsigned long	  lzo_ptr_t;
	 typedef long			   lzo_sptr_t;
#	define __LZO_HAVE_PTR_T
#  endif
#endif

#if defined(__LZO_DOS16) || defined(__LZO_WIN16)
#define PTR(a)			  ((lzo_bytep) (a))
#define PTR_ALIGNED_4(a)	((_FP_OFF(a) & 3) == 0)
#define PTR_ALIGNED2_4(a,b) (((_FP_OFF(a) | _FP_OFF(b)) & 3) == 0)
#else
#define PTR(a)			  ((lzo_ptr_t) (a))
#define PTR_LINEAR(a)	   PTR(a)
#define PTR_ALIGNED_4(a)	((PTR_LINEAR(a) & 3) == 0)
#define PTR_ALIGNED_8(a)	((PTR_LINEAR(a) & 7) == 0)
#define PTR_ALIGNED2_4(a,b) (((PTR_LINEAR(a) | PTR_LINEAR(b)) & 3) == 0)
#define PTR_ALIGNED2_8(a,b) (((PTR_LINEAR(a) | PTR_LINEAR(b)) & 7) == 0)
#endif

#define PTR_LT(a,b)		 (PTR(a) < PTR(b))
#define PTR_GE(a,b)		 (PTR(a) >= PTR(b))
#define PTR_DIFF(a,b)	   ((lzo_ptrdiff_t) (PTR(a) - PTR(b)))
#define pd(a,b)			 ((lzo_uint) ((a)-(b)))

LZO_EXTERN(lzo_ptr_t)
__lzo_ptr_linear(const lzo_voidp ptr);

typedef union
{
	char			a_char;
	unsigned char   a_uchar;
	short		   a_short;
	unsigned short  a_ushort;
	int			 a_int;
	unsigned int	a_uint;
	long			a_long;
	unsigned long   a_ulong;
	lzo_int		 a_lzo_int;
	lzo_uint		a_lzo_uint;
	lzo_int32	   a_lzo_int32;
	lzo_uint32	  a_lzo_uint32;
	ptrdiff_t	   a_ptrdiff_t;
	lzo_ptrdiff_t   a_lzo_ptrdiff_t;
	lzo_ptr_t	   a_lzo_ptr_t;
	lzo_voidp	   a_lzo_voidp;
	void *		  a_void_p;
	lzo_bytep	   a_lzo_bytep;
	lzo_bytepp	  a_lzo_bytepp;
	lzo_uintp	   a_lzo_uintp;
	lzo_uint *	  a_lzo_uint_p;
	lzo_uint32p	 a_lzo_uint32p;
	lzo_uint32 *	a_lzo_uint32_p;
	unsigned char * a_uchar_p;
	char *		  a_char_p;
}
lzo_full_align_t;

#ifdef __cplusplus
}
#endif

#endif

#define LZO_DETERMINISTIC

#define LZO_DICT_USE_PTR
#if defined(__LZO_DOS16) || defined(__LZO_WIN16) || defined(__LZO_STRICT_16BIT)
#  undef LZO_DICT_USE_PTR
#endif

#if defined(LZO_DICT_USE_PTR)
#  define lzo_dict_t	const lzo_bytep
#  define lzo_dict_p	lzo_dict_t __LZO_MMODEL *
#else
#  define lzo_dict_t	lzo_uint
#  define lzo_dict_p	lzo_dict_t __LZO_MMODEL *
#endif

#if !defined(lzo_moff_t)
#define lzo_moff_t	  lzo_uint
#endif

#endif


#ifndef __LZO_UTIL_H
#define __LZO_UTIL_H

#ifndef __LZO_CONF_H
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 1 && defined(HAVE_MEMCPY)
#if !defined(__LZO_DOS16) && !defined(__LZO_WIN16)

#define MEMCPY8_DS(dest,src,len) \
	memcpy(dest,src,len); \
	dest += len; \
	src += len

#endif
#endif

#if 0 && !defined(MEMCPY8_DS)

#define MEMCPY8_DS(dest,src,len) \
	{ do { \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		len -= 8; \
	} while (len > 0); }

#endif

#if !defined(MEMCPY8_DS)

#define MEMCPY8_DS(dest,src,len) \
	{ register lzo_uint __l = (len) / 8; \
	do { \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
		*dest++ = *src++; \
	} while (--__l > 0); }

#endif

#define MEMCPY_DS(dest,src,len) \
	do *dest++ = *src++; \
	while (--len > 0)

#define MEMMOVE_DS(dest,src,len) \
	do *dest++ = *src++; \
	while (--len > 0)

#if 0 && defined(LZO_OPTIMIZE_GNUC_i386)

#define BZERO8_PTR(s,l,n) \
__asm__ __volatile__( \
	"movl  %0,%%eax \n"			 \
	"movl  %1,%%edi \n"			 \
	"movl  %2,%%ecx \n"			 \
	"cld \n"						\
	"rep \n"						\
	"stosl %%eax,(%%edi) \n"		\
	:			   \
	:"g" (0),"g" (s),"g" (n)		\
	:"eax","edi","ecx", "memory", "cc" \
)

#elif (LZO_UINT_MAX <= SIZE_T_MAX) && defined(HAVE_MEMSET)

#if 1
#define BZERO8_PTR(s,l,n)   memset((s),0,(lzo_uint)(l)*(n))
#else
#define BZERO8_PTR(s,l,n)   memset((lzo_voidp)(s),0,(lzo_uint)(l)*(n))
#endif

#else

#define BZERO8_PTR(s,l,n) \
	lzo_memset((lzo_voidp)(s),0,(lzo_uint)(l)*(n))

#endif

#if 0
#if defined(__GNUC__) && defined(__i386__)

unsigned char lzo_rotr8(unsigned char value, int shift);
extern __inline__ unsigned char lzo_rotr8(unsigned char value, int shift)
{
	unsigned char result;

	__asm__ __volatile__ ("movb %b1, %b0; rorb %b2, %b0"
						: "=a"(result) : "g"(value), "c"(shift));
	return result;
}

unsigned short lzo_rotr16(unsigned short value, int shift);
extern __inline__ unsigned short lzo_rotr16(unsigned short value, int shift)
{
	unsigned short result;

	__asm__ __volatile__ ("movw %b1, %b0; rorw %b2, %b0"
						: "=a"(result) : "g"(value), "c"(shift));
	return result;
}

#endif
#endif

#ifdef __cplusplus
}
#endif

#endif


/* If you use the LZO library in a product, you *must* keep this
 * copyright string in the executable of your product.
 */

const lzo_byte __lzo_copyright[] =
#if !defined(__LZO_IN_MINLZO)
	LZO_VERSION_STRING;
#else
	"\n\n\n"
	"LZO real-time data compression library.\n"
	"Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002 Markus Franz Xaver Johannes Oberhumer\n"
	"<markus.oberhumer@jk.uni-linz.ac.at>\n"
	"http://www.oberhumer.com/opensource/lzo/\n"
	"\n"
	"LZO version: v" LZO_VERSION_STRING ", " LZO_VERSION_DATE "\n"
	"LZO build date: " __DATE__ " " __TIME__ "\n\n"
	"LZO special compilation options:\n"
#ifdef __cplusplus
	" __cplusplus\n"
#endif
#if defined(__PIC__)
	" __PIC__\n"
#elif defined(__pic__)
	" __pic__\n"
#endif
#if (UINT_MAX < LZO_0xffffffffL)
	" 16BIT\n"
#endif
#if defined(__LZO_STRICT_16BIT)
	" __LZO_STRICT_16BIT\n"
#endif
#if (UINT_MAX > LZO_0xffffffffL)
	" UINT_MAX=" _LZO_MEXPAND(UINT_MAX) "\n"
#endif
#if (ULONG_MAX > LZO_0xffffffffL)
	" ULONG_MAX=" _LZO_MEXPAND(ULONG_MAX) "\n"
#endif
#if defined(LZO_BYTE_ORDER)
	" LZO_BYTE_ORDER=" _LZO_MEXPAND(LZO_BYTE_ORDER) "\n"
#endif
#if defined(LZO_UNALIGNED_OK_2)
	" LZO_UNALIGNED_OK_2\n"
#endif
#if defined(LZO_UNALIGNED_OK_4)
	" LZO_UNALIGNED_OK_4\n"
#endif
#if defined(LZO_ALIGNED_OK_4)
	" LZO_ALIGNED_OK_4\n"
#endif
#if defined(LZO_DICT_USE_PTR)
	" LZO_DICT_USE_PTR\n"
#endif
#if defined(__LZO_QUERY_COMPRESS)
	" __LZO_QUERY_COMPRESS\n"
#endif
#if defined(__LZO_QUERY_DECOMPRESS)
	" __LZO_QUERY_DECOMPRESS\n"
#endif
#if defined(__LZO_IN_MINILZO)
	" __LZO_IN_MINILZO\n"
#endif
	"\n\n"
	"$Id: LZO " LZO_VERSION_STRING " built " __DATE__ " " __TIME__
#if defined(__GNUC__) && defined(__VERSION__)
	" by gcc " __VERSION__
#elif defined(__BORLANDC__)
	" by Borland C " _LZO_MEXPAND(__BORLANDC__)
#elif defined(_MSC_VER)
	" by Microsoft C " _LZO_MEXPAND(_MSC_VER)
#elif defined(__PUREC__)
	" by Pure C " _LZO_MEXPAND(__PUREC__)
#elif defined(__SC__)
	" by Symantec C " _LZO_MEXPAND(__SC__)
#elif defined(__TURBOC__)
	" by Turbo C " _LZO_MEXPAND(__TURBOC__)
#elif defined(__WATCOMC__)
	" by Watcom C " _LZO_MEXPAND(__WATCOMC__)
#endif
	" $\n"
	"$Copyright: LZO (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002 Markus Franz Xaver Johannes Oberhumer $\n";
#endif

#define LZO_BASE 65521u
#define LZO_NMAX 5552

#define LZO_DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define LZO_DO2(buf,i)  LZO_DO1(buf,i); LZO_DO1(buf,i+1);
#define LZO_DO4(buf,i)  LZO_DO2(buf,i); LZO_DO2(buf,i+2);
#define LZO_DO8(buf,i)  LZO_DO4(buf,i); LZO_DO4(buf,i+4);
#define LZO_DO16(buf,i) LZO_DO8(buf,i); LZO_DO8(buf,i+8);

LZO_PUBLIC(lzo_uint32)
lzo_adler32(lzo_uint32 adler, const lzo_byte *buf, lzo_uint len)
{
	lzo_uint32 s1 = adler & 0xffff;
	lzo_uint32 s2 = (adler >> 16) & 0xffff;
	int k;

	if (buf == NULL)
		return 1;

	while (len > 0)
	{
		k = len < LZO_NMAX ? (int) len : LZO_NMAX;
		len -= k;
		if (k >= 16) do
		{
			LZO_DO16(buf,0);
			buf += 16;
			k -= 16;
		} while (k >= 16);
		if (k != 0) do
		{
			s1 += *buf++;
			s2 += s1;
		} while (--k > 0);
		s1 %= LZO_BASE;
		s2 %= LZO_BASE;
		}
	return (s2 << 16) | s1;
}

#if 0
#  define IS_SIGNED(type)	   (((type) (1ul << (8 * sizeof(type) - 1))) < 0)
#  define IS_UNSIGNED(type)	 (((type) (1ul << (8 * sizeof(type) - 1))) > 0)
#else
#  define IS_SIGNED(type)	   (((type) (-1)) < ((type) 0))
#  define IS_UNSIGNED(type)	 (((type) (-1)) > ((type) 0))
#endif

#define IS_POWER_OF_2(x)		(((x) & ((x) - 1)) == 0)

// static lzo_bool schedule_insns_bug();
// static lzo_bool strength_reduce_bug(int *);

#if 0 || defined(LZO_DEBUG)
#include <stdio.h>
static lzo_bool __lzo_assert_fail(const char *s, unsigned line)
{
#if defined(__palmos__)
	printf("LZO assertion failed in line %u: '%s'\n",line,s);
#else
	fprintf(stderr,"LZO assertion failed in line %u: '%s'\n",line,s);
#endif
	return 0;
}
#  define __lzo_assert(x)   ((x) ? 1 : __lzo_assert_fail(#x,__LINE__))
#else
#  define __lzo_assert(x)   ((x) ? 1 : 0)
#endif

#undef COMPILE_TIME_ASSERT
#if 0
#  define COMPILE_TIME_ASSERT(expr)	 r &= __lzo_assert(expr)
#else
#  define COMPILE_TIME_ASSERT(expr)	 LZO_COMPILE_TIME_ASSERT(expr)
#endif


#define do_compress		 _lzo1x_1_do_compress

#define LZO_NEED_DICT_H
#define D_BITS		  12
#define D_INDEX1(d,p)	   d = DM((0x21*DX3(p,5,5,6)) >> 5)
#define D_INDEX2(d,p)	   d = (d & (D_MASK & 0x7ff)) ^ (D_HIGH | 0x1f)

#ifndef __LZO_CONFIG1X_H
#define __LZO_CONFIG1X_H

#define LZO_EOF_CODE
#undef LZO_DETERMINISTIC

#define M1_MAX_OFFSET   0x0400
#ifndef M2_MAX_OFFSET
#define M2_MAX_OFFSET   0x0800
#endif
#define M3_MAX_OFFSET   0x4000
#define M4_MAX_OFFSET   0xbfff

#define MX_MAX_OFFSET   (M1_MAX_OFFSET + M2_MAX_OFFSET)

#define M1_MIN_LEN	  2
#define M1_MAX_LEN	  2
#define M2_MIN_LEN	  3
#ifndef M2_MAX_LEN
#define M2_MAX_LEN	  8
#endif
#define M3_MIN_LEN	  3
#define M3_MAX_LEN	  33
#define M4_MIN_LEN	  3
#define M4_MAX_LEN	  9

#define M1_MARKER	   0
#define M2_MARKER	   64
#define M3_MARKER	   32
#define M4_MARKER	   16

#ifndef MIN_LOOKAHEAD
#define MIN_LOOKAHEAD	   (M2_MAX_LEN + 1)
#endif

#if defined(LZO_NEED_DICT_H)

#ifndef LZO_HASH
#define LZO_HASH			LZO_HASH_LZO_INCREMENTAL_B
#endif
#define DL_MIN_LEN		  M2_MIN_LEN

#ifndef __LZO_DICT_H
#define __LZO_DICT_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(D_BITS) && defined(DBITS)
#  define D_BITS		DBITS
#endif
#if !defined(D_BITS)
#  error "D_BITS is not defined"
#endif
#if (D_BITS < 16)
#  define D_SIZE		LZO_SIZE(D_BITS)
#  define D_MASK		LZO_MASK(D_BITS)
#else
#  define D_SIZE		LZO_USIZE(D_BITS)
#  define D_MASK		LZO_UMASK(D_BITS)
#endif
#define D_HIGH		  ((D_MASK >> 1) + 1)

#if !defined(DD_BITS)
#  define DD_BITS	   0
#endif
#define DD_SIZE		 LZO_SIZE(DD_BITS)
#define DD_MASK		 LZO_MASK(DD_BITS)

#if !defined(DL_BITS)
#  define DL_BITS	   (D_BITS - DD_BITS)
#endif
#if (DL_BITS < 16)
#  define DL_SIZE	   LZO_SIZE(DL_BITS)
#  define DL_MASK	   LZO_MASK(DL_BITS)
#else
#  define DL_SIZE	   LZO_USIZE(DL_BITS)
#  define DL_MASK	   LZO_UMASK(DL_BITS)
#endif

#if (D_BITS != DL_BITS + DD_BITS)
#  error "D_BITS does not match"
#endif
#if (D_BITS < 8 || D_BITS > 18)
#  error "invalid D_BITS"
#endif
#if (DL_BITS < 8 || DL_BITS > 20)
#  error "invalid DL_BITS"
#endif
#if (DD_BITS < 0 || DD_BITS > 6)
#  error "invalid DD_BITS"
#endif

#if !defined(DL_MIN_LEN)
#  define DL_MIN_LEN	3
#endif
#if !defined(DL_SHIFT)
#  define DL_SHIFT	  ((DL_BITS + (DL_MIN_LEN - 1)) / DL_MIN_LEN)
#endif

#define LZO_HASH_GZIP				   1
#define LZO_HASH_GZIP_INCREMENTAL	   2
#define LZO_HASH_LZO_INCREMENTAL_A	  3
#define LZO_HASH_LZO_INCREMENTAL_B	  4

#if !defined(LZO_HASH)
#  error "choose a hashing strategy"
#endif

#if (DL_MIN_LEN == 3)
#  define _DV2_A(p,shift1,shift2) \
		(((( (lzo_uint32)((p)[0]) << shift1) ^ (p)[1]) << shift2) ^ (p)[2])
#  define _DV2_B(p,shift1,shift2) \
		(((( (lzo_uint32)((p)[2]) << shift1) ^ (p)[1]) << shift2) ^ (p)[0])
#  define _DV3_B(p,shift1,shift2,shift3) \
		((_DV2_B((p)+1,shift1,shift2) << (shift3)) ^ (p)[0])
#elif (DL_MIN_LEN == 2)
#  define _DV2_A(p,shift1,shift2) \
		(( (lzo_uint32)(p[0]) << shift1) ^ p[1])
#  define _DV2_B(p,shift1,shift2) \
		(( (lzo_uint32)(p[1]) << shift1) ^ p[2])
#else
#  error "invalid DL_MIN_LEN"
#endif
#define _DV_A(p,shift)	  _DV2_A(p,shift,shift)
#define _DV_B(p,shift)	  _DV2_B(p,shift,shift)
#define DA2(p,s1,s2) \
		(((((lzo_uint32)((p)[2]) << (s2)) + (p)[1]) << (s1)) + (p)[0])
#define DS2(p,s1,s2) \
		(((((lzo_uint32)((p)[2]) << (s2)) - (p)[1]) << (s1)) - (p)[0])
#define DX2(p,s1,s2) \
		(((((lzo_uint32)((p)[2]) << (s2)) ^ (p)[1]) << (s1)) ^ (p)[0])
#define DA3(p,s1,s2,s3) ((DA2((p)+1,s2,s3) << (s1)) + (p)[0])
#define DS3(p,s1,s2,s3) ((DS2((p)+1,s2,s3) << (s1)) - (p)[0])
#define DX3(p,s1,s2,s3) ((DX2((p)+1,s2,s3) << (s1)) ^ (p)[0])
#define DMS(v,s)		((lzo_uint) (((v) & (D_MASK >> (s))) << (s)))
#define DM(v)		   DMS(v,0)

#if (LZO_HASH == LZO_HASH_GZIP)
#  define _DINDEX(dv,p)	 (_DV_A((p),DL_SHIFT))

#elif (LZO_HASH == LZO_HASH_GZIP_INCREMENTAL)
#  define __LZO_HASH_INCREMENTAL
#  define DVAL_FIRST(dv,p)  dv = _DV_A((p),DL_SHIFT)
#  define DVAL_NEXT(dv,p)   dv = (((dv) << DL_SHIFT) ^ p[2])
#  define _DINDEX(dv,p)	 (dv)
#  define DVAL_LOOKAHEAD	DL_MIN_LEN

#elif (LZO_HASH == LZO_HASH_LZO_INCREMENTAL_A)
#  define __LZO_HASH_INCREMENTAL
#  define DVAL_FIRST(dv,p)  dv = _DV_A((p),5)
#  define DVAL_NEXT(dv,p) \
				dv ^= (lzo_uint32)(p[-1]) << (2*5); dv = (((dv) << 5) ^ p[2])
#  define _DINDEX(dv,p)	 ((0x9f5f * (dv)) >> 5)
#  define DVAL_LOOKAHEAD	DL_MIN_LEN

#elif (LZO_HASH == LZO_HASH_LZO_INCREMENTAL_B)
#  define __LZO_HASH_INCREMENTAL
#  define DVAL_FIRST(dv,p)  dv = _DV_B((p),5)
#  define DVAL_NEXT(dv,p) \
				dv ^= p[-1]; dv = (((dv) >> 5) ^ ((lzo_uint32)(p[2]) << (2*5)))
#  define _DINDEX(dv,p)	 ((0x9f5f * (dv)) >> 5)
#  define DVAL_LOOKAHEAD	DL_MIN_LEN

#else
#  error "choose a hashing strategy"
#endif

#ifndef DINDEX
#define DINDEX(dv,p)		((lzo_uint)((_DINDEX(dv,p)) & DL_MASK) << DD_BITS)
#endif
#if !defined(DINDEX1) && defined(D_INDEX1)
#define DINDEX1			 D_INDEX1
#endif
#if !defined(DINDEX2) && defined(D_INDEX2)
#define DINDEX2			 D_INDEX2
#endif

#if !defined(__LZO_HASH_INCREMENTAL)
#  define DVAL_FIRST(dv,p)  ((void) 0)
#  define DVAL_NEXT(dv,p)   ((void) 0)
#  define DVAL_LOOKAHEAD	0
#endif

#if !defined(DVAL_ASSERT)
#if defined(__LZO_HASH_INCREMENTAL) && !defined(NDEBUG)
static void DVAL_ASSERT(lzo_uint32 dv, const lzo_byte *p)
{
	lzo_uint32 df;
	DVAL_FIRST(df,(p));
	assert(DINDEX(dv,p) == DINDEX(df,p));
}
#else
#  define DVAL_ASSERT(dv,p) ((void) 0)
#endif
#endif

#if defined(LZO_DICT_USE_PTR)
#  define DENTRY(p,in)						  (p)
#  define GINDEX(m_pos,m_off,dict,dindex,in)	m_pos = dict[dindex]
#else
#  define DENTRY(p,in)						  ((lzo_uint) ((p)-(in)))
#  define GINDEX(m_pos,m_off,dict,dindex,in)	m_off = dict[dindex]
#endif

#if (DD_BITS == 0)

#  define UPDATE_D(dict,drun,dv,p,in)	   dict[ DINDEX(dv,p) ] = DENTRY(p,in)
#  define UPDATE_I(dict,drun,index,p,in)	dict[index] = DENTRY(p,in)
#  define UPDATE_P(ptr,drun,p,in)		   (ptr)[0] = DENTRY(p,in)

#else

#  define UPDATE_D(dict,drun,dv,p,in)   \
		dict[ DINDEX(dv,p) + drun++ ] = DENTRY(p,in); drun &= DD_MASK
#  define UPDATE_I(dict,drun,index,p,in)	\
		dict[ (index) + drun++ ] = DENTRY(p,in); drun &= DD_MASK
#  define UPDATE_P(ptr,drun,p,in)   \
		(ptr) [ drun++ ] = DENTRY(p,in); drun &= DD_MASK

#endif

#if defined(LZO_DICT_USE_PTR)

#define LZO_CHECK_MPOS_DET(m_pos,m_off,in,ip,max_offset) \
		(m_pos == NULL || (m_off = (lzo_moff_t) (ip - m_pos)) > max_offset)

#define LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,max_offset) \
	(BOUNDS_CHECKING_OFF_IN_EXPR( \
		(PTR_LT(m_pos,in) || \
		 (m_off = (lzo_moff_t) PTR_DIFF(ip,m_pos)) <= 0 || \
		  m_off > max_offset) ))

#else

#define LZO_CHECK_MPOS_DET(m_pos,m_off,in,ip,max_offset) \
		(m_off == 0 || \
		 ((m_off = (lzo_moff_t) ((ip)-(in)) - m_off) > max_offset) || \
		 (m_pos = (ip) - (m_off), 0) )

#define LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,max_offset) \
		((lzo_moff_t) ((ip)-(in)) <= m_off || \
		 ((m_off = (lzo_moff_t) ((ip)-(in)) - m_off) > max_offset) || \
		 (m_pos = (ip) - (m_off), 0) )

#endif

#if defined(LZO_DETERMINISTIC)
#  define LZO_CHECK_MPOS	LZO_CHECK_MPOS_DET
#else
#  define LZO_CHECK_MPOS	LZO_CHECK_MPOS_NON_DET
#endif

#ifdef __cplusplus
}
#endif

#endif

#endif

#endif

#define DO_COMPRESS	 lzo1x_1_compress

static
lzo_uint do_compress	 ( const lzo_byte *in , lzo_uint  in_len,
								 lzo_byte *out, lzo_uintp out_len,
								 lzo_voidp wrkmem )
{
		register const lzo_byte *ip;
		lzo_byte *op;
		const lzo_byte * const in_end = in + in_len;
		const lzo_byte * const ip_end = in + in_len - M2_MAX_LEN - 5;
		const lzo_byte *ii;
		lzo_dict_p const dict = (lzo_dict_p) wrkmem;

		op = out;
		ip = in;
		ii = ip;

		ip += 4;
		for (;;)
		{
		register const lzo_byte *m_pos;
		lzo_moff_t m_off;
		lzo_ptrdiff_t m_len;
		lzo_uint dindex;

		DINDEX1(dindex,ip);
		GINDEX(m_pos,m_off,dict,dindex,in);
		if (LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,M4_MAX_OFFSET))
			goto literal;
		if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3])
			goto try_match;
		DINDEX2(dindex,ip);
		GINDEX(m_pos,m_off,dict,dindex,in);
		if (LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,M4_MAX_OFFSET))
			goto literal;
		if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3])
			goto try_match;
		goto literal;

try_match:
#if 1 && defined(LZO_UNALIGNED_OK_2)
		if (* (const lzo_ushortp) m_pos != * (const lzo_ushortp) ip)
#else
		if (m_pos[0] != ip[0] || m_pos[1] != ip[1])
#endif
		{
		}
		else
		{
			if (m_pos[2] == ip[2])
			{
					goto match;
			}
		}

literal:
		UPDATE_I(dict,0,dindex,ip,in);
		++ip;
		if (ip >= ip_end)
			break;
		continue;

match:
		UPDATE_I(dict,0,dindex,ip,in);
		if (pd(ip,ii) > 0)
		{
			register lzo_uint t = pd(ip,ii);

			if (t <= 3)
			{
				assert(op - 2 > out);
				op[-2] |= LZO_BYTE(t);
			}
			else if (t <= 18)
				*op++ = LZO_BYTE(t - 3);
			else
			{
				register lzo_uint tt = t - 18;

				*op++ = 0;
				while (tt > 255)
				{
					tt -= 255;
					*op++ = 0;
				}
				assert(tt > 0);
				*op++ = LZO_BYTE(tt);
			}
			do *op++ = *ii++; while (--t > 0);
		}

		assert(ii == ip);
		ip += 3;
		if (m_pos[3] != *ip++ || m_pos[4] != *ip++ || m_pos[5] != *ip++ ||
			m_pos[6] != *ip++ || m_pos[7] != *ip++ || m_pos[8] != *ip++) {

			--ip;
			m_len = ip - ii;
			assert(m_len >= 3); assert(m_len <= M2_MAX_LEN);

			if (m_off <= M2_MAX_OFFSET)
			{
				m_off -= 1;
				*op++ = LZO_BYTE(((m_len - 1) << 5) | ((m_off & 7) << 2));
				*op++ = LZO_BYTE(m_off >> 3);
			}
			else if (m_off <= M3_MAX_OFFSET)
			{
				m_off -= 1;
				*op++ = LZO_BYTE(M3_MARKER | (m_len - 2));
				goto m3_m4_offset;
			}
			else
			{
				m_off -= 0x4000;
				assert(m_off > 0); assert(m_off <= 0x7fff);
				*op++ = LZO_BYTE(M4_MARKER |
								 ((m_off & 0x4000) >> 11) | (m_len - 2));
				goto m3_m4_offset;
			}
		}
		else
		{
			{
				const lzo_byte *end = in_end;
				const lzo_byte *m = m_pos + M2_MAX_LEN + 1;
				while (ip < end && *m == *ip)
					m++, ip++;
				m_len = (ip - ii);
			}
			assert(m_len > M2_MAX_LEN);

			if (m_off <= M3_MAX_OFFSET)
			{
				m_off -= 1;
				if (m_len <= 33)
					*op++ = LZO_BYTE(M3_MARKER | (m_len - 2));
				else
				{
					m_len -= 33;
					*op++ = M3_MARKER | 0;
					goto m3_m4_len;
				}
			}
			else
			{
				m_off -= 0x4000;
				assert(m_off > 0); assert(m_off <= 0x7fff);
				if (m_len <= M4_MAX_LEN)
					*op++ = LZO_BYTE(M4_MARKER |
									 ((m_off & 0x4000) >> 11) | (m_len - 2));
				else
				{
					m_len -= M4_MAX_LEN;
					*op++ = LZO_BYTE(M4_MARKER | ((m_off & 0x4000) >> 11));
m3_m4_len:
					while (m_len > 255)
					{
						m_len -= 255;
						*op++ = 0;
					}
					assert(m_len > 0);
					*op++ = LZO_BYTE(m_len);
				}
			}

m3_m4_offset:
			*op++ = LZO_BYTE((m_off & 63) << 2);
			*op++ = LZO_BYTE(m_off >> 6);
		}

		ii = ip;
		if (ip >= ip_end)
			break;
	}

	*out_len = (lzo_uint)(op - out);
	return pd(in_end,ii);
}

LZO_PUBLIC(int)
DO_COMPRESS	  ( const lzo_byte *in , lzo_uint  in_len,
						 lzo_byte *out, lzo_uintp out_len,
						 lzo_voidp wrkmem )
{
	lzo_byte *op = out;
	lzo_uint t;

	if (in_len <= M2_MAX_LEN + 5)
		t = in_len;
	else
	{
		t = do_compress(in,in_len,op,out_len,wrkmem);
		op += *out_len;
	}

	if (t > 0)
	{
		const lzo_byte *ii = in + in_len - t;

		if (op == out && t <= 238)
			*op++ = LZO_BYTE(17 + t);
		else if (t <= 3)
			op[-2] |= LZO_BYTE(t);
		else if (t <= 18)
			*op++ = LZO_BYTE(t - 3);
		else
		{
			lzo_uint tt = t - 18;

			*op++ = 0;
			while (tt > 255)
			{
				tt -= 255;
				*op++ = 0;
			}
			assert(tt > 0);
			*op++ = LZO_BYTE(tt);
		}
		do *op++ = *ii++; while (--t > 0);
	}

	*op++ = M4_MARKER | 1;
	*op++ = 0;
	*op++ = 0;

	*out_len = (lzo_uint)(op - out);
	return LZO_E_OK;
}

#undef do_compress
#undef DO_COMPRESS
#undef LZO_HASH

#undef LZO_TEST_DECOMPRESS_OVERRUN
#undef LZO_TEST_DECOMPRESS_OVERRUN_INPUT
#undef LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT
#undef LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND
#undef DO_DECOMPRESS
#define DO_DECOMPRESS	   lzo1x_decompress

#if defined(LZO_TEST_DECOMPRESS_OVERRUN)
#  if !defined(LZO_TEST_DECOMPRESS_OVERRUN_INPUT)
#	define LZO_TEST_DECOMPRESS_OVERRUN_INPUT	   2
#  endif
#  if !defined(LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT)
#	define LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT	  2
#  endif
#  if !defined(LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND)
#	define LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND
#  endif
#endif

#undef TEST_IP
#undef TEST_OP
#undef TEST_LOOKBEHIND
#undef NEED_IP
#undef NEED_OP
#undef HAVE_TEST_IP
#undef HAVE_TEST_OP
#undef HAVE_NEED_IP
#undef HAVE_NEED_OP
#undef HAVE_ANY_IP
#undef HAVE_ANY_OP

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_INPUT)
#  if (LZO_TEST_DECOMPRESS_OVERRUN_INPUT >= 1)
#	define TEST_IP			 (ip < ip_end)
#  endif
#  if (LZO_TEST_DECOMPRESS_OVERRUN_INPUT >= 2)
#	define NEED_IP(x) \
			if ((lzo_uint)(ip_end - ip) < (lzo_uint)(x))  goto input_overrun
#  endif
#endif

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT)
#  if (LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT >= 1)
#	define TEST_OP			 (op <= op_end)
#  endif
#  if (LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT >= 2)
#	undef TEST_OP
#	define NEED_OP(x) \
			if ((lzo_uint)(op_end - op) < (lzo_uint)(x))  goto output_overrun
#  endif
#endif

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND)
#  define TEST_LOOKBEHIND(m_pos,out)	if (m_pos < out) goto lookbehind_overrun
#else
#  define TEST_LOOKBEHIND(m_pos,op)	 ((void) 0)
#endif

#if !defined(LZO_EOF_CODE) && !defined(TEST_IP)
#  define TEST_IP			   (ip < ip_end)
#endif

#if defined(TEST_IP)
#  define HAVE_TEST_IP
#else
#  define TEST_IP			   1
#endif
#if defined(TEST_OP)
#  define HAVE_TEST_OP
#else
#  define TEST_OP			   1
#endif

#if defined(NEED_IP)
#  define HAVE_NEED_IP
#else
#  define NEED_IP(x)			((void) 0)
#endif
#if defined(NEED_OP)
#  define HAVE_NEED_OP
#else
#  define NEED_OP(x)			((void) 0)
#endif

#if defined(HAVE_TEST_IP) || defined(HAVE_NEED_IP)
#  define HAVE_ANY_IP
#endif
#if defined(HAVE_TEST_OP) || defined(HAVE_NEED_OP)
#  define HAVE_ANY_OP
#endif

#undef __COPY4
#define __COPY4(dst,src)	* (lzo_uint32p)(dst) = * (const lzo_uint32p)(src)

#undef COPY4
#if defined(LZO_UNALIGNED_OK_4)
#  define COPY4(dst,src)	__COPY4(dst,src)
#elif defined(LZO_ALIGNED_OK_4)
#  define COPY4(dst,src)	__COPY4((lzo_ptr_t)(dst),(lzo_ptr_t)(src))
#endif

#if defined(DO_DECOMPRESS)
LZO_PUBLIC(int)
DO_DECOMPRESS  ( const lzo_byte *in , lzo_uint  in_len,
					   lzo_byte *out, lzo_uintp out_len,
					   lzo_voidp wrkmem )
#endif
{
	register lzo_byte *op;
	register const lzo_byte *ip;
	register lzo_uint t;
	register const lzo_byte *m_pos;

	const lzo_byte * const ip_end = in + in_len;
#if defined(HAVE_ANY_OP)
	lzo_byte * const op_end = out + *out_len;
#endif

	LZO_UNUSED(wrkmem);

	*out_len = 0;

	op = out;
	ip = in;

	if (*ip > 17)
	{
		t = *ip++ - 17;
		if (t < 4)
			goto match_next;
		assert(t > 0); NEED_OP(t); NEED_IP(t+1);
		do *op++ = *ip++; while (--t > 0);
		goto first_literal_run;
	}

	while (TEST_IP && TEST_OP)
	{
		t = *ip++;
		if (t >= 16)
			goto match;
		if (t == 0)
		{
			NEED_IP(1);
			while (*ip == 0)
			{
				t += 255;
				ip++;
				NEED_IP(1);
			}
			t += 15 + *ip++;
		}
		assert(t > 0); NEED_OP(t+3); NEED_IP(t+4);
#if defined(LZO_UNALIGNED_OK_4) || defined(LZO_ALIGNED_OK_4)
#if !defined(LZO_UNALIGNED_OK_4)
		if (PTR_ALIGNED2_4(op,ip))
		{
#endif
		COPY4(op,ip);
		op += 4; ip += 4;
		if (--t > 0)
		{
			if (t >= 4)
			{
				do {
					COPY4(op,ip);
					op += 4; ip += 4; t -= 4;
				} while (t >= 4);
				if (t > 0) do *op++ = *ip++; while (--t > 0);
			}
			else
				do *op++ = *ip++; while (--t > 0);
		}
#if !defined(LZO_UNALIGNED_OK_4)
		}
		else
#endif
#endif
#if !defined(LZO_UNALIGNED_OK_4)
		{
			*op++ = *ip++; *op++ = *ip++; *op++ = *ip++;
			do *op++ = *ip++; while (--t > 0);
		}
#endif

first_literal_run:

		t = *ip++;
		if (t >= 16)
			goto match;
		m_pos = op - (1 + M2_MAX_OFFSET);
		m_pos -= t >> 2;
		m_pos -= *ip++ << 2;
		TEST_LOOKBEHIND(m_pos,out); NEED_OP(3);
		*op++ = *m_pos++; *op++ = *m_pos++; *op++ = *m_pos;
		goto match_done;

		while (TEST_IP && TEST_OP)
		{
match:
			if (t >= 64)
			{
#if defined(LZO1X)
				m_pos = op - 1;
				m_pos -= (t >> 2) & 7;
				m_pos -= *ip++ << 3;
				t = (t >> 5) - 1;
#endif
				TEST_LOOKBEHIND(m_pos,out); assert(t > 0); NEED_OP(t+3-1);
				goto copy_match;
			}
			else if (t >= 32)
			{
				t &= 31;
				if (t == 0)
				{
					NEED_IP(1);
					while (*ip == 0)
					{
						t += 255;
						ip++;
						NEED_IP(1);
					}
					t += 31 + *ip++;
				}
#if defined(LZO_UNALIGNED_OK_2) && (LZO_BYTE_ORDER == LZO_LITTLE_ENDIAN)
				m_pos = op - 1;
				m_pos -= (* (const lzo_ushortp) ip) >> 2;
#else
				m_pos = op - 1;
				m_pos -= (ip[0] >> 2) + (ip[1] << 6);
#endif
				ip += 2;
			}
			else if (t >= 16)
			{
				m_pos = op;
				m_pos -= (t & 8) << 11;
				t &= 7;
				if (t == 0)
				{
					NEED_IP(1);
					while (*ip == 0)
					{
						t += 255;
						ip++;
						NEED_IP(1);
					}
					t += 7 + *ip++;
				}
#if defined(LZO_UNALIGNED_OK_2) && (LZO_BYTE_ORDER == LZO_LITTLE_ENDIAN)
				m_pos -= (* (const lzo_ushortp) ip) >> 2;
#else
				m_pos -= (ip[0] >> 2) + (ip[1] << 6);
#endif
				ip += 2;
				if (m_pos == op)
					goto eof_found;
				m_pos -= 0x4000;
			}
			else
			{
				m_pos = op - 1;
				m_pos -= t >> 2;
				m_pos -= *ip++ << 2;
				TEST_LOOKBEHIND(m_pos,out); NEED_OP(2);
				*op++ = *m_pos++; *op++ = *m_pos;
				goto match_done;
			}

			TEST_LOOKBEHIND(m_pos,out); assert(t > 0); NEED_OP(t+3-1);
#if defined(LZO_UNALIGNED_OK_4) || defined(LZO_ALIGNED_OK_4)
#if !defined(LZO_UNALIGNED_OK_4)
			if (t >= 2 * 4 - (3 - 1) && PTR_ALIGNED2_4(op,m_pos))
			{
				assert((op - m_pos) >= 4);
#else
			if (t >= 2 * 4 - (3 - 1) && (op - m_pos) >= 4)
			{
#endif
				COPY4(op,m_pos);
				op += 4; m_pos += 4; t -= 4 - (3 - 1);
				do {
					COPY4(op,m_pos);
					op += 4; m_pos += 4; t -= 4;
				} while (t >= 4);
				if (t > 0) do *op++ = *m_pos++; while (--t > 0);
			}
			else
#endif
			{
copy_match:
				*op++ = *m_pos++; *op++ = *m_pos++;
				do *op++ = *m_pos++; while (--t > 0);
			}

match_done:
			t = ip[-2] & 3;
			if (t == 0)
				break;

match_next:
			assert(t > 0); NEED_OP(t); NEED_IP(t+1);
			do *op++ = *ip++; while (--t > 0);
			t = *ip++;
		}
	}

#if defined(HAVE_TEST_IP) || defined(HAVE_TEST_OP)
	*out_len = op - out;
	return LZO_E_EOF_NOT_FOUND;
#endif

eof_found:
	assert(t == 1);
	*out_len = (lzo_uint)(op - out);
	return (ip == ip_end ? LZO_E_OK :
		   (ip < ip_end  ? LZO_E_INPUT_NOT_CONSUMED : LZO_E_INPUT_OVERRUN));

#if defined(HAVE_NEED_IP)
input_overrun:
	*out_len = op - out;
	return LZO_E_INPUT_OVERRUN;
#endif

#if defined(HAVE_NEED_OP)
output_overrun:
	*out_len = op - out;
	return LZO_E_OUTPUT_OVERRUN;
#endif

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND)
lookbehind_overrun:
	*out_len = op - out;
	return LZO_E_LOOKBEHIND_OVERRUN;
#endif
}

#define LZO_TEST_DECOMPRESS_OVERRUN
#undef DO_DECOMPRESS
#define DO_DECOMPRESS	   lzo1x_decompress_safe

#if defined(LZO_TEST_DECOMPRESS_OVERRUN)
#  if !defined(LZO_TEST_DECOMPRESS_OVERRUN_INPUT)
#	define LZO_TEST_DECOMPRESS_OVERRUN_INPUT	   2
#  endif
#  if !defined(LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT)
#	define LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT	  2
#  endif
#  if !defined(LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND)
#	define LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND
#  endif
#endif

#undef TEST_IP
#undef TEST_OP
#undef TEST_LOOKBEHIND
#undef NEED_IP
#undef NEED_OP
#undef HAVE_TEST_IP
#undef HAVE_TEST_OP
#undef HAVE_NEED_IP
#undef HAVE_NEED_OP
#undef HAVE_ANY_IP
#undef HAVE_ANY_OP


#if 0

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_INPUT)
#  if (LZO_TEST_DECOMPRESS_OVERRUN_INPUT >= 1)
#	define TEST_IP			 (ip < ip_end)
#  endif
#  if (LZO_TEST_DECOMPRESS_OVERRUN_INPUT >= 2)
#	define NEED_IP(x) \
			if ((lzo_uint)(ip_end - ip) < (lzo_uint)(x))  goto input_overrun
#  endif
#endif

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT)
#  if (LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT >= 1)
#	define TEST_OP			 (op <= op_end)
#  endif
#  if (LZO_TEST_DECOMPRESS_OVERRUN_OUTPUT >= 2)
#	undef TEST_OP
#	define NEED_OP(x) \
			if ((lzo_uint)(op_end - op) < (lzo_uint)(x))  goto output_overrun
#  endif
#endif

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND)
#  define TEST_LOOKBEHIND(m_pos,out)	if (m_pos < out) goto lookbehind_overrun
#else
#  define TEST_LOOKBEHIND(m_pos,op)	 ((void) 0)
#endif

#if !defined(LZO_EOF_CODE) && !defined(TEST_IP)
#  define TEST_IP			   (ip < ip_end)
#endif

#if defined(TEST_IP)
#  define HAVE_TEST_IP
#else
#  define TEST_IP			   1
#endif
#if defined(TEST_OP)
#  define HAVE_TEST_OP
#else
#  define TEST_OP			   1
#endif

#if defined(NEED_IP)
#  define HAVE_NEED_IP
#else
#  define NEED_IP(x)			((void) 0)
#endif
#if defined(NEED_OP)
#  define HAVE_NEED_OP
#else
#  define NEED_OP(x)			((void) 0)
#endif

#if defined(HAVE_TEST_IP) || defined(HAVE_NEED_IP)
#  define HAVE_ANY_IP
#endif
#if defined(HAVE_TEST_OP) || defined(HAVE_NEED_OP)
#  define HAVE_ANY_OP
#endif

#undef __COPY4
#define __COPY4(dst,src)	* (lzo_uint32p)(dst) = * (const lzo_uint32p)(src)

#undef COPY4
#if defined(LZO_UNALIGNED_OK_4)
#  define COPY4(dst,src)	__COPY4(dst,src)
#elif defined(LZO_ALIGNED_OK_4)
#  define COPY4(dst,src)	__COPY4((lzo_ptr_t)(dst),(lzo_ptr_t)(src))
#endif

#if defined(DO_DECOMPRESS)
LZO_PUBLIC(int)
DO_DECOMPRESS  ( const lzo_byte *in , lzo_uint  in_len,
					   lzo_byte *out, lzo_uintp out_len,
					   lzo_voidp wrkmem )
#endif
{
	register lzo_byte *op;
	register const lzo_byte *ip;
	register lzo_uint t;
	register const lzo_byte *m_pos;

	const lzo_byte * const ip_end = in + in_len;
#if defined(HAVE_ANY_OP)
	lzo_byte * const op_end = out + *out_len;
#endif

	LZO_UNUSED(wrkmem);

	*out_len = 0;

	op = out;
	ip = in;

	if (*ip > 17)
	{
		t = *ip++ - 17;
		if (t < 4)
			goto match_next;
		assert(t > 0); NEED_OP(t); NEED_IP(t+1);
		do *op++ = *ip++; while (--t > 0);
		goto first_literal_run;
	}

	while (TEST_IP && TEST_OP)
	{
		t = *ip++;
		if (t >= 16)
			goto match;
		if (t == 0)
		{
			NEED_IP(1);
			while (*ip == 0)
			{
				t += 255;
				ip++;
				NEED_IP(1);
			}
			t += 15 + *ip++;
		}
		assert(t > 0); NEED_OP(t+3); NEED_IP(t+4);
#if defined(LZO_UNALIGNED_OK_4) || defined(LZO_ALIGNED_OK_4)
#if !defined(LZO_UNALIGNED_OK_4)
		if (PTR_ALIGNED2_4(op,ip))
		{
#endif
		COPY4(op,ip);
		op += 4; ip += 4;
		if (--t > 0)
		{
			if (t >= 4)
			{
				do {
					COPY4(op,ip);
					op += 4; ip += 4; t -= 4;
				} while (t >= 4);
				if (t > 0) do *op++ = *ip++; while (--t > 0);
			}
			else
				do *op++ = *ip++; while (--t > 0);
		}
#if !defined(LZO_UNALIGNED_OK_4)
		}
		else
#endif
#endif
#if !defined(LZO_UNALIGNED_OK_4)
		{
			*op++ = *ip++; *op++ = *ip++; *op++ = *ip++;
			do *op++ = *ip++; while (--t > 0);
		}
#endif

first_literal_run:

		t = *ip++;
		if (t >= 16)
			goto match;
#if defined(LZO1Z)
		t = (1 + M2_MAX_OFFSET) + (t << 6) + (*ip++ >> 2);
		m_pos = op - t;
		last_m_off = t;
#else
		m_pos = op - (1 + M2_MAX_OFFSET);
		m_pos -= t >> 2;
		m_pos -= *ip++ << 2;
#endif
		TEST_LOOKBEHIND(m_pos,out); NEED_OP(3);
		*op++ = *m_pos++; *op++ = *m_pos++; *op++ = *m_pos;
		goto match_done;

		while (TEST_IP && TEST_OP)
		{
match:
			if (t >= 64)
			{
#if defined(LZO1X)
				m_pos = op - 1;
				m_pos -= (t >> 2) & 7;
				m_pos -= *ip++ << 3;
				t = (t >> 5) - 1;
#elif defined(LZO1Y)
				m_pos = op - 1;
				m_pos -= (t >> 2) & 3;
				m_pos -= *ip++ << 2;
				t = (t >> 4) - 3;
#elif defined(LZO1Z)
				{
					lzo_uint off = t & 0x1f;
					m_pos = op;
					if (off >= 0x1c)
					{
						assert(last_m_off > 0);
						m_pos -= last_m_off;
					}
					else
					{
						off = 1 + (off << 6) + (*ip++ >> 2);
						m_pos -= off;
						last_m_off = off;
					}
				}
				t = (t >> 5) - 1;
#endif
				TEST_LOOKBEHIND(m_pos,out); assert(t > 0); NEED_OP(t+3-1);
				goto copy_match;
			}
			else if (t >= 32)
			{
				t &= 31;
				if (t == 0)
				{
					NEED_IP(1);
					while (*ip == 0)
					{
						t += 255;
						ip++;
						NEED_IP(1);
					}
					t += 31 + *ip++;
				}
#if defined(LZO1Z)
				{
					lzo_uint off = 1 + (ip[0] << 6) + (ip[1] >> 2);
					m_pos = op - off;
					last_m_off = off;
				}
#elif defined(LZO_UNALIGNED_OK_2) && (LZO_BYTE_ORDER == LZO_LITTLE_ENDIAN)
				m_pos = op - 1;
				m_pos -= (* (const lzo_ushortp) ip) >> 2;
#else
				m_pos = op - 1;
				m_pos -= (ip[0] >> 2) + (ip[1] << 6);
#endif
				ip += 2;
			}
			else if (t >= 16)
			{
				m_pos = op;
				m_pos -= (t & 8) << 11;
				t &= 7;
				if (t == 0)
				{
					NEED_IP(1);
					while (*ip == 0)
					{
						t += 255;
						ip++;
						NEED_IP(1);
					}
					t += 7 + *ip++;
				}
#if defined(LZO1Z)
				m_pos -= (ip[0] << 6) + (ip[1] >> 2);
#elif defined(LZO_UNALIGNED_OK_2) && (LZO_BYTE_ORDER == LZO_LITTLE_ENDIAN)
				m_pos -= (* (const lzo_ushortp) ip) >> 2;
#else
				m_pos -= (ip[0] >> 2) + (ip[1] << 6);
#endif
				ip += 2;
				if (m_pos == op)
					goto eof_found;
				m_pos -= 0x4000;
#if defined(LZO1Z)
				last_m_off = op - m_pos;
#endif
			}
			else
			{
#if defined(LZO1Z)
				t = 1 + (t << 6) + (*ip++ >> 2);
				m_pos = op - t;
				last_m_off = t;
#else
				m_pos = op - 1;
				m_pos -= t >> 2;
				m_pos -= *ip++ << 2;
#endif
				TEST_LOOKBEHIND(m_pos,out); NEED_OP(2);
				*op++ = *m_pos++; *op++ = *m_pos;
				goto match_done;
			}

			TEST_LOOKBEHIND(m_pos,out); assert(t > 0); NEED_OP(t+3-1);
#if defined(LZO_UNALIGNED_OK_4) || defined(LZO_ALIGNED_OK_4)
#if !defined(LZO_UNALIGNED_OK_4)
			if (t >= 2 * 4 - (3 - 1) && PTR_ALIGNED2_4(op,m_pos))
			{
				assert((op - m_pos) >= 4);
#else
			if (t >= 2 * 4 - (3 - 1) && (op - m_pos) >= 4)
			{
#endif
				COPY4(op,m_pos);
				op += 4; m_pos += 4; t -= 4 - (3 - 1);
				do {
					COPY4(op,m_pos);
					op += 4; m_pos += 4; t -= 4;
				} while (t >= 4);
				if (t > 0) do *op++ = *m_pos++; while (--t > 0);
			}
			else
#endif
			{
copy_match:
				*op++ = *m_pos++; *op++ = *m_pos++;
				do *op++ = *m_pos++; while (--t > 0);
			}


match_done:
#if defined(LZO1Z)
			t = ip[-1] & 3;
#else
			t = ip[-2] & 3;
#endif
			if (t == 0)
				break;

match_next:
			assert(t > 0); NEED_OP(t); NEED_IP(t+1);
			do *op++ = *ip++; while (--t > 0);
			t = *ip++;
		}
	}

#if defined(HAVE_TEST_IP) || defined(HAVE_TEST_OP)
	*out_len = op - out;
	return LZO_E_EOF_NOT_FOUND;
#endif

eof_found:
	assert(t == 1);
	*out_len = op - out;
	return (ip == ip_end ? LZO_E_OK :
		   (ip < ip_end  ? LZO_E_INPUT_NOT_CONSUMED : LZO_E_INPUT_OVERRUN));

#if defined(HAVE_NEED_IP)
input_overrun:
	*out_len = op - out;
	return LZO_E_INPUT_OVERRUN;
#endif

#if defined(HAVE_NEED_OP)
output_overrun:
	*out_len = op - out;
	return LZO_E_OUTPUT_OVERRUN;
#endif

#if defined(LZO_TEST_DECOMPRESS_OVERRUN_LOOKBEHIND)
lookbehind_overrun:
	*out_len = op - out;
	return LZO_E_LOOKBEHIND_OVERRUN;
#endif
}

#endif

/***** End of minilzo.c *****/
