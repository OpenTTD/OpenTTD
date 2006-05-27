/* $Id$ */

#define UNITTEST

extern int num_tests_failed;
extern int num_tests_total;

extern bool _dbg;
#define DBG if(_dbg) printf


#define CHECK_INT(case_num, val, should_be) \
{ \
	if((val) != (should_be)) { \
	res |= (1 << case_num); \
	printf("\n****** ERROR in case %d: " #val " = %d (should be %d)!", case_num, (val), (should_be)); \
	} \
}

typedef int(*TESTPROC)(bool silent);

//#undef FORCEINLINE
//#define FORCEINLINE


#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#else
#endif





