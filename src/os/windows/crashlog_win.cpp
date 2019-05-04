/* $Id$ */

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
#include "../../core/alloc_func.hpp"
#include "../../core/math_func.hpp"
#include "../../string_func.h"
#include "../../fileio_func.h"
#include "../../strings_func.h"
#include "../../gamelog.h"
#include "../../saveload/saveload.h"
#include "../../video/video_driver.hpp"

#include <windows.h>
#include <signal.h>

#include "../../safeguards.h"

static const uint MAX_SYMBOL_LEN = 512;
static const uint MAX_FRAMES     = 64;

/* printf format specification for 32/64-bit addresses. */
#ifdef _M_AMD64
#define PRINTF_PTR "0x%016IX"
#else
#define PRINTF_PTR "0x%08X"
#endif

/**
 * Windows implementation for the crash logger.
 */
class CrashLogWindows : public CrashLog {
	/** Information about the encountered exception */
	EXCEPTION_POINTERS *ep;

	/* virtual */ char *LogOSVersion(char *buffer, const char *last) const;
	/* virtual */ char *LogError(char *buffer, const char *last, const char *message) const;
	/* virtual */ char *LogStacktrace(char *buffer, const char *last) const;
	/* virtual */ char *LogRegisters(char *buffer, const char *last) const;
	/* virtual */ char *LogModules(char *buffer, const char *last) const;
public:
#if defined(_MSC_VER)
	/* virtual */ int WriteCrashDump(char *filename, const char *filename_last) const;
	char *AppendDecodedStacktrace(char *buffer, const char *last) const;
#else
	char *AppendDecodedStacktrace(char *buffer, const char *last) const { return buffer; }
#endif /* _MSC_VER */

	/** Buffer for the generated crash log */
	char crashlog[65536];
	/** Buffer for the filename of the crash log */
	char crashlog_filename[MAX_PATH];
	/** Buffer for the filename of the crash dump */
	char crashdump_filename[MAX_PATH];
	/** Buffer for the filename of the crash screenshot */
	char screenshot_filename[MAX_PATH];

	/**
	 * A crash log is always generated when it's generated.
	 * @param ep the data related to the exception.
	 */
	CrashLogWindows(EXCEPTION_POINTERS *ep = NULL) :
		ep(ep)
	{
		this->crashlog[0] = '\0';
		this->crashlog_filename[0] = '\0';
		this->crashdump_filename[0] = '\0';
		this->screenshot_filename[0] = '\0';
	}

	/**
	 * Points to the current crash log.
	 */
	static CrashLogWindows *current;
};

/* static */ CrashLogWindows *CrashLogWindows::current = NULL;

/* virtual */ char *CrashLogWindows::LogOSVersion(char *buffer, const char *last) const
{
	_OSVERSIONINFOA os;
	os.dwOSVersionInfoSize = sizeof(os);
	GetVersionExA(&os);

	return buffer + seprintf(buffer, last,
			"Operating system:\n"
			" Name:     Windows\n"
			" Release:  %d.%d.%d (%s)\n",
			(int)os.dwMajorVersion,
			(int)os.dwMinorVersion,
			(int)os.dwBuildNumber,
			os.szCSDVersion
	);

}

/* virtual */ char *CrashLogWindows::LogError(char *buffer, const char *last, const char *message) const
{
	return buffer + seprintf(buffer, last,
			"Crash reason:\n"
			" Exception: %.8X\n"
#ifdef _M_AMD64
			" Location:  %.16IX\n"
#else
			" Location:  %.8X\n"
#endif
			" Message:   %s\n\n",
			(int)ep->ExceptionRecord->ExceptionCode,
			(size_t)ep->ExceptionRecord->ExceptionAddress,
			message == NULL ? "<none>" : message
	);
}

struct DebugFileInfo {
	uint32 size;
	uint32 crc32;
	SYSTEMTIME file_time;
};

static uint32 *_crc_table;

static void MakeCRCTable(uint32 *table)
{
	uint32 crc, poly = 0xEDB88320L;
	int i;
	int j;

	_crc_table = table;

	for (i = 0; i != 256; i++) {
		crc = i;
		for (j = 8; j != 0; j--) {
			crc = (crc & 1 ? (crc >> 1) ^ poly : crc >> 1);
		}
		table[i] = crc;
	}
}

static uint32 CalcCRC(byte *data, uint size, uint32 crc)
{
	for (; size > 0; size--) {
		crc = ((crc >> 8) & 0x00FFFFFF) ^ _crc_table[(crc ^ *data++) & 0xFF];
	}
	return crc;
}

