/* $Id$ */

/** @file win32.cpp Implementation of MS Windows system calls */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "saveload.h"
#include "gfx_func.h"
#include "textbuf_gui.h"
#include "fileio.h"
#include <windows.h>
#include <winnt.h>
#include <wininet.h>
#include <io.h>
#include <fcntl.h>
#include <shlobj.h> // SHGetFolderPath
#include "variables.h"
#include "win32.h"
#include "fios.h" // opendir/readdir/closedir
#include "fileio.h"
#include "core/alloc_func.hpp"
#include "functions.h"
#include "core/random_func.hpp"
#include "core/bitmath_func.hpp"
#include "string_func.h"
#include <ctype.h>
#include <tchar.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(_MSC_VER) && !defined(WINCE)
	#include <dbghelp.h>
#endif

static bool _has_console;

#if defined(__MINGW32__)
	#include <stdint.h>
#endif

static bool cursor_visible = true;

bool MyShowCursor(bool show)
{
	if (cursor_visible == show) return show;

	cursor_visible = show;
	ShowCursor(show);

	return !show;
}

/** Helper function needed by dynamically loading libraries
 * XXX: Hurray for MS only having an ANSI GetProcAddress function
 * on normal windows and no Wide version except for in Windows Mobile/CE */
bool LoadLibraryList(Function proc[], const char *dll)
{
	while (*dll != '\0') {
		HMODULE lib;
		lib = LoadLibrary(MB_TO_WIDE(dll));

		if (lib == NULL) return false;
		for (;;) {
			FARPROC p;

			while (*dll++ != '\0');
			if (*dll == '\0') break;
#if defined(WINCE)
			p = GetProcAddress(lib, MB_TO_WIDE(dll));
#else
			p = GetProcAddress(lib, dll);
#endif
			if (p == NULL) return false;
			*proc++ = (Function)p;
		}
		dll++;
	}
	return true;
}

#ifdef _MSC_VER
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
#endif

void ShowOSErrorBox(const char *buf)
{
	MyShowCursor(true);
	MessageBox(GetActiveWindow(), MB_TO_WIDE(buf), _T("Error!"), MB_ICONSTOP);

/* if exception tracker is enabled, we crash here to let the exception handler handle it. */
#if defined(WIN32_EXCEPTION_TRACKER) && !defined(_DEBUG)
	if (*buf == '!') {
		_exception_string = buf;
		*(byte*)0 = 0;
	}
#endif
}

#if defined(_MSC_VER) && !defined(WINCE)

static void *_safe_esp;
static char *_crash_msg;
static bool _expanded;
static bool _did_emerg_save;
static int _ident;

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
	memset(dfi, 0, sizeof(dfi));

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


