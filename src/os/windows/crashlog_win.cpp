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

#include "../../safeguards.h"

/**
 * Windows implementation for the crash logger.
 */
class CrashLogWindows : public CrashLog {
	/** Information about the encountered exception */
	EXCEPTION_POINTERS *ep;

	void LogOSVersion(std::back_insert_iterator<std::string> &output_iterator) const override;
	void LogError(std::back_insert_iterator<std::string> &output_iterator, const std::string_view message) const override;
	void LogStacktrace(std::back_insert_iterator<std::string> &output_iterator) const override;
	void LogRegisters(std::back_insert_iterator<std::string> &output_iterator) const override;
	void LogModules(std::back_insert_iterator<std::string> &output_iterator) const override;
public:
#if defined(_MSC_VER)
	int WriteCrashDump() override;
	void AppendDecodedStacktrace(std::back_insert_iterator<std::string> &output_iterator) const;
#else
	void AppendDecodedStacktrace(std::back_insert_iterator<std::string> &output_iterator) const {}
#endif /* _MSC_VER */

	/**
	 * A crash log is always generated when it's generated.
	 * @param ep the data related to the exception.
	 */
	CrashLogWindows(EXCEPTION_POINTERS *ep = nullptr) : ep(ep) {}

	/**
	 * Points to the current crash log.
	 */
	static CrashLogWindows *current;
};

/* static */ CrashLogWindows *CrashLogWindows::current = nullptr;

/* virtual */ void CrashLogWindows::LogOSVersion(std::back_insert_iterator<std::string> &output_iterator) const
{
	_OSVERSIONINFOA os;
	os.dwOSVersionInfoSize = sizeof(os);
	GetVersionExA(&os);

	fmt::format_to(output_iterator,
			"Operating system:\n"
			" Name:     Windows\n"
			" Release:  {}.{}.{} ({})\n",
			os.dwMajorVersion,
			os.dwMinorVersion,
			os.dwBuildNumber,
			os.szCSDVersion
	);
}

/* virtual */ void CrashLogWindows::LogError(std::back_insert_iterator<std::string> &output_iterator, const std::string_view message) const
{
	fmt::format_to(output_iterator,
			"Crash reason:\n"
			" Exception: {:08X}\n"
			" Location:  {:X}\n"
			" Message:   {}\n\n",
			ep->ExceptionRecord->ExceptionCode,
			(size_t)ep->ExceptionRecord->ExceptionAddress,
			message
	);
}

struct DebugFileInfo {
	uint32_t size;
	uint32_t crc32;
	SYSTEMTIME file_time;
};

static uint32_t _crc_table[256];

static void MakeCRCTable()
{
	uint32_t crc, poly = 0xEDB88320L;
	int i;
	int j;

	for (i = 0; i != 256; i++) {
		crc = i;
		for (j = 8; j != 0; j--) {
			crc = (crc & 1 ? (crc >> 1) ^ poly : crc >> 1);
		}
		_crc_table[i] = crc;
	}
}

static uint32_t CalcCRC(byte *data, uint size, uint32_t crc)
{
	for (; size > 0; size--) {
		crc = ((crc >> 8) & 0x00FFFFFF) ^ _crc_table[(crc ^ *data++) & 0xFF];
	}
	return crc;
}

static void GetFileInfo(DebugFileInfo *dfi, const wchar_t *filename)
{
	HANDLE file;
	memset(dfi, 0, sizeof(*dfi));

	file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, 0);
	if (file != INVALID_HANDLE_VALUE) {
		byte buffer[1024];
		DWORD numread;
		uint32_t filesize = 0;
		FILETIME write_time;
		uint32_t crc = (uint32_t)-1;

		for (;;) {
			if (ReadFile(file, buffer, sizeof(buffer), &numread, nullptr) == 0 || numread == 0) {
				break;
			}
			filesize += numread;
			crc = CalcCRC(buffer, numread, crc);
		}
		dfi->size = filesize;
		dfi->crc32 = crc ^ (uint32_t)-1;

		if (GetFileTime(file, nullptr, nullptr, &write_time)) {
			FileTimeToSystemTime(&write_time, &dfi->file_time);
		}
		CloseHandle(file);
	}
}


