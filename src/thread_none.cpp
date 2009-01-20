/* $Id$ */

/** @file thread_none.cpp No-Threads-Available implementation of Threads */

#include "stdafx.h"
#include "thread.h"

/* static */ bool ThreadObject::New(OTTDThreadFunc proc, void *param, ThreadObject **thread)
{
	if (thread != NULL) *thread = NULL;
	return false;
}
