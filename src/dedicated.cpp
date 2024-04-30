/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dedicated.cpp Forking support for dedicated servers. */

#include "stdafx.h"
#include "fileio_func.h"
#include "debug.h"

std::string _log_file; ///< File to reroute output of a forked OpenTTD to
AutoCloseFile _log_fd; ///< File to reroute output of a forked OpenTTD to

#if defined(UNIX)

#include <unistd.h>

#include "safeguards.h"

void DedicatedFork()
{
	/* Fork the program */
	pid_t pid = fork();
	switch (pid) {
		case -1:
			perror("Unable to fork");
			exit(1);

		case 0: { // We're the child
			/* Open the log-file to log all stuff too */
			_log_fd.reset(fopen(_log_file.c_str(), "a"));
			if (!_log_fd) {
				perror("Unable to open logfile");
				exit(1);
			}
			/* Redirect stdout and stderr to log-file */
			if (dup2(fileno(_log_fd.get()), fileno(stdout)) == -1) {
				perror("Rerouting stdout");
				exit(1);
			}
			if (dup2(fileno(_log_fd.get()), fileno(stderr)) == -1) {
				perror("Rerouting stderr");
				exit(1);
			}
			break;
		}

		default:
			/* We're the parent */
			Debug(net, 0, "Loading dedicated server...");
			Debug(net, 0, "  - Forked to background with pid {}", pid);
			exit(0);
	}
}
#endif