static char *PrintModuleInfo(char *output, HMODULE mod)
{
	TCHAR buffer[MAX_PATH];
	DebugFileInfo dfi;

	GetModuleFileName(mod, buffer, MAX_PATH);
	GetFileInfo(&dfi, buffer);
	output += sprintf(output, " %-20s handle: %p size: %d crc: %.8X date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n",
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

static char *PrintModuleList(char *output)
{
	BOOL (WINAPI *EnumProcessModules)(HANDLE, HMODULE*, DWORD, LPDWORD);

	if (LoadLibraryList((Function*)&EnumProcessModules, "psapi.dll\0EnumProcessModules\0\0")) {
		HMODULE modules[100];
		DWORD needed;
		BOOL res;
		int count, i;

		HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
		if (proc != NULL) {
			res = EnumProcessModules(proc, modules, sizeof(modules), &needed);
			CloseHandle(proc);
			if (res) {
				count = min(needed / sizeof(HMODULE), lengthof(modules));

				for (i = 0; i != count; i++) output = PrintModuleInfo(output, modules[i]);
				return output;
			}
		}
	}
	output = PrintModuleInfo(output, NULL);
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

static bool EmergencySave()
{
	SaveOrLoad("crash.sav", SL_SAVE, BASE_DIR);
	return true;
}

/* Disable the crash-save submit code as it's not used */
#if 0

struct WinInetProcs {
	HINTERNET (WINAPI *InternetOpen)(LPCTSTR, DWORD, LPCTSTR, LPCTSTR, DWORD);
	HINTERNET (WINAPI *InternetConnect)(HINTERNET, LPCTSTR, INTERNET_PORT, LPCTSTR, LPCTSTR, DWORD, DWORD, DWORD);
	HINTERNET (WINAPI *HttpOpenRequest)(HINTERNET, LPCTSTR, LPCTSTR, LPCTSTR, LPCTSTR, LPCTSTR *, DWORD, DWORD);
	BOOL (WINAPI *HttpSendRequest)(HINTERNET, LPCTSTR, DWORD, LPVOID, DWORD);
	BOOL (WINAPI *InternetCloseHandle)(HINTERNET);
	BOOL (WINAPI *HttpQueryInfo)(HINTERNET, DWORD, LPVOID, LPDWORD, LPDWORD);
};

#define M(x) x "\0"
#if defined(UNICODE)
# define W(x) x "W"
#else
# define W(x) x "A"
#endif
static const char wininet_files[] =
	M("wininet.dll")
	M(W("InternetOpen"))
	M(W("InternetConnect"))
	M(W("HttpOpenRequest"))
	M(W("HttpSendRequest"))
	M("InternetCloseHandle")
	M(W("HttpQueryInfo"))
	M("");
#undef W
#undef M

static WinInetProcs _wininet;

static const TCHAR *SubmitCrashReport(HWND wnd, void *msg, size_t msglen, const TCHAR *arg)
{
	HINTERNET inet, conn, http;
	const TCHAR *err = NULL;
	DWORD code, len;
	static TCHAR buf[100];
	TCHAR buff[100];

	if (_wininet.InternetOpen == NULL && !LoadLibraryList((Function*)&_wininet, wininet_files)) return _T("can't load wininet.dll");

	inet = _wininet.InternetOpen(_T("OTTD"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if (inet == NULL) { err = _T("internetopen failed"); goto error1; }

	conn = _wininet.InternetConnect(inet, _T("www.openttd.org"), INTERNET_DEFAULT_HTTP_PORT, _T(""), _T(""), INTERNET_SERVICE_HTTP, 0, 0);
	if (conn == NULL) { err = _T("internetconnect failed"); goto error2; }

	_sntprintf(buff, lengthof(buff), _T("/crash.php?file=%s&ident=%d"), arg, _ident);

	http = _wininet.HttpOpenRequest(conn, _T("POST"), buff, NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE , 0);
	if (http == NULL) { err = _T("httpopenrequest failed"); goto error3; }

	if (!_wininet.HttpSendRequest(http, _T("Content-type: application/binary"), -1, msg, (DWORD)msglen)) { err = _T("httpsendrequest failed"); goto error4; }

	len = sizeof(code);
	if (!_wininet.HttpQueryInfo(http, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &len, 0)) { err = _T("httpqueryinfo failed"); goto error4; }

	if (code != 200) {
		int l = _sntprintf(buf, lengthof(buf), _T("Server said: %d "), code);
		len = sizeof(buf) - l;
		_wininet.HttpQueryInfo(http, HTTP_QUERY_STATUS_TEXT, buf + l, &len, 0);
		err = buf;
	}

error4:
	_wininet.InternetCloseHandle(http);
error3:
	_wininet.InternetCloseHandle(conn);
error2:
	_wininet.InternetCloseHandle(inet);
error1:
	return err;
}

static void SubmitFile(HWND wnd, const TCHAR *file)
{
	HANDLE h;
	unsigned long size;
	unsigned long read;
	void *mem;

	h = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (h == NULL) return;

	size = GetFileSize(h, NULL);
	if (size > 500000) goto error1;

	mem = MallocT<byte>(size);
	if (mem == NULL) goto error1;

	if (!ReadFile(h, mem, size, &read, NULL) || read != size) goto error2;

	SubmitCrashReport(wnd, mem, size, file);

error2:
	free(mem);
error1:
	CloseHandle(h);
}

#endif /* Disabled crash-submit procedures */

static const TCHAR * const _expand_texts[] = {_T("S&how report >>"), _T("&Hide report <<") };

static void SetWndSize(HWND wnd, int mode)
{
	RECT r, r2;
	int offs;

	GetWindowRect(wnd, &r);

	SetDlgItemText(wnd, 15, _expand_texts[mode == 1]);

	if (mode >= 0) {
		GetWindowRect(GetDlgItem(wnd, 11), &r2);
		offs = r2.bottom - r2.top + 10;
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
				case 12: /* Close */
					ExitProcess(0);
				case 13: /* Emergency save */
					if (DoEmergencySave(wnd)) {
						MessageBox(wnd, _save_succeeded, _T("Save successful"), MB_ICONINFORMATION);
					} else {
						MessageBox(wnd, _T("Save failed"), _T("Save failed"), MB_ICONINFORMATION);
					}
					break;
/* Disable the crash-save submit code as it's not used */
#if 0
				case 14: { /* Submit crash report */
					const TCHAR *s;

					SetCursor(LoadCursor(NULL, IDC_WAIT));

					s = SubmitCrashReport(wnd, _crash_msg, strlen(_crash_msg), _T(""));
					if (s != NULL) {
						MessageBox(wnd, s, _T("Error"), MB_ICONSTOP);
						break;
					}

					// try to submit emergency savegame
					if (_did_emerg_save || DoEmergencySave(wnd)) SubmitFile(wnd, _T("crash.sav"));

					// try to submit the autosaved game
					if (_opt.autosave) {
						TCHAR buf[40];
						_sntprintf(buf, lengthof(buf), _T("autosave%d.sav"), (_autosave_ctr - 1) & 3);
						SubmitFile(wnd, buf);
					}
					EnableWindow(GetDlgItem(wnd, 14), FALSE);
					SetCursor(LoadCursor(NULL, IDC_ARROW));
					MessageBox(wnd, _T("Crash report submitted. Thank you."), _T("Crash Report"), MB_ICONINFORMATION);
				} break;
#endif /* Disabled crash-submit procedures */
				case 15: /* Expand window to show crash-message */
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

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep)
{
	extern const char _openttd_revision[];
	char *output;
	static bool had_exception = false;

	if (had_exception) ExitProcess(0);
	had_exception = true;

	_ident = GetTickCount(); // something pretty unique

	MakeCRCTable((uint32*)alloca(256 * sizeof(uint32)));
	_crash_msg = output = (char*)LocalAlloc(LMEM_FIXED, 8192);

	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		output += sprintf(output,
			"*** OpenTTD Crash Report ***\r\n"
			"Date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n"
			"Build: %s built on " __DATE__ " " __TIME__ "\r\n",
			time.wYear,
			time.wMonth,
			time.wDay,
			time.wHour,
			time.wMinute,
			time.wSecond,
			_openttd_revision
		);
	}

	if (_exception_string)
		output += sprintf(output, "Reason: %s\r\n", _exception_string);

#ifdef _M_AMD64
	output += sprintf(output, "Exception %.8X at %.16IX\r\n"
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
	output += sprintf(output, "Exception %.8X at %.8X\r\n"
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
				output += sprintf(output, " ??"); // OCR: WAS: , 0);
			} else {
				output += sprintf(output, " %.2X", *b);
			}
			b++;
		}
		output += sprintf(output,
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
					output += sprintf(output, " ????????"); //OCR: WAS - , 0);
				} else {
					output += sprintf(output, " %.8X", *b);
				}
				b++;
			}
			output += sprintf(output, "\r\n");
		}
	}

	output += sprintf(output, "\r\nModule information:\r\n");
	output = PrintModuleList(output);

	{
		OSVERSIONINFO os;
		os.dwOSVersionInfoSize = sizeof(os);
		GetVersionEx(&os);
		output += sprintf(output, "\r\nSystem information:\r\n"
			" Windows version %d.%d %d %s\r\n",
			os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber, os.szCSDVersion);
	}

	{
		HANDLE file = CreateFile(_T("crash.log"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
		DWORD num_written;
		if (file != INVALID_HANDLE_VALUE) {
			WriteFile(file, _crash_msg, output - _crash_msg, &num_written, NULL);
			CloseHandle(file);
		}
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
			char msg[] = "****** Built on " __DATE__ " " __TIME__ ". ******";

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
extern "C" void *_get_save_esp();
#endif

static void Win32InitializeExceptions()
{
#ifdef _M_AMD64
	_safe_esp = _get_save_esp();
#else
	_asm {
		mov _safe_esp, esp
	}
#endif

	SetUnhandledExceptionFilter(ExceptionHandler);
}
#endif /* _MSC_VER */

/* Code below for windows version of opendir/readdir/closedir copied and
 * modified from Jan Wassenberg's GPL implementation posted over at
 * http://www.gamedev.net/community/forums/topic.asp?topic_id=364584&whichpage=1&#2398903 */

/* suballocator - satisfies most requests with a reusable static instance.
 * this avoids hundreds of alloc/free which would fragment the heap.
 * To guarantee concurrency, we fall back to malloc if the instance is
 * already in use (it's important to avoid suprises since this is such a
 * low-level routine). */
static DIR _global_dir;
static LONG _global_dir_is_in_use = false;

static inline DIR *dir_calloc()
{
	DIR *d;

	if (InterlockedExchange(&_global_dir_is_in_use, true) == (LONG)true) {
		d = CallocT<DIR>(1);
	} else {
		d = &_global_dir;
		memset(d, 0, sizeof(*d));
	}
	return d;
}

static inline void dir_free(DIR *d)
{
	if (d == &_global_dir) {
		_global_dir_is_in_use = (LONG)false;
	} else {
		free(d);
	}
}

DIR *opendir(const TCHAR *path)
{
	DIR *d;
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS); // disable 'no-disk' message box
	DWORD fa = GetFileAttributes(path);

	if ((fa != INVALID_FILE_ATTRIBUTES) && (fa & FILE_ATTRIBUTE_DIRECTORY)) {
		d = dir_calloc();
		if (d != NULL) {
			TCHAR search_path[MAX_PATH];
			bool slash = path[_tcslen(path) - 1] == '\\';

			/* build search path for FindFirstFile, try not to append additional slashes
			 * as it throws Win9x off its groove for root directories */
			_sntprintf(search_path, lengthof(search_path), _T("%s%s*"), path, slash ? _T("") : _T("\\"));
			*lastof(search_path) = '\0';
			d->hFind = FindFirstFile(search_path, &d->fd);

			if (d->hFind != INVALID_HANDLE_VALUE ||
					GetLastError() == ERROR_NO_MORE_FILES) { // the directory is empty
				d->ent.dir = d;
				d->at_first_entry = true;
			} else {
				dir_free(d);
				d = NULL;
			}
		} else {
			errno = ENOMEM;
		}
	} else {
		/* path not found or not a directory */
		d = NULL;
		errno = ENOENT;
	}

	SetErrorMode(sem); // restore previous setting
	return d;
}

struct dirent *readdir(DIR *d)
{
	DWORD prev_err = GetLastError(); // avoid polluting last error

	if (d->at_first_entry) {
		/* the directory was empty when opened */
		if (d->hFind == INVALID_HANDLE_VALUE) return NULL;
		d->at_first_entry = false;
	} else if (!FindNextFile(d->hFind, &d->fd)) { // determine cause and bail
		if (GetLastError() == ERROR_NO_MORE_FILES) SetLastError(prev_err);
		return NULL;
	}

	/* This entry has passed all checks; return information about it.
	 * (note: d_name is a pointer; see struct dirent definition) */
	d->ent.d_name = d->fd.cFileName;
	return &d->ent;
}

int closedir(DIR *d)
{
	FindClose(d->hFind);
	dir_free(d);
	return 0;
}

bool FiosIsRoot(const char *file)
{
	return file[3] == '\0'; // C:\...
}

void FiosGetDrives()
{
#if defined(WINCE)
	/* WinCE only knows one drive: / */
	FiosItem *fios = FiosAlloc();
	fios->type = FIOS_TYPE_DRIVE;
	fios->mtime = 0;
	snprintf(fios->name, lengthof(fios->name), PATHSEP "");
	ttd_strlcpy(fios->title, fios->name, lengthof(fios->title));
#else
	TCHAR drives[256];
	const TCHAR *s;

	GetLogicalDriveStrings(sizeof(drives), drives);
	for (s = drives; *s != '\0';) {
		FiosItem *fios = FiosAlloc();
		fios->type = FIOS_TYPE_DRIVE;
		fios->mtime = 0;
		snprintf(fios->name, lengthof(fios->name),  "%c:", s[0] & 0xFF);
		ttd_strlcpy(fios->title, fios->name, lengthof(fios->title));
		while (*s++ != '\0');
	}
#endif
}

bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb)
{
	/* hectonanoseconds between Windows and POSIX epoch */
	static const int64 posix_epoch_hns = 0x019DB1DED53E8000LL;
	const WIN32_FIND_DATA *fd = &ent->dir->fd;

	sb->st_size  = ((uint64) fd->nFileSizeHigh << 32) + fd->nFileSizeLow;
	/* UTC FILETIME to seconds-since-1970 UTC
	 * we just have to subtract POSIX epoch and scale down to units of seconds.
	 * http://www.gamedev.net/community/forums/topic.asp?topic_id=294070&whichpage=1&#1860504
	 * XXX - not entirely correct, since filetimes on FAT aren't UTC but local,
	 * this won't entirely be correct, but we use the time only for comparsion. */
	sb->st_mtime = (time_t)((*(uint64*)&fd->ftLastWriteTime - posix_epoch_hns) / 1E7);
	sb->st_mode  = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)? S_IFDIR : S_IFREG;

	return true;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return (ent->dir->fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

bool FiosGetDiskFreeSpace(const char *path, uint32 *tot)
{
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS);  // disable 'no-disk' message box
	bool retval = false;
	TCHAR root[4];
	DWORD spc, bps, nfc, tnc;

	_sntprintf(root, lengthof(root), _T("%c:") _T(PATHSEP), path[0]);
	if (tot != NULL && GetDiskFreeSpace(root, &spc, &bps, &nfc, &tnc)) {
		*tot = ((spc * bps) * (uint64)nfc) >> 20;
		retval = true;
	}

	SetErrorMode(sem); // reset previous setting
	return retval;
}

static int ParseCommandLine(char *line, char **argv, int max_argc)
{
	int n = 0;

	do {
		/* skip whitespace */
		while (*line == ' ' || *line == '\t') line++;

		/* end? */
		if (*line == '\0') break;

		/* special handling when quoted */
		if (*line == '"') {
			argv[n++] = ++line;
			while (*line != '"') {
				if (*line == '\0') return n;
				line++;
			}
		} else {
			argv[n++] = line;
			while (*line != ' ' && *line != '\t') {
				if (*line == '\0') return n;
				line++;
			}
		}
		*line++ = '\0';
	} while (n != max_argc);

	return n;
}

void CreateConsole()
{
#if defined(WINCE)
	/* WinCE doesn't support console stuff */
#else
	HANDLE hand;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	if (_has_console) return;
	_has_console = true;

	AllocConsole();

	hand = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hand, &coninfo);
	coninfo.dwSize.Y = 500;
	SetConsoleScreenBufferSize(hand, coninfo.dwSize);

	/* redirect unbuffered STDIN, STDOUT, STDERR to the console */
#if !defined(__CYGWIN__)
	*stdout = *_fdopen( _open_osfhandle((intptr_t)hand, _O_TEXT), "w" );
	*stdin = *_fdopen(_open_osfhandle((intptr_t)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT), "r" );
	*stderr = *_fdopen(_open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT), "w" );
#else
	/* open_osfhandle is not in cygwin */
	*stdout = *fdopen(1, "w" );
	*stdin = *fdopen(0, "r" );
	*stderr = *fdopen(2, "w" );
#endif

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
#endif
}

void ShowInfo(const char *str)
{
	if (_has_console) {
		fprintf(stderr, "%s\n", str);
	} else {
		bool old;
#if defined(UNICODE)
			/* We need to put the text in a seperate buffer because the default
			 * buffer in MB_TO_WIDE might not be large enough (512 chars) */
			wchar_t help_msgW[4096];
#endif
		ReleaseCapture();
		_left_button_clicked =_left_button_down = false;

		old = MyShowCursor(true);
		if (MessageBox(GetActiveWindow(), MB_TO_WIDE_BUFFER(str, help_msgW, lengthof(help_msgW)), _T("OpenTTD"), MB_ICONINFORMATION | MB_OKCANCEL) == IDCANCEL) {
			CreateConsole();
		}
		MyShowCursor(old);
	}
}

#ifdef __MINGW32__
	/* _set_error_mode() constants&function (do not exist in mingw headers) */
	#define _OUT_TO_DEFAULT      0
	#define _OUT_TO_STDERR       1
	#define _OUT_TO_MSGBOX       2
	#define _REPORT_ERRMODE      3
	int _set_error_mode(int);
#endif

#if defined(WINCE)
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#else
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif
{
	int argc;
	char *argv[64]; // max 64 command line arguments
	char *cmdline;

#if !defined(UNICODE)
	_codepage = GetACP(); // get system codepage as some kind of a default
#endif /* UNICODE */

#if defined(UNICODE)

#if !defined(WINCE)
	/* Check if a win9x user started the win32 version */
	if (HasBit(GetVersion(), 31)) error("This version of OpenTTD doesn't run on windows 95/98/ME.\nPlease download the win9x binary and try again.");
#endif

	/* For UNICODE we need to convert the commandline to char* _AND_
	 * save it because argv[] points into this buffer and thus needs to
	 * be available between subsequent calls to FS2OTTD() */
	char cmdlinebuf[MAX_PATH];
#endif /* UNICODE */

	cmdline = WIDE_TO_MB_BUFFER(GetCommandLine(), cmdlinebuf, lengthof(cmdlinebuf));

#if defined(_DEBUG)
	CreateConsole();
#endif

#if !defined(WINCE)
	_set_error_mode(_OUT_TO_MSGBOX); // force assertion output to messagebox
#endif

	/* setup random seed to something quite random */
	SetRandomSeed(GetTickCount());

	argc = ParseCommandLine(cmdline, argv, lengthof(argv));

#if defined(WIN32_EXCEPTION_TRACKER)
	Win32InitializeExceptions();
#endif

#if defined(WIN32_EXCEPTION_TRACKER_DEBUG)
	_try {
		LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep);
#endif
		ttd_main(argc, argv);

#if defined(WIN32_EXCEPTION_TRACKER_DEBUG)
	} _except (ExceptionHandler(_exception_info())) {}
#endif

	return 0;
}

#if defined(WINCE)
void GetCurrentDirectoryW(int length, wchar_t *path)
{
	/* Get the name of this module */
	GetModuleFileName(NULL, path, length);

	/* Remove the executable name, this we call CurrentDir */
	wchar_t *pDest = wcsrchr(path, '\\');
	if (pDest != NULL) {
		int result = pDest - path + 1;
		path[result] = '\0';
	}
}
#endif

char *getcwd(char *buf, size_t size)
{
#if defined(WINCE)
	TCHAR path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);
	convert_from_fs(path, buf, size);
	/* GetModuleFileName returns dir with file, so remove everything behind latest '\\' */
	char *p = strrchr(buf, '\\');
	if (p != NULL) *p = '\0';
#elif defined(UNICODE)
	TCHAR path[MAX_PATH];
	GetCurrentDirectory(MAX_PATH - 1, path);
	convert_from_fs(path, buf, size);
#else
	GetCurrentDirectory(size, buf);
#endif
	return buf;
}


