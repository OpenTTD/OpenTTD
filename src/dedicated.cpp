/* $Id$ */

/** @file dedicated.cpp */

#include "stdafx.h"

#ifdef ENABLE_NETWORK

#if defined(UNIX) && !defined(__MORPHOS__)

#include "openttd.h"
#include "variables.h"

#include <sys/types.h>
#include <unistd.h>

#if defined(SUNOS) && !defined(_LP64) && !defined(_I32LPx)
/* Solaris has, in certain situation, pid_t defined as long, while in other
 *  cases it has it defined as int... this handles all cases nicely. */
# define PRINTF_PID_T "%ld"
#else
# define PRINTF_PID_T "%d"
#endif

void DedicatedFork(void)
{
	/* Fork the program */
	pid_t pid = fork();
	switch (pid) {
		case -1:
			perror("Unable to fork");
			exit(1);

		case 0: { // We're the child
			FILE* f;

			/* Open the log-file to log all stuff too */
			f = fopen(_log_file, "a");
			if (f == NULL) {
				perror("Unable to open logfile");
				exit(1);
			}
			/* Redirect stdout and stderr to log-file */
			if (dup2(fileno(f), fileno(stdout)) == -1) {
				perror("Rerouting stdout");
				exit(1);
			}
			if (dup2(fileno(f), fileno(stderr)) == -1) {
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

#else

void DedicatedFork(void) {}

#endif /* ENABLE_NETWORK */
