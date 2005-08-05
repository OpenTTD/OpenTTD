/* $Id$ */

#include "stdafx.h"
#include "thread.h"
#include <stdlib.h>

#if defined(__AMIGA__) || defined(__MORPHOS__)
Thread* OTTDCreateThread(ThreadFunc function, void* arg) { return NULL; }
void* OTTDJoinThread(Thread*) { return NULL; }


#elif defined(__OS2__)

#define INCL_DOS
#include <os2.h>
#include <process.h>

struct Thread {
	TID thread;
	ThreadFunc func;
	void* arg;
	void* ret;
};

static void Proxy(void* arg)
{
	Thread* t = arg;
	t->ret = t->func(t->arg);
}

Thread* OTTDCreateThread(ThreadFunc function, void* arg)
{
	Thread* t = malloc(sizeof(*t));

	if (t == NULL) return NULL;

	t->func = function;
	t->arg  = arg;
	t->thread = _beginthread(Proxy, NULL, 32768, t);
	if (t->thread != -1) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void* OTTDJoinThread(Thread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	DosWaitThread(&t->thread, DCWW_WAIT);
	ret = t->ret;
	free(t);
	return ret;
}


#elif defined(UNIX)

#include <pthread.h>

struct Thread {
	pthread_t thread;
};

Thread* OTTDCreateThread(ThreadFunc function, void* arg)
{
	Thread* t = malloc(sizeof(*t));

	if (t == NULL) return NULL;

	if (pthread_create(&t->thread, NULL, function, arg) == 0) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void* OTTDJoinThread(Thread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	pthread_join(t->thread, &ret);
	free(t);
	return ret;
}


#elif defined(WIN32)

#include <windows.h>

struct Thread {
	HANDLE thread;
	ThreadFunc func;
	void* arg;
	void* ret;
};

static DWORD WINAPI Proxy(LPVOID arg)
{
	Thread* t = arg;
	t->ret = t->func(t->arg);
	return 0;
}

Thread* OTTDCreateThread(ThreadFunc function, void* arg)
{
	Thread* t = malloc(sizeof(*t));
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

void* OTTDJoinThread(Thread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	WaitForSingleObject(t->thread, INFINITE);
	CloseHandle(t->thread);
	ret = t->ret;
	free(t);
	return ret;
}
#endif