static void PrintModuleInfo(std::back_insert_iterator<std::string> &output_iterator, HMODULE mod)
{
	wchar_t buffer[MAX_PATH];
	DebugFileInfo dfi;

	GetModuleFileName(mod, buffer, MAX_PATH);
	GetFileInfo(&dfi, buffer);
	fmt::format_to(output_iterator, " {:20s} handle: {:X} size: {} crc: {:8X} date: {}-{:02}-{:02} {:02}:{:02}:{:02}\n",
		FS2OTTD(buffer),
		(size_t)mod,
		dfi.size,
		dfi.crc32,
		dfi.file_time.wYear,
		dfi.file_time.wMonth,
		dfi.file_time.wDay,
		dfi.file_time.wHour,
		dfi.file_time.wMinute,
		dfi.file_time.wSecond
	);
}

/* virtual */ void CrashLogWindows::LogModules(std::back_insert_iterator<std::string> &output_iterator) const
{
	MakeCRCTable();

	fmt::format_to(output_iterator, "Module information:\n");

	HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
	if (proc != nullptr) {
		HMODULE modules[100];
		DWORD needed;
		BOOL res = EnumProcessModules(proc, modules, sizeof(modules), &needed);
		CloseHandle(proc);
		if (res) {
			size_t count = std::min<DWORD>(needed / sizeof(HMODULE), lengthof(modules));

			for (size_t i = 0; i != count; i++) PrintModuleInfo(output_iterator, modules[i]);
			fmt::format_to(output_iterator, "\n");
			return;
		}
	}
	PrintModuleInfo(output_iterator, nullptr);
	fmt::format_to(output_iterator, "\n");
}

/* virtual */ void CrashLogWindows::LogRegisters(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator, "Registers:\n");
#ifdef _M_AMD64
	fmt::format_to(output_iterator,
		" RAX: {:016X} RBX: {:016X} RCX: {:016X} RDX: {:016X}\n"
		" RSI: {:016X} RDI: {:016X} RBP: {:016X} RSP: {:016X}\n"
		" R8:  {:016X} R9:  {:016X} R10: {:016X} R11: {:016X}\n"
		" R12: {:016X} R13: {:016X} R14: {:016X} R15: {:016X}\n"
		" RIP: {:016X} EFLAGS: {:08X}\n",
		ep->ContextRecord->Rax,
		ep->ContextRecord->Rbx,
		ep->ContextRecord->Rcx,
		ep->ContextRecord->Rdx,
		ep->ContextRecord->Rsi,
		ep->ContextRecord->Rdi,
		ep->ContextRecord->Rbp,
		ep->ContextRecord->Rsp,
		ep->ContextRecord->R8,
		ep->ContextRecord->R9,
		ep->ContextRecord->R10,
		ep->ContextRecord->R11,
		ep->ContextRecord->R12,
		ep->ContextRecord->R13,
		ep->ContextRecord->R14,
		ep->ContextRecord->R15,
		ep->ContextRecord->Rip,
		ep->ContextRecord->EFlags
	);
#elif defined(_M_IX86)
	fmt::format_to(output_iterator,
		" EAX: {:08X} EBX: {:08X} ECX: {:08X} EDX: {:08X}\n"
		" ESI: {:08X} EDI: {:08X} EBP: {:08X} ESP: {:08X}\n"
		" EIP: {:08X} EFLAGS: {:08X}\n",
		(int)ep->ContextRecord->Eax,
		(int)ep->ContextRecord->Ebx,
		(int)ep->ContextRecord->Ecx,
		(int)ep->ContextRecord->Edx,
		(int)ep->ContextRecord->Esi,
		(int)ep->ContextRecord->Edi,
		(int)ep->ContextRecord->Ebp,
		(int)ep->ContextRecord->Esp,
		(int)ep->ContextRecord->Eip,
		(int)ep->ContextRecord->EFlags
	);