void DetermineBasePaths(const char *exe)
{
	extern void ScanForTarFiles();
	char tmp[MAX_PATH];
	TCHAR path[MAX_PATH];
#ifdef WITH_PERSONAL_DIR
	SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path);
	strncpy(tmp, WIDE_TO_MB_BUFFER(path, tmp, lengthof(tmp)), lengthof(tmp));
	AppendPathSeparator(tmp, MAX_PATH);
	ttd_strlcat(tmp, PERSONAL_DIR, MAX_PATH);
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_PERSONAL_DIR] = strdup(tmp);

	SHGetFolderPath(NULL, CSIDL_COMMON_DOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path);
	strncpy(tmp, WIDE_TO_MB_BUFFER(path, tmp, lengthof(tmp)), lengthof(tmp));
	AppendPathSeparator(tmp, MAX_PATH);
	ttd_strlcat(tmp, PERSONAL_DIR, MAX_PATH);
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_SHARED_DIR] = strdup(tmp);
#else
	_searchpaths[SP_PERSONAL_DIR] = NULL;
	_searchpaths[SP_SHARED_DIR]   = NULL;
#endif

	/* Get the path to working directory of OpenTTD */
	getcwd(tmp, lengthof(tmp));
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_WORKING_DIR] = strdup(tmp);

	if (!GetModuleFileName(NULL, path, lengthof(path))) {
		DEBUG(misc, 0, "GetModuleFileName failed (%d)\n", GetLastError());
		_searchpaths[SP_BINARY_DIR] = NULL;
	} else {
		TCHAR exec_dir[MAX_PATH];
		_tcsncpy(path, MB_TO_WIDE_BUFFER(exe, path, lengthof(path)), lengthof(path));
		if (!GetFullPathName(path, lengthof(exec_dir), exec_dir, NULL)) {
			DEBUG(misc, 0, "GetFullPathName failed (%d)\n", GetLastError());
			_searchpaths[SP_BINARY_DIR] = NULL;
		} else {
			strncpy(tmp, WIDE_TO_MB_BUFFER(exec_dir, tmp, lengthof(tmp)), lengthof(tmp));
			char *s = strrchr(tmp, PATHSEPCHAR);
			*(s + 1) = '\0';
			_searchpaths[SP_BINARY_DIR] = strdup(tmp);
		}
	}

	_searchpaths[SP_INSTALLATION_DIR]       = NULL;
	_searchpaths[SP_APPLICATION_BUNDLE_DIR] = NULL;

	ScanForTarFiles();
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @param tb Textbuf type to be changed
 * @return true on successful change of Textbuf, or false otherwise
 */
