/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file crashlog_osx.cpp OS X crash log handler */

#include "../../stdafx.h"
#include "../../crashlog.h"
#include "../../fileio_func.h"
#include "../../string_func.h"
#include "../../gamelog.h"
#include "../../saveload/saveload.h"
#include "../../video/video_driver.hpp"
#include "macos.h"

#include <setjmp.h>
#include <signal.h>
#include <mach-o/arch.h>
#include <dlfcn.h>
#include <cxxabi.h>

#ifdef WITH_UNOFFICIAL_BREAKPAD
#	include <client/mac/handler/exception_handler.h>
#endif

#include "../../safeguards.h"


/* Macro testing a stack address for valid alignment. */
#if defined(__i386__)
#define IS_ALIGNED(addr) (((uintptr_t)(addr) & 0xf) == 8)
#else
#define IS_ALIGNED(addr) (((uintptr_t)(addr) & 0xf) == 0)
#endif

#define MAX_STACK_FRAMES 64

/** The signals we want our crash handler to handle. */
static constexpr int _signals_to_handle[] = { SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL, SIGSYS, SIGQUIT };

/**
 * OSX implementation for the crash logger.
 */
class CrashLogOSX : public CrashLog {
	/** Signal that has been thrown. */
	int signum;

	void LogOSVersion(std::back_insert_iterator<std::string> &output_iterator) const override
	{
		int ver_maj, ver_min, ver_bug;
		GetMacOSVersion(&ver_maj, &ver_min, &ver_bug);

		const NXArchInfo *arch = NXGetLocalArchInfo();

		fmt::format_to(output_iterator,
				"Operating system:\n"
				" Name:     Mac OS X\n"
				" Release:  {}.{}.{}\n"
				" Machine:  {}\n"
				" Min Ver:  {}\n"
				" Max Ver:  {}\n",
				ver_maj, ver_min, ver_bug,
				arch != nullptr ? arch->description : "unknown",
				MAC_OS_X_VERSION_MIN_REQUIRED,
				MAC_OS_X_VERSION_MAX_ALLOWED
		);
	}

	void LogError(std::back_insert_iterator<std::string> &output_iterator, const std::string_view message) const override
	{
		fmt::format_to(output_iterator,
				"Crash reason:\n"
				" Signal:  {} ({})\n"
				" Message: {}\n\n",
				strsignal(this->signum),
				this->signum,
				message
		);
	}

	void LogStacktrace(std::back_insert_iterator<std::string> &output_iterator) const override
	{
		/* As backtrace() is only implemented in 10.5 or later,
		 * we're rolling our own here. Mostly based on
		 * http://stackoverflow.com/questions/289820/getting-the-current-stack-trace-on-mac-os-x
		 * and some details looked up in the Darwin sources. */
		fmt::format_to(output_iterator, "\nStacktrace:\n");

		void **frame;
#if defined(__ppc__) || defined(__ppc64__)
		/* Apple says __builtin_frame_address can be broken on PPC. */
		__asm__ volatile("mr %0, r1" : "=r" (frame));
#else
		frame = (void **)__builtin_frame_address(0);
#endif

		for (int i = 0; frame != nullptr && i < MAX_STACK_FRAMES; i++) {
			/* Get IP for current stack frame. */
#if defined(__ppc__) || defined(__ppc64__)
			void *ip = frame[2];
#else
			void *ip = frame[1];
#endif
			if (ip == nullptr) break;

			/* Print running index. */
			fmt::format_to(output_iterator, " [{:02}]", i);

			Dl_info dli;
			bool dl_valid = dladdr(ip, &dli) != 0;

			const char *fname = "???";
			if (dl_valid && dli.dli_fname) {
				/* Valid image name? Extract filename from the complete path. */
				const char *s = strrchr(dli.dli_fname, '/');
				if (s != nullptr) {
					fname = s + 1;
				} else {
					fname = dli.dli_fname;
				}
			}
			/* Print image name and IP. */
			fmt::format_to(output_iterator, " {:20s} {}", fname, (uintptr_t)ip);

			/* Print function offset if information is available. */
			if (dl_valid && dli.dli_sname != nullptr && dli.dli_saddr != nullptr) {
				/* Try to demangle a possible C++ symbol. */
				int status = -1;
				char *func_name = abi::__cxa_demangle(dli.dli_sname, nullptr, 0, &status);

				long int offset = (intptr_t)ip - (intptr_t)dli.dli_saddr;
				fmt::format_to(output_iterator, " ({} + {})", func_name != nullptr ? func_name : dli.dli_sname, offset);

				free(func_name);
			}
			fmt::format_to(output_iterator, "\n");

			/* Get address of next stack frame. */
			void **next = (void **)frame[0];
			/* Frame address not increasing or not aligned? Broken stack, exit! */
			if (next <= frame || !IS_ALIGNED(next)) break;
			frame = next;
		}

		fmt::format_to(output_iterator, "\n");
	}

#ifdef WITH_UNOFFICIAL_BREAKPAD
	static bool MinidumpCallback(const char* dump_dir, const char* minidump_id, void* context, bool succeeded)
	{
		CrashLogOSX *crashlog = reinterpret_cast<CrashLogOSX *>(context);

		crashlog->crashdump_filename = crashlog->CreateFileName(".dmp");
		std::rename(fmt::format("{}/{}.dmp", dump_dir, minidump_id).c_str(), crashlog->crashdump_filename.c_str());
		return succeeded;
	}