static void GetFileInfo(DebugFileInfo *dfi, const TCHAR *filename)
{
	HANDLE file;
	memset(dfi, 0, sizeof(*dfi));

	file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (file != INVALID_HANDLE_VALUE) {
		byte buffer[1024];
		DWORD numread;
		uint32 filesize = 0;
		FILETIME write_time;
		uint32 crc = (uint32)-1;

		for (;;) {
			if (ReadFile(file, buffer, sizeof(buffer), &numread, NULL) == 0 || numread == 0) {
				break;
			}
			filesize += numread;
			crc = CalcCRC(buffer, numread, crc);
		}
		dfi->size = filesize;
		dfi->crc32 = crc ^ (uint32)-1;

		if (GetFileTime(file, NULL, NULL, &write_time)) {
			FileTimeToSystemTime(&write_time, &dfi->file_time);
		}
		CloseHandle(file);
	}
}


static char *PrintModuleInfo(char *output, const char *last, HMODULE mod)
{
	TCHAR buffer[MAX_PATH];
	DebugFileInfo dfi;

	GetModuleFileName(mod, buffer, MAX_PATH);
	GetFileInfo(&dfi, buffer);
	output += seprintf(output, last, " %-20s handle: %p size: %d crc: %.8X date: %d-%.2d-%.2d %.2d:%.2d:%.2d\n",
		FS2OTTD(buffer),
		mod,
		dfi.size,
		dfi.crc32,
		dfi.file_time.wYear,
		dfi.file_time.wMonth,
		dfi.file_time.wDay,
		dfi.file_time.wHour,
		dfi.file_time.wMinute,
		dfi.file_time.wSecond
	);
	return output;
}

/* virtual */ char *CrashLogWindows::LogModules(char *output, const char *last) const
{
	MakeCRCTable(AllocaM(uint32, 256));
	BOOL (WINAPI *EnumProcessModules)(HANDLE, HMODULE*, DWORD, LPDWORD);

	output += seprintf(output, last, "Module information:\n");

	if (LoadLibraryList((Function*)&EnumProcessModules, "psapi.dll\0EnumProcessModules\0\0")) {
		HMODULE modules[100];
		DWORD needed;
		BOOL res;

		HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
		if (proc != NULL) {
			res = EnumProcessModules(proc, modules, sizeof(modules), &needed);
			CloseHandle(proc);
			if (res) {
				size_t count = min(needed / sizeof(HMODULE), lengthof(modules));

				for (size_t i = 0; i != count; i++) output = PrintModuleInfo(output, last, modules[i]);
				return output + seprintf(output, last, "\n");
			}
		}
	}
	output = PrintModuleInfo(output, last, NULL);
	return output + seprintf(output, last, "\n");
}