bool InsertTextBufferClipboard(Textbuf *tb)
{
	HGLOBAL cbuf;
	char utf8_buf[512];
	const char *ptr;

	WChar c;
	uint16 width, length;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_UNICODETEXT);

		ptr = (const char*)GlobalLock(cbuf);
		const char *ret = convert_from_fs((wchar_t*)ptr, utf8_buf, lengthof(utf8_buf));
		GlobalUnlock(cbuf);
		CloseClipboard();

		if (*ret == '\0') return false;
#if !defined(UNICODE)
	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_TEXT);

		ptr = (const char*)GlobalLock(cbuf);
		ttd_strlcpy(utf8_buf, FS2OTTD(ptr), lengthof(utf8_buf));

		GlobalUnlock(cbuf);
		CloseClipboard();
#endif /* UNICODE */
	} else {
		return false;
	}

	width = length = 0;

	for (ptr = utf8_buf; (c = Utf8Consume(&ptr)) != '\0';) {
		if (!IsPrintable(c)) break;

		size_t len = Utf8CharLen(c);
		if (tb->length + length >= tb->maxlength - (uint16)len) break;

		byte charwidth = GetCharacterWidth(FS_NORMAL, c);
		if (tb->maxwidth != 0 && width + tb->width + charwidth > tb->maxwidth) break;

		width += charwidth;
		length += len;
	}

	if (length == 0) return false;

	memmove(tb->buf + tb->caretpos + length, tb->buf + tb->caretpos, tb->length - tb->caretpos);
	memcpy(tb->buf + tb->caretpos, utf8_buf, length);
	tb->width += width;
	tb->caretxoffs += width;

	tb->length += length;
	tb->caretpos += length;
	assert(tb->length < tb->maxlength);
	tb->buf[tb->length] = '\0'; // terminating zero

	return true;
}


