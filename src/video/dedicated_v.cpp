/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dedicated_v.cpp Dedicated server video 'driver'. */

#include "../stdafx.h"

#include "../gfx_func.h"
#include "../error_func.h"
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
#include "../thread.h"
#include "../window_func.h"
#include <iostream>
#include "dedicated_v.h"

#if defined(UNIX)
#	include <sys/time.h> /* gettimeofday */
#	include <sys/types.h>
#	include <unistd.h>
#	include <signal.h>
#	define STDIN 0  /* file descriptor for standard input */

/* Signal handlers */
static void DedicatedSignalHandler(int sig)
{
	if (_game_mode == GM_NORMAL && _settings_client.gui.autosave_on_exit) DoExitSave();
	_exit_game = true;
	signal(sig, DedicatedSignalHandler);
}
#endif

#if defined(_WIN32)
# include <windows.h> /* GetTickCount */
# include <conio.h>
# include <time.h>
# include <tchar.h>
# include "../os/windows/win32.h"

static HANDLE _hInputReady, _hWaitForInputHandling;
static HANDLE _hThread; // Thread to close
static std::string _win_console_thread_buffer;

/* Windows Console thread. Just loop and signal when input has been received */
static void WINAPI CheckForConsoleInput()
{
	SetCurrentThreadName("ottd:win-console");

	for (;;) {
		std::getline(std::cin, _win_console_thread_buffer);

		/* Signal input waiting that input is read and wait for it being handled. */
		SignalObjectAndWait(_hInputReady, _hWaitForInputHandling, INFINITE, FALSE);
	}
}

static void CreateWindowsConsoleThread()
{
	DWORD dwThreadId;
	/* Create event to signal when console input is ready */
	_hInputReady = CreateEvent(nullptr, false, false, nullptr);
	_hWaitForInputHandling = CreateEvent(nullptr, false, false, nullptr);
	if (_hInputReady == nullptr || _hWaitForInputHandling == nullptr) UserError("Cannot create console event!");

	_hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)CheckForConsoleInput, nullptr, 0, &dwThreadId);
	if (_hThread == nullptr) UserError("Cannot create console thread!");

	Debug(driver, 2, "Windows console thread started");
}

static void CloseWindowsConsoleThread()
{
	CloseHandle(_hThread);
	CloseHandle(_hInputReady);
	CloseHandle(_hWaitForInputHandling);
	Debug(driver, 2, "Windows console thread shut down");
}

#endif

#include "../safeguards.h"


static void *_dedicated_video_mem;

/* Whether a fork has been done. */
bool _dedicated_forks;

extern bool SafeLoad(const std::string &filename, SaveLoadOperation fop, DetailedFileType dft, GameMode newgm, Subdirectory subdir, struct LoadFilter *lf = nullptr);

static FVideoDriver_Dedicated iFVideoDriver_Dedicated;


const char *VideoDriver_Dedicated::Start(const StringList &)
{
	this->UpdateAutoResolution();

	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	_dedicated_video_mem = (bpp == 0) ? nullptr : MallocT<byte>(static_cast<size_t>(_cur_resolution.width) * _cur_resolution.height * (bpp / 8));

	_screen.width  = _screen.pitch = _cur_resolution.width;
	_screen.height = _cur_resolution.height;
	_screen.dst_ptr = _dedicated_video_mem;
	ScreenSizeChanged();
	BlitterFactory::GetCurrentBlitter()->PostResize();

#if defined(_WIN32)
	/* For win32 we need to allocate a console (debug mode does the same) */
	CreateConsole();
	CreateWindowsConsoleThread();
	SetConsoleTitle(L"OpenTTD Dedicated Server");
#endif

#ifdef _MSC_VER
	/* Disable the MSVC assertion message box. */
	_set_error_mode(_OUT_TO_STDERR);
#endif

	Debug(driver, 1, "Loading dedicated server");
	return nullptr;
}

void VideoDriver_Dedicated::Stop()
{
#ifdef _WIN32
	CloseWindowsConsoleThread();
#endif
	free(_dedicated_video_mem);
}

void VideoDriver_Dedicated::MakeDirty(int, int, int, int) {}
bool VideoDriver_Dedicated::ChangeResolution(int, int) { return false; }
bool VideoDriver_Dedicated::ToggleFullscreen(bool) { return false; }

#if defined(UNIX)
static bool InputWaiting()
{
	struct timeval tv;
	fd_set readfds;

	tv.tv_sec = 0;
	tv.tv_usec = 1;

	FD_ZERO(&readfds);
	FD_SET(STDIN, &readfds);

	/* don't care about writefds and exceptfds: */
	return select(STDIN + 1, &readfds, nullptr, nullptr, &tv) > 0;
}

#else

static bool InputWaiting()
{
	return WaitForSingleObject(_hInputReady, 1) == WAIT_OBJECT_0;
}

#endif

static void DedicatedHandleKeyInput()
{
	if (!InputWaiting()) return;

	if (_exit_game) return;

	std::string input_line;
#if defined(UNIX)
	if (!std::getline(std::cin, input_line)) return;
#else
	/* Handle console input, and signal console thread, it can accept input again */
	std::swap(input_line, _win_console_thread_buffer);
	SetEvent(_hWaitForInputHandling);
#endif

	/* Remove any trailing \r or \n, and ensure the string is valid. */
	auto p = input_line.find_last_not_of("\r\n");
	if (p != std::string::npos) p++;
	IConsoleCmdExec(StrMakeValid(input_line.substr(0, p)));
}

void VideoDriver_Dedicated::MainLoop()
{
	/* Signal handlers */
#if defined(UNIX)
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
	}

	this->is_game_threaded = false;

	/* Done loading, start game! */

	while (!_exit_game) {
		if (!_dedicated_forks) DedicatedHandleKeyInput();
		this->DrainCommandQueue();

		ChangeGameSpeed(_ddc_fastforward);
		this->Tick();
		this->SleepTillNextTick();
	}
}
