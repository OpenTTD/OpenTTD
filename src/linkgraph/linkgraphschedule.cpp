/* $Id$ */

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

/**
 * Spawn a thread if possible and run the link graph job in the thread. If
 * that's not possible run the job right now in the current thread.
 * @param job Job to be executed.
 */
void LinkGraphSchedule::SpawnThread(LinkGraphJob *job)
{
	if (!ThreadObject::New(&(LinkGraphSchedule::Run), job, &job->thread)) {
		job->thread = NULL;
		/* Of course this will hang a bit.
		 * On the other hand, if you want to play games which make this hang noticably
		 * on a platform without threads then you'll probably get other problems first.
		 * OK:
		 * If someone comes and tells me that this hangs for him/her, I'll implement a
		 * smaller grained "Step" method for all handlers and add some more ticks where
		 * "Step" is called. No problem in principle.
		 */
		LinkGraphSchedule::Run(job);
	}
}

/**
 * Join the calling thread with the given job's thread if threading is enabled.
 * @param job Job whose execution thread is to be joined.
 */
void LinkGraphSchedule::JoinThread(LinkGraphJob *job)
{
	if (job->thread != NULL) {
		job->thread->Join();
		delete job->thread;
		job->thread = NULL;
	}
}

/**
 * Start the next job in the schedule.
 */
void LinkGraphSchedule::SpawnNext()
{
	if (this->schedule.empty()) return;
	LinkGraph *next = this->schedule.front();
	assert(next == LinkGraph::Get(next->index));
	this->schedule.pop_front();
	if (LinkGraphJob::CanAllocateItem()) {
		LinkGraphJob *job = new LinkGraphJob(*next);
		this->SpawnThread(job);
		this->running.push_back(job);
	} else {
		NOT_REACHED();
	}
}

/**
 * Join the next finished job, if available.
 */
void LinkGraphSchedule::JoinNext()
{
	if (this->running.empty()) return;
	LinkGraphJob *next = this->running.front();
	if (!next->IsFinished()) return;
	this->running.pop_front();
	LinkGraphID id = next->LinkGraphIndex();
	this->JoinThread(next);
	delete next;
	if (LinkGraph::IsValidID(id)) {
		LinkGraph *lg = LinkGraph::Get(id);
		this->Unqueue(lg); // Unqueue to avoid double-queueing recycled IDs.
		this->Queue(lg);
	}
}

/**
 * Run all handlers for the given Job. This method is tailored to
 * ThreadObject::New.
 * @param j Pointer to a link graph job.
 */
/* static */ void LinkGraphSchedule::Run(void *j)
{
	LinkGraphJob *job = (LinkGraphJob *)j;
	LinkGraphSchedule *schedule = LinkGraphSchedule::Instance();
	for (uint i = 0; i < lengthof(schedule->handlers); ++i) {
		schedule->handlers[i]->Run(*job);
	}
}

/**
 * Start all threads in the running list. This is only useful for save/load.
 * Usually threads are started when the job is created.
 */
void LinkGraphSchedule::SpawnAll()
{
	for (JobList::iterator i = this->running.begin(); i != this->running.end(); ++i) {
		this->SpawnThread(*i);
	}
}

/**
 * Clear all link graphs and jobs from the schedule.
 */
/* static */ void LinkGraphSchedule::Clear()
{
	LinkGraphSchedule *inst = LinkGraphSchedule::Instance();
	for (JobList::iterator i(inst->running.begin()); i != inst->running.end(); ++i) {
		inst->JoinThread(*i);
	}
	inst->running.clear();
	inst->schedule.clear();
}

/**
 * Shift all dates (join dates and edge annotations) of link graphs and link
 * graph jobs by the number of days given.
 * @param interval Number of days to be added or subtracted.
 */
void LinkGraphSchedule::ShiftDates(int interval)
{
	LinkGraph *lg;
	FOR_ALL_LINK_GRAPHS(lg) lg->ShiftDates(interval);
	LinkGraphJob *lgj;
	FOR_ALL_LINK_GRAPH_JOBS(lgj) lgj->ShiftJoinDate(interval);
}

/**
 * Create a link graph schedule and initialize its handlers.
 */
LinkGraphSchedule::LinkGraphSchedule()
{
	this->handlers[0] = new InitHandler;
	this->handlers[1] = new DemandHandler;
	this->handlers[2] = new MCFHandler<MCF1stPass>;
	this->handlers[3] = new FlowMapper;
	this->handlers[4] = new MCFHandler<MCF2ndPass>;
	this->handlers[5] = new FlowMapper;
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
 * Retrieve the link graph schedule or create it if necessary.
 */
/* static */ LinkGraphSchedule *LinkGraphSchedule::Instance()
{
	static LinkGraphSchedule inst;
	return &inst;
}

/**
 * Spawn or join a link graph job or compress a link graph if any link graph is
 * due to do so.
 */
void OnTick_LinkGraph()
{
	if (_date_fract != LinkGraphSchedule::SPAWN_JOIN_TICK) return;
	Date offset = _date % _settings_game.linkgraph.recalc_interval;
	if (offset == 0) {
		LinkGraphSchedule::Instance()->SpawnNext();
	} else if (offset == _settings_game.linkgraph.recalc_interval / 2) {
		LinkGraphSchedule::Instance()->JoinNext();
	}
}


