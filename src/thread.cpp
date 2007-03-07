/* $Id$ */

#include "stdafx.h"
#include "thread.h"
#include <stdlib.h>
#include "helpers.hpp"

#if defined(__AMIGA__) || defined(__MORPHOS__) || defined(PSP) || defined(NO_THREADS)
OTTDThread *OTTDCreateThread(OTTDThreadFunc function, void *arg) { return NULL; }
void *OTTDJoinThread(OTTDThread *t) { return NULL; }
void OTTDExitThread() { NOT_REACHED(); };

#elif defined(__OS2__)

#define INCL_DOS
#include <os2.h>
#include <process.h>

struct OTTDThread {
	TID thread;
	OTTDThreadFunc func;
	void* arg;
	void* ret;
};

static void Proxy(void* arg)
{
	OTTDThread* t = (OTTDThread*)arg;
	t->ret = t->func(t->arg);
}

OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void* arg)
{
	OTTDThread* t = MallocT<OTTDThread>(1);

	if (t == NULL) return NULL;

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

void* OTTDJoinThread(OTTDThread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	DosWaitThread(&t->thread, DCWW_WAIT);
	ret = t->ret;
	free(t);
	return ret;
}

void OTTDExitThread()
{
	_endthread();
}

#elif defined(UNIX)

#include <pthread.h>

struct OTTDThread {
	pthread_t thread;
};

OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void* arg)
{
	OTTDThread* t = MallocT<OTTDThread>(1);

	if (t == NULL) return NULL;

	if (pthread_create(&t->thread, NULL, function, arg) == 0) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void* OTTDJoinThread(OTTDThread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	pthread_join(t->thread, &ret);
	free(t);
	return ret;
}

void OTTDExitThread()
{
	pthread_exit(NULL);
}

#elif defined(WIN32)

#include <windows.h>

struct OTTDThread {
	HANDLE thread;
	OTTDThreadFunc func;
	void* arg;
	void* ret;
};

static DWORD WINAPI Proxy(LPVOID arg)
{
	OTTDThread* t = (OTTDThread*)arg;
	t->ret = t->func(t->arg);
	return 0;
}

OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void* arg)
{
	OTTDThread* t = MallocT<OTTDThread>(1);
	DWORD dwThreadId;

	if (t == NULL) return NULL;

	t->func = function;
	t->arg  = arg;
	t->thread = CreateThread(NULL, 0, Proxy, t, 0, &dwThreadId);

	if (t->thread != NULL) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void* OTTDJoinThread(OTTDThread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	WaitForSingleObject(t->thread, INFINITE);
	CloseHandle(t->thread);
	ret = t->ret;
	free(t);
	return ret;
}

void OTTDExitThread()
{
	ExitThread(0);
}
#endif
