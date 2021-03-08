/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file progress.cpp Functions for modal progress windows. */

#include "stdafx.h"
#include "progress.h"

#include <chrono>
#include <mutex>
#include <condition_variable>

#include "safeguards.h"

/** Are we in a modal progress or not? */
bool _in_modal_progress = false;
bool _first_in_modal_loop = false;
/** Threading usable for modal progress? */
bool _use_threaded_modal_progress = true;
/** Rights for the performing work. */
std::mutex _modal_progress_work_mutex;
/** Rights for the painting. */
std::mutex _modal_progress_paint_mutex;

static std::mutex _modal_progress_cv_mutex;
static std::condition_variable _modal_progress_cv;

/**
 * Set the modal progress state.
 * @note Makes IsFirstModalProgressLoop return true for the next call.
 * @param state The new state; are we modal or not?
 */
void SetModalProgress(bool state)
{
	_modal_progress_cv_mutex.lock();
	_in_modal_progress = state;
	_first_in_modal_loop = true;
	_modal_progress_cv_mutex.unlock();
	if (!state) _modal_progress_cv.notify_all();
}

/**
 * Check whether this is the first modal progress loop.
 * @note Set by SetModalProgress, unset by calling this method.
 * @return True if this is the first loop.
 */
bool IsFirstModalProgressLoop()
{
	bool ret = _first_in_modal_loop;
	_first_in_modal_loop = false;
	return ret;
}

/**
 * Sleep until the first of: the specified time duration in milliseconds elapses, the modal progress state is false.
 * The modal progress paint and work mutexes MUST NOT be held by the caller.
 * @param milliseconds Time duration in milliseconds
 */
void SleepWhileModalProgress(int milliseconds)
{
	std::unique_lock<std::mutex> lk(_modal_progress_cv_mutex);
	_modal_progress_cv.wait_for(lk, std::chrono::milliseconds(milliseconds), []{ return !_in_modal_progress; });
}
