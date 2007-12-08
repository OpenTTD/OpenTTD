/* $Id$ */

#include "../stdafx.h"

#ifdef ENABLE_NETWORK

#include "../openttd.h"
#include "../debug.h"
#include "../functions.h"
#include "../gfx.h"
#include "../network/network.h"
#include "../window.h"
#include "../console.h"
#include "../variables.h"
#include "../genworld.h"
#include "../fileio.h"
#include "../blitter/factory.hpp"
#include "dedicated_v.h"

#ifdef BEOS_NET_SERVER
#include <net/socket.h>
#endif

#ifdef __OS2__
#	include <sys/time.h> /* gettimeofday */
#	include <sys/types.h>
#	include <unistd.h>
#	include <conio.h>

#	define INCL_DOS
#	include <os2.h>

#	define STDIN 0  /* file descriptor for standard input */

/**
 * Switches OpenTTD to a console app at run-time, instead of a PM app
 * Necessary to see stdout, etc. */
static void OS2_SwitchToConsoleMode()
{
	PPIB pib;
	PTIB tib;

	DosGetInfoBlocks(&tib, &pib);

	// Change flag from PM to VIO
	pib->pib_ultype = 3;
}
#endif

#if defined(UNIX) || defined(PSP)
#	include <sys/time.h> /* gettimeofday */
#	include <sys/types.h>
#	include <unistd.h>
#	include <signal.h>
#	define STDIN 0  /* file descriptor for standard input */
#	if defined(PSP)
#		include <sys/fd_set.h>
#		include <sys/select.h>
#	endif /* PSP */

/* Signal handlers */
static void DedicatedSignalHandler(int sig)
{
	_exit_game = true;
	signal(sig, DedicatedSignalHandler);
}
#endif

#if defined(WIN32)
# include <windows.h> /* GetTickCount */
# if !defined(WINCE)
#  include <conio.h>
# endif
# include <time.h>
# include <tchar.h>
static HANDLE _hInputReady, _hWaitForInputHandling;
static HANDLE _hThread; // Thread to close
static char _win_console_thread_buffer[200];

/* Windows Console thread. Just loop and signal when input has been received */
static void WINAPI CheckForConsoleInput()
{
#if defined(WINCE)
	/* WinCE doesn't support console stuff */
	return;
#else
	DWORD nb;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	while (true) {
		ReadFile(hStdin, _win_console_thread_buffer, lengthof(_win_console_thread_buffer), &nb, NULL);
		/* Signal input waiting that input is read and wait for it being handled
		 * SignalObjectAndWait() should be used here, but it's unsupported in Win98< */
		SetEvent(_hInputReady);
		WaitForSingleObject(_hWaitForInputHandling, INFINITE);
	}
#endif
}

static void CreateWindowsConsoleThread()
{
	DWORD dwThreadId;
	/* Create event to signal when console input is ready */
	_hInputReady = CreateEvent(NULL, false, false, NULL);
	_hWaitForInputHandling = CreateEvent(NULL, false, false, NULL);
	if (_hInputReady == NULL || _hWaitForInputHandling == NULL) error("Cannot create console event!");

	_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckForConsoleInput, NULL, 0, &dwThreadId);
	if (_hThread == NULL) error("Cannot create console thread!");

	DEBUG(driver, 2, "Windows console thread started");
}

static void CloseWindowsConsoleThread()
{
	CloseHandle(_hThread);
	CloseHandle(_hInputReady);
	CloseHandle(_hWaitForInputHandling);
	DEBUG(driver, 2, "Windows console thread shut down");
}

#endif


static void *_dedicated_video_mem;

extern bool SafeSaveOrLoad(const char *filename, int mode, int newgm, Subdirectory subdir);
extern void SwitchMode(int new_mode);

static FVideoDriver_Dedicated iFVideoDriver_Dedicated;


const char *VideoDriver_Dedicated::Start(const char * const *parm)
{
	int bpp = BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth();
	if (bpp == 0) _dedicated_video_mem = NULL;
	else          _dedicated_video_mem = MallocT<byte>(_cur_resolution[0] * _cur_resolution[1] * (bpp / 8));

	_screen.width = _screen.pitch = _cur_resolution[0];
	_screen.height = _cur_resolution[1];

	SetDebugString("net=6");

#if defined(WINCE)
	/* WinCE doesn't support console stuff */
#elif defined(WIN32)
	// For win32 we need to allocate a console (debug mode does the same)
	CreateConsole();
	CreateWindowsConsoleThread();
	SetConsoleTitle(_T("OpenTTD Dedicated Server"));
#endif

#ifdef __OS2__
	// For OS/2 we also need to switch to console mode instead of PM mode
	OS2_SwitchToConsoleMode();
#endif

	DEBUG(driver, 1, "Loading dedicated server");
	return NULL;
}

