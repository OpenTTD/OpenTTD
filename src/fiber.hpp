/* $Id$ */

/** @file fiber.hpp */

#ifndef FIBER_HPP
#define FIBER_HPP

typedef void (CDECL *FiberFunc)(void *);

class Fiber {
public:
	/**
	 * Switch to this fiber.
	 */
	virtual void SwitchToFiber() = 0;

	/**
	 * Exit a fiber.
	 */
	virtual void Exit() = 0;

	/**
	 * Check if a fiber is running.
	 */
	virtual bool IsRunning() = 0;

	/**
	 * Get the 'param' data of the Fiber.
	 */
	virtual void *GetFiberData() = 0;

	/**
	 * Virtual Destructor to mute warnings.
	 */
	virtual ~Fiber() {};

	/**
	 * Create a new fiber, calling proc(param) when running.
	 */
	static Fiber *New(FiberFunc proc, void *param);

	/**
	 * Attach a current thread to a new fiber.
	 */
	static Fiber *AttachCurrent(void *param);

	/**
	 * Get the 'param' data of the current active Fiber.
	 */
	static void *GetCurrentFiberData();
};

#endif /* FIBER_HPP */
