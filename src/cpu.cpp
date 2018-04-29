/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cpu.cpp OS/CPU/compiler dependent CPU specific calls. */

#include "stdafx.h"
#include "core/bitmath_func.hpp"

#include "safeguards.h"

#undef RDTSC_AVAILABLE

/* rdtsc for MSC_VER, uses simple inline assembly, or _rdtsc
 * from external win64.asm because VS2005 does not support inline assembly */
#if defined(_MSC_VER) && !defined(RDTSC_AVAILABLE)
#include <intrin.h>
uint64 ottd_rdtsc()
{
	return __rdtsc();
}
#define RDTSC_AVAILABLE
#endif

/* rdtsc for OS/2. Hopefully this works, who knows */
#if defined (__WATCOMC__) && !defined(RDTSC_AVAILABLE)
unsigned __int64 ottd_rdtsc();
# pragma aux ottd_rdtsc = 0x0F 0x31 value [edx eax] parm nomemory modify exact [edx eax] nomemory;
# define RDTSC_AVAILABLE
#endif

/* rdtsc for all other *nix-en (hopefully). Use GCC syntax */
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__DJGPP__) && !defined(RDTSC_AVAILABLE)
uint64 ottd_rdtsc()
{
	uint32 high, low;
	__asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
	return ((uint64)high << 32) | low;
}
# define RDTSC_AVAILABLE
#endif

/* rdtsc for PPC which has this not */
#if (defined(__POWERPC__) || defined(__powerpc__)) && !defined(RDTSC_AVAILABLE)
uint64 ottd_rdtsc()
{
	uint32 high = 0, high2 = 0, low;
	/* PPC does not have rdtsc, so we cheat by reading the two 32-bit time-counters
	 * it has, 'Move From Time Base (Upper)'. Since these are two reads, in the
	 * very unlikely event that the lower part overflows to the upper part while we
	 * read it; we double-check and reread the registers */
	asm volatile (
				  "mftbu %0\n"
				  "mftb %1\n"
				  "mftbu %2\n"
				  "cmpw %3,%4\n"
				  "bne- $-16\n"
				  : "=r" (high), "=r" (low), "=r" (high2)
				  : "0" (high), "2" (high2)
				  );
	return ((uint64)high << 32) | low;
}
# define RDTSC_AVAILABLE
#endif

/* In all other cases we have no support for rdtsc. No major issue,
 * you just won't be able to profile your code with TIC()/TOC() */
#if !defined(RDTSC_AVAILABLE)
#warning "(non-fatal) No support for rdtsc(), you won't be able to profile with TIC/TOC"
uint64 ottd_rdtsc() {return 0;}
#endif


/**
 * Definitions for CPU detection:
 *
 * MSVC offers cpu information while gcc only implements in gcc 4.8
 * __builtin_cpu_supports and friends
 * http://msdn.microsoft.com/library/vstudio/hskdteyh%28v=vs.100%29.aspx
 * http://gcc.gnu.org/onlinedocs/gcc/X86-Built-in-Functions.html
 *
 * Other platforms/architectures don't have CPUID, so zero the info and then
 * most (if not all) of the features are set as if they do not exist.
 */
#if defined(_MSC_VER)
void ottd_cpuid(int info[4], int type)
{
	__cpuid(info, type);
}
#elif defined(__x86_64__) || defined(__i386)
void ottd_cpuid(int info[4], int type)
{
#if defined(__i386) && defined(__PIC__)
	/* The easy variant would be just cpuid, however... ebx is being used by the GOT (Global Offset Table)
	 * in case of PIC;
	 * clobbering ebx is no alternative: some compiler versions don't like this
	 * and will issue an error message like
	 *   "can't find a register in class 'BREG' while reloading 'asm'"
	 */
	__asm__ __volatile__ (
			"xchgl %%ebx, %1 \n\t"
			"cpuid           \n\t"
			"xchgl %%ebx, %1 \n\t"
			: "=a" (info[0]), "=r" (info[1]), "=c" (info[2]), "=d" (info[3])
			/* It is safe to write "=r" for (info[1]) as in case that PIC is enabled for i386,
			 * the compiler will not choose EBX as target register (but something else).
			 */
			: "a" (type)
	);
#else
	__asm__ __volatile__ (
			"cpuid           \n\t"
			: "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
			: "a" (type)
	);
#endif /* i386 PIC */
}
#else
void ottd_cpuid(int info[4], int type)
{
	info[0] = info[1] = info[2] = info[3] = 0;
}
#endif

bool HasCPUIDFlag(uint type, uint index, uint bit)
{
	int cpu_info[4] = {-1};
	ottd_cpuid(cpu_info, 0);
	uint max_info_type = cpu_info[0];
	if (max_info_type < type) return false;

	ottd_cpuid(cpu_info, type);
	return HasBit(cpu_info[index], bit);
}
