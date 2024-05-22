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
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
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
#elif defined(__e2k__) /* MCST Elbrus 2000*/
void ottd_cpuid(int info[4], int type)
{
	info[0] = info[1] = info[2] = info[3] = 0;
	if (type == 0) {
		info[0] = 1;
	} else if (type == 1) {
#if defined(__SSE4_1__)
		info[2] |= (1<<19); /* HasCPUIDFlag(1, 2, 19) */
#endif
#if defined(__SSSE3__)
		info[2] |= (1<<9); /* HasCPUIDFlag(1, 2, 9) */
#endif
#if defined(__SSE2__)
		info[3] |= (1<<26); /* HasCPUIDFlag(1, 3, 26) */
#endif
	}
}
#else
void ottd_cpuid(int info[4], int)
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
