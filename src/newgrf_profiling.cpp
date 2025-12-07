/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

 /** @file newgrf_profiling.cpp Profiling of NewGRF action 2 handling. */

#include "stdafx.h"

#include "newgrf_profiling.h"
#include "fileio_func.h"
#include "string_func.h"
#include "console_func.h"
#include "spritecache.h"
#include "3rdparty/fmt/chrono.h"
#include "timer/timer.h"
#include "timer/timer_game_tick.h"

#include <chrono>

#include "safeguards.h"

std::vector<NewGRFProfiler> _newgrf_profilers;


/**
 * Create profiler object and begin profiling session.
 * @param grffile   The GRF file to collect profiling data on
 * @param end_date  Game date to end profiling on
 */
NewGRFProfiler::NewGRFProfiler(const GRFFile *grffile) : grffile(grffile)
{
}

/**
 * Complete profiling session and write data to file
 */
NewGRFProfiler::~NewGRFProfiler()
{
}

/**
 * Capture the start of a sprite group resolution.
 * @param resolver  Data about sprite group being resolved
 */
void NewGRFProfiler::BeginResolve(const ResolverObject &resolver)
{
	using namespace std::chrono;
	this->cur_call.root_sprite = resolver.root_spritegroup->nfo_line;
	this->cur_call.subs = 0;
	this->cur_call.time = (uint32_t)time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count();
	this->cur_call.tick = TimerGameTick::counter;
	this->cur_call.cb = resolver.callback;
	this->cur_call.feat = resolver.GetFeature();
	this->cur_call.item = resolver.GetDebugID();
}

/**
 * Capture the completion of a sprite group resolution.
 */
void NewGRFProfiler::EndResolve(const ResolverResult &result)
{
	using namespace std::chrono;
	this->cur_call.time = (uint32_t)time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count() - this->cur_call.time;

	struct visitor {
		uint32_t operator()(std::monostate)
		{
			return 0;
		}
		uint32_t operator()(CallbackResult cb_result)
		{
			return cb_result;
		}
		uint32_t operator()(const ResultSpriteGroup *group)
		{
			return group == nullptr ? 0 : GetSpriteLocalID(group->sprite);
		}
		uint32_t operator()(const TileLayoutSpriteGroup *group)
		{
			return group == nullptr ? 0 : group->nfo_line;
		}
		uint32_t operator()(const IndustryProductionSpriteGroup *group)
		{
			return group == nullptr ? 0 : group->nfo_line;
		}
	};
	this->cur_call.result = std::visit(visitor{}, result);

	this->calls.push_back(this->cur_call);
}

/**
 * Capture a recursive sprite group resolution.
 */
void NewGRFProfiler::RecursiveResolve()
{
	this->cur_call.subs += 1;
}

void NewGRFProfiler::Start()
{
	this->Abort();
	this->active = true;
	this->start_tick = TimerGameTick::counter;
}

uint32_t NewGRFProfiler::Finish()
{
	if (!this->active) return 0;

	if (this->calls.empty()) {
		IConsolePrint(CC_DEBUG, "Finished profile of NewGRF [{:08X}], no events collected, not writing a file.", std::byteswap(this->grffile->grfid));

		this->Abort();
		return 0;
	}

	std::string filename = this->GetOutputFilename();
	IConsolePrint(CC_DEBUG, "Finished profile of NewGRF [{:08X}], writing {} events to '{}'.", std::byteswap(this->grffile->grfid), this->calls.size(), filename);

	uint32_t total_microseconds = 0;

	auto f = FioFOpenFile(filename, "wt", Subdirectory::NO_DIRECTORY);

	if (!f.has_value()) {
		IConsolePrint(CC_ERROR, "Failed to open '{}' for writing.", filename);
	} else {
		fmt::print(*f, "Tick,Sprite,Feature,Item,CallbackID,Microseconds,Depth,Result\n");
		for (const Call &c : this->calls) {
			fmt::print(*f, "{},{},0x{:X},{},0x{:X},{},{},{}\n", c.tick, c.root_sprite, c.feat, c.item, (uint)c.cb, c.time, c.subs, c.result);
			total_microseconds += c.time;
		}
	}

	this->Abort();
	return total_microseconds;
}

void NewGRFProfiler::Abort()
{
	this->active = false;
	this->calls.clear();
}

/**
 * Get name of the file that will be written.
 * @return File name of profiling output file.
 */
std::string NewGRFProfiler::GetOutputFilename() const
{
	return fmt::format("{}grfprofile-{:%Y%m%d-%H%M}-{:08X}.csv", FiosGetScreenshotDir(), fmt::localtime(time(nullptr)), std::byteswap(this->grffile->grfid));
}

/* static */ uint32_t NewGRFProfiler::FinishAll()
{
	NewGRFProfiler::AbortTimer();

	uint64_t max_ticks = 0;
	uint32_t total_microseconds = 0;
	for (NewGRFProfiler &pr : _newgrf_profilers) {
		if (pr.active) {
			total_microseconds += pr.Finish();
			max_ticks = std::max(max_ticks, TimerGameTick::counter - pr.start_tick);
		}
	}

	if (total_microseconds > 0 && max_ticks > 0) {
		IConsolePrint(CC_DEBUG, "Total NewGRF callback processing: {} microseconds over {} ticks.", total_microseconds, max_ticks);
	}

	return total_microseconds;
}

/**
 * Check whether profiling is active and should be finished.
 */
static TimeoutTimer<TimerGameTick> _profiling_finish_timeout({ TimerGameTick::Priority::NONE, 0 }, []()
{
	NewGRFProfiler::FinishAll();
});

/**
 * Start the timeout timer that will finish all profiling sessions.
 */
/* static */ void NewGRFProfiler::StartTimer(uint64_t ticks)
{
	_profiling_finish_timeout.Reset({ TimerGameTick::Priority::NONE, static_cast<uint>(ticks) });
}

/**
 * Abort the timeout timer, so the timer callback is never called.
 */
/* static */ void NewGRFProfiler::AbortTimer()
{
	_profiling_finish_timeout.Abort();
}
