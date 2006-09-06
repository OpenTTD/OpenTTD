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
#include "../date.h"
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

	FORCEINLINE int64 QueryTime() {return _rdtsc();}
	FORCEINLINE int64 QueryFrequency() {return ((int64)2200 * 1000000);}
};

struct CPerfStartReal
{
	CPerformanceTimer* m_pperf;

	FORCEINLINE CPerfStartReal(CPerformanceTimer& perf) : m_pperf(&perf) {if (m_pperf != NULL) m_pperf->Start();}
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
