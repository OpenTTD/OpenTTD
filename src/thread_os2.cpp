/* $Id$ */

/** @file thread_os2.cpp OS2 implementation of Threads. */

#include "stdafx.h"
#include "thread.h"

#if 0
#include "debug.h"
#include "core/alloc_func.hpp"
#include <stdlib.h>

#define INCL_DOS
#include <os2.h>
#include <process.h>

struct OTTDThread {
	TID thread;
	OTTDThreadFunc func;
	void *arg;
	void *ret;
};

static void Proxy(void *arg)
{
	OTTDThread *t = (OTTDThread *)arg;
	t->ret = t->func(t->arg);
}

OTTDThread *OTTDCreateThread(OTTDThreadFunc function, void *arg)
{
	OTTDThread *t = MallocT<OTTDThread>(1);

	t->func = function;
	t->arg  = arg;
	t->thread = _beginthread(Proxy, NULL, 32768, t);
	if (t->thread != (TID)-1) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void *OTTDJoinThread(OTTDThread *t)
{
	if (t == NULL) return NULL;

	DosWaitThread(&t->thread, DCWW_WAIT);
	void *ret = t->ret;
	free(t);
	return ret;
}

void OTTDExitThread()
{
	_endthread();
}

#endif

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
