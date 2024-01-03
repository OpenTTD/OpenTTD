/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cpu.h Functions related to CPU specific instructions. */

#ifndef CPU_H
#define CPU_H

/**
 * Get the tick counter from the CPU (high precision timing).
 * @return The count.
 */
uint64_t ottd_rdtsc();

/**
 * Get the CPUID information from the CPU.
 * @param info The retrieved info. All zeros on architectures without CPUID.
 * @param type The information this instruction should retrieve.
 */
void ottd_cpuid(int info[4], int type);

/**
 * Check whether the current CPU has the given flag.
 * @param type  The type to be passing to cpuid (usually 1).
 * @param index The index in the returned info array.
 * @param bit   The bit index that needs to be set.
 * @return The value of the bit, or false when there is no CPUID or the type is not available.
 */
bool HasCPUIDFlag(uint type, uint index, uint bit);

#endif /* CPU_H */
