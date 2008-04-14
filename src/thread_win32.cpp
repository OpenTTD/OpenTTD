/* $Id$ */

/** @file thread_win32.cpp Win32 thread implementation of Threads. */

#include "stdafx.h"
#include "thread.h"
#include "debug.h"
#include "core/alloc_func.hpp"
#include <stdlib.h>
#include <windows.h>
#include <process.h>

/**
 * Win32 thread version for ThreadObject.
 */
class ThreadObject_Win32 : public ThreadObject {
private:
	uint     m_id_thr;
	HANDLE   m_h_thr;
	OTTDThreadFunc m_proc;
	void     *m_param;
	bool     m_attached;
	void     *ret;

public:
	/**
	 * Create a win32 thread and start it, calling proc(param).
	 */
	ThreadObject_Win32(OTTDThreadFunc proc, void *param) :
		m_id_thr(0),
		m_h_thr(NULL),
		m_proc(proc),
		m_param(param),
		m_attached(false)
	{
		m_h_thr = (HANDLE)_beginthreadex(NULL, 0, &stThreadProc, this, CREATE_SUSPENDED, &m_id_thr);
		if (m_h_thr == NULL) return;
		ResumeThread(m_h_thr);
	}

	/**
	 * Create a win32 thread and attach current thread to it.
	 */
	ThreadObject_Win32() :
		m_id_thr(0),
		m_h_thr(NULL),
		m_proc(NULL),
		m_param(NULL),
		m_attached(false)
	{
		BOOL ret = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_h_thr, 0, FALSE, DUPLICATE_SAME_ACCESS);
		if (!ret) return;
		m_id_thr = GetCurrentThreadId();
	}

	/* virtual */ ~ThreadObject_Win32()
	{
		if (m_h_thr != NULL) {
			CloseHandle(m_h_thr);
			m_h_thr = NULL;
		}
	}

	/* virtual */ bool IsRunning()
	{
		if (m_h_thr == NULL) return false;
		DWORD exit_code = 0;
		if (!GetExitCodeThread(m_h_thr, &exit_code)) return false;
		return (exit_code == STILL_ACTIVE);
	}

	/* virtual */ bool WaitForStop()
	{
		/* You can't wait on yourself */
		assert(!IsCurrent());
		/* If the thread is not running, waiting is over */
		if (!IsRunning()) return true;

		DWORD res = WaitForSingleObject(m_h_thr, INFINITE);
		return res == WAIT_OBJECT_0;
	}

	/* virtual */ bool Exit()
	{
		/* You can only exit yourself */
		assert(IsCurrent());
		/* If the thread is not running, we are already closed */
		if (!IsRunning()) return false;

		/* For now we terminate by throwing an error, gives much cleaner cleanup */
		throw 0;
	}

	/* virtual */ void *Join()
	{
		/* You cannot join yourself */
		assert(!IsCurrent());

		WaitForSingleObject(m_h_thr, INFINITE);

		return this->ret;
	}

	/* virtual */ bool IsCurrent()
	{
		DWORD id_cur = GetCurrentThreadId();
		return id_cur == m_id_thr;
	}

	/* virtual */ uint GetId()
	{
		return m_id_thr;
	}

private:
	/**
	 * On thread creation, this function is called, which calls the real startup
	 *  function. This to get back into the correct instance again.
	 */
	static uint CALLBACK stThreadProc(void *thr)
	{
		return ((ThreadObject_Win32 *)thr)->ThreadProc();
	}

	/**
	 * A new thread is created, and this function is called. Call the custom
	 *  function of the creator of the thread.
	 */
	uint ThreadProc()
	{
		try {
			this->ret = m_proc(m_param);
		} catch (...) {
		}

		return 0;
	}
};

/* static */ ThreadObject *ThreadObject::New(OTTDThreadFunc proc, void *param)
{
	return new ThreadObject_Win32(proc, param);
}

/* static */ ThreadObject* ThreadObject::AttachCurrent()
{
	return new ThreadObject_Win32();
}

/* static */ uint ThreadObject::CurrentId()
{
	return GetCurrentThreadId();
}


/**
 * Win32 thread version of ThreadSemaphore.
 */
class ThreadSemaphore_Win32 : public ThreadSemaphore {
private:
	HANDLE m_handle;

public:
	ThreadSemaphore_Win32()
	{
		m_handle = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	/* virtual */ ~ThreadSemaphore_Win32()
	{
		::CloseHandle(m_handle);
	}

	/* virtual */ void Set()
	{
		::SetEvent(m_handle);
	}

	/* virtual */ void Wait()
	{
		::WaitForSingleObject(m_handle, INFINITE);
	}
};

/* static */ ThreadSemaphore *ThreadSemaphore::New()
{
	return new ThreadSemaphore_Win32();
}
