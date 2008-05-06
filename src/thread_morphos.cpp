/* $Id$ */

/** @file thread_morphos.cpp MorphOS implementation of Threads. */

#include "stdafx.h"
#include "thread.h"
#include "debug.h"
#include "core/alloc_func.hpp"
#include <stdlib.h>
#include <unistd.h>

#include <exec/types.h>
#include <exec/rawfmt.h>
#include <dos/dostags.h>

#include <proto/dos.h>
#include <proto/exec.h>

/**
 *  avoid name clashes with MorphOS API functions
 */
#undef Exit
#undef Wait


/**
 *  NOTE: this code heavily depends on latest libnix updates. So make
 *        sure you link with new stuff which supports semaphore locking of
 *        the IO resources, else it will just go foobar.
 */


struct OTTDThreadStartupMessage {
	struct Message msg;  ///< standard exec.library message (MUST be the first thing in the message struct!)
	OTTDThreadFunc func; ///< function the thread will execute
	void *arg;           ///< functions arguments for the thread function
	void *ret;           ///< return value of the thread function
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


/**
 * MorphOS version for ThreadObject.
 */
class ThreadObject_MorphOS : public ThreadObject {
private:
	APTR m_thr;                  ///< System thread identifier.
	struct MsgPort *m_replyport;
	struct OTTDThreadStartupMessage m_msg;

public:
	/**
	 * Create a sub process and start it, calling proc(param).
	 */
	ThreadObject_MorphOS(OTTDThreadFunc proc, void *param) : m_thr(0)
	{
		struct Task *parent;

		KPutStr("[OpenTTD] Create thread...\n");

		parent = FindTask(NULL);

		/* Make sure main thread runs with sane priority */
		SetTaskPri(parent, 0);

		/* Things we'll pass down to the child by utilizing NP_StartupMsg */
		m_msg.func = proc;
		m_msg.arg  = param;
		m_msg.ret  = NULL;

		m_replyport = CreateMsgPort();

		if (m_replyport != NULL) {
			struct Process *child;

			m_msg.msg.mn_Node.ln_Type = NT_MESSAGE;
			m_msg.msg.mn_ReplyPort    = m_replyport;
			m_msg.msg.mn_Length       = sizeof(struct OTTDThreadStartupMessage);

			child = CreateNewProcTags(
				NP_CodeType,     CODETYPE_PPC,
				NP_Entry,        ThreadObject_MorphOS::Proxy,
				NP_StartupMsg,   (IPTR)&m_msg,
				NP_Priority,     5UL,
				NP_Name,         (IPTR)"OpenTTD Thread",
				NP_PPCStackSize, 131072UL,
				TAG_DONE);

			m_thr = (APTR) child;

			if (child != NULL) {
				KPutStr("[OpenTTD] Child process launched.\n");
			} else {
				KPutStr("[OpenTTD] Couldn't create child process. (constructors never fail, yeah!)\n");
				DeleteMsgPort(m_replyport);
			}
		}
	}

	/**
	 * Create a thread and attach current thread to it.
	 */
	ThreadObject_MorphOS() : m_thr(0)
	{
		m_thr = FindTask(NULL);
	}

	/* virtual */ ~ThreadObject_MorphOS()
	{
	}

	/* virtual */ bool IsRunning()
	{
		return m_thr != 0;
	}

	/* virtual */ bool WaitForStop()
	{
		/* You can't wait on yourself */
		assert(!IsCurrent());
		/* If the thread is not running, waiting is over */
		if (!IsRunning()) return true;

		WaitPort(m_replyport);

		GetMsg(m_replyport);
		DeleteMsgPort(m_replyport);

		return true;
	}

	/* virtual */ bool Exit()
	{
		struct OTTDThreadStartupMessage *msg;

		/* You can only exit yourself */
		assert(IsCurrent());
		/* If the thread is not running, we are already closed */
		if (!IsRunning()) return false;

		KPutStr("[Child] Aborting...\n");

		if (NewGetTaskAttrs(NULL, &msg, sizeof(struct OTTDThreadStartupMessage *), TASKINFOTYPE_STARTUPMSG, TAG_DONE) && msg != NULL) {
			/* For now we terminate by throwing an error, gives much cleaner cleanup */
			throw 0;
		}

		return true;
	}

	/* virtual */ void *Join()
	{
		struct OTTDThreadStartupMessage *reply;
		void *ret;

		/* You cannot join yourself */
		assert(!IsCurrent());

		KPutStr("[OpenTTD] Join threads...\n");
		KPutStr("[OpenTTD] Wait for child to quit...\n");
		WaitPort(m_replyport);

		reply = (struct OTTDThreadStartupMessage *)GetMsg(m_replyport);
		ret   = reply->ret;

		DeleteMsgPort(m_replyport);
		m_thr = 0;

		return ret;
	}

	/* virtual */ bool IsCurrent()
	{
		return FindTask(NULL) == m_thr;
	}

	/* virtual */ uint GetId()
	{
		return (uint)m_thr;
	}

private:
	/**
	 * On thread creation, this function is called, which calls the real startup
	 *  function. This to get back into the correct instance again.
	 */
	static void Proxy(void)
	{
		struct Task *child = FindTask(NULL);
		struct OTTDThreadStartupMessage *msg;

		/* Make sure, we don't block the parent. */
		SetTaskPri(child, -5);

		KPutStr("[Child] Progressing...\n");

		if (NewGetTaskAttrs(NULL, &msg, sizeof(struct OTTDThreadStartupMessage *), TASKINFOTYPE_STARTUPMSG, TAG_DONE) && msg != NULL) {
			try {
				msg->ret = msg->func(msg->arg);
			} catch(...) {
				KPutStr("[Child] Returned to main()\n");
			}
		}

		/*  Quit the child, exec.library will reply the startup msg internally. */
		KPutStr("[Child] Done.\n");
	}
};

/* static */ ThreadObject *ThreadObject::New(OTTDThreadFunc proc, void *param)
{
	return new ThreadObject_MorphOS(proc, param);
}

/* static */ ThreadObject *ThreadObject::AttachCurrent()
{
	return new ThreadObject_MorphOS();
}

/* static */ uint ThreadObject::CurrentId()
{
	return (uint) FindTask(NULL);
}


/**
 * MorphOS version of ThreadSemaphore.
 */
class ThreadSemaphore_MorphOS : public ThreadSemaphore {
private:
	struct SignalSemaphore m_sem;

public:
	ThreadSemaphore_MorphOS()
	{
		InitSemaphore(&m_sem);
	}

	/* virtual */ ~ThreadSemaphore_MorphOS()
	{

	}

	/* virtual */ void Set()
	{
		// Check if semaphore count is really important there.
		ReleaseSemaphore(&m_sem);
	}

	/* virtual */ void Wait()
	{
		ObtainSemaphore(&m_sem);
	}
};

/* static */ ThreadSemaphore *ThreadSemaphore::New()
{
	return new ThreadSemaphore_MorphOS();
}
