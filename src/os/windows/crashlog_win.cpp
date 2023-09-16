/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file crashlog_win.cpp Implementation of a crashlogger for Windows */

#include "../../stdafx.h"
#include "../../crashlog.h"
#include "win32.h"
#include "../../core/math_func.hpp"
#include "../../string_func.h"
#include "../../fileio_func.h"
#include "../../strings_func.h"
#include "../../gamelog.h"
#include "../../saveload/saveload.h"
#include "../../video/video_driver.hpp"

#include <windows.h>
#include <mmsystem.h>
#include <signal.h>
#include <psapi.h>

#if defined(_MSC_VER)
#	include <dbghelp.h>
#else
#	include <setjmp.h>
#endif

#ifdef WITH_UNOFFICIAL_BREAKPAD
#	include <client/windows/handler/exception_handler.h>
#endif

#include "../../safeguards.h"

/** Exception code used for custom abort. */
static constexpr DWORD CUSTOM_ABORT_EXCEPTION = 0xE1212012;

/** A map between exception code and its name. */
static const std::map<DWORD, std::string> exception_code_to_name{
	{EXCEPTION_ACCESS_VIOLATION, "EXCEPTION_ACCESS_VIOLATION"},
	{EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"},
	{EXCEPTION_BREAKPOINT, "EXCEPTION_BREAKPOINT"},
	{EXCEPTION_DATATYPE_MISALIGNMENT, "EXCEPTION_DATATYPE_MISALIGNMENT"},
	{EXCEPTION_FLT_DENORMAL_OPERAND, "EXCEPTION_FLT_DENORMAL_OPERAND"},
	{EXCEPTION_FLT_DIVIDE_BY_ZERO, "EXCEPTION_FLT_DIVIDE_BY_ZERO"},
	{EXCEPTION_FLT_INEXACT_RESULT, "EXCEPTION_FLT_INEXACT_RESULT"},
	{EXCEPTION_FLT_INVALID_OPERATION, "EXCEPTION_FLT_INVALID_OPERATION"},
	{EXCEPTION_FLT_OVERFLOW, "EXCEPTION_FLT_OVERFLOW"},
	{EXCEPTION_FLT_STACK_CHECK, "EXCEPTION_FLT_STACK_CHECK"},
	{EXCEPTION_FLT_UNDERFLOW, "EXCEPTION_FLT_UNDERFLOW"},
	{EXCEPTION_GUARD_PAGE, "EXCEPTION_GUARD_PAGE"},
	{EXCEPTION_ILLEGAL_INSTRUCTION, "EXCEPTION_ILLEGAL_INSTRUCTION"},
	{EXCEPTION_IN_PAGE_ERROR, "EXCEPTION_IN_PAGE_ERROR"},
	{EXCEPTION_INT_DIVIDE_BY_ZERO, "EXCEPTION_INT_DIVIDE_BY_ZERO"},
	{EXCEPTION_INT_OVERFLOW, "EXCEPTION_INT_OVERFLOW"},
	{EXCEPTION_INVALID_DISPOSITION, "EXCEPTION_INVALID_DISPOSITION"},
	{EXCEPTION_INVALID_HANDLE, "EXCEPTION_INVALID_HANDLE"},
	{EXCEPTION_NONCONTINUABLE_EXCEPTION, "EXCEPTION_NONCONTINUABLE_EXCEPTION"},
	{EXCEPTION_PRIV_INSTRUCTION, "EXCEPTION_PRIV_INSTRUCTION"},
	{EXCEPTION_SINGLE_STEP, "EXCEPTION_SINGLE_STEP"},
	{EXCEPTION_STACK_OVERFLOW, "EXCEPTION_STACK_OVERFLOW"},
	{STATUS_UNWIND_CONSOLIDATE, "STATUS_UNWIND_CONSOLIDATE"},
};

/**
 * Forcefully try to terminate the application.
 *
 * @param exit_code The exit code to return.
 */
static void NORETURN ImmediateExitProcess(uint exit_code)
{
	/* TerminateProcess may fail in some special edge cases; fall back to ExitProcess in this case. */
	TerminateProcess(GetCurrentProcess(), exit_code);
	ExitProcess(exit_code);
}

/**
 * Windows implementation for the crash logger.
 */
class CrashLogWindows : public CrashLog {
	/** Information about the encountered exception */
	EXCEPTION_POINTERS *ep;

	void SurveyCrash(nlohmann::json &survey) const override
	{
		survey["id"] = ep->ExceptionRecord->ExceptionCode;
		if (exception_code_to_name.count(ep->ExceptionRecord->ExceptionCode) > 0) {
			survey["reason"] = exception_code_to_name.at(ep->ExceptionRecord->ExceptionCode);
		} else {
			survey["reason"] = "Unknown exception code";
		}
	}

	void SurveyStacktrace(nlohmann::json &survey) const override;
public:

#ifdef WITH_UNOFFICIAL_BREAKPAD
	static bool MinidumpCallback(const wchar_t *dump_dir, const wchar_t *minidump_id, void *context, EXCEPTION_POINTERS *, MDRawAssertionInfo *, bool succeeded)
	{
		CrashLogWindows *crashlog = reinterpret_cast<CrashLogWindows *>(context);

		crashlog->crashdump_filename = crashlog->CreateFileName(".dmp");
		std::rename(fmt::format("{}/{}.dmp", FS2OTTD(dump_dir), FS2OTTD(minidump_id)).c_str(), crashlog->crashdump_filename.c_str());
		return succeeded;
	}

	bool WriteCrashDump() override
	{
		return google_breakpad::ExceptionHandler::WriteMinidump(OTTD2FS(_personal_dir), MinidumpCallback, this);
	}
#endif

#if defined(_MSC_VER)
	/* virtual */ bool TryExecute(std::string_view section_name, std::function<bool()> &&func) override
	{
		this->try_execute_active = true;
		bool res;

		__try {
			res = func();
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			fmt::print("Something went wrong when attempting to fill {} section of the crash log.\n", section_name);
			res = false;
		}

		this->try_execute_active = false;
		return res;
	}
#else
	/* virtual */ bool TryExecute(std::string_view section_name, std::function<bool()> &&func) override
	{
		this->try_execute_active = true;

		/* Setup a longjump in case a crash happens. */
		if (setjmp(this->internal_fault_jmp_buf) != 0) {
			fmt::print("Something went wrong when attempting to fill {} section of the crash log.\n", section_name);

			this->try_execute_active = false;
			return false;
		}

		bool res = func();
		this->try_execute_active = false;
		return res;
	}
#endif /* _MSC_VER */

	/**
	 * A crash log is always generated when it's generated.
	 * @param ep the data related to the exception.
	 */
	CrashLogWindows(EXCEPTION_POINTERS *ep = nullptr) :
		ep(ep)
	{
	}

#if !defined(_MSC_VER)
	/** Buffer to track the long jump set setup. */
	jmp_buf internal_fault_jmp_buf;
#endif

	/** Whether we are in a TryExecute block. */
	bool try_execute_active = false;

	/** Points to the current crash log. */
	static CrashLogWindows *current;
};

/* static */ CrashLogWindows *CrashLogWindows::current = nullptr;

#if defined(_MSC_VER)
static const uint MAX_SYMBOL_LEN = 512;
static const uint MAX_FRAMES     = 64;

/* virtual */ void CrashLogWindows::SurveyStacktrace(nlohmann::json &survey) const
{
	DllLoader dbghelp(L"dbghelp.dll");
	struct ProcPtrs {
		BOOL (WINAPI * pSymInitialize)(HANDLE, PCSTR, BOOL);
		BOOL (WINAPI * pSymSetOptions)(DWORD);
		BOOL (WINAPI * pSymCleanup)(HANDLE);
		BOOL (WINAPI * pStackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
		PVOID (WINAPI * pSymFunctionTableAccess64)(HANDLE, DWORD64);
		DWORD64 (WINAPI * pSymGetModuleBase64)(HANDLE, DWORD64);
		BOOL (WINAPI * pSymGetModuleInfo64)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);
		BOOL (WINAPI * pSymGetSymFromAddr64)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64);
		BOOL (WINAPI * pSymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
	} proc = {
		dbghelp.GetProcAddress("SymInitialize"),
		dbghelp.GetProcAddress("SymSetOptions"),
		dbghelp.GetProcAddress("SymCleanup"),
		dbghelp.GetProcAddress("StackWalk64"),
		dbghelp.GetProcAddress("SymFunctionTableAccess64"),
		dbghelp.GetProcAddress("SymGetModuleBase64"),
		dbghelp.GetProcAddress("SymGetModuleInfo64"),
		dbghelp.GetProcAddress("SymGetSymFromAddr64"),
		dbghelp.GetProcAddress("SymGetLineFromAddr64"),
	};

	survey = nlohmann::json::array();

	/* Try to load the functions from the DLL, if that fails because of a too old dbghelp.dll, just skip it. */
	if (dbghelp.Success()) {
		/* Initialize symbol handler. */
		HANDLE hCur = GetCurrentProcess();
		proc.pSymInitialize(hCur, nullptr, TRUE);
		/* Load symbols only when needed, fail silently on errors, demangle symbol names. */
		proc.pSymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_UNDNAME);

		/* Initialize starting stack frame from context record. */
		STACKFRAME64 frame;
		memset(&frame, 0, sizeof(frame));
#ifdef _M_AMD64
		frame.AddrPC.Offset = ep->ContextRecord->Rip;
		frame.AddrFrame.Offset = ep->ContextRecord->Rbp;
		frame.AddrStack.Offset = ep->ContextRecord->Rsp;
#elif defined(_M_IX86)
		frame.AddrPC.Offset = ep->ContextRecord->Eip;
		frame.AddrFrame.Offset = ep->ContextRecord->Ebp;
		frame.AddrStack.Offset = ep->ContextRecord->Esp;
#elif defined(_M_ARM64)
		frame.AddrPC.Offset = ep->ContextRecord->Pc;
		frame.AddrFrame.Offset = ep->ContextRecord->Fp;
		frame.AddrStack.Offset = ep->ContextRecord->Sp;
#endif
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Mode = AddrModeFlat;

		/* Copy context record as StackWalk64 may modify it. */
		CONTEXT ctx;
		memcpy(&ctx, ep->ContextRecord, sizeof(ctx));

		/* Allocate space for symbol info. */
		char sym_info_raw[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYMBOL_LEN - 1];
		IMAGEHLP_SYMBOL64 *sym_info = (IMAGEHLP_SYMBOL64*)sym_info_raw;
		sym_info->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		sym_info->MaxNameLength = MAX_SYMBOL_LEN;

		/* Walk stack at most MAX_FRAMES deep in case the stack is corrupt. */
		for (uint num = 0; num < MAX_FRAMES; num++) {
			if (!proc.pStackWalk64(
#ifdef _M_AMD64
				IMAGE_FILE_MACHINE_AMD64,
#else
				IMAGE_FILE_MACHINE_I386,
#endif
				hCur, GetCurrentThread(), &frame, &ctx, nullptr, proc.pSymFunctionTableAccess64, proc.pSymGetModuleBase64, nullptr)) break;

			if (frame.AddrPC.Offset == frame.AddrReturn.Offset) {
				survey.push_back("<infinite loop>");
				break;
			}

			/* Get module name. */
			const char *mod_name = "???";

			IMAGEHLP_MODULE64 module;
			module.SizeOfStruct = sizeof(module);
			if (proc.pSymGetModuleInfo64(hCur, frame.AddrPC.Offset, &module)) {
				mod_name = module.ModuleName;
			}

			/* Print module and instruction pointer. */
			std::string message = fmt::format("{:20s} {:X}",  mod_name, frame.AddrPC.Offset);

			/* Get symbol name and line info if possible. */
			DWORD64 offset;
			if (proc.pSymGetSymFromAddr64(hCur, frame.AddrPC.Offset, &offset, sym_info)) {
				message += fmt::format(" {} + {}", sym_info->Name, offset);

				DWORD line_offs;
				IMAGEHLP_LINE64 line;
				line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
				if (proc.pSymGetLineFromAddr64(hCur, frame.AddrPC.Offset, &line_offs, &line)) {
					message += fmt::format(" ({}:{})", line.FileName, line.LineNumber);
				}
			}

			survey.push_back(message);
		}

		proc.pSymCleanup(hCur);
	}
}
#else
/* virtual */ void CrashLogWindows::SurveyStacktrace(nlohmann::json &) const
{
	/* Not supported. */
}
#endif /* _MSC_VER */

