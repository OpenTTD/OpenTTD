#include "stdafx.h"
#include "ttd.h"
#include "network.h"
#include "hal.h"

#ifdef ENABLE_NETWORK

#include "gfx.h"
#include "window.h"
#include "command.h"
#include "console.h"
#ifdef WIN32
#	include <windows.h> /* GetTickCount */
#	include <conio.h>
#endif
#ifdef UNIX
#	include <sys/time.h> /* gettimeofday */
#	include <sys/types.h>
#	include <unistd.h>
#	define STDIN 0  /* file descriptor for standard input */
#endif

// This file handles all dedicated-server in- and outputs

static void *_dedicated_video_mem;

static const char *DedicatedVideoStart(char **parm) {
	_screen.width = _screen.pitch = _cur_resolution[0];
	_screen.height = _cur_resolution[1];
	_dedicated_video_mem = malloc(_cur_resolution[0]*_cur_resolution[1]);

	_debug_net_level = 6;
	_debug_misc_level = 0;

#ifdef WIN32
	// For win32 we need to allocate an console (debug mode does the same)
	CreateConsole();
	SetConsoleTitle("OpenTTD Dedicated Server");
#endif

	DEBUG(misc,0)("Loading dedicated server...");
	return NULL;
}
static void DedicatedVideoStop() { free(_dedicated_video_mem); }
static void DedicatedVideoMakeDirty(int left, int top, int width, int height) {}
static bool DedicatedVideoChangeRes(int w, int h) { return false; }

#ifdef UNIX

bool InputWaiting()
{
	struct timeval tv;
	fd_set readfds;

	tv.tv_sec = 0;
	tv.tv_usec = 1;

	FD_ZERO(&readfds);
	FD_SET(STDIN, &readfds);

	/* don't care about writefds and exceptfds: */
	select(STDIN+1, &readfds, NULL, NULL, &tv);

	if (FD_ISSET(STDIN, &readfds))
		return true;
	else
		return false;
}
#else
bool InputWaiting()
{
	return kbhit();
}
#endif

static int DedicatedVideoMainLoop() {
#ifndef WIN32
	struct timeval tim;
#else
	char input;
#endif
	uint32 next_tick;
	uint32 cur_ticks;
	char input_line[200];

#ifdef WIN32
	next_tick = GetTickCount() + 30;
#else
	gettimeofday(&tim, NULL);
	next_tick = (tim.tv_usec / 1000) + 30 + (tim.tv_sec * 1000);
#endif

	// Load the dedicated server stuff
	_is_network_server = true;
	_network_dedicated = true;
	_switch_mode = SM_NONE;
	_network_playas = OWNER_SPECTATOR;
	_local_player = OWNER_SPECTATOR;
	DoCommandP(0, Random(), InteractiveRandom(), NULL, CMD_GEN_RANDOM_NEW_GAME);
	// Done loading, start game!

	if (!_networking) {
		DEBUG(net, 1)("Dedicated server could not be launced. Aborting..");
		return ML_QUIT;
	}

	while (true) {
		InteractiveRandom(); // randomness

#ifdef UNIX
		if (InputWaiting()) {
			fgets(input_line, 200, stdin);
			// Forget about the final \n (or \r)
			strtok(input_line, "\r\n");
			IConsoleCmdExec(input_line);
		}
#else
		if (InputWaiting()) {
			input = getch();
			printf("%c", input);
			if (input != '\r')
				snprintf(input_line, 200, "%s%c", input_line, input);
			else {
				printf("\n");
				IConsoleCmdExec(input_line);
				sprintf(input_line, "");
			}
		}
#endif

		if (_exit_game) return ML_QUIT;

#ifdef WIN32
		cur_ticks = GetTickCount();
#else
		gettimeofday(&tim, NULL);
		cur_ticks = (tim.tv_usec / 1000) + (tim.tv_sec * 1000);
#endif

		if (cur_ticks >= next_tick) {
			next_tick += 30;

			GameLoop();
			_screen.dst_ptr = _dedicated_video_mem;
			UpdateWindows();
		}
		CSleep(1);
	}

	return ML_QUIT;
}


const HalVideoDriver _dedicated_video_driver = {
	DedicatedVideoStart,
	DedicatedVideoStop,
	DedicatedVideoMakeDirty,
	DedicatedVideoMainLoop,
	DedicatedVideoChangeRes,
};

#else

static void *_dedicated_video_mem;

static const char *DedicatedVideoStart(char **parm) {
	DEBUG(misc,0)("OpenTTD compiled without network-support, quiting...");

	return NULL;
}

static void DedicatedVideoStop() { free(_dedicated_video_mem); }
static void DedicatedVideoMakeDirty(int left, int top, int width, int height) {}
static bool DedicatedVideoChangeRes(int w, int h) { return false; }
static int DedicatedVideoMainLoop() { return ML_QUIT; }

const HalVideoDriver _dedicated_video_driver = {
	DedicatedVideoStart,
	DedicatedVideoStop,
	DedicatedVideoMakeDirty,
	DedicatedVideoMainLoop,
	DedicatedVideoChangeRes,
};

#endif /* ENABLE_NETWORK */
