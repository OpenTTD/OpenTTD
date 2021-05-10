/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file G5_detector.cpp Detection for G5 machines (PowerPC). */

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/machine.h>
#include <stdio.h>


#ifndef CPU_SUBTYPE_POWERPC_970
#define CPU_SUBTYPE_POWERPC_970 ((cpu_subtype_t) 100)
#endif

/* this function is a lightly modified version of some code from Apple's developer homepage to detect G5 CPUs at runtime */
int main()
{
	host_basic_info_data_t hostInfo;
	mach_msg_type_number_t infoCount;
	boolean_t is_G5;

	infoCount = HOST_BASIC_INFO_COUNT;
	host_info(mach_host_self(), HOST_BASIC_INFO,
			  (host_info_t)&hostInfo, &infoCount);

	 is_G5 = ((hostInfo.cpu_type == CPU_TYPE_POWERPC) &&
			(hostInfo.cpu_subtype == CPU_SUBTYPE_POWERPC_970));
	 if (is_G5)
		 printf("1");
}