void CSleep(int milliseconds)
{
	Sleep(milliseconds);
}


/** Utility function to get the current timestamp in milliseconds
 * Useful for profiling */
int64 GetTS()
{
	static double freq;
	__int64 value;
	if (!freq) {
		QueryPerformanceFrequency((LARGE_INTEGER*)&value);
		freq = (double)1000000 / value;
	}
	QueryPerformanceCounter((LARGE_INTEGER*)&value);
	return (__int64)(value * freq);
}


/**
 * Convert to OpenTTD's encoding from that of the local environment.
 * When the project is built in UNICODE, the system codepage is irrelevant and
 * the input string is wide. In ANSI mode, the string is in the
 * local codepage which we'll convert to wide-char, and then to UTF-8.
 * OpenTTD internal encoding is UTF8.
 * The returned value's contents can only be guaranteed until the next call to
 * this function. So if the value is needed for anything else, use convert_from_fs
 * @param name pointer to a valid string that will be converted (local, or wide)
 * @return pointer to the converted string; if failed string is of zero-length
 * @see the current code-page comes from video\win32_v.cpp, event-notification
 * WM_INPUTLANGCHANGE */
const char *FS2OTTD(const TCHAR *name)
{
	static char utf8_buf[512];
#if defined(UNICODE)
	return convert_from_fs(name, utf8_buf, lengthof(utf8_buf));
#else
	char *s = utf8_buf;

	for (; *name != '\0'; name++) {
		wchar_t w;
		int len = MultiByteToWideChar(_codepage, 0, name, 1, &w, 1);
		if (len != 1) {
			DEBUG(misc, 0, "[utf8] M2W error converting '%c'. Errno %d", *name, GetLastError());
			continue;
		}

		if (s + Utf8CharLen(w) >= lastof(utf8_buf)) break;
		s += Utf8Encode(s, w);
	}

	*s = '\0';
	return utf8_buf;
#endif /* UNICODE */
}

