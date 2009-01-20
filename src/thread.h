/* $Id$ */

/** @file thread.h Base of all threads. */

#ifndef THREAD_H
#define THREAD_H

typedef void (*OTTDThreadFunc)(void *);

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
	 *  with optinal params.
	 * @param proc The procedure to call inside the thread.
	 * @param param The params to give with 'proc'.
	 * @param thread Place to store a pointer to the thread in. May be NULL.
	 * @return True if the thread was started correctly.
	 */
	static bool New(OTTDThreadFunc proc, void *param, ThreadObject **thread = NULL);
};

/**
 * Cross-platform Mutex
 */
class ThreadMutex {
public:
	static ThreadMutex *New();

	/**
	 * Virtual Destructor to avoid compiler warnings.
	 */
	virtual ~ThreadMutex() {};

	/**
	 * Begin the critical section
	 */
	virtual void BeginCritical() = 0;

	/**
	 * End of the critical section
	 */
	virtual void EndCritical() = 0;
};

#endif /* THREAD_H */
