/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file thread_win32.cpp Win32 thread implementation of Threads. */

#include "../stdafx.h"
#include "thread.h"
#include "../debug.h"
#include "../core/alloc_func.hpp"
#include <stdlib.h>
#include <windows.h>
#include <process.h>
#include "../os/windows/win32.h"

#include "../safeguards.h"

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
	const char *name;    ///< Thread name.

public:
	/**
	 * Create a win32 thread and start it, calling proc(param).
	 */
	ThreadObject_Win32(OTTDThreadFunc proc, void *param, bool self_destruct, const char *name) :
		thread(NULL),
		id(0),
		proc(proc),
		param(param),
		self_destruct(self_destruct),
		name(name)
	{
		this->thread = (HANDLE)_beginthreadex(NULL, 0, &stThreadProc, this, CREATE_SUSPENDED, &this->id);
		if (this->thread == NULL) return;
		ResumeThread(this->thread);
	}

	~ThreadObject_Win32() override
	{
		if (this->thread != NULL) {
			CloseHandle(this->thread);
			this->thread = NULL;
		}
	}

	bool Exit() override
	{
		assert(GetCurrentThreadId() == this->id);
		/* For now we terminate by throwing an error, gives much cleaner cleanup */
		throw OTTDThreadExitSignal();
	}

	void Join() override
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
#ifdef _MSC_VER
		/* Set thread name for debuggers. Has to be done from the thread due to a race condition in older MS debuggers. */
		SetWin32ThreadName(-1, this->name);
#endif
		try {
			this->proc(this->param);
		} catch (OTTDThreadExitSignal) {
		} catch (...) {
			NOT_REACHED();
		}

		if (self_destruct) delete this;
	}
};

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread, const char *name)
{
	ThreadObject *to = new ThreadObject_Win32(proc, param, thread == NULL, name);
	if (thread != NULL) *thread = to;
	return true;
}