/**
 * Convert from OpenTTD's encoding to that of the local environment.
 * When the project is built in UNICODE the system codepage is irrelevant and
 * the converted string is wide. In ANSI mode, the UTF8 string is converted
 * to multi-byte.
 * OpenTTD internal encoding is UTF8.
 * The returned value's contents can only be guaranteed until the next call to
 * this function. So if the value is needed for anything else, use convert_from_fs
 * @param name pointer to a valid string that will be converted (UTF8)
 * @return pointer to the converted string; if failed string is of zero-length
 * @see the current code-page comes from video\win32_v.cpp, event-notification
 * WM_INPUTLANGCHANGE */
const TCHAR *OTTD2FS(const char *name)
{
	static TCHAR system_buf[512];
#if defined(UNICODE)
	return convert_to_fs(name, system_buf, lengthof(system_buf));
#else
	char *s = system_buf;

	for (WChar c; (c = Utf8Consume(&name)) != '\0';) {
		if (s >= lastof(system_buf)) break;

		char mb;
		int len = WideCharToMultiByte(_codepage, 0, (wchar_t*)&c, 1, &mb, 1, NULL, NULL);
		if (len != 1) {
			DEBUG(misc, 0, "[utf8] W2M error converting '0x%X'. Errno %d", c, GetLastError());
			continue;
		}

		*s++ = mb;
	}

	*s = '\0';
	return system_buf;
#endif /* UNICODE */
}


