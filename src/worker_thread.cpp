/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file worker_thread.cpp Worker thread pool utility. */

#include "stdafx.h"
#include "worker_thread.h"
#include "thread.h"

#include "safeguards.h"

WorkerThreadPool _general_worker_pool;

void WorkerThreadPool::Start(const char *thread_name, uint max_workers)
{
	uint cpus = std::thread::hardware_concurrency();
	if (cpus <= 1) return;

	std::lock_guard<std::mutex> lk(this->lock);

	this->exit = false;

	uint worker_target = std::min<uint>(max_workers, cpus);
	if (this->workers >= worker_target) return;

	uint new_workers = worker_target - this->workers;

	for (uint i = 0; i < new_workers; i++) {
		this->workers++;
		if (!StartNewThread(nullptr, thread_name, &WorkerThreadPool::Run, this)) {
			this->workers--;
			return;
		}
	}
}

void WorkerThreadPool::Stop()
{
	std::unique_lock<std::mutex> lk(this->lock);
	this->exit = true;
	this->empty_cv.notify_all();
	this->done_cv.wait(lk, [this]() { return this->workers == 0; });
}

void WorkerThreadPool::EnqueueJob(WorkerJobFunc *func, TileIndex tile, uint count)
{
	std::unique_lock<std::mutex> lk(this->lock);
	if (this->workers == 0) {
		/* Just execute it here and now */
		lk.unlock();
		func(tile, count);
		return;
	}
	this->jobs.push({ func, tile, count });
	lk.unlock();
	this->empty_cv.notify_one();
}

void WorkerThreadPool::WaitTillEmpty()
{
    std::unique_lock<std::mutex> lk(this->lock);
    this->job_cv.wait(lk, [this]() { return this->jobs.empty() && this->jobs_pending == 0; });
}

void WorkerThreadPool::Run(WorkerThreadPool *pool)
{
	std::unique_lock<std::mutex> lk(pool->lock);
	while (!pool->exit || !pool->jobs.empty()) {
		if (pool->jobs.empty()) {
			pool->empty_cv.wait(lk);
		} else {
			WorkerJob job = pool->jobs.front();
			pool->jobs.pop();
			pool->jobs_pending++;
			lk.unlock();
			job.func(job.tile, job.count);
			lk.lock();
			pool->jobs_pending--;
			pool->job_cv.notify_one();
		}
	}
	pool->workers--;
	if (pool->workers == 0) {
		pool->done_cv.notify_all();
	}
}
