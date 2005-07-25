/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "macros.h"
#include "saveload.h"
#include "string.h"
#include "table/strings.h"
#include "gfx.h"
#include "window.h"
#include <windows.h>
#include <winnt.h>
#include <wininet.h>
#include <io.h>
#include <fcntl.h>
#include "variables.h"
#include "win32.h"

#include "driver.h"

#include "music/dmusic.h"
#include "music/null_m.h"
#include "music/win32_m.h"

#include "sound/null_s.h"
#include "sound/sdl_s.h"
#include "sound/win32_s.h"

#include "video/dedicated_v.h"
#include "video/null_v.h"
#include "video/sdl_v.h"
#include "video/win32_v.h"

static bool _has_console;

#if defined(__MINGW32__) || defined(__CYGWIN__)
	#define __TIMESTAMP__   __DATE__ __TIME__
#endif


// Helper function needed by dynamically loading SDL
bool LoadLibraryList(Function proc[], const char* dll)
{
	while (*dll != '\0') {
		HMODULE lib = LoadLibrary(dll);

		if (lib == NULL) return false;
		while (true) {
		  	FARPROC p;

			while (*dll++ != '\0');
			if (*dll == '\0') break;
			p = GetProcAddress(lib, dll);
			if (p == NULL) return false;
			*proc++ = (Function)p;
		}
		dll++;
	}
	return true;
}

#ifdef _MSC_VER

static const char *_exception_string;
static void *_safe_esp;
static char *_crash_msg;
static bool _expanded;
static bool _did_emerg_save;
static int _ident;

void ShowOSErrorBox(const char *buf)
{
	MyShowCursor(true);
	MessageBoxA(GetActiveWindow(), buf, "Error!", MB_ICONSTOP);

// if exception tracker is enabled, we crash here to let the exception handler handle it.
#if defined(WIN32_EXCEPTION_TRACKER) && !defined(_DEBUG)
	if (*buf == '!') {
		_exception_string = buf;
		*(byte*)0 = 0;
	}
#endif
}

typedef struct DebugFileInfo {
	uint32 size;
	uint32 crc32;
	SYSTEMTIME file_time;
} DebugFileInfo;

static uint32 *_crc_table;