/* virtual */ char *CrashLogWindows::LogRegisters(char *buffer, const char *last) const
{
	buffer += seprintf(buffer, last, "Registers:\n");
#ifdef _M_AMD64
	buffer += seprintf(buffer, last,
		" RAX: %.16I64X RBX: %.16I64X RCX: %.16I64X RDX: %.16I64X\n"
		" RSI: %.16I64X RDI: %.16I64X RBP: %.16I64X RSP: %.16I64X\n"
		" R8:  %.16I64X R9:  %.16I64X R10: %.16I64X R11: %.16I64X\n"
		" R12: %.16I64X R13: %.16I64X R14: %.16I64X R15: %.16I64X\n"
		" RIP: %.16I64X EFLAGS: %.8lX\n",
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
#else
	buffer += seprintf(buffer, last,
		" EAX: %.8X EBX: %.8X ECX: %.8X EDX: %.8X\n"
		" ESI: %.8X EDI: %.8X EBP: %.8X ESP: %.8X\n"
		" EIP: %.8X EFLAGS: %.8X\n",
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
#endif

	buffer += seprintf(buffer, last, "\n Bytes at instruction pointer:\n");
#ifdef _M_AMD64
	byte *b = (byte*)ep->ContextRecord->Rip;
#else
	byte *b = (byte*)ep->ContextRecord->Eip;
#endif
	for (int i = 0; i != 24; i++) {
		if (IsBadReadPtr(b, 1)) {
			buffer += seprintf(buffer, last, " ??"); // OCR: WAS: , 0);
		} else {
			buffer += seprintf(buffer, last, " %.2X", *b);
		}
		b++;
	}
	return buffer + seprintf(buffer, last, "\n\n");
}

/* virtual */ char *CrashLogWindows::LogStacktrace(char *buffer, const char *last) const
{
	buffer += seprintf(buffer, last, "Stack trace:\n");
#ifdef _M_AMD64
	uint32 *b = (uint32*)ep->ContextRecord->Rsp;
#else
	uint32 *b = (uint32*)ep->ContextRecord->Esp;
#endif
	for (int j = 0; j != 24; j++) {
		for (int i = 0; i != 8; i++) {
			if (IsBadReadPtr(b, sizeof(uint32))) {
				buffer += seprintf(buffer, last, " ????????"); // OCR: WAS - , 0);
			} else {
				buffer += seprintf(buffer, last, " %.8X", *b);
			}
			b++;
		}
		buffer += seprintf(buffer, last, "\n");
	}
	return buffer + seprintf(buffer, last, "\n");
}

#if defined(_MSC_VER)
#pragma warning(disable:4091)
#include <dbghelp.h>
#pragma warning(default:4091)

char *CrashLogWindows::AppendDecodedStacktrace(char *buffer, const char *last) const
{
#define M(x) x "\0"
	static const char dbg_import[] =
		M("dbghelp.dll")
		M("SymInitialize")
		M("SymSetOptions")
		M("SymCleanup")
		M("StackWalk64")
		M("SymFunctionTableAccess64")
		M("SymGetModuleBase64")
		M("SymGetModuleInfo64")
		M("SymGetSymFromAddr64")
		M("SymGetLineFromAddr64")
		M("")
		;
#undef M

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
	} proc;

	buffer += seprintf(buffer, last, "\nDecoded stack trace:\n");

	/* Try to load the functions from the DLL, if that fails because of a too old dbghelp.dll, just skip it. */
	if (LoadLibraryList((Function*)&proc, dbg_import)) {
		/* Initialize symbol handler. */
		HANDLE hCur = GetCurrentProcess();
		proc.pSymInitialize(hCur, NULL, TRUE);
		/* Load symbols only when needed, fail silently on errors, demangle symbol names. */
		proc.pSymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_UNDNAME);

		/* Initialize starting stack frame from context record. */
		STACKFRAME64 frame;
		memset(&frame, 0, sizeof(frame));
#ifdef _M_AMD64
		frame.AddrPC.Offset = ep->ContextRecord->Rip;
		frame.AddrFrame.Offset = ep->ContextRecord->Rbp;
		frame.AddrStack.Offset = ep->ContextRecord->Rsp;
#else
		frame.AddrPC.Offset = ep->ContextRecord->Eip;
		frame.AddrFrame.Offset = ep->ContextRecord->Ebp;
		frame.AddrStack.Offset = ep->ContextRecord->Esp;
#endif
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Mode = AddrModeFlat;

		/* Copy context record as StackWalk64 may modify it. */
		CONTEXT ctx;
		memcpy(&ctx, ep->ContextRecord, sizeof(ctx));

		/* Allocate space for symbol info. */
		IMAGEHLP_SYMBOL64 *sym_info = (IMAGEHLP_SYMBOL64*)alloca(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYMBOL_LEN - 1);
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
				hCur, GetCurrentThread(), &frame, &ctx, NULL, proc.pSymFunctionTableAccess64, proc.pSymGetModuleBase64, NULL)) break;

			if (frame.AddrPC.Offset == frame.AddrReturn.Offset) {
				buffer += seprintf(buffer, last, " <infinite loop>\n");
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
			buffer += seprintf(buffer, last, "[%02d] %-20s " PRINTF_PTR, num, mod_name, frame.AddrPC.Offset);

			/* Get symbol name and line info if possible. */
			DWORD64 offset;
			if (proc.pSymGetSymFromAddr64(hCur, frame.AddrPC.Offset, &offset, sym_info)) {
				buffer += seprintf(buffer, last, " %s + %I64u", sym_info->Name, offset);

				DWORD line_offs;
				IMAGEHLP_LINE64 line;
				line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
				if (proc.pSymGetLineFromAddr64(hCur, frame.AddrPC.Offset, &line_offs, &line)) {
					buffer += seprintf(buffer, last, " (%s:%d)", line.FileName, line.LineNumber);
				}
			}
			buffer += seprintf(buffer, last, "\n");
		}

		proc.pSymCleanup(hCur);
	}

	return buffer + seprintf(buffer, last, "\n*** End of additional info ***\n");
}

