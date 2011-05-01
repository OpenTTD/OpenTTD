/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file thread_pthread.cpp POSIX pthread implementation of Threads. */

#include "../stdafx.h"
#include "thread.h"
#include <pthread.h>
#include <errno.h>

/**
 * POSIX pthread version for ThreadObject.
 */
class ThreadObject_pthread : public ThreadObject {
private:
	pthread_t thread;    ///< System thread identifier.
	OTTDThreadFunc proc; ///< External thread procedure.
	void *param;         ///< Parameter for the external thread procedure.
	bool self_destruct;  ///< Free ourselves when done?

public:
	/**
	 * Create a pthread and start it, calling proc(param).
	 */
	ThreadObject_pthread(OTTDThreadFunc proc, void *param, bool self_destruct) :
		thread(0),
		proc(proc),
		param(param),
		self_destruct(self_destruct)
	{
		pthread_create(&this->thread, NULL, &stThreadProc, this);
	}

	/* virtual */ bool Exit()
	{
		assert(pthread_self() == this->thread);
		/* For now we terminate by throwing an error, gives much cleaner cleanup */
		throw OTTDThreadExitSignal();
	}

	/* virtual */ void Join()
	{
		/* You cannot join yourself */
		assert(pthread_self() != this->thread);
		pthread_join(this->thread, NULL);
		this->thread = 0;
	}
private:
	/**
	 * On thread creation, this function is called, which calls the real startup
	 *  function. This to get back into the correct instance again.
	 */
	static void *stThreadProc(void *thr)
	{
		((ThreadObject_pthread *)thr)->ThreadProc();
		pthread_exit(NULL);
	}

	/**
	 * A new thread is created, and this function is called. Call the custom
	 *  function of the creator of the thread.
	 */
	void ThreadProc()
	{
		/* Call the proc of the creator to continue this thread */
		try {
			this->proc(this->param);
		} catch (OTTDThreadExitSignal) {
		} catch (...) {
			NOT_REACHED();
		}

		if (self_destruct) {
			pthread_detach(pthread_self());
			delete this;
		}
	}
};

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread)
{
	ThreadObject *to = new ThreadObject_pthread(proc, param, thread == NULL);
	if (thread != NULL) *thread = to;
	return true;
}

/**
 * POSIX pthread version of ThreadMutex.
 */
class ThreadMutex_pthread : public ThreadMutex {
private:
	pthread_mutex_t mutex;    ///< The actual mutex.
	pthread_cond_t condition; ///< Data for conditional waiting.
	pthread_mutexattr_t attr; ///< Attributes set for the mutex.

public:
	ThreadMutex_pthread()
	{
		pthread_mutexattr_init(&this->attr);
		pthread_mutexattr_settype(&this->attr, PTHREAD_MUTEX_ERRORCHECK);
		pthread_mutex_init(&this->mutex, &this->attr);
		pthread_cond_init(&this->condition, NULL);
	}

	/* virtual */ ~ThreadMutex_pthread()
	{
		int err = pthread_cond_destroy(&this->condition);
		assert(err != EBUSY);
		err = pthread_mutex_destroy(&this->mutex);
		assert(err != EBUSY);
	}

	/* virtual */ void BeginCritical()
	{
		int err = pthread_mutex_lock(&this->mutex);
		assert(err == 0);
	}

	/* virtual */ void EndCritical()
	{
		int err = pthread_mutex_unlock(&this->mutex);
		assert(err == 0);
	}

	/* virtual */ void WaitForSignal()
	{
		int err = pthread_cond_wait(&this->condition, &this->mutex);
		assert(err == 0);
	}

	/* virtual */ void SendSignal()
	{
		int err = pthread_cond_signal(&this->condition);
		assert(err == 0);
	}
};

/* static */ ThreadMutex *ThreadMutex::New()
{
	return new ThreadMutex_pthread();
}
