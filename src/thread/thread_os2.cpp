/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file thread_os2.cpp OS/2 implementation of Threads. */

#include "../stdafx.h"
#include "thread.h"

#define INCL_DOS
#include <os2.h>
#include <process.h>

#include "../safeguards.h"

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
		thread = _beginthread(stThreadProc, NULL, 1048576, this);
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

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread, const char *name)
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
	HMTX mutex; ///< The mutex.
	HEV event;  ///< Event for waiting.
	uint recursive_count;     ///< Recursive lock count.

public:
	ThreadMutex_OS2() : recursive_count(0)
	{
		DosCreateMutexSem(NULL, &mutex, 0, FALSE);
		DosCreateEventSem(NULL, &event, 0, FALSE);
	}

	/* virtual */ ~ThreadMutex_OS2()
	{
		DosCloseMutexSem(mutex);
		DosCloseEventSem(event);
	}

	/* virtual */ void BeginCritical(bool allow_recursive = false)
	{
		/* os2 mutex is recursive by itself */
		DosRequestMutexSem(mutex, (unsigned long) SEM_INDEFINITE_WAIT);
		this->recursive_count++;
		if (!allow_recursive && this->recursive_count != 1) NOT_REACHED();
	}

	/* virtual */ void EndCritical(bool allow_recursive = false)
	{
		if (!allow_recursive && this->recursive_count != 1) NOT_REACHED();
		this->recursive_count--;
		DosReleaseMutexSem(mutex);
	}

	/* virtual */ void WaitForSignal()
	{
		assert(this->recursive_count == 1); // Do we need to call Begin/EndCritical multiple times otherwise?
		this->EndCritical();
		DosWaitEventSem(event, SEM_INDEFINITE_WAIT);
		this->BeginCritical();
	}

	/* virtual */ void SendSignal()
	{
		DosPostEventSem(event);
	}
};

/* static */ ThreadMutex *ThreadMutex::New()
{
	return new ThreadMutex_OS2();
}
