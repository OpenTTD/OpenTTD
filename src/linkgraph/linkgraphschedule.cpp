/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraphschedule.cpp Definition of link graph schedule used for cargo distribution. */

#include "../stdafx.h"
#include "linkgraphschedule.h"
#include "init.h"
#include "demands.h"
#include "mcf.h"
#include "flowmapper.h"
#include "../framerate_type.h"
#include "../command_func.h"
#include "../network/network.h"
#include "../misc_cmd.h"

#include "../safeguards.h"

/**
 * Static instance of LinkGraphSchedule.
 * Note: This instance is created on task start.
 *       Lazy creation on first usage results in a data race between the CDist threads.
 */
/* static */ LinkGraphSchedule LinkGraphSchedule::instance;

/**
 * Start the next job in the schedule.
 */
void LinkGraphSchedule::SpawnNext()
{
	if (this->schedule.empty()) return;
	LinkGraph *next = this->schedule.front();
	LinkGraph *first = next;
	while (next->Size() < 2) {
		this->schedule.splice(this->schedule.end(), this->schedule, this->schedule.begin());
		next = this->schedule.front();
		if (next == first) return;
	}
	assert(next == LinkGraph::Get(next->index));
	this->schedule.pop_front();
	if (LinkGraphJob::CanAllocateItem()) {
		LinkGraphJob *job = new LinkGraphJob(*next);
		job->SpawnThread();
		this->running.push_back(job);
	} else {
		NOT_REACHED();
	}
}

/**
 * Check if the next job is supposed to be finished, but has not yet completed.
 * @return True if job should be finished by now but is still running, false if not.
 */
bool LinkGraphSchedule::IsJoinWithUnfinishedJobDue() const
{
	if (this->running.empty()) return false;
	const LinkGraphJob *next = this->running.front();
	return next->IsScheduledToBeJoined() && !next->IsJobCompleted();
}

/**
 * Join the next finished job, if available.
 */
void LinkGraphSchedule::JoinNext()
{
	if (this->running.empty()) return;
	LinkGraphJob *next = this->running.front();
	if (!next->IsScheduledToBeJoined()) return;
	this->running.pop_front();
	LinkGraphID id = next->LinkGraphIndex();
	delete next; // implicitly joins the thread
	if (LinkGraph::IsValidID(id)) {
		LinkGraph *lg = LinkGraph::Get(id);
		this->Unqueue(lg); // Unqueue to avoid double-queueing recycled IDs.
		this->Queue(lg);
	}
}

/**
 * Run all handlers for the given Job.
 * @param job Pointer to a link graph job.
 */
/* static */ void LinkGraphSchedule::Run(LinkGraphJob *job)
{
	for (uint i = 0; i < lengthof(instance.handlers); ++i) {
		if (job->IsJobAborted()) return;
		instance.handlers[i]->Run(*job);
	}

	/*
	 * Readers of this variable in another thread may see an out of date value.
	 * However this is OK as this will only happen just as a job is completing,
	 * and the real synchronisation is provided by the thread join operation.
	 * In the worst case the main thread will be paused for longer than
	 * strictly necessary before joining.
	 * This is just a hint variable to avoid performing the join excessively
	 * early and blocking the main thread.
	 */

	job->job_completed.store(true, std::memory_order_release);
}

/**
 * Start all threads in the running list. This is only useful for save/load.
 * Usually threads are started when the job is created.
 */
void LinkGraphSchedule::SpawnAll()
{
	for (auto &it : this->running) {
		it->SpawnThread();
	}
}

/**
 * Clear all link graphs and jobs from the schedule.
 */
/* static */ void LinkGraphSchedule::Clear()
{
	for (auto &it : instance.running) {
		it->AbortJob();
	}
	instance.running.clear();
	instance.schedule.clear();
}

/**
 * Shift all dates (join dates and edge annotations) of link graphs and link
 * graph jobs by the number of days given.
 * @param interval Number of days to be added or subtracted.
 */