static void MakeCRCTable(uint32 *table) {
	uint32 crc, poly = 0xEDB88320L;
	int i;
	int j;

	_crc_table = table;

	for (i = 0; i != 256; i++) {
		crc = i;
		for (j = 8; j != 0; j--) {
			if (crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;
		}
		table[i] = crc;
	}
}

static uint32 CalcCRC(byte *data, uint size, uint32 crc) {
	for (; size > 0; size--) {
		crc = ((crc >> 8) & 0x00FFFFFF) ^ _crc_table[(crc ^ *data++) & 0xFF];
	}
	return crc;
}

static void GetFileInfo(DebugFileInfo *dfi, const char *filename)
{
	memset(dfi, 0, sizeof(dfi));

	{
		HANDLE file;
		byte buffer[1024];
		DWORD numread;
		uint32 filesize = 0;
		FILETIME write_time;
		uint32 crc = (uint32)-1;

		file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, 0);
		if (file != INVALID_HANDLE_VALUE) {
			while(true) {
				if (ReadFile(file, buffer, sizeof(buffer), &numread, NULL) == 0 ||
						numread == 0)
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
}


static char *PrintModuleInfo(char *output, HMODULE mod)
{
	char buffer[MAX_PATH];
	DebugFileInfo dfi;

	GetModuleFileName(mod, buffer, MAX_PATH);
	GetFileInfo(&dfi, buffer);
	output += sprintf(output, " %-20s handle: %.8X size: %d crc: %.8X date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n",
		buffer,
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
	BOOL (WINAPI *EnumProcessModules)(HANDLE,HMODULE*,DWORD,LPDWORD);
	HANDLE proc;
	HMODULE modules[100];
	DWORD needed;
	BOOL res;
	int count,i;

	if (LoadLibraryList((Function*)&EnumProcessModules, "psapi.dll\0EnumProcessModules\0")) {
		proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
		if (proc) {
			res = EnumProcessModules(proc, modules, sizeof(modules), &needed);
			CloseHandle(proc);
			if (res) {
				count =
					min(needed / sizeof(HMODULE), lengthof(modules));
				for (i = 0; i != count; i++)
					output = PrintModuleInfo(output, modules[i]);
				return output;
			}
		}
	}
	output = PrintModuleInfo(output, NULL);
	return output;
}

static const char _crash_desc[] =
	"A serious fault condition occured in the game. The game will shut down.\n"
	"Press \"Submit report\" to send crash information to the developers. "
	"This will greatly help debugging. "
	"The information contained in the report is displayed below.\n"
	"Press \"Emergency save\" to attempt saving the game.";

static const char _save_succeeded[] =
	"Emergency save succeeded.\n"
	"Be aware that critical parts of the internal game state may have become "
	"corrupted. The saved game is not guaranteed to work.";

static bool EmergencySave(void)
{
	SaveOrLoad("crash.sav", SL_SAVE);
	return true;
}

typedef struct {
	HINTERNET (WINAPI *InternetOpenA)(LPCSTR,DWORD, LPCSTR, LPCSTR, DWORD);
	HINTERNET (WINAPI *InternetConnectA)(HINTERNET, LPCSTR, INTERNET_PORT, LPCSTR, LPCSTR, DWORD, DWORD, DWORD);
	HINTERNET (WINAPI *HttpOpenRequestA)(HINTERNET, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR *, DWORD, DWORD);
	BOOL (WINAPI *HttpSendRequestA)(HINTERNET, LPCSTR, DWORD, LPVOID, DWORD);
	BOOL (WINAPI *InternetCloseHandle)(HINTERNET);
	BOOL (WINAPI *HttpQueryInfo)(HINTERNET, DWORD, LPVOID, LPDWORD, LPDWORD);
} WinInetProcs;

#define M(x) x "\0"
static const char wininet_files[] =
	M("wininet.dll")
	M("InternetOpenA")
	M("InternetConnectA")
	M("HttpOpenRequestA")
	M("HttpSendRequestA")
	M("InternetCloseHandle")
	M("HttpQueryInfoA")
	M("");
#undef M

static WinInetProcs _wininet;


static const char *SubmitCrashReport(HWND wnd, void *msg, size_t msglen, const char *arg)
{
	HINTERNET inet, conn, http;
	const char *err = NULL;
	DWORD code, len;
	static char buf[100];
	char buff[100];

	if (_wininet.InternetOpen == NULL && !LoadLibraryList((Function*)&_wininet, wininet_files)) return "can't load wininet.dll";

	inet = _wininet.InternetOpen("OTTD", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if (inet == NULL) { err = "internetopen failed"; goto error1; }

	conn = _wininet.InternetConnect(inet, "openttd.com", INTERNET_DEFAULT_HTTP_PORT, "", "", INTERNET_SERVICE_HTTP, 0, 0);
	if (conn == NULL) { err = "internetconnect failed"; goto error2; }

	sprintf(buff, "/crash.php?file=%s&ident=%d", arg, _ident);

	http = _wininet.HttpOpenRequest(conn, "POST", buff, NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE , 0);
	if (http == NULL) { err = "httpopenrequest failed"; goto error3; }

	if (!_wininet.HttpSendRequest(http, "Content-type: application/binary", -1, msg, msglen)) { err = "httpsendrequest failed"; goto error4; }

	len = sizeof(code);
	if (!_wininet.HttpQueryInfo(http, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &len, 0)) { err = "httpqueryinfo failed"; goto error4; }

	if (code != 200) {
		int l = sprintf(buf, "Server said: %d ", code);
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

static void SubmitFile(HWND wnd, const char *file)
{
	HANDLE h;
	unsigned long size;
	unsigned long read;
	void *mem;

	h = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (h == NULL) return;

	size = GetFileSize(h, NULL);
	if (size > 500000) goto error1;

	mem = malloc(size);
	if (mem == NULL) goto error1;

	if (!ReadFile(h, mem, size, &read, NULL) || read != size) goto error2;

	SubmitCrashReport(wnd, mem, size, file);

error2:
	free(mem);
error1:
	CloseHandle(h);
}

static const char * const _expand_texts[] = {"S&how report >>", "&Hide report <<" };

static void SetWndSize(HWND wnd, int mode)
{
	RECT r,r2;
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

static BOOL CALLBACK CrashDialogFunc(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg) {
	case WM_INITDIALOG:
		SetDlgItemText(wnd, 10, _crash_desc);
		SetDlgItemText(wnd, 11, _crash_msg);
		SendDlgItemMessage(wnd, 11, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);
		SetWndSize(wnd, -1);
		return TRUE;
	case WM_COMMAND:
		switch(wParam) {
		case 12: // Close
			ExitProcess(0);
		case 13: { // Emergency save
			if (DoEmergencySave(wnd))
				MessageBoxA(wnd, _save_succeeded, "Save successful", MB_ICONINFORMATION);
			else
				MessageBoxA(wnd, "Save failed", "Save failed", MB_ICONINFORMATION);
			break;
		}
		case 14: { // Submit crash report
			const char *s;

			SetCursor(LoadCursor(NULL, IDC_WAIT));

			s = SubmitCrashReport(wnd, _crash_msg, strlen(_crash_msg), "");
			if (s) {
				MessageBoxA(wnd, s, "Error", MB_ICONSTOP);
				break;
			}

			// try to submit emergency savegame
			if (_did_emerg_save || DoEmergencySave(wnd)) {
				SubmitFile(wnd, "crash.sav");
			}
			// try to submit the autosaved game
			if (_opt.autosave) {
				char buf[40];
				sprintf(buf, "autosave%d.sav", (_autosave_ctr - 1) & 3);
				SubmitFile(wnd, buf);
			}
			EnableWindow(GetDlgItem(wnd, 14), FALSE);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			MessageBoxA(wnd, "Crash report submitted. Thank you.", "Crash Report", MB_ICONINFORMATION);
			break;
		}
		case 15: // Expand
			_expanded ^= 1;
			SetWndSize(wnd, _expanded);
			break;
		}
		return TRUE;
	case WM_CLOSE:
		ExitProcess(0);
	}

	return FALSE;
}

static void Handler2(void)
{
	ShowCursor(TRUE);
	ShowWindow(GetActiveWindow(), FALSE);
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(100), NULL, CrashDialogFunc);
}

extern bool CloseConsoleLogIfActive(void);

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep)
{
	char *output;
	static bool had_exception;

	if (had_exception) ExitProcess(0);
	had_exception = true;

	_ident = GetTickCount(); // something pretty unique

	MakeCRCTable(alloca(256 * sizeof(uint32)));
	_crash_msg = output = LocalAlloc(LMEM_FIXED, 8192);

	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		output += sprintf(output,
			"*** OpenTTD Crash Report ***\r\n"
			"Date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n"
			"Build: %s built on " __TIMESTAMP__ "\r\n",
			time.wYear,
			time.wMonth,
			time.wDay,
			time.wHour,
			time.wMinute,
			time.wSecond,
			"???"
		);
	}

	if (_exception_string)
		output += sprintf(output, "Reason: %s\r\n", _exception_string);

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

	{
		byte *b = (byte*)ep->ContextRecord->Eip;
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
		int i,j;
		uint32 *b = (uint32*)ep->ContextRecord->Esp;
		for (j = 0; j != 24; j++) {
			for (i = 0; i != 8; i++) {
				if (IsBadReadPtr(b,sizeof(uint32))) {
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
		HANDLE file = CreateFile("crash.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
		DWORD num_written;
		if (file != INVALID_HANDLE_VALUE) {
			WriteFile(file, _crash_msg, output - _crash_msg, &num_written, NULL);
			CloseHandle(file);
		}
	}

	/* Close any possible log files */
	CloseConsoleLogIfActive();

	if (_safe_esp) {
		ep->ContextRecord->Eip = (DWORD)Handler2;
		ep->ContextRecord->Esp = (DWORD)_safe_esp;
		return EXCEPTION_CONTINUE_EXECUTION;
	}


	return EXCEPTION_EXECUTE_HANDLER;
}

static void Win32InitializeExceptions(void)
{
	_asm {
		mov _safe_esp, esp
	}

	SetUnhandledExceptionFilter(ExceptionHandler);
}
#else
/* Get rid of unused variable warnings.. ShowOSErrorBox
 * is now used twice, once in MSVC, and once in all other Win
 * compilers (cygwin, mingw, etc.) */
void ShowOSErrorBox(const char *buf)
{
	MyShowCursor(true);
	MessageBoxA(GetActiveWindow(), buf, "Error!", MB_ICONSTOP);
}
#endif

#ifndef __MINGW32__
static inline int strcasecmp(const char* s1, const char* s2)
{
	return stricmp(s1, s2);
}
#endif

static char *_fios_path;
static char *_fios_save_path;
static char *_fios_scn_path;
static FiosItem *_fios_items;
static int _fios_count, _fios_alloc;

static FiosItem *FiosAlloc(void)
{
	if (_fios_count == _fios_alloc) {
		_fios_alloc += 256;
		_fios_items = realloc(_fios_items, _fios_alloc * sizeof(FiosItem));
	}
	return &_fios_items[_fios_count++];
}

static HANDLE MyFindFirstFile(const char *path, const char *file, WIN32_FIND_DATA *fd)
{
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS); // disable 'no-disk' message box
	HANDLE h;
	char paths[MAX_PATH];

	sprintf(paths, "%s\\%s", path, file);
	h = FindFirstFile(paths, fd);

	SetErrorMode(sem); // restore previous setting
	return h;
}

int CDECL compare_FiosItems(const void *a, const void *b)
{
	const FiosItem *da = (const FiosItem *)a;
	const FiosItem *db = (const FiosItem *)b;
	int r;

	if (_savegame_sort_order < 2) // sort by date
		r = da->mtime < db->mtime ? -1 : 1;
	else
		r = strcasecmp(da->title, db->title);

	if (_savegame_sort_order & 1) r = -r;
	return r;
}


// Get a list of savegames
FiosItem *FiosGetSavegameList(int *num, int mode)
{
	WIN32_FIND_DATA fd;
	HANDLE h;
	FiosItem *fios;
	int sort_start;

	if (_fios_save_path == NULL) {
		_fios_save_path = malloc(MAX_PATH);
		strcpy(_fios_save_path, _path.save_dir);
	}

	_fios_path = _fios_save_path;

	// Parent directory, only if not of the type C:\.
	if (_fios_path[3] != '\0') {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		strcpy(fios->name, "..");
		strcpy(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	h = MyFindFirstFile(_fios_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
					strcmp(fd.cFileName, ".") != 0 &&
					strcmp(fd.cFileName, "..") != 0) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, fd.cFileName, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title), "%s\\ (Directory)", fd.cFileName);
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/* Show savegame files
	 * .SAV OpenTTD saved game
	 * .SS1 Transport Tycoon Deluxe preset game
	 * .SV1 Transport Tycoon Deluxe (Patch) saved game
	 * .SV2 Transport Tycoon Deluxe (Patch) saved 2-player game
	 */
	h = MyFindFirstFile(_fios_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			char *t;

			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

			t = strrchr(fd.cFileName, '.');
			if (t != NULL && strcasecmp(t, ".sav") == 0) { // OpenTTD
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_FILE;
				fios->mtime = *(uint64*)&fd.ftLastWriteTime;
				ttd_strlcpy(fios->name, fd.cFileName, lengthof(fios->name));

				*t = '\0'; // strip extension
				ttd_strlcpy(fios->title, fd.cFileName, lengthof(fios->title));

			} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO) {
				if (t != NULL && (
							strcasecmp(t, ".ss1") == 0 ||
							strcasecmp(t, ".sv1") == 0 ||
							strcasecmp(t, ".sv2") == 0
						)) { // TTDLX(Patch)
					char buf[MAX_PATH];

					fios = FiosAlloc();
					fios->type = FIOS_TYPE_OLDFILE;
					fios->mtime = *(uint64*)&fd.ftLastWriteTime;
					ttd_strlcpy(fios->name, fd.cFileName, lengthof(fios->name));
					sprintf(buf, "%s\\%s", _fios_path, fd.cFileName);
					GetOldSaveGameName(fios->title, buf);
				}
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	// Drives
	{
		char drives[256];
		const char *s;

		GetLogicalDriveStrings(sizeof(drives), drives);
		for (s = drives; *s != '\0';) {
			fios = FiosAlloc();
			fios->type = FIOS_TYPE_DRIVE;
			sprintf(fios->name, "%c:", s[0]);
			sprintf(fios->title, "%c:", s[0]);
			while (*s++ != '\0') {}
		}
	}

	*num = _fios_count;
	return _fios_items;
}

// Get a list of scenarios
FiosItem *FiosGetScenarioList(int *num, int mode)
{
	FiosItem *fios;
	WIN32_FIND_DATA fd;
	HANDLE h;
	int sort_start;

	if (_fios_scn_path == NULL) {
		_fios_scn_path = malloc(MAX_PATH);
		strcpy(_fios_scn_path, _path.scenario_dir);
	}

	_fios_path = _fios_scn_path;

	// Parent directory, only if not of the type C:\.
	if (_fios_path[3] != '\0' && mode != SLD_NEW_GAME) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		strcpy(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	h = MyFindFirstFile(_fios_scn_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE && mode != SLD_NEW_GAME) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
					strcmp(fd.cFileName, ".") != 0 &&
					strcmp(fd.cFileName, "..") != 0) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, fd.cFileName, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title), "%s\\ (Directory)", fd.cFileName);
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/* Show scenario files
	 * .SCN OpenTTD style scenario file
	 * .SV0 Transport Tycoon Deluxe (Patch) scenario
	 * .SS0 Transport Tycoon Deluxe preset scenario
	 */
	h = MyFindFirstFile(_fios_scn_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			char *t;

			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;

			t = strrchr(fd.cFileName, '.');
			if (t != NULL && strcasecmp(t, ".scn") == 0) { // OpenTTD
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_SCENARIO;
				fios->mtime = *(uint64*)&fd.ftLastWriteTime;
				ttd_strlcpy(fios->name, fd.cFileName, lengthof(fios->name));

				*t  = '\0'; // strip extension
				ttd_strlcpy(fios->title, fd.cFileName, lengthof(fios->title));

			} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO ||
					mode == SLD_NEW_GAME) {
				if (t != NULL && (
							strcasecmp(t, ".sv0") == 0 ||
							strcasecmp(t, ".ss0") == 0
						)) { // TTDLX(Patch)
					char buf[MAX_PATH];

					fios = FiosAlloc();
					fios->type = FIOS_TYPE_OLD_SCENARIO;
					fios->mtime = *(uint64*)&fd.ftLastWriteTime;
					sprintf(buf, "%s\\%s", _fios_path, fd.cFileName);
					GetOldScenarioGameName(fios->title, buf);
					ttd_strlcpy(fios->name, fd.cFileName, lengthof(fios->name));
				}
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	// Drives
	if (mode != SLD_NEW_GAME) {
		char drives[256];
		const char *s;

		GetLogicalDriveStrings(sizeof(drives), drives);
		for (s = drives; *s != '\0';) {
			fios = FiosAlloc();
			fios->type = FIOS_TYPE_DRIVE;
			sprintf(fios->name, "%c:", s[0]);
			sprintf(fios->title, "%c:", s[0]);
			while (*s++ != '\0') {}
		}
	}

	*num = _fios_count;
	return _fios_items;
}


// Free the list of savegames
void FiosFreeSavegameList(void)
{
	free(_fios_items);
	_fios_items = NULL;
	_fios_alloc = _fios_count = 0;
}

// Browse to
char *FiosBrowseTo(const FiosItem *item)
{
	char *path = _fios_path;
	char *s;

	switch (item->type) {
		case FIOS_TYPE_DRIVE:
			sprintf(path, "%c:\\", item->title[0]);
			break;

		case FIOS_TYPE_PARENT:
			s = strrchr(path, '\\');
			if (s != NULL) *s = '\0';
			if (path[2] == '\0' ) strcat(path, "\\");
			break;

		case FIOS_TYPE_DIR:
			s = strchr(item->name, '\\');
			if (s != NULL) *s = '\0';
			if (path[3] != '\0' ) strcat(path, "\\");
			strcat(path, item->name);
			break;

		case FIOS_TYPE_FILE:
		case FIOS_TYPE_OLDFILE:
		case FIOS_TYPE_SCENARIO:
		case FIOS_TYPE_OLD_SCENARIO: {
			static char str_buffr[512];

			sprintf(str_buffr, "%s\\%s", path, item->name);
			return str_buffr;
		}
	}

	return NULL;
}

/**
 * Get descriptive texts. Returns the path and free space
 * left on the device
 * @param path string describing the path
 * @param tfs total free space in megabytes, optional (can be NULL)
 * @return StringID describing the path (free space or failure)
 */
StringID FiosGetDescText(const char **path, uint32 *tot)
{
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS);  // disable 'no-disk' message box
	char root[4];
	DWORD spc, bps, nfc, tnc;
	StringID sid;

	*path = _fios_path;

	sprintf(root, "%c:\\", _fios_path[0]);
	if (tot != NULL && GetDiskFreeSpace(root, &spc, &bps, &nfc, &tnc)) {
		*tot = ((spc * bps) * (uint64)nfc) >> 20;
		sid = STR_4005_BYTES_FREE;
	} else
		sid = STR_4006_UNABLE_TO_READ_DRIVE;

	SetErrorMode(sem); // reset previous setting
	return sid;
}

void FiosMakeSavegameName(char *buf, const char *name)
{
	const char* extension;
	const char* period;

	if (_game_mode == GM_EDITOR)
		extension = ".scn";
	else
		extension = ".sav";

	// Don't append the extension, if it is already there
	period = strrchr(name, '.');
	if (period != NULL && strcasecmp(period, extension) == 0) extension = "";

	sprintf(buf, "%s\\%s%s", _fios_path, name, extension);
}

void FiosDelete(const char *name)
{
	char path[512];

	snprintf(path, lengthof(path), "%s\\%s", _fios_path, name);
	DeleteFile(path);
}

#define Windows_2000		5
#define Windows_NT3_51	4

/* flags show the minimum required OS to use a given feature. Currently
	 only dwMajorVersion and dwMinorVersion (WindowsME) are used
														MajorVersion	MinorVersion
		Windows Server 2003					5							 2				dmusic
		Windows XP									5							 1				dmusic
		Windows 2000								5							 0				dmusic
		Windows NT 4.0							4							 0				win32
		Windows Me									4							90				dmusic
		Windows 98									4							10				win32
		Windows 95									4							 0				win32
		Windows NT 3.51							3							51				?????
*/

const DriverDesc _video_driver_descs[] = {
	{"null", "Null Video Driver",				&_null_video_driver,		0},
#if defined(WITH_SDL)
	{"sdl", "SDL Video Driver",					&_sdl_video_driver,			1},
#endif
	{"win32", "Win32 GDI Video Driver",	&_win32_video_driver,		Windows_NT3_51},
#ifdef ENABLE_NETWORK
	{ "dedicated", "Dedicated Video Driver", &_dedicated_video_driver, 0},
#endif
	{ NULL, NULL, NULL, 0 }
};

const DriverDesc _sound_driver_descs[] = {
	{"null", "Null Sound Driver",			&_null_sound_driver,	0},
#if defined(WITH_SDL)
	{"sdl", "SDL Sound Driver",				&_sdl_sound_driver,		1},
#endif
	{"win32", "Win32 WaveOut Driver",	&_win32_sound_driver,	Windows_NT3_51},
	{ NULL, NULL, NULL, 0 }
};

const DriverDesc _music_driver_descs[] = {
	{"null", "Null Music Driver",		&_null_music_driver,				0},
#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT
	{"dmusic", "DirectMusic MIDI Driver",	&_dmusic_midi_driver,	Windows_2000},
#endif
	// Win32 MIDI driver has higher priority than DMusic, so this one is chosen
	{"win32", "Win32 MIDI Driver",	&_win32_music_driver,				Windows_NT3_51},
	{ NULL, NULL, NULL, 0 }
};

byte GetOSVersion(void)
{
	OSVERSIONINFO osvi;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (GetVersionEx(&osvi)) {
		DEBUG(misc, 2) ("Windows Version is %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
		// WinME needs directmusic too (dmusic, Windows_2000 mode), all others default to OK
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90) { return Windows_2000;} // WinME

		return osvi.dwMajorVersion;
	}

	// GetVersionEx failed, but we can safely assume at least Win95/WinNT3.51 is used
	DEBUG(misc, 0) ("Windows version retrieval failed, defaulting to level 4");
	return Windows_NT3_51;
}

bool FileExists(const char *filename)
{
	HANDLE hand = CreateFile(filename, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hand == INVALID_HANDLE_VALUE) return false;
	CloseHandle(hand);
	return true;
}

static int CDECL LanguageCompareFunc(const void *a, const void *b)
{
	return strcmp(*(const char* const *)a, *(const char* const *)b);
}

int GetLanguageList(char **languages, int max)
{
	HANDLE hand;
	int num = 0;
	char filedir[MAX_PATH];
	WIN32_FIND_DATA fd;
	sprintf(filedir, "%s*.lng", _path.lang_dir);

	hand = FindFirstFile(filedir, &fd);
	if (hand != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				languages[num++] = strdup(fd.cFileName);
				if (num == max) break;
			}
		} while (FindNextFile(hand, &fd));
		FindClose(hand);
	}

	qsort(languages, num, sizeof(char*), LanguageCompareFunc);
	return num;
}

static int ParseCommandLine(char *line, char **argv, int max_argc)
{
	int n = 0;

	do {
		// skip whitespace
		while (*line == ' ' || *line == '\t')
			line++;

		// end?
		if (*line == '\0')
			break;

		// special handling when quoted
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


#if defined(_MSC_VER)
uint32 _declspec(naked) rdtsc(void)
{
	_asm {
		rdtsc
		ret
	}
}
#endif

void CreateConsole(void)
{
	HANDLE hand;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	if (_has_console) return;

	_has_console = true;

	AllocConsole();

	hand = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hand, &coninfo);
	coninfo.dwSize.Y = 500;
	SetConsoleScreenBufferSize(hand, coninfo.dwSize);

	// redirect unbuffered STDIN, STDOUT, STDERR to the console
#if !defined(__CYGWIN__)
	*stdout = *_fdopen( _open_osfhandle((long)hand, _O_TEXT), "w" );
	*stdin = *_fdopen(_open_osfhandle((long)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT), "r" );
	*stderr = *_fdopen(_open_osfhandle((long)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT), "w" );
#else
	// open_osfhandle is not in cygwin
	*stdout = *fdopen(1, "w" );
	*stdin = *fdopen(0, "r" );
	*stderr = *fdopen(2, "w" );
#endif

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

void ShowInfo(const char *str)
{
	if (_has_console)
		puts(str);
	else {
		bool old;

		ReleaseCapture();
		_left_button_clicked =_left_button_down = false;

		old = MyShowCursor(true);
		if (MessageBoxA(GetActiveWindow(), str, "OpenTTD", MB_ICONINFORMATION | MB_OKCANCEL) == IDCANCEL) {
			CreateConsole();
		}
		MyShowCursor(old);
	}
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPTSTR lpCmdLine, int nCmdShow)
{
	int argc;
	char *argv[64]; // max 64 command line arguments

#if defined(_DEBUG)
	CreateConsole();
#endif

	// make sure we have an autosave folder - Done in DeterminePaths
	// CreateDirectory("autosave", NULL);

	// setup random seed to something quite random
#if defined(_MSC_VER)
	{
		uint64 seed = rdtsc();
		_random_seeds[0][0] = GB(seed,  0, 32);
		_random_seeds[0][1] = GB(seed, 32, 32);
	}
#else
	_random_seeds[0][0] = GetTickCount();
	_random_seeds[0][1] = _random_seeds[0][0] * 0x1234567;
#endif
	SeedMT(_random_seeds[0][0]);

	argc = ParseCommandLine(GetCommandLine(), argv, lengthof(argv));

#if defined(WIN32_EXCEPTION_TRACKER)
	{
		Win32InitializeExceptions();
	}
#endif

#if defined(WIN32_EXCEPTION_TRACKER_DEBUG)
	_try {
		uint32 _stdcall ExceptionHandler(void *ep);
#endif
		ttd_main(argc, argv);

#if defined(WIN32_EXCEPTION_TRACKER_DEBUG)
	} _except (ExceptionHandler(_exception_info())) {}
#endif

	return 0;
}

void DeterminePaths(void)
{
	char *s;
	char *cfg;

	_path.personal_dir = _path.game_data_dir = cfg = malloc(MAX_PATH);
	GetCurrentDirectory(MAX_PATH - 1, cfg);


	s = strchr(cfg, 0);
	if (s[-1] != '\\') strcpy(s, "\\");

	_path.save_dir = str_fmt("%ssave", cfg);
	_path.autosave_dir = str_fmt("%s\\autosave", _path.save_dir);
	_path.scenario_dir = str_fmt("%sscenario", cfg);
	_path.gm_dir = str_fmt("%sgm\\", cfg);
	_path.data_dir = str_fmt("%sdata\\", cfg);
	_path.lang_dir = str_fmt("%slang\\", cfg);

	if (_config_file == NULL)
		_config_file = str_fmt("%sopenttd.cfg", _path.personal_dir);

	_highscore_file = str_fmt("%shs.dat", _path.personal_dir);
	_log_file = str_fmt("%sopenttd.log", _path.personal_dir);

	// make (auto)save and scenario folder
	CreateDirectory(_path.save_dir, NULL);
	CreateDirectory(_path.autosave_dir, NULL);
	CreateDirectory(_path.scenario_dir, NULL);
}

int CDECL snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	return ret;
}

int CDECL vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	int ret;
	ret = _vsnprintf(str, size, format, ap);
	if (ret < 0) str[size - 1] = '\0';
	return ret;
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @param tb @Textbuf type to be changed
 * @return Return true on successfull change of Textbuf, or false otherwise
 */
bool InsertTextBufferClipboard(Textbuf *tb)
{
	if (IsClipboardFormatAvailable(CF_TEXT)) {
		HGLOBAL cbuf;
		const byte *data, *dataptr;
		uint16 width = 0;
		uint16 length = 0;

		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_TEXT);
		data = GlobalLock(cbuf); // clipboard data
		dataptr = data;

		for (; IsValidAsciiChar(*dataptr) && (tb->length + length) < tb->maxlength - 1 &&
				(tb->maxwidth == 0 || width + tb->width + GetCharacterWidth((byte)*dataptr) <= tb->maxwidth); dataptr++) {
					width += GetCharacterWidth((byte)*dataptr);
			length++;
		}

		if (length == 0)
			return false;

		memmove(tb->buf + tb->caretpos + length, tb->buf + tb->caretpos, tb->length - tb->caretpos);
		memcpy(tb->buf + tb->caretpos, data, length);
		tb->width += width;
		tb->caretxoffs += width;

		tb->length += length;
		tb->caretpos += length;
		tb->buf[tb->length + 1] = '\0'; // terminating zero

		GlobalUnlock(cbuf);
		CloseClipboard();
		return true;
	}
	return false;
}

static HANDLE hThread;

bool CreateOTTDThread(void *func, void *param)
{
	DWORD dwThreadId;
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, param, 0, &dwThreadId);
	SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

	return hThread != NULL;
}

void CloseOTTDThread(void)
{
	if (!CloseHandle(hThread)) DEBUG(misc, 0) ("Failed to close thread?...");
}

void JoinOTTDThread(void)
{
	if (hThread == NULL) return;

	WaitForSingleObject(hThread, INFINITE);
}


void CSleep(int milliseconds)
{
	Sleep(milliseconds);
}


// Utility function to get the current timestamp in milliseconds
// Useful for profiling
int64 GetTS(void)
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
