/* $Id$ */

/** @file thread_pthread.cpp POSIX pthread implementation of Threads. */

#include "stdafx.h"
#include "thread.h"
#include <pthread.h>

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
		} catch (OTTDThreadExitSignal e) {
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
	pthread_mutex_t mutex;

public:
	ThreadMutex_pthread()
	{
		pthread_mutex_init(&this->mutex, NULL);
	}

	/* virtual */ ~ThreadMutex_pthread()
	{
		pthread_mutex_destroy(&this->mutex);
	}

	/* virtual */ void BeginCritical()
	{
		pthread_mutex_lock(&this->mutex);
	}

	/* virtual */ void EndCritical()
	{
		pthread_mutex_unlock(&this->mutex);
	}
};

/* static */ ThreadMutex *ThreadMutex::New()
{
	return new ThreadMutex_pthread();
}
