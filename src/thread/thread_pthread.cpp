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

#if defined(__APPLE__)
#include "../os/macosx/macos.h"
#endif

#include "../safeguards.h"

/**
 * POSIX pthread version for ThreadObject.
 */
class ThreadObject_pthread : public ThreadObject {
private:
	pthread_t thread;    ///< System thread identifier.
	OTTDThreadFunc proc; ///< External thread procedure.
	void *param;         ///< Parameter for the external thread procedure.
	bool self_destruct;  ///< Free ourselves when done?
	const char *name;    ///< Name for the thread

public:
	/**
	 * Create a pthread and start it, calling proc(param).
	 */
	ThreadObject_pthread(OTTDThreadFunc proc, void *param, bool self_destruct, const char *name) :
		thread(0),
		proc(proc),
		param(param),
		self_destruct(self_destruct),
		name(name)
	{
		pthread_create(&this->thread, NULL, &stThreadProc, this);
	}

	bool Exit() override
	{
		assert(pthread_self() == this->thread);
		/* For now we terminate by throwing an error, gives much cleaner cleanup */
		throw OTTDThreadExitSignal();
	}

	void Join() override
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
		ThreadObject_pthread *self = (ThreadObject_pthread *) thr;
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2, 12)
		if (self->name) {
			pthread_setname_np(pthread_self(), self->name);
		}
#endif
#endif
#if defined(__APPLE__)
		MacOSSetThreadName(self->name);
#endif
		self->ThreadProc();
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

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread, const char *name)
{
	ThreadObject *to = new ThreadObject_pthread(proc, param, thread == NULL, name);
	if (thread != NULL) *thread = to;
	return true;
}
