/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32.cpp Implementation of MS Windows system calls */

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../gfx_func.h"
#include "../../textbuf_gui.h"
#include "../../fileio_func.h"
#include <windows.h>
#include <fcntl.h>
#include <regstr.h>
#include <shlobj.h> /* SHGetFolderPath */
#include <shellapi.h>
#include "win32.h"
#include "../../fios.h"
#include "../../core/alloc_func.hpp"
#include "../../openttd.h"
#include "../../core/random_func.hpp"
#include "../../string_func.h"
#include "../../crashlog.h"
#include <errno.h>
#include <sys/stat.h>

/* Due to TCHAR, strncat and strncpy have to remain (for a while). */
#include "../../safeguards.h"
#undef strncat
#undef strncpy

static bool _has_console;
static bool _cursor_disable = true;
static bool _cursor_visible = true;

bool MyShowCursor(bool show, bool toggle)
{
	if (toggle) _cursor_disable = !_cursor_disable;
	if (_cursor_disable) return show;
	if (_cursor_visible == show) return show;

	_cursor_visible = show;
	ShowCursor(show);

	return !show;
}

/**
 * Helper function needed by dynamically loading libraries
 * XXX: Hurray for MS only having an ANSI GetProcAddress function
 * on normal windows and no Wide version except for in Windows Mobile/CE
 */
bool LoadLibraryList(Function proc[], const char *dll)
{
	while (*dll != '\0') {
		HMODULE lib;
		lib = LoadLibrary(MB_TO_WIDE(dll));

		if (lib == NULL) return false;
		for (;;) {
			FARPROC p;

			while (*dll++ != '\0') { /* Nothing */ }
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

void ShowOSErrorBox(const char *buf, bool system)
{
	MyShowCursor(true);
	MessageBox(GetActiveWindow(), OTTD2FS(buf), _T("Error!"), MB_ICONSTOP);
}

void OSOpenBrowser(const char *url)
{
	ShellExecute(GetActiveWindow(), _T("open"), OTTD2FS(url), NULL, NULL, SW_SHOWNORMAL);
}

/* Code below for windows version of opendir/readdir/closedir copied and
 * modified from Jan Wassenberg's GPL implementation posted over at
 * http://www.gamedev.net/community/forums/topic.asp?topic_id=364584&whichpage=1&#2398903 */

struct DIR {
	HANDLE hFind;
	/* the dirent returned by readdir.
	 * note: having only one global instance is not possible because
	 * multiple independent opendir/readdir sequences must be supported. */
	dirent ent;
	WIN32_FIND_DATA fd;
	/* since opendir calls FindFirstFile, we need a means of telling the
	 * first call to readdir that we already have a file.
	 * that's the case iff this is true */
	bool at_first_entry;
};

/* suballocator - satisfies most requests with a reusable static instance.
 * this avoids hundreds of alloc/free which would fragment the heap.
 * To guarantee concurrency, we fall back to malloc if the instance is
 * already in use (it's important to avoid surprises since this is such a
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

void FiosGetDrives(FileList &file_list)
{
#if defined(WINCE)
	/* WinCE only knows one drive: / */
	FiosItem *fios = file_list.Append();
	fios->type = FIOS_TYPE_DRIVE;
	fios->mtime = 0;
	seprintf(fios->name, lastof(fios->name), PATHSEP "");
	strecpy(fios->title, fios->name, lastof(fios->title));
#else
	TCHAR drives[256];
	const TCHAR *s;

	GetLogicalDriveStrings(lengthof(drives), drives);
	for (s = drives; *s != '\0';) {
		FiosItem *fios = file_list.Append();
		fios->type = FIOS_TYPE_DRIVE;
		fios->mtime = 0;
		seprintf(fios->name, lastof(fios->name),  "%c:", s[0] & 0xFF);
		strecpy(fios->title, fios->name, lastof(fios->title));
		while (*s++ != '\0') { /* Nothing */ }
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
	 * this won't entirely be correct, but we use the time only for comparison. */
	sb->st_mtime = (time_t)((*(const uint64*)&fd->ftLastWriteTime - posix_epoch_hns) / 1E7);
	sb->st_mode  = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)? S_IFDIR : S_IFREG;

	return true;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return (ent->dir->fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

bool FiosGetDiskFreeSpace(const char *path, uint64 *tot)
{
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS);  // disable 'no-disk' message box
	bool retval = false;
	TCHAR root[4];
	DWORD spc, bps, nfc, tnc;

	_sntprintf(root, lengthof(root), _T("%c:") _T(PATHSEP), path[0]);
	if (tot != NULL && GetDiskFreeSpace(root, &spc, &bps, &nfc, &tnc)) {
		*tot = ((spc * bps) * (uint64)nfc);
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

	/* Check if we can open a handle to STDOUT. */
	int fd = _open_osfhandle((intptr_t)hand, _O_TEXT);
	if (fd == -1) {
		/* Free everything related to the console. */
		FreeConsole();
		_has_console = false;
		_close(fd);
		CloseHandle(hand);

		ShowInfo("Unable to open an output handle to the console. Check known-bugs.txt for details.");
		return;
	}

#if defined(_MSC_VER) && _MSC_VER >= 1900
	freopen("CONOUT$", "a", stdout);
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "a", stderr);
#else
	*stdout = *_fdopen(fd, "w");
	*stdin = *_fdopen(_open_osfhandle((intptr_t)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT), "r" );
	*stderr = *_fdopen(_open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT), "w" );
#endif

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

/** Temporary pointer to get the help message to the window */
static const char *_help_msg;

/** Callback function to handle the window */
static INT_PTR CALLBACK HelpDialogFunc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG: {
			char help_msg[8192];
			const char *p = _help_msg;
			char *q = help_msg;
			while (q != lastof(help_msg) && *p != '\0') {
				if (*p == '\n') {
					*q++ = '\r';
					if (q == lastof(help_msg)) {
						q[-1] = '\0';
						break;
					}
				}
				*q++ = *p++;
			}
			*q = '\0';
			/* We need to put the text in a separate buffer because the default
			 * buffer in OTTD2FS might not be large enough (512 chars). */
			TCHAR help_msg_buf[8192];
			SetDlgItemText(wnd, 11, convert_to_fs(help_msg, help_msg_buf, lengthof(help_msg_buf)));
			SendDlgItemMessage(wnd, 11, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);
		} return TRUE;

		case WM_COMMAND:
			if (wParam == 12) ExitProcess(0);
			return TRUE;
		case WM_CLOSE:
			ExitProcess(0);
	}

	return FALSE;
}

void ShowInfo(const char *str)
{
	if (_has_console) {
		fprintf(stderr, "%s\n", str);
	} else {
		bool old;
		ReleaseCapture();
		_left_button_clicked = _left_button_down = false;

		old = MyShowCursor(true);
		if (strlen(str) > 2048) {
			/* The minimum length of the help message is 2048. Other messages sent via
			 * ShowInfo are much shorter, or so long they need this way of displaying
			 * them anyway. */
			_help_msg = str;
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(101), NULL, HelpDialogFunc);
		} else {
			/* We need to put the text in a separate buffer because the default
			 * buffer in OTTD2FS might not be large enough (512 chars). */
			TCHAR help_msg_buf[8192];
			MessageBox(GetActiveWindow(), convert_to_fs(str, help_msg_buf, lengthof(help_msg_buf)), _T("OpenTTD"), MB_ICONINFORMATION | MB_OK);
		}
		MyShowCursor(old);
	}
}

