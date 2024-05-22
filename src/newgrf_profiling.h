/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file newgrf_profiling.h Profiling of NewGRF action 2 handling. */

#ifndef NEWGRF_PROFILING_H
#define NEWGRF_PROFILING_H

#include "stdafx.h"
#include "timer/timer_game_calendar.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"


/**
 * Callback profiler for NewGRF development
 */
struct NewGRFProfiler {
	NewGRFProfiler(const GRFFile *grffile);
	~NewGRFProfiler();

	void BeginResolve(const ResolverObject &resolver);
	void EndResolve(const SpriteGroup *result);
	void RecursiveResolve();

	void Start();
	uint32_t Finish();
	void Abort();
	std::string GetOutputFilename() const;

	static void StartTimer(uint64_t ticks);
	static void AbortTimer();
	static uint32_t FinishAll();

	/** Measurement of a single sprite group resolution */
	struct Call {
		uint32_t root_sprite;  ///< Pseudo-sprite index in GRF file
		uint32_t item;         ///< Local ID of item being resolved for
		uint32_t result;       ///< Result of callback
		uint32_t subs;         ///< Sub-calls to other sprite groups
		uint32_t time;         ///< Time taken for resolution (microseconds)
		uint64_t tick;         ///< Game tick
		CallbackID cb;       ///< Callback ID
		GrfSpecFeature feat; ///< GRF feature being resolved for
	};

	const GRFFile *grffile;  ///< Which GRF is being profiled
	bool active;             ///< Is this profiler collecting data
	uint64_t start_tick;       ///< Tick number this profiler was started on
	Call cur_call;           ///< Data for current call in progress
	std::vector<Call> calls; ///< All calls collected so far
};

extern std::vector<NewGRFProfiler> _newgrf_profilers;

#endif /* NEWGRF_PROFILING_H */
