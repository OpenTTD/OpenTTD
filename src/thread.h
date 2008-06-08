/* $Id$ */

/** @file thread.h Base of all threads. */

#ifndef THREAD_H
#define THREAD_H

typedef void (*OTTDThreadFunc)(void *);

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
	 * Check if the thread is currently running.
	 * @return True if the thread is running.
	 */
	virtual bool IsRunning() = 0;

	/**
	 * Waits for the thread to exit.
	 * @return True if the thread has exited.
	 */
	virtual bool WaitForStop() = 0;

	/**
	 * Exit this thread.
	 */
	virtual bool Exit() = 0;

	/**
	 * Join this thread.
	 */
	virtual void Join() = 0;

	/**
	 * Check if this thread is the current active thread.
	 * @return True if it is the current active thread.
	 */
	virtual bool IsCurrent() = 0;

	/**
	 * Get the unique ID of this thread.
	 * @return A value unique to each thread.
	 */
	virtual uint GetId() = 0;

	/**
	 * Create a thread; proc will be called as first function inside the thread,
	 *  with optinal params.
	 * @param proc The procedure to call inside the thread.
	 * @param param The params to give with 'proc'.
	 * @return True if the thread was started correctly.
	 */
	static ThreadObject *New(OTTDThreadFunc proc, void *param);

	/**
	 * Convert the current thread to a new ThreadObject.
	 * @return A new ThreadObject with the current thread attached to it.
	 */
	static ThreadObject *AttachCurrent();

	/**
	 * Find the Id of the current running thread.
	 * @return The thread ID of the current active thread.
	 */
	static uint CurrentId();
};

/**
 * Cross-platform Thread Semaphore. Wait() waits for a Set() of someone else.
 */
class ThreadSemaphore {
public:
	static ThreadSemaphore *New();

	/**
	 * Virtual Destructor to avoid compiler warnings.
	 */
	virtual ~ThreadSemaphore() {};

	/**
	 * Signal all threads that are in Wait() to continue.
	 */
	virtual void Set() = 0;

	/**
	 * Wait until we are signaled by a call to Set().
	 */
	virtual void Wait() = 0;
};

#endif /* THREAD_H */
