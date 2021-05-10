/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dedicated.cpp Forking support for dedicated servers. */

#include "stdafx.h"
#include "fileio_func.h"
#include <string>

std::string _log_file; ///< File to reroute output of a forked OpenTTD to
std::unique_ptr<FILE, FileDeleter> _log_fd; ///< File to reroute output of a forked OpenTTD to

#if defined(UNIX)

#include <unistd.h>

#include "safeguards.h"

#if defined(SUNOS) && !defined(_LP64) && !defined(_I32LPx)
/* Solaris has, in certain situation, pid_t defined as long, while in other
 *  cases it has it defined as int... this handles all cases nicely.
 */
# define PRINTF_PID_T "%ld"
#else
# define PRINTF_PID_T "%d"
#endif

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
			printf("Loading dedicated server...\n");
			printf("  - Forked to background with pid " PRINTF_PID_T "\n", pid);
			exit(0);
	}
}
#endif