/** Convert to OpenTTD's encoding from that of the environment in
 * UNICODE. OpenTTD encoding is UTF8, local is wide
 * @param name pointer to a valid string that will be converted
 * @param utf8_buf pointer to a valid buffer that will receive the converted string
 * @param buflen length in characters of the receiving buffer
 * @return pointer to utf8_buf. If conversion fails the string is of zero-length */
char *convert_from_fs(const wchar_t *name, char *utf8_buf, size_t buflen)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8_buf, buflen, NULL, NULL);
	if (len == 0) {
		DEBUG(misc, 0, "[utf8] W2M error converting wide-string. Errno %d", GetLastError());
		utf8_buf[0] = '\0';
	}

	return utf8_buf;
}


/** Convert from OpenTTD's encoding to that of the environment in
 * UNICODE. OpenTTD encoding is UTF8, local is wide
 * @param name pointer to a valid string that will be converted
 * @param utf16_buf pointer to a valid wide-char buffer that will receive the
 * converted string
 * @param buflen length in wide characters of the receiving buffer
 * @return pointer to utf16_buf. If conversion fails the string is of zero-length */
wchar_t *convert_to_fs(const char *name, wchar_t *utf16_buf, size_t buflen)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, utf16_buf, buflen);
	if (len == 0) {
		DEBUG(misc, 0, "[utf8] M2W error converting '%s'. Errno %d", name, GetLastError());
		utf16_buf[0] = '\0';
	}

	return utf16_buf;
}

