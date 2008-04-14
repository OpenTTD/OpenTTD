/* $Id$ */

/** @file fiber_thread.cpp ThreadObject implementation of Fiber. */

#include "stdafx.h"
#include "fiber.hpp"
#include "thread.h"
#include <stdlib.h>

class Fiber_Thread : public Fiber {
private:
	ThreadObject *m_thread;
	FiberFunc m_proc;
	void *m_param;
	bool m_attached;
	ThreadSemaphore *m_sem;
	bool m_kill;

	static Fiber_Thread *s_current;
	static Fiber_Thread *s_main;

public:
	/**
	 * Create a ThreadObject fiber and start it, calling proc(param).
	 */
	Fiber_Thread(FiberFunc proc, void *param) :
		m_thread(NULL),
		m_proc(proc),
		m_param(param),
		m_attached(false),
		m_kill(false)
	{
		this->m_sem = ThreadSemaphore::New();
		/* Create a thread and start stFiberProc */
		this->m_thread = ThreadObject::New(&stFiberProc, this);
	}

	/**
	 * Create a ThreadObject fiber and attach current thread to it.
	 */
	Fiber_Thread(void *param) :
		m_thread(NULL),
		m_proc(NULL),
		m_param(param),
		m_attached(true),
		m_kill(false)
	{
		this->m_sem = ThreadSemaphore::New();
		/* Attach the current thread to this Fiber */
		this->m_thread = ThreadObject::AttachCurrent();
		/* We are the current thread */
		if (s_current == NULL) s_current = this;
		if (s_main == NULL) s_main = this;
	}

	~Fiber_Thread()
	{
		/* Remove the thread if needed */
		if (this->m_thread != NULL) {
			assert(this->m_attached || !this->m_thread->IsRunning());
			delete this->m_thread;
		}
		/* Remove the semaphore */
		delete this->m_sem;
	}

	/* virtual */ void SwitchToFiber()
	{
		/* You can't switch to yourself */
		assert(s_current != this);
		Fiber_Thread *cur = s_current;

		/* Continue the execution of 'this' Fiber */
		this->m_sem->Set();
		/* Hold the execution of the current Fiber */
		cur->m_sem->Wait();
		if (this->m_kill) {
			/* If the thread we switched too was killed, join it so it can finish quiting */
			this->m_thread->Join();
		}
		/* If we continue, we are the current thread */
		s_current = cur;
	}

	/* virtual */ void Exit()
	{
		/* Kill off our thread */
		this->m_kill = true;
		this->m_thread->Exit();
	}

	/* virtual */ bool IsRunning()
	{
		if (this->m_thread == NULL) return false;
		return this->m_thread->IsRunning();
	}

	/* virtual */ void *GetFiberData()
	{
		return this->m_param;
	}

	static Fiber_Thread *GetCurrentFiber()
	{
		return s_current;
	}

private:
	/**
	 * First function which is called within the fiber.
	 */
	static void * CDECL stFiberProc(void *fiber)
	{
		Fiber_Thread *cur = (Fiber_Thread *)fiber;
		/* Now suspend the thread until we get SwitchToFiber() for the first time */
		cur->m_sem->Wait();
		/* If we continue, we are the current thread */
		s_current = cur;

		try {
			cur->m_proc(cur->m_param);
		} catch (...) {
			/* Unlock the main thread */
			s_main->m_sem->Set();
			throw;
		}

		return NULL;
	}
};

/* Initialize the static member of Fiber_Thread */
/* static */ Fiber_Thread *Fiber_Thread::s_current = NULL;
/* static */ Fiber_Thread *Fiber_Thread::s_main = NULL;

#ifndef WIN32

/* static */ Fiber *Fiber::New(FiberFunc proc, void *param)
{
	return new Fiber_Thread(proc, param);
}

/* static */ Fiber *Fiber::AttachCurrent(void *param)
{
	return new Fiber_Thread(param);
}

/* static */ void *Fiber::GetCurrentFiberData()
{
	return Fiber_Thread::GetCurrentFiber()->GetFiberData();
}

#endif /* WIN32 */
