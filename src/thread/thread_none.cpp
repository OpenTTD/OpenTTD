/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file thread_none.cpp No-Threads-Available implementation of Threads */

#include "../stdafx.h"
#include "thread.h"

#include "../safeguards.h"

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread, const char *name)
{
	if (thread != NULL) *thread = NULL;
	return false;
}

/** Mutex that doesn't do locking because it ain't needed when there're no threads */
class ThreadMutex_None : public ThreadMutex {
public:
	virtual void BeginCritical(bool allow_recursive = false) {}
	virtual void EndCritical(bool allow_recursive = false) {}
	virtual void WaitForSignal() {}
	virtual void SendSignal() {}
};

/* static */ ThreadMutex *ThreadMutex::New()
{
	return new ThreadMutex_None();
}
