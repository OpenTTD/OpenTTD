/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dedicated_v.cpp Dedicated server video 'driver'. */

#include "../stdafx.h"

#ifdef ENABLE_NETWORK

#include "../gfx_func.h"
#include "../network/network.h"
#include "../network/network_internal.h"
#include "../console_func.h"
#include "../genworld.h"
#include "../fileio_type.h"
#include "../fios.h"
#include "../blitter/factory.hpp"
#include "../company_func.h"
#include "../core/random_func.hpp"
#include "../saveload/saveload.h"
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
 * Necessary to see stdout, etc.
 */
static void OS2_SwitchToConsoleMode()
{
	PPIB pib;
	PTIB tib;

	DosGetInfoBlocks(&tib, &pib);

	/* Change flag from PM to VIO */
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
	if (_game_mode == GM_NORMAL && _settings_client.gui.autosave_on_exit) DoExitSave();
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
	for (;;) {
		ReadFile(hStdin, _win_console_thread_buffer, lengthof(_win_console_thread_buffer), &nb, NULL);
		if (nb >= lengthof(_win_console_thread_buffer)) nb = lengthof(_win_console_thread_buffer) - 1;
		_win_console_thread_buffer[nb] = '\0';

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
	if (_hInputReady == NULL || _hWaitForInputHandling == NULL) usererror("Cannot create console event!");

	_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckForConsoleInput, NULL, 0, &dwThreadId);
	if (_hThread == NULL) usererror("Cannot create console thread!");

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

#include "../safeguards.h"


static void *_dedicated_video_mem;

/* Whether a fork has been done. */
bool _dedicated_forks;

extern bool SafeLoad(const char *filename, SaveLoadOperation fop, DetailedFileType dft, GameMode newgm, Subdirectory subdir, struct LoadFilter *lf = NULL);

static FVideoDriver_Dedicated iFVideoDriver_Dedicated;


const char *VideoDriver_Dedicated::Start(const char * const *parm)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	_dedicated_video_mem = (bpp == 0) ? NULL : MallocT<byte>(_cur_resolution.width * _cur_resolution.height * (bpp / 8));

	_screen.width  = _screen.pitch = _cur_resolution.width;
	_screen.height = _cur_resolution.height;
	_screen.dst_ptr = _dedicated_video_mem;
	ScreenSizeChanged();
	BlitterFactory::GetCurrentBlitter()->PostResize();

#if defined(WINCE)
	/* WinCE doesn't support console stuff */
#elif defined(WIN32)
	/* For win32 we need to allocate a console (debug mode does the same) */
	CreateConsole();
	CreateWindowsConsoleThread();
	SetConsoleTitle(_T("OpenTTD Dedicated Server"));
#endif

#ifdef _MSC_VER
	/* Disable the MSVC assertion message box. */
	_set_error_mode(_OUT_TO_STDERR);
#endif

#ifdef __OS2__
	/* For OS/2 we also need to switch to console mode instead of PM mode */
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
bool VideoDriver_Dedicated::ToggleFullscreen(bool fs) { return false; }

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
	static char input_line[1024] = "";

	if (!InputWaiting()) return;

	if (_exit_game) return;

#if defined(UNIX) || defined(__OS2__) || defined(PSP)
	if (fgets(input_line, lengthof(input_line), stdin) == NULL) return;
#else
	/* Handle console input, and signal console thread, it can accept input again */
	assert_compile(lengthof(_win_console_thread_buffer) <= lengthof(input_line));
	strecpy(input_line, _win_console_thread_buffer, lastof(input_line));
	SetEvent(_hWaitForInputHandling);
#endif

	/* Remove trailing \r or \n */
	for (char *c = input_line; *c != '\0'; c++) {
		if (*c == '\n' || *c == '\r' || c == lastof(input_line)) {
			*c = '\0';
			break;
		}
	}
	str_validate(input_line, lastof(input_line));

	IConsoleCmdExec(input_line); // execute command
}

void VideoDriver_Dedicated::MainLoop()
{
	uint32 cur_ticks = GetTime();
	uint32 next_tick = cur_ticks + MILLISECONDS_PER_TICK;

	/* Signal handlers */
#if defined(UNIX) || defined(PSP)
	signal(SIGTERM, DedicatedSignalHandler);
	signal(SIGINT, DedicatedSignalHandler);
	signal(SIGQUIT, DedicatedSignalHandler);
#endif

	/* Load the dedicated server stuff */
	_is_network_server = true;
	_network_dedicated = true;
	_current_company = _local_company = COMPANY_SPECTATOR;

	/* If SwitchMode is SM_LOAD_GAME, it means that the user used the '-g' options */
	if (_switch_mode != SM_LOAD_GAME) {
		StartNewGameWithoutGUI(GENERATE_NEW_SEED);
		SwitchToMode(_switch_mode);
		_switch_mode = SM_NONE;
	} else {
		_switch_mode = SM_NONE;
		/* First we need to test if the savegame can be loaded, else we will end up playing the
		 *  intro game... */
		if (!SafeLoad(_file_to_saveload.name, _file_to_saveload.file_op, _file_to_saveload.detail_ftype, GM_NORMAL, BASE_DIR)) {
			/* Loading failed, pop out.. */
			DEBUG(net, 0, "Loading requested map failed, aborting");
			_networking = false;
		} else {
			/* We can load this game, so go ahead */
			SwitchToMode(SM_LOAD_GAME);
		}
	}

	/* Done loading, start game! */

	if (!_networking) {
		DEBUG(net, 0, "Dedicated server could not be started, aborting");
		return;
	}

	while (!_exit_game) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping
		InteractiveRandom(); // randomness

		if (!_dedicated_forks) DedicatedHandleKeyInput();

		cur_ticks = GetTime();
		_realtime_tick += cur_ticks - prev_cur_ticks;
		if (cur_ticks >= next_tick || cur_ticks < prev_cur_ticks || _ddc_fastforward) {
			next_tick = cur_ticks + MILLISECONDS_PER_TICK;

			GameLoop();
			UpdateWindows();
		}

		/* Don't sleep when fast forwarding (for desync debugging) */
		if (!_ddc_fastforward) {
			/* Sleep longer on a dedicated server, if the game is paused and no clients connected.
			 * That can allow the CPU to better use deep sleep states. */
			if (_pause_mode != 0 && !HasClients()) {
				CSleep(100);
			} else {
				CSleep(1);
			}
		}
	}
}

#endif /* ENABLE_NETWORK */
