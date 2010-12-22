/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pf_performance_timer.hpp Performance timer for pathfinders. */

#ifndef PF_PERFORMANCE_TIMER_HPP
#define PF_PERFORMANCE_TIMER_HPP

extern uint64 ottd_rdtsc();

struct CPerformanceTimer
{
	int64    m_start;
	int64    m_acc;

	CPerformanceTimer() : m_start(0), m_acc(0) {}

	FORCEINLINE void Start()
	{
		m_start = QueryTime();
	}

	FORCEINLINE void Stop()
	{
		m_acc += QueryTime() - m_start;
	}

	FORCEINLINE int Get(int64 coef)
	{
		return (int)(m_acc * coef / QueryFrequency());
	}

	FORCEINLINE int64 QueryTime()
	{
		return ottd_rdtsc();
	}

	FORCEINLINE int64 QueryFrequency()
	{
		return ((int64)2200 * 1000000);
	}
};

struct CPerfStartReal
{
	CPerformanceTimer *m_pperf;

	FORCEINLINE CPerfStartReal(CPerformanceTimer& perf) : m_pperf(&perf)
	{
		if (m_pperf != NULL) m_pperf->Start();
	}

	FORCEINLINE ~CPerfStartReal()
	{
		Stop();
	}

	FORCEINLINE void Stop()
	{
		if (m_pperf != NULL) {
			m_pperf->Stop();
			m_pperf = NULL;
		}
	}
};

struct CPerfStartFake
{
	FORCEINLINE CPerfStartFake(CPerformanceTimer& perf) {}
	FORCEINLINE ~CPerfStartFake() {}
	FORCEINLINE void Stop() {}
};

typedef CPerfStartFake CPerfStart;

#endif /* PF_PERFORMANCE_TIMER_HPP */