#elif defined(_M_ARM64)
	fmt::format_to(output_iterator,
		" X0:  {:016X} X1:  {:016X} X2:  {:016X} X3:  {:016X}\n"
		" X4:  {:016X} X5:  {:016X} X6:  {:016X} X7:  {:016X}\n"
		" X8:  {:016X} X9:  {:016X} X10: {:016X} X11: {:016X}\n"
		" X12: {:016X} X13: {:016X} X14: {:016X} X15: {:016X}\n"
		" X16: {:016X} X17: {:016X} X18: {:016X} X19: {:016X}\n"
		" X20: {:016X} X21: {:016X} X22: {:016X} X23: {:016X}\n"
		" X24: {:016X} X25: {:016X} X26: {:016X} X27: {:016X}\n"
		" X28: {:016X} Fp:  {:016X} Lr:  {:016X}\n",
		ep->ContextRecord->X0,
		ep->ContextRecord->X1,
		ep->ContextRecord->X2,
		ep->ContextRecord->X3,
		ep->ContextRecord->X4,
		ep->ContextRecord->X5,
		ep->ContextRecord->X6,
		ep->ContextRecord->X7,
		ep->ContextRecord->X8,
		ep->ContextRecord->X9,
		ep->ContextRecord->X10,
		ep->ContextRecord->X11,
		ep->ContextRecord->X12,
		ep->ContextRecord->X13,
		ep->ContextRecord->X14,
		ep->ContextRecord->X15,
		ep->ContextRecord->X16,
		ep->ContextRecord->X17,
		ep->ContextRecord->X18,
		ep->ContextRecord->X19,
		ep->ContextRecord->X20,
		ep->ContextRecord->X21,
		ep->ContextRecord->X22,
		ep->ContextRecord->X23,
		ep->ContextRecord->X24,
		ep->ContextRecord->X25,
		ep->ContextRecord->X26,
		ep->ContextRecord->X27,
		ep->ContextRecord->X28,
		ep->ContextRecord->Fp,
		ep->ContextRecord->Lr
	);
#endif

	fmt::format_to(output_iterator, "\n Bytes at instruction pointer:\n");
#ifdef _M_AMD64
	byte *b = (byte*)ep->ContextRecord->Rip;
#elif defined(_M_IX86)
	byte *b = (byte*)ep->ContextRecord->Eip;
#elif defined(_M_ARM64)
	byte *b = (byte*)ep->ContextRecord->Pc;
#endif
	for (int i = 0; i != 24; i++) {
		if (IsBadReadPtr(b, 1)) {
			fmt::format_to(output_iterator, " ??"); // OCR: WAS: , 0);
		} else {
			fmt::format_to(output_iterator, " {:02X}", *b);
		}
		b++;
	}
	fmt::format_to(output_iterator, "\n\n");
}

/* virtual */ void CrashLogWindows::LogStacktrace(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator, "Stack trace:\n");
	fmt::format_to(output_iterator, " Not supported.\n");
}

#if defined(_MSC_VER)
static const uint MAX_SYMBOL_LEN = 512;
static const uint MAX_FRAMES     = 64;

#pragma warning(disable:4091)
#include <dbghelp.h>
#pragma warning(default:4091)

void CrashLogWindows::AppendDecodedStacktrace(std::back_insert_iterator<std::string> &output_iterator) const
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

	fmt::format_to(output_iterator, "\nDecoded stack trace:\n");

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
				fmt::format_to(output_iterator, " <infinite loop>\n");
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
			fmt::format_to(output_iterator, "[{:02}] {:20s} {:X}", num, mod_name, frame.AddrPC.Offset);

			/* Get symbol name and line info if possible. */
			DWORD64 offset;
			if (proc.pSymGetSymFromAddr64(hCur, frame.AddrPC.Offset, &offset, sym_info)) {
				fmt::format_to(output_iterator, " {} + {}", sym_info->Name, offset);

				DWORD line_offs;
				IMAGEHLP_LINE64 line;
				line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
				if (proc.pSymGetLineFromAddr64(hCur, frame.AddrPC.Offset, &line_offs, &line)) {
					fmt::format_to(output_iterator, " ({}:{})", line.FileName, line.LineNumber);
				}
			}
			fmt::format_to(output_iterator, "\n");
		}

		proc.pSymCleanup(hCur);
	}

	fmt::format_to(output_iterator, "\n*** End of additional info ***\n");
}

