/* $Id$ */

/** @file thread_os2.cpp OS/2 implementation of Threads. */

#include "stdafx.h"
#include "thread.h"

#define INCL_DOS
#include <os2.h>
#include <process.h>

/**
 * OS/2 version for ThreadObject.
 */
class ThreadObject_OS2 : public ThreadObject {
private:
	TID thread;          ///< System thread identifier.
	OTTDThreadFunc proc; ///< External thread procedure.
	void *param;         ///< Parameter for the external thread procedure.
	bool self_destruct;  ///< Free ourselves when done?

public:
	/**
	 * Create a thread and start it, calling proc(param).
	 */
	ThreadObject_OS2(OTTDThreadFunc proc, void *param, bool self_destruct) :
		thread(0),
		proc(proc),
		param(param),
		self_destruct(self_destruct)
	{
		thread = _beginthread(stThreadProc, NULL, 32768, this);
	}

	/* virtual */ bool Exit()
	{
		_endthread();
		return true;
	}

	/* virtual */ void Join()
	{
		DosWaitThread(&this->thread, DCWW_WAIT);
		this->thread = 0;
	}
private:
	/**
	 * On thread creation, this function is called, which calls the real startup
	 *  function. This to get back into the correct instance again.
	 */
	static void stThreadProc(void *thr)
	{
		((ThreadObject_OS2 *)thr)->ThreadProc();
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
			this->Exit();
			delete this;
		}
	}
};

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread)
{
	ThreadObject *to = new ThreadObject_OS2(proc, param, thread == NULL);
	if (thread != NULL) *thread = to;
	return true;
}

/**
 * OS/2 version of ThreadMutex.
 */
class ThreadMutex_OS2 : public ThreadMutex {
private:
	HMTX mutex;

public:
	ThreadMutex_OS2()
	{
		DosCreateMutexSem(NULL, &mutex, 0, FALSE);
	}

	/* virtual */ ~ThreadMutex_OS2()
	{
		DosCloseMutexSem(mutex);
	}

	/* virtual */ void BeginCritical()
	{
		DosRequestMutexSem(mutex, (unsigned long) SEM_INDEFINITE_WAIT);
	}

	/* virtual */ void EndCritical()
	{
		DosReleaseMutexSem(mutex);
	}
};

/* static */ ThreadMutex *ThreadMutex::New()
{
	return new ThreadMutex_OS2();
}
