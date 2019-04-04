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

	bool Exit() override
	{
		_endthread();
		return true;
	}

	void Join() override
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
