/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file crashlog_win.cpp Implementation of a crashlogger for Windows */

#if defined(WIN32_EXCEPTION_TRACKER)

#include "../../stdafx.h"
#include "win32.h"
#include "../../core/alloc_func.hpp"
#include "../../string_func.h"
#include "../../gamelog.h"
#include "../../saveload/saveload.h"
#include "../../fileio_func.h"
#include "../../rev.h"
#include "../../strings_func.h"

#include <windows.h>
#include <dbghelp.h>

static const char *_exception_string = NULL;
void SetExceptionString(const char *s, ...)
{
	va_list va;
	char buf[512];

	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);

	_exception_string = strdup(buf);
}

static void *_safe_esp;
static char *_crash_msg;
static bool _expanded;
static bool _did_emerg_save;

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
			if (ReadFile(file, buffer, sizeof(buffer), &numread, NULL) == 0 || numread == 0)
				break;
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
	output += seprintf(output, last, " %-20s handle: %p size: %d crc: %.8X date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n",
		WIDE_TO_MB(buffer),
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

static char *PrintModuleList(char *output, const char *last)
{
	BOOL (WINAPI *EnumProcessModules)(HANDLE, HMODULE*, DWORD, LPDWORD);

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
				return output;
			}
		}
	}
	output = PrintModuleInfo(output, last, NULL);
	return output;
}

static const TCHAR _crash_desc[] =
	_T("A serious fault condition occured in the game. The game will shut down.\n")
	_T("Please send the crash information and the crash.dmp file (if any) to the developers.\n")
	_T("This will greatly help debugging. The correct place to do this is http://bugs.openttd.org. ")
	_T("The information contained in the report is displayed below.\n")
	_T("Press \"Emergency save\" to attempt saving the game.");

static const TCHAR _save_succeeded[] =
	_T("Emergency save succeeded.\n")
	_T("Be aware that critical parts of the internal game state may have become ")
	_T("corrupted. The saved game is not guaranteed to work.");

static const TCHAR _emergency_crash[] =
	_T("A serious fault condition occured in the game. The game will shut down.\n")
	_T("As you loaded an emergency savegame no crash information will be generated.\n");

static bool EmergencySave()
{
	GamelogStartAction(GLAT_EMERGENCY);
	GamelogEmergency();
	GamelogStopAction();
	SaveOrLoad("crash.sav", SL_SAVE, BASE_DIR);
	return true;
}

static const TCHAR * const _expand_texts[] = {_T("S&how report >>"), _T("&Hide report <<") };

static void SetWndSize(HWND wnd, int mode)
{
	RECT r, r2;

	GetWindowRect(wnd, &r);
	SetDlgItemText(wnd, 15, _expand_texts[mode == 1]);

	if (mode >= 0) {
		GetWindowRect(GetDlgItem(wnd, 11), &r2);
		int offs = r2.bottom - r2.top + 10;
		if (!mode) offs = -offs;
		SetWindowPos(wnd, HWND_TOPMOST, 0, 0,
			r.right - r.left, r.bottom - r.top + offs, SWP_NOMOVE | SWP_NOZORDER);
	} else {
		SetWindowPos(wnd, HWND_TOPMOST,
			(GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 2,
			0, 0, SWP_NOSIZE);
	}
}

static bool DoEmergencySave(HWND wnd)
{
	bool b = false;

	EnableWindow(GetDlgItem(wnd, 13), FALSE);
	_did_emerg_save = true;
	__try {
		b = EmergencySave();
	} __except (1) {}
	return b;
}

static INT_PTR CALLBACK CrashDialogFunc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG: {
#if defined(UNICODE)
			/* We need to put the crash-log in a seperate buffer because the default
			 * buffer in MB_TO_WIDE is not large enough (512 chars) */
			wchar_t crash_msgW[8096];
#endif
			SetDlgItemText(wnd, 10, _crash_desc);
			SetDlgItemText(wnd, 11, MB_TO_WIDE_BUFFER(_crash_msg, crash_msgW, lengthof(crash_msgW)));
			SendDlgItemMessage(wnd, 11, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);
			SetWndSize(wnd, -1);
		} return TRUE;
		case WM_COMMAND:
			switch (wParam) {
				case 12: // Close
					ExitProcess(0);
				case 13: // Emergency save
					if (DoEmergencySave(wnd)) {
						MessageBox(wnd, _save_succeeded, _T("Save successful"), MB_ICONINFORMATION);
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
		case WM_CLOSE: ExitProcess(0);
	}

	return FALSE;
}

static void Handler2()
{
	ShowCursor(TRUE);
	ShowWindow(GetActiveWindow(), FALSE);
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(100), NULL, CrashDialogFunc);
}

