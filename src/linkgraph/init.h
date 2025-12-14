/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

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
	void Run(LinkGraphJob &job) const override { job.Init(); }
};

#endif /* INIT_H */
