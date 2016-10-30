/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file thread.h Base of all threads. */

#ifndef THREAD_H
#define THREAD_H

/** Definition of all thread entry functions. */
typedef void (*OTTDThreadFunc)(void *);

/** Signal used for signalling we knowingly want to end the thread. */
class OTTDThreadExitSignal { };

/**
 * A Thread Object which works on all our supported OSes.
 */
class ThreadObject {
public:
	/**
	 * Virtual destructor to allow 'delete' operator to work properly.
	 */
	virtual ~ThreadObject() {};

	/**
	 * Exit this thread.
	 */
	virtual bool Exit() = 0;

	/**
	 * Join this thread.
	 */
	virtual void Join() = 0;

	/**
	 * Create a thread; proc will be called as first function inside the thread,
	 *  with optional params.
	 * @param proc The procedure to call inside the thread.
	 * @param param The params to give with 'proc'.
	 * @param thread Place to store a pointer to the thread in. May be NULL.
	 * @param name A name for the thread. May be NULL.
	 * @return True if the thread was started correctly.
	 */
	static bool New(OTTDThreadFunc proc, void *param, ThreadObject **thread = NULL, const char *name = NULL);
};

/**
 * Cross-platform Mutex
 */
class ThreadMutex {
public:
	/**
	 * Create a new mutex.
	 */
	static ThreadMutex *New();

	/**
	 * Virtual Destructor to avoid compiler warnings.
	 */
	virtual ~ThreadMutex() {};

	/**
	 * Begin the critical section
	 * @param allow_recursive Whether recursive locking is intentional.
	 *                        If false, NOT_REACHED() will be called when the mutex is already locked
	 *                        by the current thread.
	 */
	virtual void BeginCritical(bool allow_recursive = false) = 0;

	/**
	 * End of the critical section
	 * @param allow_recursive Whether recursive unlocking is intentional.
	 *                        If false, NOT_REACHED() will be called when the mutex was locked more
	 *                        than once by the current thread.
	 */
	virtual void EndCritical(bool allow_recursive = false) = 0;

	/**
	 * Wait for a signal to be send.
	 * @pre You must be in the critical section.
	 * @note While waiting the critical section is left.
	 * @post You will be in the critical section.
	 */
	virtual void WaitForSignal() = 0;

	/**
	 * Send a signal and wake the 'thread' that was waiting for it.
	 */
	virtual void SendSignal() = 0;
};

/**
 * Simple mutex locker to keep a mutex locked until the locker goes out of scope.
 */
class ThreadMutexLocker {
public:
	/**
	 * Lock the mutex and keep it locked for the life time of this object.
	 * @param mutex Mutex to be locked.
	 */
	ThreadMutexLocker(ThreadMutex *mutex) : mutex(mutex) { mutex->BeginCritical(); }

	/**
	 * Unlock the mutex.
	 */
	~ThreadMutexLocker() { this->mutex->EndCritical(); }

private:
	ThreadMutexLocker(const ThreadMutexLocker &) { NOT_REACHED(); }
	ThreadMutexLocker &operator=(const ThreadMutexLocker &) { NOT_REACHED(); return *this; }
	ThreadMutex *mutex;
};

/**
 * Get number of processor cores in the system, including HyperThreading or similar.
 * @return Total number of processor cores.
 */
uint GetCPUCoreCount();

#endif /* THREAD_H */