#if defined(WINCE)
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#else
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif
{
	int argc;
	char *argv[64]; // max 64 command line arguments

	CrashLog::InitialiseCrashLog();

#if defined(UNICODE) && !defined(WINCE)
	/* Check if a win9x user started the win32 version */
	if (HasBit(GetVersion(), 31)) usererror("This version of OpenTTD doesn't run on windows 95/98/ME.\nPlease download the win9x binary and try again.");
#endif

	/* Convert the command line to UTF-8. We need a dedicated buffer
	 * for this because argv[] points into this buffer and this needs to
	 * be available between subsequent calls to FS2OTTD(). */
	char *cmdline = stredup(FS2OTTD(GetCommandLine()));

#if defined(_DEBUG)
	CreateConsole();
#endif

#if !defined(WINCE)
	_set_error_mode(_OUT_TO_MSGBOX); // force assertion output to messagebox
#endif

	/* setup random seed to something quite random */
	SetRandomSeed(GetTickCount());

	argc = ParseCommandLine(cmdline, argv, lengthof(argv));

	/* Make sure our arguments contain only valid UTF-8 characters. */
	for (int i = 0; i < argc; i++) ValidateString(argv[i]);

	openttd_main(argc, argv);
	free(cmdline);
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
#else
	TCHAR path[MAX_PATH];
	GetCurrentDirectory(MAX_PATH - 1, path);
	convert_from_fs(path, buf, size);
#endif
	return buf;
}