/* virtual */ int CrashLogWindows::WriteCrashDump()
{
	int ret = 0;
	DllLoader dbghelp(L"dbghelp.dll");
	if (dbghelp.Success()) {
		typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE,
				MINIDUMP_TYPE,
				CONST PMINIDUMP_EXCEPTION_INFORMATION,
				CONST PMINIDUMP_USER_STREAM_INFORMATION,
				CONST PMINIDUMP_CALLBACK_INFORMATION);
		MiniDumpWriteDump_t funcMiniDumpWriteDump = dbghelp.GetProcAddress("MiniDumpWriteDump");
		if (funcMiniDumpWriteDump != nullptr) {
			this->crashdump_filename = this->CreateFileName(".dmp");
			HANDLE file  = CreateFile(OTTD2FS(this->crashdump_filename).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, 0);
			HANDLE proc  = GetCurrentProcess();
			DWORD procid = GetCurrentProcessId();
			MINIDUMP_EXCEPTION_INFORMATION mdei;
			MINIDUMP_USER_STREAM userstream;
			MINIDUMP_USER_STREAM_INFORMATION musi;

			userstream.Type        = LastReservedStream + 1;
			userstream.Buffer      = (void*)this->crashlog.data();
			userstream.BufferSize  = (ULONG)this->crashlog.size() + 1;

			musi.UserStreamCount   = 1;
			musi.UserStreamArray   = &userstream;

			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers  = ep;
			mdei.ClientPointers     = false;

			funcMiniDumpWriteDump(proc, procid, file, MiniDumpWithDataSegs, &mdei, &musi, nullptr);
			ret = 1;
		} else {
			ret = -1;
		}
	}
	return ret;
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
		ExitProcess(2);
	}

	if (_gamelog.TestEmergency()) {
		static const wchar_t _emergency_crash[] =
			L"A serious fault condition occurred in the game. The game will shut down.\n"
			L"As you loaded an emergency savegame no crash information will be generated.\n";
		MessageBox(nullptr, _emergency_crash, L"Fatal Application Failure", MB_ICONERROR);
		ExitProcess(3);
	}

	if (SaveloadCrashWithMissingNewGRFs()) {
		static const wchar_t _saveload_crash[] =
			L"A serious fault condition occurred in the game. The game will shut down.\n"
			L"As you loaded an savegame for which you do not have the required NewGRFs\n"
			L"no crash information will be generated.\n";
		MessageBox(nullptr, _saveload_crash, L"Fatal Application Failure", MB_ICONERROR);
		ExitProcess(3);
	}

	CrashLogWindows *log = new CrashLogWindows(ep);
	CrashLogWindows::current = log;
	auto output_iterator = std::back_inserter(log->crashlog);
	log->FillCrashLog(output_iterator);
	log->WriteCrashDump();
	log->AppendDecodedStacktrace(output_iterator);
	log->WriteCrashLog();
	log->WriteScreenshot();
	log->SendSurvey();

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

