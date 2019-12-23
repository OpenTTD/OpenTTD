/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file newgrf_profiling.cpp Profiling of NewGRF action 2 handling. */

#include "newgrf_profiling.h"
#include "date_func.h"
#include "fileio_func.h"
#include "string_func.h"
#include "console_func.h"
#include "spritecache.h"

#include <chrono>
#include <time.h>


std::vector<NewGRFProfiler> _newgrf_profilers;
Date _newgrf_profile_end_date;


/**
 * Create profiler object and begin profiling session.
 * @param grffile   The GRF file to collect profiling data on
 * @param end_date  Game date to end profiling on
 */
NewGRFProfiler::NewGRFProfiler(const GRFFile *grffile) : grffile{ grffile }, active{ false }, cur_call{}
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
	this->cur_call.time = (uint32)time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count();
	this->cur_call.tick = _tick_counter;
	this->cur_call.cb = resolver.callback;
	this->cur_call.feat = resolver.GetFeature();
	this->cur_call.item = resolver.GetDebugID();
}

/**
 * Capture the completion of a sprite group resolution.
 */
void NewGRFProfiler::EndResolve(const SpriteGroup *result)
{
	using namespace std::chrono;
	this->cur_call.time = (uint32)time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count() - this->cur_call.time;

	if (result == nullptr) {
		this->cur_call.result = 0;
	} else if (result->type == SGT_CALLBACK) {
		this->cur_call.result = static_cast<const CallbackResultSpriteGroup *>(result)->result;
	} else if (result->type == SGT_RESULT) {
		this->cur_call.result = GetSpriteLocalID(static_cast<const ResultSpriteGroup *>(result)->sprite);
	} else {
		this->cur_call.result = result->nfo_line;
	}

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
}

void NewGRFProfiler::Finish()
{
	if (!this->active) return;

	std::string filename = this->GetOutputFilename();
	IConsolePrintF(TC_LIGHT_BROWN, "Finished profile of NewGRF [%08X], writing %u events to %s", this->grffile->grfid, (uint)this->calls.size(), filename.c_str());

	FILE *f = FioFOpenFile(filename.c_str(), "wt", Subdirectory::NO_DIRECTORY);
	FileCloser fcloser(f);

	fputs("Tick,Sprite,Feature,Item,CallbackID,Microseconds,Depth,Result\n", f);
	for (const Call &c : this->calls) {
		fprintf(f, "%u,%u,0x%X,%d,0x%X,%u,%u,%u\n", c.tick, c.root_sprite, c.feat, c.item, (uint)c.cb, c.time, c.subs, c.result);
	}

	this->Abort();
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
	time_t write_time = time(nullptr);

	char timestamp[16] = {};
	strftime(timestamp, lengthof(timestamp), "%Y%m%d-%H%M", localtime(&write_time));

	char filepath[MAX_PATH] = {};
	seprintf(filepath, lastof(filepath), "%sgrfprofile-%s-%08X.csv", FiosGetScreenshotDir(), timestamp, this->grffile->grfid);

	return std::string(filepath);
}
