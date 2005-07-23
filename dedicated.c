#include "stdafx.h"
#include "openttd.h"

#ifdef ENABLE_NETWORK

#include "variables.h"

#ifdef __OS2__
#	include <sys/types.h>
#	include <unistd.h>
#endif

#ifdef UNIX
#	include <sys/types.h>
#	include <unistd.h>
#endif

#ifdef __MORPHOS__
/* Voids the fork, option will be disabled for MorphOS build anyway, because
 * MorphOS doesn't support forking (could only implemented with lots of code
 * changes here). */
int fork(void) { return -1; }
int dup2(int oldd, int newd) { return -1; }
#endif

#ifdef UNIX
/* We want to fork our dedicated server */
void DedicatedFork(void)
{
	/* Fork the program */
	pid_t pid = fork();
	switch (pid) {
		case -1:
			perror("Unable to fork");
			exit(1);
		case 0:
			// We're the child

			/* Open the log-file to log all stuff too */
			_log_file_fd = fopen(_log_file, "a");
			if (!_log_file_fd) {
				perror("Unable to open logfile");
				exit(1);
			}
			/* Redirect stdout and stderr to log-file */
			if (dup2(fileno(_log_file_fd), fileno(stdout)) == -1) {
				perror("Rerouting stdout");
				exit(1);
			}
			if (dup2(fileno(_log_file_fd), fileno(stderr)) == -1) {
				perror("Rerouting stderr");
				exit(1);
			}
			break;
		default:
			// We're the parent
			printf("Loading dedicated server...\n");
			printf("  - Forked to background with pid %d\n", pid);
			exit(0);
	}
}
#endif

#else

void DedicatedFork(void) {}

#endif /* ENABLE_NETWORK */
