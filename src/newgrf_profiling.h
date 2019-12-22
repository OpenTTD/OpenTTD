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
#include "date_type.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"

#include <vector>
#include <string>
#include <memory>

/**
 * Callback profiler for NewGRF development
 */
struct NewGRFProfiler {
	NewGRFProfiler(const GRFFile *grffile, Date end_date);
	~NewGRFProfiler();

	void BeginResolve(const ResolverObject &resolver);
	void EndResolve(const SpriteGroup *result);
	void RecursiveResolve();

	std::string GetOutputFilename() const;

	/** Measurement of a single sprite group resolution */
	struct Call {
		uint32 root_sprite;  ///< Pseudo-sprite index in GRF file
		uint32 item;         ///< Local ID of item being resolved for
		uint32 result;       ///< Result of callback
		uint32 subs;         ///< Sub-calls to other sprite groups
		uint32 time;         ///< Time taken for resolution (microseconds)
		uint16 tick;         ///< Game tick
		CallbackID cb;       ///< Callback ID
		GrfSpecFeature feat; ///< GRF feature being resolved for
	};

	const GRFFile *grffile;  ///< Which GRF is being profiled
	Date profile_end_date;   ///< Game date to end this profiling session
	Call cur_call;           ///< Data for current call in progress
	std::vector<Call> calls; ///< All calls collected so far
};

extern std::unique_ptr<NewGRFProfiler> _newgrf_profiler;

#endif /* NEWGRF_PROFILING_H */
