/* $Id$ */

/** @file dedicated.cpp */

#include "stdafx.h"

#ifdef ENABLE_NETWORK

#if defined(UNIX) && !defined(__MORPHOS__)

#include "openttd.h"
#include "variables.h"

#include <sys/types.h>
#include <unistd.h>

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
			printf("  - Forked to background with pid %d\n", pid);
			exit(0);
	}
}
#endif

#else

void DedicatedFork(void) {}

#endif /* ENABLE_NETWORK */