extern bool CloseConsoleLogIfActive();
static void ShowCrashlogWindow();

/**
 * Stack pointer for use when 'starting' the crash handler.
 * Not static as gcc's inline assembly needs it that way.
 */
thread_local void *_safe_esp = nullptr;

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep)
{
	/* Restore system timer resolution. */
	timeEndPeriod(1);

	/* Disable our event loop. */
	SetWindowLongPtr(GetActiveWindow(), GWLP_WNDPROC, (LONG_PTR)&DefWindowProc);

	if (CrashLogWindows::current != nullptr) {
		CrashLog::AfterCrashLogCleanup();
		ImmediateExitProcess(2);
	}

	if (_gamelog.TestEmergency()) {
		static const wchar_t _emergency_crash[] =
			L"A serious fault condition occurred in the game. The game will shut down.\n"
			L"As you loaded an emergency savegame no crash information will be generated.\n";
		MessageBox(nullptr, _emergency_crash, L"Fatal Application Failure", MB_ICONERROR);
		ImmediateExitProcess(3);
	}

	if (SaveloadCrashWithMissingNewGRFs()) {
		static const wchar_t _saveload_crash[] =
			L"A serious fault condition occurred in the game. The game will shut down.\n"
			L"As you loaded an savegame for which you do not have the required NewGRFs\n"
			L"no crash information will be generated.\n";
		MessageBox(nullptr, _saveload_crash, L"Fatal Application Failure", MB_ICONERROR);
		ImmediateExitProcess(3);
	}

	CrashLogWindows *log = new CrashLogWindows(ep);
	CrashLogWindows::current = log;
	log->MakeCrashLog();

	/* Close any possible log files */
	CloseConsoleLogIfActive();

	if ((VideoDriver::GetInstance() == nullptr || VideoDriver::GetInstance()->HasGUI()) && _safe_esp != nullptr) {
#ifdef _M_AMD64
		ep->ContextRecord->Rip = (DWORD64)ShowCrashlogWindow;
		ep->ContextRecord->Rsp = (DWORD64)_safe_esp;
#elif defined(_M_IX86)
		ep->ContextRecord->Eip = (DWORD)ShowCrashlogWindow;
		ep->ContextRecord->Esp = (DWORD)_safe_esp;
#elif defined(_M_ARM64)
		ep->ContextRecord->Pc = (DWORD64)ShowCrashlogWindow;
		ep->ContextRecord->Sp = (DWORD64)_safe_esp;
#endif
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	CrashLog::AfterCrashLogCleanup();
	return EXCEPTION_EXECUTE_HANDLER;
}

static LONG WINAPI VectoredExceptionHandler(EXCEPTION_POINTERS *ep)
{
	if (CrashLogWindows::current != nullptr && CrashLogWindows::current->try_execute_active) {
#if defined(_MSC_VER)
		return EXCEPTION_CONTINUE_SEARCH;
#else
		longjmp(CrashLogWindows::current->internal_fault_jmp_buf, 1);
#endif
	}

	if (ep->ExceptionRecord->ExceptionCode == 0xC0000374 /* heap corruption */) {
		return ExceptionHandler(ep);
	}
	if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
		return ExceptionHandler(ep);
	}
	if (ep->ExceptionRecord->ExceptionCode == CUSTOM_ABORT_EXCEPTION) {
		return ExceptionHandler(ep);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

static void CDECL CustomAbort(int)
{
	RaiseException(CUSTOM_ABORT_EXCEPTION, 0, 0, nullptr);
}

/* static */ void CrashLog::InitialiseCrashLog()
{
	CrashLog::InitThread();

	/* SIGABRT is not an unhandled exception, so we need to intercept it. */
	signal(SIGABRT, CustomAbort);
#if defined(_MSC_VER)
	/* Don't show abort message as we will get the crashlog window anyway. */
	_set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
	SetUnhandledExceptionFilter(ExceptionHandler);
	AddVectoredExceptionHandler(1, VectoredExceptionHandler);
}

/* static */ void CrashLog::InitThread()
{
#if defined(_M_AMD64) || defined(_M_ARM64)
	CONTEXT ctx;
	RtlCaptureContext(&ctx);

	/* The stack pointer for AMD64 must always be 16-byte aligned inside a
	 * function. As we are simulating a function call with the safe ESP value,
	 * we need to subtract 8 for the imaginary return address otherwise stack
	 * alignment would be wrong in the called function. */
#	if defined(_M_ARM64)
	_safe_esp = (void *)(ctx.Sp - 8);
#	else
	_safe_esp = (void *)(ctx.Rsp - 8);
#	endif
#else
	void *safe_esp;
#	if defined(_MSC_VER)
	_asm {
		mov safe_esp, esp
	}
#	else
	asm("movl %%esp, %0" : "=rm" (safe_esp));
#	endif
	_safe_esp = safe_esp;
#endif
}

/* The crash log GUI */

static bool _expanded;

static const wchar_t _crash_desc[] =
	L"A serious fault condition occurred in the game. The game will shut down.\n"
	L"Please send crash.json.log, crash.dmp, and crash.sav to the developers.\n"
	L"This will greatly help debugging.\n\n"
	L"https://github.com/OpenTTD/OpenTTD/issues\n\n"
	L"%s\n%s\n%s\n%s\n";

static const wchar_t * const _expand_texts[] = {L"S&how report >>", L"&Hide report <<" };

static void SetWndSize(HWND wnd, int mode)
{
	RECT r, r2;

	GetWindowRect(wnd, &r);
	SetDlgItemText(wnd, 15, _expand_texts[mode == 1]);

	if (mode >= 0) {
		GetWindowRect(GetDlgItem(wnd, 11), &r2);
		int offs = r2.bottom - r2.top + 10;
		if (mode == 0) offs = -offs;
		SetWindowPos(wnd, HWND_TOPMOST, 0, 0,
			r.right - r.left, r.bottom - r.top + offs, SWP_NOMOVE | SWP_NOZORDER);
	} else {
		SetWindowPos(wnd, HWND_TOPMOST,
			(GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 2,
			0, 0, SWP_NOSIZE);
	}
}

static INT_PTR CALLBACK CrashDialogFunc(HWND wnd, UINT msg, WPARAM wParam, LPARAM)
{
	switch (msg) {
		case WM_INITDIALOG: {
			std::string crashlog = CrashLogWindows::current->survey.dump(4);
			size_t crashlog_length = crashlog.size() + 1;
			/* Reserve extra space for LF to CRLF conversion. */
			crashlog_length += std::count(crashlog.begin(), crashlog.end(), '\n');

			const size_t filename_count = 4;
			const size_t filename_buf_length = MAX_PATH + 1;
			const size_t crash_desc_buf_length = lengthof(_crash_desc) + filename_buf_length * filename_count + 1;

			/* We need to put the crash-log in a separate buffer because the default
			 * buffer in MB_TO_WIDE is not large enough (512 chars).
			 * Use VirtualAlloc to allocate pages for the buffer to avoid overflowing the stack.
			 * Avoid the heap in case the crash is because the heap became corrupted. */
			const size_t total_length = crash_desc_buf_length * sizeof(wchar_t) +
										crashlog_length * sizeof(wchar_t) +
										filename_buf_length * sizeof(wchar_t) * filename_count +
										crashlog_length;
			void *raw_buffer = VirtualAlloc(nullptr, total_length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

			wchar_t *crash_desc_buf = reinterpret_cast<wchar_t *>(raw_buffer);
			wchar_t *crashlog_buf = crash_desc_buf + crash_desc_buf_length;
			wchar_t *filename_buf = crashlog_buf + crashlog_length;
			char *crashlog_dos_nl = reinterpret_cast<char *>(filename_buf + filename_buf_length * filename_count);

			/* Convert unix -> dos newlines because the edit box only supports that properly. */
			const char *crashlog_unix_nl = crashlog.data();
			char *p = crashlog_dos_nl;
			char32_t c;
			while ((c = Utf8Consume(&crashlog_unix_nl))) {
				if (c == '\n') p += Utf8Encode(p, '\r');
				p += Utf8Encode(p, c);
			}
			*p = '\0';

			_snwprintf(
				crash_desc_buf,
				crash_desc_buf_length,
				_crash_desc,
				convert_to_fs(CrashLogWindows::current->crashlog_filename,   filename_buf + filename_buf_length * 0, filename_buf_length),
				convert_to_fs(CrashLogWindows::current->crashdump_filename,  filename_buf + filename_buf_length * 1, filename_buf_length),
				convert_to_fs(CrashLogWindows::current->savegame_filename,   filename_buf + filename_buf_length * 2, filename_buf_length),
				convert_to_fs(CrashLogWindows::current->screenshot_filename, filename_buf + filename_buf_length * 3, filename_buf_length)
			);

			SetDlgItemText(wnd, 10, crash_desc_buf);
			SetDlgItemText(wnd, 11, convert_to_fs(crashlog_dos_nl, crashlog_buf, crashlog_length));
			SendDlgItemMessage(wnd, 11, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);
			SetWndSize(wnd, -1);
		} return TRUE;
		case WM_COMMAND:
			switch (wParam) {
				case 12: // Close
					CrashLog::AfterCrashLogCleanup();
					ImmediateExitProcess(2);
				case 15: // Expand window to show crash-message
					_expanded = !_expanded;
					SetWndSize(wnd, _expanded);
					break;
			}
			return TRUE;
		case WM_CLOSE:
			CrashLog::AfterCrashLogCleanup();
			ImmediateExitProcess(2);
	}

	return FALSE;
}

static void ShowCrashlogWindow()
{
	ShowCursor(TRUE);
	ShowWindow(GetActiveWindow(), FALSE);
	DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(100), nullptr, CrashDialogFunc);
}