void DetermineBasePaths(const char *exe)
{
	char tmp[MAX_PATH];
	TCHAR path[MAX_PATH];
#ifdef WITH_PERSONAL_DIR
	if (SUCCEEDED(OTTDSHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path))) {
		strecpy(tmp, FS2OTTD(path), lastof(tmp));
		AppendPathSeparator(tmp, lastof(tmp));
		strecat(tmp, PERSONAL_DIR, lastof(tmp));
		AppendPathSeparator(tmp, lastof(tmp));
		_searchpaths[SP_PERSONAL_DIR] = stredup(tmp);
	} else {
		_searchpaths[SP_PERSONAL_DIR] = NULL;
	}

	if (SUCCEEDED(OTTDSHGetFolderPath(NULL, CSIDL_COMMON_DOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path))) {
		strecpy(tmp, FS2OTTD(path), lastof(tmp));
		AppendPathSeparator(tmp, lastof(tmp));
		strecat(tmp, PERSONAL_DIR, lastof(tmp));
		AppendPathSeparator(tmp, lastof(tmp));
		_searchpaths[SP_SHARED_DIR] = stredup(tmp);
	} else {
		_searchpaths[SP_SHARED_DIR] = NULL;
	}
#else
	_searchpaths[SP_PERSONAL_DIR] = NULL;
	_searchpaths[SP_SHARED_DIR]   = NULL;
#endif

	/* Get the path to working directory of OpenTTD */
	getcwd(tmp, lengthof(tmp));
	AppendPathSeparator(tmp, lastof(tmp));
	_searchpaths[SP_WORKING_DIR] = stredup(tmp);

	if (!GetModuleFileName(NULL, path, lengthof(path))) {
		DEBUG(misc, 0, "GetModuleFileName failed (%lu)\n", GetLastError());
		_searchpaths[SP_BINARY_DIR] = NULL;
	} else {
		TCHAR exec_dir[MAX_PATH];
		_tcsncpy(path, convert_to_fs(exe, path, lengthof(path)), lengthof(path));
		if (!GetFullPathName(path, lengthof(exec_dir), exec_dir, NULL)) {
			DEBUG(misc, 0, "GetFullPathName failed (%lu)\n", GetLastError());
			_searchpaths[SP_BINARY_DIR] = NULL;
		} else {
			strecpy(tmp, convert_from_fs(exec_dir, tmp, lengthof(tmp)), lastof(tmp));
			char *s = strrchr(tmp, PATHSEPCHAR);
			*(s + 1) = '\0';
			_searchpaths[SP_BINARY_DIR] = stredup(tmp);
		}
	}

	_searchpaths[SP_INSTALLATION_DIR]       = NULL;
	_searchpaths[SP_APPLICATION_BUNDLE_DIR] = NULL;
}


bool GetClipboardContents(char *buffer, const char *last)
{
	HGLOBAL cbuf;
	const char *ptr;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_UNICODETEXT);

		ptr = (const char*)GlobalLock(cbuf);
		int out_len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)ptr, -1, buffer, (last - buffer) + 1, NULL, NULL);
		GlobalUnlock(cbuf);
		CloseClipboard();

		if (out_len == 0) return false;
#if !defined(UNICODE)
	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_TEXT);

		ptr = (const char*)GlobalLock(cbuf);
		strecpy(buffer, FS2OTTD(ptr), last);

		GlobalUnlock(cbuf);
		CloseClipboard();
#endif /* UNICODE */
	} else {
		return false;
	}

	return true;
}