/** Our very own SHGetFolderPath function for support of windows operating
 * systems that don't have this function (eg Win9x, etc.). We try using the
 * native function, and if that doesn't exist we will try a more crude approach
 * of environment variables and hope for the best */
HRESULT OTTDSHGetFolderPath(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath)
{
	static HRESULT (WINAPI *SHGetFolderPath)(HWND, int, HANDLE, DWORD, LPTSTR) = NULL;
	static bool first_time = true;

	/* We only try to load the library one time; if it fails, it fails */
	if (first_time) {
#if defined(UNICODE)
# define W(x) x "W"
#else
# define W(x) x "A"
#endif
		if (!LoadLibraryList((Function*)&SHGetFolderPath, "SHFolder.dll\0" W("SHGetFolderPath") "\0\0")) {
			DEBUG(misc, 0, "Unable to load " W("SHGetFolderPath") "from SHFolder.dll");
		}
#undef W
		first_time = false;
	}

	if (SHGetFolderPath != NULL) return SHGetFolderPath(hwnd, csidl, hToken, dwFlags, pszPath);

	/* SHGetFolderPath doesn't exist, try a more conservative approach,
	 * eg environment variables. This is only included for legacy modes
	 * MSDN says: that 'pszPath' is a "Pointer to a null-terminated string of
	 * length MAX_PATH which will receive the path" so let's assume that
	 * Windows 95 with Internet Explorer 5.0, Windows 98 with Internet Explorer 5.0,
	 * Windows 98 Second Edition (SE), Windows NT 4.0 with Internet Explorer 5.0,
	 * Windows NT 4.0 with Service Pack 4 (SP4) */
	{
		DWORD ret;
		switch (csidl) {
			case CSIDL_FONTS: /* Get the system font path, eg %WINDIR%\Fonts */
				ret = GetEnvironmentVariable(_T("WINDIR"), pszPath, MAX_PATH);
				if (ret == 0) break;
				_tcsncat(pszPath, _T("\\Fonts"), MAX_PATH);

				return (HRESULT)0;
				break;
			/* XXX - other types to go here when needed... */
		}
	}

	return E_INVALIDARG;
}

/** Determine the current user's locale. */
const char *GetCurrentLocale(const char *)
{
	char lang[9], country[9];
	if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, lengthof(lang)) == 0 ||
	    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, lengthof(country)) == 0) {
		/* Unable to retrieve the locale. */
		return NULL;
	}
	/* Format it as 'en_us'. */
	static char retbuf[6] = {lang[0], lang[1], '_', country[0], country[1], 0};
	return retbuf;
}