static void CDECL CustomAbort(int signal)
{
	RaiseException(0xE1212012, 0, 0, nullptr);
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
	L"Please send the crash information and the crash.dmp file (if any) to the developers.\n"
	L"This will greatly help debugging. The correct place to do this is https://github.com/OpenTTD/OpenTTD/issues. "
	L"The information contained in the report is displayed below.\n"
	L"Press \"Emergency save\" to attempt saving the game. Generated file(s):\n"
	L"%s";

static const wchar_t _save_succeeded[] =
	L"Emergency save succeeded.\nIts location is '%s'.\n"
	L"Be aware that critical parts of the internal game state may have become "
	L"corrupted. The saved game is not guaranteed to work.";

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

static INT_PTR CALLBACK CrashDialogFunc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG: {
			/* We need to put the crash-log in a separate buffer because the default
			 * buffer in MB_TO_WIDE is not large enough (512 chars) */
			wchar_t filenamebuf[MAX_PATH * 2];
			wchar_t crash_msgW[65536];
			/* Convert unix -> dos newlines because the edit box only supports that properly :( */
			const char *unix_nl = CrashLogWindows::current->crashlog.data();
			char dos_nl[65536];
			char *p = dos_nl;
			char32_t c;
			while ((c = Utf8Consume(&unix_nl)) && p < lastof(dos_nl) - 4) { // 4 is max number of bytes per character
				if (c == '\n') p += Utf8Encode(p, '\r');
				p += Utf8Encode(p, c);
			}
			*p = '\0';

			/* Add path to crash.log and crash.dmp (if any) to the crash window text */
			size_t len = wcslen(_crash_desc) + 2;
			len += wcslen(convert_to_fs(CrashLogWindows::current->crashlog_filename, filenamebuf, lengthof(filenamebuf))) + 2;
			len += wcslen(convert_to_fs(CrashLogWindows::current->crashdump_filename, filenamebuf, lengthof(filenamebuf))) + 2;
			len += wcslen(convert_to_fs(CrashLogWindows::current->screenshot_filename, filenamebuf, lengthof(filenamebuf))) + 1;

			static wchar_t text[lengthof(_crash_desc) + 3 * MAX_PATH * 2 + 7];
			int printed = _snwprintf(text, len, _crash_desc, convert_to_fs(CrashLogWindows::current->crashlog_filename, filenamebuf, lengthof(filenamebuf)));
			if (printed < 0 || (size_t)printed > len) {
				MessageBox(wnd, L"Catastrophic failure trying to display crash message. Could not perform text formatting.", L"OpenTTD", MB_ICONERROR);
				return FALSE;
			}
			if (convert_to_fs(CrashLogWindows::current->crashdump_filename, filenamebuf, lengthof(filenamebuf))[0] != L'\0') {
				wcscat(text, L"\n");
				wcscat(text, filenamebuf);
			}
			if (convert_to_fs(CrashLogWindows::current->screenshot_filename, filenamebuf, lengthof(filenamebuf))[0] != L'\0') {
				wcscat(text, L"\n");
				wcscat(text, filenamebuf);
			}

			SetDlgItemText(wnd, 10, text);
			SetDlgItemText(wnd, 11, convert_to_fs(dos_nl, crash_msgW, lengthof(crash_msgW)));
			SendDlgItemMessage(wnd, 11, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);
			SetWndSize(wnd, -1);
		} return TRUE;
		case WM_COMMAND:
			switch (wParam) {
				case 12: // Close
					CrashLog::AfterCrashLogCleanup();
					ExitProcess(2);
				case 13: // Emergency save
					wchar_t filenamebuf[MAX_PATH * 2];
					if (CrashLogWindows::current->WriteSavegame()) {
						convert_to_fs(CrashLogWindows::current->savegame_filename, filenamebuf, lengthof(filenamebuf));
						size_t len = lengthof(_save_succeeded) + wcslen(filenamebuf) + 1;
						static wchar_t text[lengthof(_save_succeeded) + MAX_PATH * 2 + 1];
						_snwprintf(text, len, _save_succeeded, filenamebuf);
						MessageBox(wnd, text, L"Save successful", MB_ICONINFORMATION);
					} else {
						MessageBox(wnd, L"Save failed", L"Save failed", MB_ICONINFORMATION);
					}
					break;
				case 15: // Expand window to show crash-message
					_expanded = !_expanded;
					SetWndSize(wnd, _expanded);
					break;
			}
			return TRUE;
		case WM_CLOSE:
			CrashLog::AfterCrashLogCleanup();
			ExitProcess(2);
	}

	return FALSE;
}

static void ShowCrashlogWindow()
{
	ShowCursor(TRUE);
	ShowWindow(GetActiveWindow(), FALSE);
	DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(100), nullptr, CrashDialogFunc);
}