void CSleep(int milliseconds)
{
	Sleep(milliseconds);
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
 * WM_INPUTLANGCHANGE
 */
const char *FS2OTTD(const TCHAR *name)
{
	static char utf8_buf[512];
	return convert_from_fs(name, utf8_buf, lengthof(utf8_buf));
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
 * @param console_cp convert to the console encoding instead of the normal system encoding.
 * @return pointer to the converted string; if failed string is of zero-length
 */
const TCHAR *OTTD2FS(const char *name, bool console_cp)
{
	static TCHAR system_buf[512];
	return convert_to_fs(name, system_buf, lengthof(system_buf), console_cp);
}


/**
 * Convert to OpenTTD's encoding from that of the environment in
 * UNICODE. OpenTTD encoding is UTF8, local is wide
 * @param name pointer to a valid string that will be converted
 * @param utf8_buf pointer to a valid buffer that will receive the converted string
 * @param buflen length in characters of the receiving buffer
 * @return pointer to utf8_buf. If conversion fails the string is of zero-length
 */
char *convert_from_fs(const TCHAR *name, char *utf8_buf, size_t buflen)
{
#if defined(UNICODE)
	const WCHAR *wide_buf = name;
#else
	/* Convert string from the local codepage to UTF-16. */
	int wide_len = MultiByteToWideChar(CP_ACP, 0, name, -1, NULL, 0);
	if (wide_len == 0) {
		utf8_buf[0] = '\0';
		return utf8_buf;
	}

	WCHAR *wide_buf = AllocaM(WCHAR, wide_len);
	MultiByteToWideChar(CP_ACP, 0, name, -1, wide_buf, wide_len);
#endif

	/* Convert UTF-16 string to UTF-8. */
	int len = WideCharToMultiByte(CP_UTF8, 0, wide_buf, -1, utf8_buf, (int)buflen, NULL, NULL);
	if (len == 0) utf8_buf[0] = '\0';

	return utf8_buf;
}


/**
 * Convert from OpenTTD's encoding to that of the environment in
 * UNICODE. OpenTTD encoding is UTF8, local is wide
 * @param name pointer to a valid string that will be converted
 * @param utf16_buf pointer to a valid wide-char buffer that will receive the
 * converted string
 * @param buflen length in wide characters of the receiving buffer
 * @param console_cp convert to the console encoding instead of the normal system encoding.
 * @return pointer to utf16_buf. If conversion fails the string is of zero-length
 */
TCHAR *convert_to_fs(const char *name, TCHAR *system_buf, size_t buflen, bool console_cp)
{
#if defined(UNICODE)
	int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, system_buf, (int)buflen);
	if (len == 0) system_buf[0] = '\0';
#else
	int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
	if (len == 0) {
		system_buf[0] = '\0';
		return system_buf;
	}

	WCHAR *wide_buf = AllocaM(WCHAR, len);
	MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_buf, len);

	len = WideCharToMultiByte(console_cp ? CP_OEMCP : CP_ACP, 0, wide_buf, len, system_buf, (int)buflen, NULL, NULL);
	if (len == 0) system_buf[0] = '\0';
#endif

	return system_buf;
}

/**
 * Our very own SHGetFolderPath function for support of windows operating
 * systems that don't have this function (eg Win9x, etc.). We try using the
 * native function, and if that doesn't exist we will try a more crude approach
 * of environment variables and hope for the best
 */
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
		/* The function lives in shell32.dll for all current Windows versions, but it first started to appear in SHFolder.dll. */
		if (!LoadLibraryList((Function*)&SHGetFolderPath, "shell32.dll\0" W("SHGetFolderPath") "\0\0")) {
			if (!LoadLibraryList((Function*)&SHGetFolderPath, "SHFolder.dll\0" W("SHGetFolderPath") "\0\0")) {
				DEBUG(misc, 0, "Unable to load " W("SHGetFolderPath") "from either shell32.dll or SHFolder.dll");
			}
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
			case CSIDL_FONTS: // Get the system font path, eg %WINDIR%\Fonts
				ret = GetEnvironmentVariable(_T("WINDIR"), pszPath, MAX_PATH);
				if (ret == 0) break;
				_tcsncat(pszPath, _T("\\Fonts"), MAX_PATH);

				return (HRESULT)0;

			case CSIDL_PERSONAL:
			case CSIDL_COMMON_DOCUMENTS: {
				HKEY key;
				if (RegOpenKeyEx(csidl == CSIDL_PERSONAL ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, REGSTR_PATH_SPECIAL_FOLDERS, 0, KEY_READ, &key) != ERROR_SUCCESS) break;
				DWORD len = MAX_PATH;
				ret = RegQueryValueEx(key, csidl == CSIDL_PERSONAL ? _T("Personal") : _T("Common Documents"), NULL, NULL, (LPBYTE)pszPath, &len);
				RegCloseKey(key);
				if (ret == ERROR_SUCCESS) return (HRESULT)0;
				break;
			}

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

uint GetCPUCoreCount()
{
	SYSTEM_INFO info;

	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}

#ifdef _MSC_VER
/* Code from MSDN: https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx */
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct {
	DWORD dwType;     ///< Must be 0x1000.
	LPCSTR szName;    ///< Pointer to name (in user addr space).
	DWORD dwThreadID; ///< Thread ID (-1=caller thread).
	DWORD dwFlags;    ///< Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

/**
 * Signal thread name to any attached debuggers.
 */
void SetWin32ThreadName(DWORD dwThreadID, const char* threadName)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

#pragma warning(push)
#pragma warning(disable: 6320 6322)
	__try {
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
#pragma warning(pop)
}
#endif
