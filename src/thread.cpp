/* $Id$ */

/** @file thread.cpp */

#include "stdafx.h"
#include "thread.h"
#include "core/alloc_func.hpp"
#include <stdlib.h>

#if defined(__AMIGA__) || defined(PSP) || defined(NO_THREADS)
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

#elif defined(UNIX) && !defined(MORPHOS)

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


#elif defined(MORPHOS)

#include <exec/types.h>
#include <exec/rawfmt.h>
#include <dos/dostags.h>

#include <proto/dos.h>
#include <proto/exec.h>

#include <setjmp.h>

/* NOTE: this code heavily depends on latest libnix updates. So make
 *        sure you link with new stuff which supports semaphore locking of
 *        the IO resources, else it will just go foobar. */

struct OTTDThreadStartupMessage {
	struct Message msg;  ///< standard exec.library message (MUST be the first thing in the message struct!)
	OTTDThreadFunc func; ///< function the thread will execute
	void *arg;           ///< functions arguments for the thread function
	void *ret;           ///< return value of the thread function
	jmp_buf jumpstore;   ///< storage for the setjump state
};

struct OTTDThread {
	struct MsgPort *replyport;
	struct OTTDThreadStartupMessage msg;
};


/**
 *  Default OpenTTD STDIO/ERR debug output is not very useful for this, so we
 *  utilize serial/ramdebug instead.
 */
#ifndef NO_DEBUG_MESSAGES
void KPutStr(CONST_STRPTR format)
{
	RawDoFmt(format, NULL, (void (*)())RAWFMTFUNC_SERIAL, NULL);
}
#else
#define KPutStr(x)
#endif

static void Proxy(void)
{
	struct Task *child = FindTask(NULL);
	struct OTTDThreadStartupMessage *msg;

	/* Make sure, we don't block the parent. */
	SetTaskPri(child, -5);

	KPutStr("[Child] Progressing...\n");

	if (NewGetTaskAttrs(NULL, &msg, sizeof(struct OTTDThreadStartupMessage *), TASKINFOTYPE_STARTUPMSG, TAG_DONE) && msg != NULL) {
		/* Make use of setjmp() here, so this point can be reached again from inside
		 *  OTTDExitThread() which can be called from anythere inside msg->func.
		 *  It's a bit ugly and in worst case it leaks some memory. */
		if (setjmp(msg->jumpstore) == 0) {
			msg->ret = msg->func(msg->arg);
		} else {
			KPutStr("[Child] Returned to main()\n");
		}
	}

	/*  Quit the child, exec.library will reply the startup msg internally. */
	KPutStr("[Child] Done.\n");
}

OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void *arg)
{
	OTTDThread *t;
	struct Task *parent;

	KPutStr("[OpenTTD] Create thread...\n");

	t = (struct OTTDThread *)AllocVecTaskPooled(sizeof(struct OTTDThread));
	if (t == NULL) return NULL;

	parent = FindTask(NULL);

	/* Make sure main thread runs with sane priority */
	SetTaskPri(parent, 0);

	/* Things we'll pass down to the child by utilizing NP_StartupMsg */
	t->msg.func = function;
	t->msg.arg  = arg;
	t->msg.ret  = NULL;

	t->replyport = CreateMsgPort();

	if (t->replyport != NULL) {
		struct Process *child;

		t->msg.msg.mn_Node.ln_Type = NT_MESSAGE;
		t->msg.msg.mn_ReplyPort    = t->replyport;
		t->msg.msg.mn_Length       = sizeof(struct OTTDThreadStartupMessage);

		child = CreateNewProcTags(
			NP_CodeType,     CODETYPE_PPC,
			NP_Entry,        Proxy,
			NP_StartupMsg,   (ULONG)&t->msg,
			NP_Priority,     5UL,
			NP_Name,         (ULONG)"OpenTTD Thread",
			NP_PPCStackSize, 131072UL,
			TAG_DONE);

		if (child != NULL) {
			KPutStr("[OpenTTD] Child process launched.\n");
			return t;
		}
		DeleteMsgPort(t->replyport);
	}
	FreeVecTaskPooled(t);

	return NULL;
}

void* OTTDJoinThread(OTTDThread *t)
{
	struct OTTDThreadStartupMessage *reply;
	void *ret;

	KPutStr("[OpenTTD] Join threads...\n");

	if (t == NULL) return NULL;

	KPutStr("[OpenTTD] Wait for child to quit...\n");
	WaitPort(t->replyport);

	reply = (struct OTTDThreadStartupMessage *)GetMsg(t->replyport);
	ret   = reply->ret;

	DeleteMsgPort(t->replyport);
	FreeVecTaskPooled(t);

	return ret;
}

void OTTDExitThread()
{
	struct OTTDThreadStartupMessage *msg;

	KPutStr("[Child] Aborting...\n");

	if (NewGetTaskAttrs(NULL, &msg, sizeof(struct OTTDThreadStartupMessage *), TASKINFOTYPE_STARTUPMSG, TAG_DONE) && msg != NULL) {
		KPutStr("[Child] Jumping back...\n");
		longjmp(msg->jumpstore, 0xBEAFCAFE);
	}

	NOT_REACHED();
}

#endif
