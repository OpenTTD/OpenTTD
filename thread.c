/* $Id$ */

#include "stdafx.h"
#include "thread.h"
#include <stdlib.h>

#if defined(__AMIGA__) || defined(__MORPHOS__)
Thread* OTTDCreateThread(ThreadFunc function, void* arg) { return NULL; }
void OTTDJoinThread(Thread*) {}


#elif defined(__OS2__)

#define INCL_DOS
#include <os2.h>
#include <process.h>

struct Thread {
	TID thread;
};

Thread* OTTDCreateThread(ThreadFunc function, void* arg)
{
	Thread* t = malloc(sizeof(*t));

	if (t == NULL) return NULL;

	t->thread = _beginthread(function, NULL, 32768, arg);
	if (t->thread != -1) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void OTTDJoinThread(Thread* t)
{
	if (t == NULL) return;

	DosWaitThread(&t->thread, DCWW_WAIT);
	free(t);
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

	if (pthread_create(&t->thread, NULL, (void* (*)(void*))function, arg) == 0) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void OTTDJoinThread(Thread* t)
{
	if (t == NULL) return;

	pthread_join(t->thread, NULL);
	free(t);
}


#elif defined(WIN32)

#include <windows.h>

struct Thread {
	HANDLE thread;
};

Thread* OTTDCreateThread(ThreadFunc function, void* arg)
{
	Thread* t = malloc(sizeof(*t));
	DWORD dwThreadId;

	if (t == NULL) return NULL;

	t->thread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)function, arg, 0, &dwThreadId
	);

	if (t->thread != NULL) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void OTTDJoinThread(Thread* t)
{
	if (t == NULL) return;

	WaitForSingleObject(t->thread, INFINITE);
	CloseHandle(t->thread);
	free(t);
}
#endif
