/* $Id$ */

#ifndef  YAPF_HPP
#define  YAPF_HPP



#include "track_dir.hpp"

EXTERN_C_BEGIN
#include "../depot.h"
#include "../road_map.h"
#include "../tunnel_map.h"
#include "../bridge_map.h"
#include "../bridge.h"
#include "../station.h"
#include "../station_map.h"
#include "../vehicle.h"
#include "../variables.h"
#include "../functions.h"
#include "yapf.h"
#include "../pathfind.h"
#include "../waypoint.h"
#include "../debug.h"
EXTERN_C_END

EXTERN_C_BEGIN
	extern Patches _patches_newgame;
	extern uint64 _rdtsc(void);
EXTERN_C_END

#include <limits.h>
#include <new>

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#else
#  include <time.h>
#endif

struct CPerformanceTimer
{
	int64    m_start;
	int64    m_acc;

	CPerformanceTimer() : m_start(0), m_acc(0) {}

	FORCEINLINE void Start() {m_start = QueryTime();}
	FORCEINLINE void Stop() {m_acc += QueryTime() - m_start;}
	FORCEINLINE int Get(int64 coef) {return (int)(m_acc * coef / QueryFrequency());}

#if !defined(UNITTEST) && 1
	FORCEINLINE int64 QueryTime() {return _rdtsc();}
	FORCEINLINE int64 QueryFrequency() {return ((int64)2200 * 1000000);}
#elif defined(_WIN32) || defined(_WIN64)
	FORCEINLINE int64 QueryTime() {LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart;}
	FORCEINLINE int64 QueryFrequency() {LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart;}
#elif defined(CLOCK_THREAD_CPUTIME_ID)
	FORCEINLINE int64 QueryTime()
	{
		timespec ts;
		int ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
		if (ret != 0) return 0;
		return (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
	}
	FORCEINLINE int64 QueryFrequency()
	{
		return 1000000000;
	}
#else
	int64 QueryTime() {return 0;}
	int64 QueryFrequency() {return 1;}
#endif
};

struct CPerfStartReal
{
	CPerformanceTimer* m_pperf;

	FORCEINLINE CPerfStartReal(CPerformanceTimer& perf)	: m_pperf(&perf) {if (m_pperf != NULL) m_pperf->Start();}
	FORCEINLINE ~CPerfStartReal() {Stop();}
	FORCEINLINE void Stop() {if (m_pperf != NULL) {m_pperf->Stop(); m_pperf = NULL;}}
};

struct CPerfStartFake
{
	FORCEINLINE CPerfStartFake(CPerformanceTimer& perf) {}
	FORCEINLINE ~CPerfStartFake() {}
	FORCEINLINE void Stop() {}
};

typedef CPerfStartFake CPerfStart;


//#undef FORCEINLINE
//#define FORCEINLINE inline

#include "crc32.hpp"
#include "blob.hpp"
#include "fixedsizearray.hpp"
#include "array.hpp"
#include "hashtable.hpp"
#include "binaryheap.hpp"
#include "nodelist.hpp"
#include "yapf_base.hpp"
#include "yapf_node.hpp"
#include "yapf_common.hpp"
#include "follow_track.hpp"
#include "yapf_costbase.hpp"
#include "yapf_costcache.hpp"


#endif /* YAPF_HPP */
