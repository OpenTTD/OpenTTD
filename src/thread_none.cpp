/* $Id$ */

/** @file thread_none.cpp No-Threads-Available implementation of Threads */

#include "stdafx.h"
#include "thread.h"

/* static */ ThreadObject *ThreadObject::New(OTTDThreadFunc proc, void *param)
{
	return NULL;
}

/* static */ ThreadObject *ThreadObject::AttachCurrent()
{
	return NULL;
}

/* static */ uint ThreadObject::CurrentId()
{
	return -1;
}

/* static */ ThreadSemaphore *ThreadSemaphore::New()
{
	return NULL;
}