extern bool CloseConsoleLogIfActive();

static HANDLE _file_crash_log;

static void GamelogPrintCrashLogProc(const char *s)
{
	DWORD num_written;
	WriteFile(_file_crash_log, s, (DWORD)strlen(s), &num_written, NULL);
	WriteFile(_file_crash_log, "\r\n", (DWORD)strlen("\r\n"), &num_written, NULL);
}

/** Amount of output for the execption handler. */
static const int EXCEPTION_OUTPUT_SIZE = 8192;

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep)
{
	char *output;
	static bool had_exception = false;

	if (had_exception) ExitProcess(0);
	if (GamelogTestEmergency()) {
		MessageBox(NULL, _emergency_crash, _T("Fatal Application Failure"), MB_ICONERROR);
		ExitProcess(0);
	}
	had_exception = true;

	MakeCRCTable(AllocaM(uint32, 256));
	_crash_msg = output = (char*)LocalAlloc(LMEM_FIXED, EXCEPTION_OUTPUT_SIZE);
	const char *last = output + EXCEPTION_OUTPUT_SIZE - 1;

	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		output += seprintf(output, last,
			"*** OpenTTD Crash Report ***\r\n"
			"Date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n"
			"Build: %s (%d) built on %s\r\n",
			time.wYear,
			time.wMonth,
			time.wDay,
			time.wHour,
			time.wMinute,
			time.wSecond,
			_openttd_revision,
			_openttd_revision_modified,
			_openttd_build_date
		);
	}

	if (_exception_string)
		output += seprintf(output, last, "Reason: %s\r\n", _exception_string);

	output += seprintf(output, last, "Language: %s\r\n", _dynlang.curr_file);