void VideoDriver_Dedicated::Stop()
{
#ifdef WIN32
	CloseWindowsConsoleThread();
#endif
	free(_dedicated_video_mem);
}

void VideoDriver_Dedicated::MakeDirty(int left, int top, int width, int height) {}
bool VideoDriver_Dedicated::ChangeResolution(int w, int h) { return false; }
void VideoDriver_Dedicated::ToggleFullscreen(bool fs) {}

#if defined(UNIX) || defined(__OS2__) || defined(PSP)
static bool InputWaiting()
{
	struct timeval tv;
	fd_set readfds;

	tv.tv_sec = 0;
	tv.tv_usec = 1;

	FD_ZERO(&readfds);
	FD_SET(STDIN, &readfds);

	/* don't care about writefds and exceptfds: */
	return select(STDIN + 1, &readfds, NULL, NULL, &tv) > 0;
}

static uint32 GetTime()
{
	struct timeval tim;

	gettimeofday(&tim, NULL);
	return tim.tv_usec / 1000 + tim.tv_sec * 1000;
}

#else

static bool InputWaiting()
{
	return WaitForSingleObject(_hInputReady, 1) == WAIT_OBJECT_0;
}

static uint32 GetTime()
{
	return GetTickCount();
}

#endif

static void DedicatedHandleKeyInput()
{
	static char input_line[200] = "";

	if (!InputWaiting()) return;

	if (_exit_game) return;

#if defined(UNIX) || defined(__OS2__) || defined(PSP)
	if (fgets(input_line, lengthof(input_line), stdin) == NULL) return;
#else
	/* Handle console input, and singal console thread, it can accept input again */
	strncpy(input_line, _win_console_thread_buffer, lengthof(input_line));
	SetEvent(_hWaitForInputHandling);
#endif

	/* XXX - strtok() does not 'forget' \n\r if it is the first character! */
	strtok(input_line, "\r\n"); // Forget about the final \n (or \r)
	{ /* Remove any special control characters */
		uint i;
		for (i = 0; i < lengthof(input_line); i++) {
			if (input_line[i] == '\n' || input_line[i] == '\r') // cut missed beginning '\0'
				input_line[i] = '\0';

			if (input_line[i] == '\0')
				break;

			if (!IsInsideMM(input_line[i], ' ', 256))
				input_line[i] = ' ';
		}
	}

	IConsoleCmdExec(input_line); // execute command
}

void VideoDriver_Dedicated::MainLoop()
{
	uint32 cur_ticks = GetTime();
	uint32 next_tick = cur_ticks + 30;

	/* Signal handlers */
#if defined(UNIX) || defined(PSP)
	signal(SIGTERM, DedicatedSignalHandler);
	signal(SIGINT, DedicatedSignalHandler);
	signal(SIGQUIT, DedicatedSignalHandler);
#endif

	// Load the dedicated server stuff
	_is_network_server = true;
	_network_dedicated = true;
	_network_playas = PLAYER_SPECTATOR;
	_local_player = PLAYER_SPECTATOR;

	/* If SwitchMode is SM_LOAD, it means that the user used the '-g' options */
	if (_switch_mode != SM_LOAD) {
		StartNewGameWithoutGUI(GENERATE_NEW_SEED);
		SwitchMode(_switch_mode);
		_switch_mode = SM_NONE;
	} else {
		_switch_mode = SM_NONE;
		/* First we need to test if the savegame can be loaded, else we will end up playing the
		 *  intro game... */
		if (!SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_NORMAL, BASE_DIR)) {
			/* Loading failed, pop out.. */
			DEBUG(net, 0, "Loading requested map failed, aborting");
			_networking = false;
		} else {
			/* We can load this game, so go ahead */
			SwitchMode(SM_LOAD);
		}
	}

	// Done loading, start game!

	if (!_networking) {
		DEBUG(net, 0, "Dedicated server could not be started, aborting");
		return;
	}

	while (!_exit_game) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping
		InteractiveRandom(); // randomness

		if (!_dedicated_forks)
			DedicatedHandleKeyInput();

		cur_ticks = GetTime();
		if (cur_ticks >= next_tick || cur_ticks < prev_cur_ticks) {
			next_tick = cur_ticks + 30;

			GameLoop();
			_screen.dst_ptr = _dedicated_video_mem;
			UpdateWindows();
		}
		CSleep(1);
	}
}

#endif /* ENABLE_NETWORK */
