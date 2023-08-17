/** @file init.h Declaration of initializing link graph handler. */

#ifndef INIT_H
#define INIT_H

#include "linkgraphjob_base.h"

/**
 * Stateless, thread safe initialization handler. Initializes node and edge
 * annotations.
 */
class InitHandler : public ComponentHandler {
public:

	/**
	 * Initialize the link graph job.
	 * @param job Job to be initialized.
	 */
	virtual void Run(LinkGraphJob &job) const { job.Init(); }
};

#endif /* INIT_H */