/* virtual */ int CrashLogWindows::WriteCrashDump(char *filename, const char *filename_last) const
{
	int ret = 0;
	HMODULE dbghelp = LoadLibrary(_T("dbghelp.dll"));
	if (dbghelp != NULL) {
		typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE,
				MINIDUMP_TYPE,
				CONST PMINIDUMP_EXCEPTION_INFORMATION,
				CONST PMINIDUMP_USER_STREAM_INFORMATION,
				CONST PMINIDUMP_CALLBACK_INFORMATION);
		MiniDumpWriteDump_t funcMiniDumpWriteDump = (MiniDumpWriteDump_t)GetProcAddress(dbghelp, "MiniDumpWriteDump");
		if (funcMiniDumpWriteDump != NULL) {
			seprintf(filename, filename_last, "%scrash.dmp", _personal_dir);
			HANDLE file  = CreateFile(OTTD2FS(filename), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
			HANDLE proc  = GetCurrentProcess();
			DWORD procid = GetCurrentProcessId();
			MINIDUMP_EXCEPTION_INFORMATION mdei;
			MINIDUMP_USER_STREAM userstream;
			MINIDUMP_USER_STREAM_INFORMATION musi;

			userstream.Type        = LastReservedStream + 1;
			userstream.Buffer      = (void*)this->crashlog;
			userstream.BufferSize  = (ULONG)strlen(this->crashlog) + 1;

			musi.UserStreamCount   = 1;
			musi.UserStreamArray   = &userstream;

			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers  = ep;
			mdei.ClientPointers     = false;

			funcMiniDumpWriteDump(proc, procid, file, MiniDumpWithDataSegs, &mdei, &musi, NULL);
			ret = 1;
		} else {
			ret = -1;
		}
		FreeLibrary(dbghelp);
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
void *_safe_esp = NULL;

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep)
{
	if (CrashLogWindows::current != NULL) {
		CrashLog::AfterCrashLogCleanup();
		ExitProcess(2);
	}

	if (GamelogTestEmergency()) {
		static const TCHAR _emergency_crash[] =
			_T("A serious fault condition occurred in the game. The game will shut down.\n")
			_T("As you loaded an emergency savegame no crash information will be generated.\n");
		MessageBox(NULL, _emergency_crash, _T("Fatal Application Failure"), MB_ICONERROR);
		ExitProcess(3);
	}

	if (SaveloadCrashWithMissingNewGRFs()) {
		static const TCHAR _saveload_crash[] =
			_T("A serious fault condition occurred in the game. The game will shut down.\n")
			_T("As you loaded an savegame for which you do not have the required NewGRFs\n")
			_T("no crash information will be generated.\n");
		MessageBox(NULL, _saveload_crash, _T("Fatal Application Failure"), MB_ICONERROR);
		ExitProcess(3);
	}

	CrashLogWindows *log = new CrashLogWindows(ep);
	CrashLogWindows::current = log;
	char *buf = log->FillCrashLog(log->crashlog, lastof(log->crashlog));
	log->WriteCrashDump(log->crashdump_filename, lastof(log->crashdump_filename));
	log->AppendDecodedStacktrace(buf, lastof(log->crashlog));
	log->WriteCrashLog(log->crashlog, log->crashlog_filename, lastof(log->crashlog_filename));
	log->WriteScreenshot(log->screenshot_filename, lastof(log->screenshot_filename));

	/* Close any possible log files */
	CloseConsoleLogIfActive();

	if ((VideoDriver::GetInstance() == NULL || VideoDriver::GetInstance()->HasGUI()) && _safe_esp != NULL) {
#ifdef _M_AMD64
		ep->ContextRecord->Rip = (DWORD64)ShowCrashlogWindow;
		ep->ContextRecord->Rsp = (DWORD64)_safe_esp;
#else
		ep->ContextRecord->Eip = (DWORD)ShowCrashlogWindow;
		ep->ContextRecord->Esp = (DWORD)_safe_esp;
#endif
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	CrashLog::AfterCrashLogCleanup();
	return EXCEPTION_EXECUTE_HANDLER;
}

static void CDECL CustomAbort(int signal)
{
	RaiseException(0xE1212012, 0, 0, NULL);
}

/* static */ void CrashLog::InitialiseCrashLog()
{
#ifdef _M_AMD64
	CONTEXT ctx;
	RtlCaptureContext(&ctx);

	/* The stack pointer for AMD64 must always be 16-byte aligned inside a
	 * function. As we are simulating a function call with the safe ESP value,
	 * we need to subtract 8 for the imaginary return address otherwise stack
	 * alignment would be wrong in the called function. */
	_safe_esp = (void *)(ctx.Rsp - 8);
#else
#if defined(_MSC_VER)
	_asm {
		mov _safe_esp, esp
	}
#else
	asm("movl %esp, __safe_esp");
#endif
#endif

	/* SIGABRT is not an unhandled exception, so we need to intercept it. */
	signal(SIGABRT, CustomAbort);
#if defined(_MSC_VER)
	/* Don't show abort message as we will get the crashlog window anyway. */
	_set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
	SetUnhandledExceptionFilter(ExceptionHandler);
}

/* The crash log GUI */

static bool _expanded;

static const TCHAR _crash_desc[] =
	_T("A serious fault condition occurred in the game. The game will shut down.\n")
	_T("Please send the crash information and the crash.dmp file (if any) to the developers.\n")
	_T("This will greatly help debugging. The correct place to do this is https://github.com/OpenTTD/OpenTTD/issues. ")
	_T("The information contained in the report is displayed below.\n")
	_T("Press \"Emergency save\" to attempt saving the game. Generated file(s):\n")
	_T("%s");

static const TCHAR _save_succeeded[] =
	_T("Emergency save succeeded.\nIts location is '%s'.\n")
	_T("Be aware that critical parts of the internal game state may have become ")
	_T("corrupted. The saved game is not guaranteed to work.");

static const TCHAR * const _expand_texts[] = {_T("S&how report >>"), _T("&Hide report <<") };

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

/* When TCHAR is char, then _sntprintf becomes snprintf. When TCHAR is wchar it doesn't. Likewise for strcat. */
#undef snprintf
#undef strcat

static INT_PTR CALLBACK CrashDialogFunc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG: {
			/* We need to put the crash-log in a separate buffer because the default
			 * buffer in MB_TO_WIDE is not large enough (512 chars) */
			TCHAR crash_msgW[lengthof(CrashLogWindows::current->crashlog)];
			/* Convert unix -> dos newlines because the edit box only supports that properly :( */
			const char *unix_nl = CrashLogWindows::current->crashlog;
			char dos_nl[lengthof(CrashLogWindows::current->crashlog)];
			char *p = dos_nl;
			WChar c;
			while ((c = Utf8Consume(&unix_nl)) && p < lastof(dos_nl) - 4) { // 4 is max number of bytes per character
				if (c == '\n') p += Utf8Encode(p, '\r');
				p += Utf8Encode(p, c);
			}
			*p = '\0';

			/* Add path to crash.log and crash.dmp (if any) to the crash window text */
			size_t len = _tcslen(_crash_desc) + 2;
			len += _tcslen(OTTD2FS(CrashLogWindows::current->crashlog_filename)) + 2;
			len += _tcslen(OTTD2FS(CrashLogWindows::current->crashdump_filename)) + 2;
			len += _tcslen(OTTD2FS(CrashLogWindows::current->screenshot_filename)) + 1;

			TCHAR *text = AllocaM(TCHAR, len);
			_sntprintf(text, len, _crash_desc, OTTD2FS(CrashLogWindows::current->crashlog_filename));
			if (OTTD2FS(CrashLogWindows::current->crashdump_filename)[0] != _T('\0')) {
				_tcscat(text, _T("\n"));
				_tcscat(text, OTTD2FS(CrashLogWindows::current->crashdump_filename));
			}
			if (OTTD2FS(CrashLogWindows::current->screenshot_filename)[0] != _T('\0')) {
				_tcscat(text, _T("\n"));
				_tcscat(text, OTTD2FS(CrashLogWindows::current->screenshot_filename));
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
					char filename[MAX_PATH];
					if (CrashLogWindows::current->WriteSavegame(filename, lastof(filename))) {
						size_t len = _tcslen(_save_succeeded) + _tcslen(OTTD2FS(filename)) + 1;
						TCHAR *text = AllocaM(TCHAR, len);
						_sntprintf(text, len, _save_succeeded, OTTD2FS(filename));
						MessageBox(wnd, text, _T("Save successful"), MB_ICONINFORMATION);
					} else {
						MessageBox(wnd, _T("Save failed"), _T("Save failed"), MB_ICONINFORMATION);
					}
					break;
				case 15: // Expand window to show crash-message
					_expanded ^= 1;
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
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(100), NULL, CrashDialogFunc);
}
