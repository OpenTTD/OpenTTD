/* $Id$ */

/** @file G5_detector.cpp Detection for G5 machines (PowerPC). */

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/machine.h>
#include <stdio.h>


#ifndef CPU_SUBTYPE_POWERPC_970
#define CPU_SUBTYPE_POWERPC_970 ((cpu_subtype_t) 100)
#endif

// this function is a lightly modified version of some code from Apple's developer homepage to detect G5 CPUs at runtime
main()
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