void LinkGraphSchedule::ShiftDates(TimerGameCalendar::Date interval)
{
	for (LinkGraph *lg : LinkGraph::Iterate()) lg->ShiftDates(interval);
	for (LinkGraphJob *lgj : LinkGraphJob::Iterate()) lgj->ShiftJoinDate(interval);
}

/**
 * Create a link graph schedule and initialize its handlers.
 */
LinkGraphSchedule::LinkGraphSchedule()
{
	this->handlers[0] = new InitHandler;
	this->handlers[1] = new DemandHandler;
	this->handlers[2] = new MCFHandler<MCF1stPass>;
	this->handlers[3] = new FlowMapper(false);
	this->handlers[4] = new MCFHandler<MCF2ndPass>;
	this->handlers[5] = new FlowMapper(true);
}

/**
 * Delete a link graph schedule and its handlers.
 */
LinkGraphSchedule::~LinkGraphSchedule()
{
	this->Clear();
	for (uint i = 0; i < lengthof(this->handlers); ++i) {
		delete this->handlers[i];
	}
}

/**
 * Pause the game if in 2 TimerGameCalendar::date_fract ticks, we would do a join with the next
 * link graph job, but it is still running.
 * The check is done 2 TimerGameCalendar::date_fract ticks early instead of 1, as in multiplayer
 * calls to DoCommandP are executed after a delay of 1 TimerGameCalendar::date_fract tick.
 * If we previously paused, unpause if the job is now ready to be joined with.
 */
void StateGameLoop_LinkGraphPauseControl()
{
	if (_pause_mode & PM_PAUSED_LINK_GRAPH) {
		/* We are paused waiting on a job, check the job every tick. */
		if (!LinkGraphSchedule::instance.IsJoinWithUnfinishedJobDue()) {
			Command<CMD_PAUSE>::Post(PM_PAUSED_LINK_GRAPH, false);
		}
	} else if (_pause_mode == PM_UNPAUSED &&
			TimerGameCalendar::date_fract == LinkGraphSchedule::SPAWN_JOIN_TICK - 2 &&
			TimerGameCalendar::date.base() % (_settings_game.linkgraph.recalc_interval / CalendarTime::SECONDS_PER_DAY) == (_settings_game.linkgraph.recalc_interval / CalendarTime::SECONDS_PER_DAY) / 2 &&
			LinkGraphSchedule::instance.IsJoinWithUnfinishedJobDue()) {
		/* Perform check two TimerGameCalendar::date_fract ticks before we would join, to make
		 * sure it also works in multiplayer. */
		Command<CMD_PAUSE>::Post(PM_PAUSED_LINK_GRAPH, true);
	}
}

/**
 * Pause the game on load if we would do a join with the next link graph job,
 * but it is still running, and it would not be caught by a call to
 * StateGameLoop_LinkGraphPauseControl().
 */
void AfterLoad_LinkGraphPauseControl()
{
	if (LinkGraphSchedule::instance.IsJoinWithUnfinishedJobDue()) {
		_pause_mode |= PM_PAUSED_LINK_GRAPH;
	}
}

/**
 * Spawn or join a link graph job or compress a link graph if any link graph is
 * due to do so.
 */
void OnTick_LinkGraph()
{
	if (TimerGameCalendar::date_fract != LinkGraphSchedule::SPAWN_JOIN_TICK) return;
	TimerGameCalendar::Date offset = TimerGameCalendar::date.base() % (_settings_game.linkgraph.recalc_interval / CalendarTime::SECONDS_PER_DAY);
	if (offset == 0) {
		LinkGraphSchedule::instance.SpawnNext();
	} else if (offset == (_settings_game.linkgraph.recalc_interval / CalendarTime::SECONDS_PER_DAY) / 2) {
		if (!_networking || _network_server) {
			PerformanceMeasurer::SetInactive(PFE_GL_LINKGRAPH);
			LinkGraphSchedule::instance.JoinNext();
		} else {
			PerformanceMeasurer framerate(PFE_GL_LINKGRAPH);
			LinkGraphSchedule::instance.JoinNext();
		}
	}
}


