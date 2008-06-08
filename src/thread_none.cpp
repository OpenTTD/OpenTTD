/* $Id$ */

/** @file thread_none.cpp No-Threads-Available implementation of Threads */

#include "stdafx.h"
#include "thread.h"
#include "fiber.hpp"

/* static */ ThreadObject *ThreadObject::New(OTTDThreadFunc proc, void *param, OTTDThreadTerminateFunc terminate_func)
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

/* static */ Fiber *Fiber::New(FiberFunc proc, void *param)
{
	return NULL;
}

/* static */ Fiber *Fiber::AttachCurrent(void *param)
{
	return NULL;
}

/* static */ void *Fiber::GetCurrentFiberData()
{
	return NULL;
}
