/* $Id$ */

/** @file fiber_win32.cpp Win32 implementation of Fiber. */

#include "stdafx.h"
#include "fiber.hpp"
#include <stdlib.h>
#include <windows.h>
#include <process.h>

class Fiber_Win32 : public Fiber {
private:
	LPVOID m_fiber;
	FiberFunc m_proc;
	void *m_param;
	bool m_attached;

	static Fiber_Win32 *s_main;

public:
	/**
	 * Create a win32 fiber and start it, calling proc(param).
	 */
	Fiber_Win32(FiberFunc proc, void *param) :
		m_fiber(NULL),
		m_proc(proc),
		m_param(param),
		m_attached(false)
	{
		CreateFiber();
	}

	/**
	 * Create a win32 fiber and attach current thread to it.
	 */
	Fiber_Win32(void *param) :
		m_fiber(NULL),
		m_proc(NULL),
		m_param(param),
		m_attached(true)
	{
		ConvertThreadToFiber();
		if (s_main == NULL) s_main = this;
	}

	/* virtual */ ~Fiber_Win32()
	{
		if (this->m_fiber != NULL) {
			if (this->m_attached) {
				this->ConvertFiberToThread();
			} else {
				this->DeleteFiber();
			}
		}
	}

	/* virtual */ void SwitchToFiber()
	{
		typedef VOID (WINAPI *FnSwitchToFiber)(LPVOID fiber);

		static FnSwitchToFiber fnSwitchToFiber = (FnSwitchToFiber)stGetProcAddr("SwitchToFiber");
		assert(fnSwitchToFiber != NULL);

		fnSwitchToFiber(this->m_fiber);
	}

	/* virtual */ void Exit()
	{
		/* Simply switch back to the main fiber, we kill the fiber sooner or later */
		s_main->SwitchToFiber();
	}

	/* virtual */ bool IsRunning()
	{
		return this->m_fiber != NULL;
	}

	/* virtual */ void *GetFiberData()
	{
		return this->m_param;
	}

	/**
	 * Win95 doesn't have Fiber support. So check if we have Fiber support,
	 *  and else fall back on Fiber_Thread.
	 */
	static bool IsSupported()
	{
		static bool first_run = true;
		static bool is_supported = false;

		if (first_run) {
			first_run = false;
			static const char *names[] = {
				"ConvertThreadToFiber",
				"CreateFiber",
				"DeleteFiber",
				"ConvertFiberToThread",
				"SwitchToFiber"};
			for (size_t i = 0; i < lengthof(names); i++) {
				if (stGetProcAddr(names[i]) == NULL) return false;
			}
			is_supported = true;
		}
		return is_supported;
	}

private:
	/**
	 * Get a function from kernel32.dll.
	 * @param name Function to get.
	 * @return Proc to the function, or NULL when not found.
	 */
	static FARPROC stGetProcAddr(const char *name)
	{
		static HMODULE hKernel = LoadLibraryA("kernel32.dll");
		return GetProcAddress(hKernel, name);
	}

	/**
	 * First function which is called within the fiber.
	 */
	static VOID CALLBACK stFiberProc(LPVOID fiber)
	{
		Fiber_Win32 *cur = (Fiber_Win32 *)fiber;
		cur->m_proc(cur->m_param);
	}

	/**
	 * Delete a fiber.
	 */
	void DeleteFiber()
	{
		typedef VOID (WINAPI *FnDeleteFiber)(LPVOID lpFiber);

		static FnDeleteFiber fnDeleteFiber = (FnDeleteFiber)stGetProcAddr("DeleteFiber");
		assert(fnDeleteFiber != NULL);

		fnDeleteFiber(this->m_fiber);
		this->m_fiber = NULL;
	}

	/**
	 * Convert a current thread to a fiber.
	 */
	void ConvertThreadToFiber()
	{
		typedef LPVOID (WINAPI *FnConvertThreadToFiber)(LPVOID lpParameter);

		static FnConvertThreadToFiber fnConvertThreadToFiber = (FnConvertThreadToFiber)stGetProcAddr("ConvertThreadToFiber");
		assert(fnConvertThreadToFiber != NULL);

		this->m_fiber = fnConvertThreadToFiber(this);
	}

	/**
	 * Create a new fiber.
	 */
	void CreateFiber()
	{
		typedef LPVOID (WINAPI *FnCreateFiber)(SIZE_T dwStackSize, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter);

		static FnCreateFiber fnCreateFiber = (FnCreateFiber)stGetProcAddr("CreateFiber");
		assert(fnCreateFiber != NULL);

		this->m_fiber = fnCreateFiber(0, &stFiberProc, this);
	}

	/**
	 * Convert a fiber back to a thread.
	 */
	void ConvertFiberToThread()
	{
		typedef BOOL (WINAPI *FnConvertFiberToThread)();

		static FnConvertFiberToThread fnConvertFiberToThread = (FnConvertFiberToThread)stGetProcAddr("ConvertFiberToThread");
		assert(fnConvertFiberToThread != NULL);

		fnConvertFiberToThread();
		this->m_fiber = NULL;
	}
};

/* Initialize the static member of Fiber_Win32 */
/* static */ Fiber_Win32 *Fiber_Win32::s_main = NULL;

/* Include Fiber_Thread, as Win95 needs it */
#include "fiber_thread.cpp"

/* static */ Fiber *Fiber::New(FiberFunc proc, void *param)
{
	if (Fiber_Win32::IsSupported()) return new Fiber_Win32(proc, param);
	return new Fiber_Thread(proc, param);
}

/* static */ Fiber *Fiber::AttachCurrent(void *param)
{
	if (Fiber_Win32::IsSupported()) return new Fiber_Win32(param);
	return new Fiber_Thread(param);
}

/* static */ void *Fiber::GetCurrentFiberData()
{
	if (Fiber_Win32::IsSupported()) return ((Fiber *)::GetFiberData())->GetFiberData();
	return Fiber_Thread::GetCurrentFiber()->GetFiberData();
}
