/* $Id$ */

/** @file thread_none.cpp No-Threads-Available implementation of Threads */

#include "stdafx.h"
#include "thread.h"

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread)
{
	if (thread != NULL) *thread = NULL;
	return false;
}

/** Mutex that doesn't do locking because it ain't needed when there're no threads */
class ThreadMutex_None : public ThreadMutex {
public:
	virtual void BeginCritical() {}
	virtual void EndCritical() {}
};

/* static */ ThreadMutex *ThreadMutex::New()
{
	return new ThreadMutex_None();
}