	bool WriteCrashDump() override
	{
		return google_breakpad::ExceptionHandler::WriteMinidump(_personal_dir, MinidumpCallback, this);
	}
#endif

	/* virtual */ bool TryExecute(std::string_view section_name, std::function<bool()> &&func) override
	{
		this->try_execute_active = true;

		/* Setup a longjump in case a crash happens. */
		if (setjmp(this->internal_fault_jmp_buf) != 0) {
			fmt::print("Something went wrong when attempting to fill {} section of the crash log.\n", section_name);

			/* Reset the signals and continue on. The handler is responsible for dealing with the crash. */
			sigset_t sigs;
			sigemptyset(&sigs);
			for (int signum : _signals_to_handle) {
				sigaddset(&sigs, signum);
			}
			sigprocmask(SIG_UNBLOCK, &sigs, nullptr);

			this->try_execute_active = false;
			return false;
		}

		bool res = func();
		this->try_execute_active = false;
		return res;
	}

public:
	/**
	 * A crash log is always generated by signal.
	 * @param signum the signal that was caused by the crash.
	 */
	CrashLogOSX(int signum) : signum(signum) {}

	/** Show a dialog with the crash information. */
	void DisplayCrashDialog() const
	{
		static const char crash_title[] =
			"A serious fault condition occurred in the game. The game will shut down.";

		std::string message = fmt::format(
				 "Please send crash.log, crash.dmp, and crash.sav to the developers. "
				 "This will greatly help debugging.\n\n"
				 "https://github.com/OpenTTD/OpenTTD/issues.\n\n"
				 "{}\n{}\n{}\n{}",
				 this->crashlog_filename, this->crashdump_filename, this->savegame_filename, this->screenshot_filename);

		ShowMacDialog(crash_title, message.c_str(), "Quit");
	}

	/** Buffer to track the long jump set setup. */
	jmp_buf internal_fault_jmp_buf;

	/** Whether we are in a TryExecute block. */
	bool try_execute_active = false;

	/** Points to the current crash log. */
	static CrashLogOSX *current;
};

/* static */ CrashLogOSX *CrashLogOSX::current = nullptr;

/**
 * Set a signal handler for all signals we want to capture.
 *
 * @param handler The handler to use.
 * @return sigset_t A sigset_t containing all signals we want to capture.
 */
static sigset_t SetSignals(void(*handler)(int))
{
	sigset_t sigs;
	sigemptyset(&sigs);
	for (int signum : _signals_to_handle) {
		sigaddset(&sigs, signum);
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_RESTART;

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	sa.sa_mask = sigs;

	for (int signum : _signals_to_handle) {
		sigaction(signum, &sa, nullptr);
	}

	return sigs;
}

/**
 * Entry point for a crash that happened during the handling of a crash.
 *
 * @param signum the signal that caused us to crash.
 */
static void CDECL HandleInternalCrash(int signum)
{
	if (CrashLogOSX::current == nullptr || !CrashLogOSX::current->try_execute_active) {
		fmt::print("Something went seriously wrong when creating the crash log. Aborting.\n");
		_exit(1);
	}

	longjmp(CrashLogOSX::current->internal_fault_jmp_buf, 1);
}

/**
 * Entry point for the crash handler.
 *
 * @param signum the signal that caused us to crash.
 */
static void CDECL HandleCrash(int signum)
{
	if (CrashLogOSX::current != nullptr) {
		CrashLog::AfterCrashLogCleanup();
		_exit(2);
	}

	/* Capture crashing during the handling of a crash. */
	sigset_t sigs = SetSignals(HandleInternalCrash);
	sigset_t old_sigset;
	sigprocmask(SIG_UNBLOCK, &sigs, &old_sigset);

	if (_gamelog.TestEmergency()) {
		ShowMacDialog("A serious fault condition occurred in the game. The game will shut down.",
				"As you loaded an emergency savegame no crash information will be generated.\n",
				"Quit");
		_exit(3);
	}

	if (SaveloadCrashWithMissingNewGRFs()) {
		ShowMacDialog("A serious fault condition occurred in the game. The game will shut down.",
				"As you loaded an savegame for which you do not have the required NewGRFs no crash information will be generated.\n",
				"Quit");
		_exit(3);
	}

	CrashLogOSX *log = new CrashLogOSX(signum);
	CrashLogOSX::current = log;
	log->MakeCrashLog();
	if (VideoDriver::GetInstance() == nullptr || VideoDriver::GetInstance()->HasGUI()) {
		log->DisplayCrashDialog();
	}

	CrashLog::AfterCrashLogCleanup();
	_exit(2);
}

/* static */ void CrashLog::InitialiseCrashLog()
{
	SetSignals(HandleCrash);
}

/* static */ void CrashLog::InitThread()
{
}