#ifdef _M_AMD64
	output += seprintf(output, last, "Exception %.8X at %.16IX\r\n"
		"Registers:\r\n"
		"RAX: %.16llX RBX: %.16llX RCX: %.16llX RDX: %.16llX\r\n"
		"RSI: %.16llX RDI: %.16llX RBP: %.16llX RSP: %.16llX\r\n"
		"R8:  %.16llX R9:  %.16llX R10: %.16llX R11: %.16llX\r\n"
		"R12: %.16llX R13: %.16llX R14: %.16llX R15: %.16llX\r\n"
		"RIP: %.16llX EFLAGS: %.8X\r\n"
		"\r\nBytes at CS:RIP:\r\n",
		ep->ExceptionRecord->ExceptionCode,
		ep->ExceptionRecord->ExceptionAddress,
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
	output += seprintf(output, last, "Exception %.8X at %.8p\r\n"
		"Registers:\r\n"
		" EAX: %.8X EBX: %.8X ECX: %.8X EDX: %.8X\r\n"
		" ESI: %.8X EDI: %.8X EBP: %.8X ESP: %.8X\r\n"
		" EIP: %.8X EFLAGS: %.8X\r\n"
		"\r\nBytes at CS:EIP:\r\n",
		ep->ExceptionRecord->ExceptionCode,
		ep->ExceptionRecord->ExceptionAddress,
		ep->ContextRecord->Eax,
		ep->ContextRecord->Ebx,
		ep->ContextRecord->Ecx,
		ep->ContextRecord->Edx,
		ep->ContextRecord->Esi,
		ep->ContextRecord->Edi,
		ep->ContextRecord->Ebp,
		ep->ContextRecord->Esp,
		ep->ContextRecord->Eip,
		ep->ContextRecord->EFlags
	);
#endif

	{
#ifdef _M_AMD64
		byte *b = (byte*)ep->ContextRecord->Rip;
#else
		byte *b = (byte*)ep->ContextRecord->Eip;
#endif
		int i;
		for (i = 0; i != 24; i++) {
			if (IsBadReadPtr(b, 1)) {
				output += seprintf(output, last, " ??"); // OCR: WAS: , 0);
			} else {
				output += seprintf(output, last, " %.2X", *b);
			}
			b++;
		}
		output += seprintf(output, last,
			"\r\n"
			"\r\nStack trace: \r\n"
		);
	}

	{
		int i, j;
#ifdef _M_AMD64
		uint32 *b = (uint32*)ep->ContextRecord->Rsp;
#else
		uint32 *b = (uint32*)ep->ContextRecord->Esp;
#endif
		for (j = 0; j != 24; j++) {
			for (i = 0; i != 8; i++) {
				if (IsBadReadPtr(b, sizeof(uint32))) {
					output += seprintf(output, last, " ????????"); // OCR: WAS - , 0);
				} else {
					output += seprintf(output, last, " %.8X", *b);
				}
				b++;
			}
			output += seprintf(output, last, "\r\n");
		}
	}

	output += seprintf(output, last, "\r\nModule information:\r\n");
	output = PrintModuleList(output, last);

	{
		_OSVERSIONINFOA os;
		os.dwOSVersionInfoSize = sizeof(os);
		GetVersionExA(&os);
		output += seprintf(output, last, "\r\nSystem information:\r\n"
			" Windows version %d.%d %d %s\r\n\r\n",
			os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber, os.szCSDVersion);
	}

	_file_crash_log = CreateFile(_T("crash.log"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);

	if (_file_crash_log != INVALID_HANDLE_VALUE) {
		DWORD num_written;
		WriteFile(_file_crash_log, _crash_msg, output - _crash_msg, &num_written, NULL);
	}

#if !defined(_DEBUG)
	HMODULE dbghelp = LoadLibrary(_T("dbghelp.dll"));
	if (dbghelp != NULL) {
		typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE,
				MINIDUMP_TYPE,
				CONST PMINIDUMP_EXCEPTION_INFORMATION,
				CONST PMINIDUMP_USER_STREAM_INFORMATION,
				CONST PMINIDUMP_CALLBACK_INFORMATION);
		MiniDumpWriteDump_t funcMiniDumpWriteDump = (MiniDumpWriteDump_t)GetProcAddress(dbghelp, "MiniDumpWriteDump");
		if (funcMiniDumpWriteDump != NULL) {
			HANDLE file  = CreateFile(_T("crash.dmp"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
			HANDLE proc  = GetCurrentProcess();
			DWORD procid = GetCurrentProcessId();
			MINIDUMP_EXCEPTION_INFORMATION mdei;
			MINIDUMP_USER_STREAM userstream;
			MINIDUMP_USER_STREAM_INFORMATION musi;
			char msg[64];
			seprintf(msg, lastof(msg), "****** Built on %s. ******", _openttd_build_date);

			userstream.Type        = LastReservedStream + 1;
			userstream.Buffer      = msg;
			userstream.BufferSize  = sizeof(msg);

			musi.UserStreamCount   = 1;
			musi.UserStreamArray   = &userstream;

			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers  = ep;
			mdei.ClientPointers     = false;

			funcMiniDumpWriteDump(proc, procid, file, MiniDumpWithDataSegs, &mdei, &musi, NULL);
		}
		FreeLibrary(dbghelp);
	}
#endif

	if (_file_crash_log != INVALID_HANDLE_VALUE) {
		GamelogPrint(&GamelogPrintCrashLogProc);
		CloseHandle(_file_crash_log);
	}

	/* Close any possible log files */
	CloseConsoleLogIfActive();

	if (_safe_esp) {
#ifdef _M_AMD64
		ep->ContextRecord->Rip = (DWORD64)Handler2;
		ep->ContextRecord->Rsp = (DWORD64)_safe_esp;
#else
		ep->ContextRecord->Eip = (DWORD)Handler2;
		ep->ContextRecord->Esp = (DWORD)_safe_esp;
#endif
		return EXCEPTION_CONTINUE_EXECUTION;
	}


	return EXCEPTION_EXECUTE_HANDLER;
}

#ifdef _M_AMD64
extern "C" void *_get_safe_esp();
#endif

void Win32InitializeExceptions()
{
#ifdef _M_AMD64
	_safe_esp = _get_safe_esp();
#else
	_asm {
		mov _safe_esp, esp
	}
#endif

	SetUnhandledExceptionFilter(ExceptionHandler);
}
#endif /* WIN32_EXCEPTION_TRACKER */
