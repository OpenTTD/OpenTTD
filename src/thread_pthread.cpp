/* $Id$ */

/** @file thread_pthread.cpp POSIX pthread implementation of Threads. */

#include "stdafx.h"
#include "thread.h"
#include "debug.h"
#include "core/alloc_func.hpp"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

/**
 * POSIX pthread version for ThreadObject.
 */
class ThreadObject_pthread : public ThreadObject {
private:
	pthread_t m_thr;             ///< System thread identifier.
	OTTDThreadFunc m_proc;       ///< External thread procedure.
	void     *m_param;           ///< Parameter for the external thread procedure.
	bool      m_attached;        ///< True if the ThreadObject was attached to an existing thread.
	sem_t     m_sem_start;       ///< Here the new thread waits before it starts.
	sem_t     m_sem_stop;        ///< Here the other thread can wait for this thread to end.

public:
	/**
	 * Create a pthread and start it, calling proc(param).
	 */
	ThreadObject_pthread(OTTDThreadFunc proc, void *param) :
		m_thr(0),
		m_proc(proc),
		m_param(param),
		m_attached(false)
	{
		sem_init(&m_sem_start, 0, 0);
		sem_init(&m_sem_stop, 0, 0);

		pthread_create(&m_thr, NULL, &stThreadProc, this);
		sem_post(&m_sem_start);
	}

	/**
	 * Create a pthread and attach current thread to it.
	 */
	ThreadObject_pthread() :
		m_thr(0),
		m_proc(NULL),
		m_param(0),
		m_attached(true)
	{
		sem_init(&m_sem_start, 0, 0);
		sem_init(&m_sem_stop, 0, 0);

		m_thr = pthread_self();
	}

	/* virtual */ ~ThreadObject_pthread()
	{
		sem_destroy(&m_sem_stop);
		sem_destroy(&m_sem_start);
	};

	/* virtual */ bool IsRunning()
	{
		int sval;
		sem_getvalue(&m_sem_stop, &sval);
		return sval == 0;
	}

	/* virtual */ bool WaitForStop()
	{
		/* You can't wait on yourself */
		assert(!IsCurrent());
		/* If the thread is not running, waiting is over */
		if (!IsRunning()) return true;

		int ret = sem_wait(&m_sem_stop);
		if (ret == 0) {
			/* We have passed semaphore so increment it again */
			sem_post(&m_sem_stop);
			return true;
		}
		return false;
	}

	/* virtual */ bool Exit()
	{
		/* You can only exit yourself */
		assert(IsCurrent());
		/* If the thread is not running, we are already closed */
		if (!IsRunning()) return false;

		/* For now we terminate by throwing an error, gives much cleaner cleanup */
		throw 0;
	}

	/* virtual */ void Join()
	{
		/* You cannot join yourself */
		assert(!IsCurrent());

		pthread_join(m_thr, NULL);
		m_thr = 0;
	}

	/* virtual */ bool IsCurrent()
	{
		return pthread_self() == m_thr;
	}

	/* virtual */ uint GetId()
	{
		return (uint)m_thr;
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
		/* The new thread stops here so the calling thread can complete pthread_create() call */
		sem_wait(&m_sem_start);

		/* Call the proc of the creator to continue this thread */
		try {
			m_proc(m_param);
		} catch (...) {
		}

		/* Notify threads waiting for our completion */
		sem_post(&m_sem_stop);
	}
};

/* static */ ThreadObject *ThreadObject::New(OTTDThreadFunc proc, void *param)
{
	return new ThreadObject_pthread(proc, param);
}

/* static */ ThreadObject *ThreadObject::AttachCurrent()
{
	return new ThreadObject_pthread();
}

/* static */ uint ThreadObject::CurrentId()
{
	return (uint)pthread_self();
}


/**
 * POSIX pthread version of ThreadSemaphore.
 */
class ThreadSemaphore_pthread : public ThreadSemaphore {
private:
	sem_t m_sem;

public:
	ThreadSemaphore_pthread()
	{
		sem_init(&m_sem, 0, 0);
	}

	/* virtual */ ~ThreadSemaphore_pthread()
	{
		sem_destroy(&m_sem);
	}

	/* virtual */ void Set()
	{
		int val = 0;
		if (sem_getvalue(&m_sem, &val) == 0 && val == 0) sem_post(&m_sem);
	}

	/* virtual */ void Wait()
	{
		sem_wait(&m_sem);
	}
};

/* static */ ThreadSemaphore *ThreadSemaphore::New()
{
	return new ThreadSemaphore_pthread();
}
