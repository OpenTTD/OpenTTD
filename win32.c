/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "macros.h"
#include "saveload.h"
#include "string.h"
#include "gfx.h"
#include "window.h"
#include <windows.h>
#include <winnt.h>
#include <wininet.h>
#include <io.h>
#include <fcntl.h>
#include "variables.h"
#include "win32.h"
#include "fios.h" // opendir/readdir/closedir
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

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


// Helper function needed by dynamically loading SDL
bool LoadLibraryList(Function proc[], const char* dll)
{
	while (*dll != '\0') {
		HMODULE lib = LoadLibrary(dll);

		if (lib == NULL) return false;
		for (;;) {
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
#endif

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

#ifdef _MSC_VER

static void *_safe_esp;
static char *_crash_msg;
static bool _expanded;
static bool _did_emerg_save;
static int _ident;

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
			crc = (crc & 1 ? (crc >> 1) ^ poly : crc >> 1);
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
			for (;;) {
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
	output += sprintf(output, " %-20s handle: %p size: %d crc: %.8X date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n",
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

	if (!_wininet.HttpSendRequest(http, "Content-type: application/binary", -1, msg, (DWORD)msglen)) { err = "httpsendrequest failed"; goto error4; }

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

static INT_PTR CALLBACK CrashDialogFunc(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		SetDlgItemText(wnd, 10, _crash_desc);
		SetDlgItemText(wnd, 11, _crash_msg);
		SendDlgItemMessage(wnd, 11, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);
		SetWndSize(wnd, -1);
		return TRUE;
	case WM_COMMAND:
		switch (wParam) {
		case 12: // Close
			ExitProcess(0);
		case 13: { // Emergency save
			if (DoEmergencySave(wnd)) {
				MessageBoxA(wnd, _save_succeeded, "Save successful", MB_ICONINFORMATION);
			} else {
				MessageBoxA(wnd, "Save failed", "Save failed", MB_ICONINFORMATION);
			}
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
	extern const char _openttd_revision[];
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
		output += snprintf(output, 8192,
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
		int i,j;
#ifdef _M_AMD64
		uint32 *b = (uint32*)ep->ContextRecord->Rsp;
#else
		uint32 *b = (uint32*)ep->ContextRecord->Esp;
#endif
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

static void Win32InitializeExceptions(void)
{
#ifdef _M_AMD64
	extern void *_get_save_esp(void);
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
 * To guarantee reentrancy, we fall back to malloc if the instance is
 * already in use (it's important to avoid suprises since this is such a
 * low-level routine). */
static DIR _global_dir;
static bool _global_dir_is_in_use = false;

static inline DIR *dir_calloc(void)
{
	DIR *d;

	if (_global_dir_is_in_use) {
		d = calloc(1, sizeof(*d));
	} else {
		_global_dir_is_in_use = true;
		d = &_global_dir;
		memset(d, 0, sizeof(*d));
	}
	return d;
}

static inline void dir_free(DIR *d)
{
	if (d == &_global_dir) {
		_global_dir_is_in_use = false;
	} else {
		free(d);
	}
}

DIR *opendir(const char *path)
{
	char search_path[MAX_PATH];
	DIR *d;
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS); // disable 'no-disk' message box
	DWORD fa = GetFileAttributes(path);

	if ((fa != INVALID_FILE_ATTRIBUTES) && (fa & FILE_ATTRIBUTE_DIRECTORY)) {
		d = dir_calloc();
		if (d != NULL) {
			/* build search path for FindFirstFile */
			snprintf(search_path, lengthof(search_path), "%s" PATHSEP "*", path);
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

void FiosGetDrives(void)
{
	char drives[256];
	const char *s;

	GetLogicalDriveStrings(sizeof(drives), drives);
	for (s = drives; *s != '\0';) {
		FiosItem *fios = FiosAlloc();
		fios->type = FIOS_TYPE_DRIVE;
		fios->mtime = 0;
		snprintf(fios->name,  lengthof(fios->name),  "%c:", s[0]);
		ttd_strlcpy(fios->title, fios->name, lengthof(fios->title));
		while (*s++ != '\0');
	}
}

bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb)
{
	// hectonanoseconds between Windows and POSIX epoch
	static const int64 posix_epoch_hns = 0x019DB1DED53E8000LL;
	const WIN32_FIND_DATA *fd = &ent->dir->fd;
	if (fd->dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) return false;

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

bool FiosGetDiskFreeSpace(const char *path, uint32 *tot)
{
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS);  // disable 'no-disk' message box
	bool retval = false;
	char root[4];
	DWORD spc, bps, nfc, tnc;

	snprintf(root, lengthof(root), "%c:" PATHSEP, path[0]);
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
	*stdout = *_fdopen( _open_osfhandle((intptr_t)hand, _O_TEXT), "w" );
	*stdin = *_fdopen(_open_osfhandle((intptr_t)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT), "r" );
	*stderr = *_fdopen(_open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT), "w" );
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
	if (_has_console) {
		puts(str);
	} else {
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

#ifdef __MINGW32__
	/* _set_error_mode() constants&function (do not exist in mingw headers) */
	#define _OUT_TO_DEFAULT      0
	#define _OUT_TO_STDERR       1
	#define _OUT_TO_MSGBOX       2
	#define _REPORT_ERRMODE      3
	int _set_error_mode(int);
#endif

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPTSTR lpCmdLine, int nCmdShow)
{
	int argc;
	char *argv[64]; // max 64 command line arguments

#if defined(_DEBUG)
	CreateConsole();
#endif

	_set_error_mode(_OUT_TO_MSGBOX); // force assertion output to messagebox

	// setup random seed to something quite random
	_random_seeds[1][0] = _random_seeds[0][0] = GetTickCount();
	_random_seeds[1][1] = _random_seeds[0][1] = _random_seeds[0][0] * 0x1234567;
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

	cfg[0] = toupper(cfg[0]);
	s = strchr(cfg, 0);
	if (s[-1] != '\\') strcpy(s, "\\");

	_path.save_dir = str_fmt("%ssave", cfg);
	_path.autosave_dir = str_fmt("%s\\autosave", _path.save_dir);
	_path.scenario_dir = str_fmt("%sscenario", cfg);
	_path.heightmap_dir = str_fmt("%sscenario\\heightmap", cfg);
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
	CreateDirectory(_path.heightmap_dir, NULL);
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
	HGLOBAL cbuf;
	char utf8_buf[512];
	const char *ptr;

	WChar c;
	uint16 width, length;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		int bytec;

		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_UNICODETEXT);

		ptr = GlobalLock(cbuf);
		bytec = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)ptr, -1, utf8_buf, lengthof(utf8_buf), NULL, NULL);
		GlobalUnlock(cbuf);
		CloseClipboard();

		if (bytec == 0) {
			DEBUG(misc, 0) ("[utf8] Error converting '%s'. Errno %d", ptr, GetLastError());
			return false;
		}
	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_TEXT);

		ptr = GlobalLock(cbuf);
		ttd_strlcpy(utf8_buf, ptr, lengthof(utf8_buf));
		GlobalUnlock(cbuf);
		CloseClipboard();
	} else {
		return false;
	}

	width = length = 0;

	for (ptr = utf8_buf; (c = Utf8Consume(&ptr)) != '\0';) {
		byte charwidth;

		if (!IsPrintable(c)) break;
		if (tb->length + length >= tb->maxlength - 1) break;
		charwidth = GetCharacterWidth(FS_NORMAL, c);

		if (tb->maxwidth != 0 && width + tb->width + charwidth > tb->maxwidth) break;

		width += charwidth;
		length += Utf8CharLen(c);
	}

	if (length == 0) return false;

	memmove(tb->buf + tb->caretpos + length, tb->buf + tb->caretpos, tb->length - tb->caretpos);
	memcpy(tb->buf + tb->caretpos, utf8_buf, length);
	tb->width += width;
	tb->caretxoffs += width;

	tb->length += length;
	tb->caretpos += length;
	tb->buf[tb->length] = '\0'; // terminating zero

	return true;
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
