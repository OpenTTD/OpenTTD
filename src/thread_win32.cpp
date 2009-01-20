/* $Id$ */

/** @file thread_win32.cpp Win32 thread implementation of Threads. */

#include "stdafx.h"
#include "thread.h"
#include "debug.h"
#include "core/alloc_func.hpp"
#include <stdlib.h>
#include <windows.h>
#include <process.h>

/**
 * Win32 thread version for ThreadObject.
 */
class ThreadObject_Win32 : public ThreadObject {
private:
	HANDLE thread;       ///< System thread identifier.
	uint id;             ///< Thread identifier.
	OTTDThreadFunc proc; ///< External thread procedure.
	void *param;         ///< Parameter for the external thread procedure.
	bool self_destruct;  ///< Free ourselves when done?

public:
	/**
	 * Create a win32 thread and start it, calling proc(param).
	 */
	ThreadObject_Win32(OTTDThreadFunc proc, void *param, bool self_destruct) :
		thread(NULL),
		id(0),
		proc(proc),
		param(param),
		self_destruct(self_destruct)
	{
		this->thread = (HANDLE)_beginthreadex(NULL, 0, &stThreadProc, this, CREATE_SUSPENDED, &this->id);
		if (this->thread == NULL) return;
		ResumeThread(this->thread);
	}

	/* virtual */ ~ThreadObject_Win32()
	{
		if (this->thread != NULL) {
			CloseHandle(this->thread);
			this->thread = NULL;
		}
	}

	/* virtual */ bool Exit()
	{
		assert(GetCurrentThreadId() == this->id);
		/* For now we terminate by throwing an error, gives much cleaner cleanup */
		throw OTTDThreadExitSignal();
	}

	/* virtual */ void Join()
	{
		/* You cannot join yourself */
		assert(GetCurrentThreadId() != this->id);
		WaitForSingleObject(this->thread, INFINITE);
	}

private:
	/**
	 * On thread creation, this function is called, which calls the real startup
	 *  function. This to get back into the correct instance again.
	 */
	static uint CALLBACK stThreadProc(void *thr)
	{
		((ThreadObject_Win32 *)thr)->ThreadProc();
		return 0;
	}

	/**
	 * A new thread is created, and this function is called. Call the custom
	 *  function of the creator of the thread.
	 */
	void ThreadProc()
	{
		try {
			this->proc(this->param);
		} catch (OTTDThreadExitSignal) {
		} catch (...) {
			NOT_REACHED();
		}

		if (self_destruct) delete this;
	}
};

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread)
{
	ThreadObject *to = new ThreadObject_Win32(proc, param, thread == NULL);
	if (thread != NULL) *thread = to;
	return true;
}

/**
 * Win32 thread version of ThreadMutex.
 */
class ThreadMutex_Win32 : public ThreadMutex {
private:
	CRITICAL_SECTION critical_section;

public:
	ThreadMutex_Win32()
	{
		InitializeCriticalSection(&this->critical_section);
	}

	/* virtual */ ~ThreadMutex_Win32()
	{
		DeleteCriticalSection(&this->critical_section);
	}

	/* virtual */ void BeginCritical()
	{
		EnterCriticalSection(&this->critical_section);
	}

	/* virtual */ void EndCritical()
	{
		LeaveCriticalSection(&this->critical_section);
	}
};

/* static */ ThreadMutex *ThreadMutex::New()
{
	return new ThreadMutex_Win32();
}
