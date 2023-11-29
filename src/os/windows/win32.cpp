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
#include <mmsystem.h>
#include <regstr.h>
#define NO_SHOBJIDL_SORTDIRECTION // Avoid multiple definition of SORT_ASCENDING
#include <shlobj.h> /* SHGetFolderPath */
#include <shellapi.h>
#include <WinNls.h>
#include "win32.h"
#include "../../fios.h"
#include "../../core/alloc_func.hpp"
#include "../../string_func.h"
#include <sys/stat.h>
#include "../../language.h"
#include "../../thread.h"

#include "../../safeguards.h"

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

void ShowOSErrorBox(const char *buf, bool)
{
	MyShowCursor(true);
	MessageBox(GetActiveWindow(), OTTD2FS(buf).c_str(), L"Error!", MB_ICONSTOP | MB_TASKMODAL);
}

void OSOpenBrowser(const std::string &url)
{
	ShellExecute(GetActiveWindow(), L"open", OTTD2FS(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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

DIR *opendir(const wchar_t *path)
{
	DIR *d;
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS); // disable 'no-disk' message box
	DWORD fa = GetFileAttributes(path);

	if ((fa != INVALID_FILE_ATTRIBUTES) && (fa & FILE_ATTRIBUTE_DIRECTORY)) {
		d = dir_calloc();
		if (d != nullptr) {
			std::wstring search_path = path;
			bool slash = path[wcslen(path) - 1] == '\\';

			/* build search path for FindFirstFile, try not to append additional slashes
			 * as it throws Win9x off its groove for root directories */
			if (!slash) search_path += L"\\";
			search_path += L"*";
			d->hFind = FindFirstFile(search_path.c_str(), &d->fd);

			if (d->hFind != INVALID_HANDLE_VALUE ||
					GetLastError() == ERROR_NO_MORE_FILES) { // the directory is empty
				d->ent.dir = d;
				d->at_first_entry = true;
			} else {
				dir_free(d);
				d = nullptr;
			}
		} else {
			errno = ENOMEM;
		}
	} else {
		/* path not found or not a directory */
		d = nullptr;
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
		if (d->hFind == INVALID_HANDLE_VALUE) return nullptr;
		d->at_first_entry = false;
	} else if (!FindNextFile(d->hFind, &d->fd)) { // determine cause and bail
		if (GetLastError() == ERROR_NO_MORE_FILES) SetLastError(prev_err);
		return nullptr;
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

bool FiosIsRoot(const std::string &file)
{
	return file.size() == 3; // C:\...
}

void FiosGetDrives(FileList &file_list)
{
	wchar_t drives[256];
	const wchar_t *s;

	GetLogicalDriveStrings(lengthof(drives), drives);
	for (s = drives; *s != '\0';) {
		FiosItem *fios = &file_list.emplace_back();
		fios->type = FIOS_TYPE_DRIVE;
		fios->mtime = 0;
		fios->name += (char)(s[0] & 0xFF);
		fios->name += ':';
		fios->title = fios->name;
		while (*s++ != '\0') { /* Nothing */ }
	}
}

bool FiosIsValidFile(const std::string &, const struct dirent *ent, struct stat *sb)
{
	/* hectonanoseconds between Windows and POSIX epoch */
	static const int64_t posix_epoch_hns = 0x019DB1DED53E8000LL;
	const WIN32_FIND_DATA *fd = &ent->dir->fd;

	sb->st_size  = ((uint64_t) fd->nFileSizeHigh << 32) + fd->nFileSizeLow;
	/* UTC FILETIME to seconds-since-1970 UTC
	 * we just have to subtract POSIX epoch and scale down to units of seconds.
	 * http://www.gamedev.net/community/forums/topic.asp?topic_id=294070&whichpage=1&#1860504
	 * XXX - not entirely correct, since filetimes on FAT aren't UTC but local,
	 * this won't entirely be correct, but we use the time only for comparison. */
	sb->st_mtime = (time_t)((*(const uint64_t*)&fd->ftLastWriteTime - posix_epoch_hns) / 1E7);
	sb->st_mode  = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)? S_IFDIR : S_IFREG;

	return true;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return (ent->dir->fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

std::optional<uint64_t> FiosGetDiskFreeSpace(const std::string &path)
{
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS);  // disable 'no-disk' message box

	ULARGE_INTEGER bytes_free;
	bool retval = GetDiskFreeSpaceEx(OTTD2FS(path).c_str(), &bytes_free, nullptr, nullptr);

	SetErrorMode(sem); // reset previous setting

	if (retval) return bytes_free.QuadPart;
	return std::nullopt;
}

void CreateConsole()
{
	HANDLE hand;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	if (_has_console) return;
	_has_console = true;

	if (!AllocConsole()) return;

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

	setvbuf(stdin, nullptr, _IONBF, 0);
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);
}

/** Temporary pointer to get the help message to the window */
static const char *_help_msg;

/** Callback function to handle the window */
static INT_PTR CALLBACK HelpDialogFunc(HWND wnd, UINT msg, WPARAM wParam, LPARAM)
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
			wchar_t help_msg_buf[8192];
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

void ShowInfoI(const std::string &str)
{
	if (_has_console) {
		fmt::print(stderr, "{}\n", str);
	} else {
		bool old;
		ReleaseCapture();
		_left_button_clicked = _left_button_down = false;

		old = MyShowCursor(true);
		if (str.size() > 2048) {
			/* The minimum length of the help message is 2048. Other messages sent via
			 * ShowInfo are much shorter, or so long they need this way of displaying
			 * them anyway. */
			_help_msg = str.c_str();
			DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(101), nullptr, HelpDialogFunc);
		} else {
			/* We need to put the text in a separate buffer because the default
			 * buffer in OTTD2FS might not be large enough (512 chars). */
			wchar_t help_msg_buf[8192];
			MessageBox(GetActiveWindow(), convert_to_fs(str, help_msg_buf, lengthof(help_msg_buf)), L"OpenTTD", MB_ICONINFORMATION | MB_OK);
		}
		MyShowCursor(old);
	}
}

char *getcwd(char *buf, size_t size)
{
	wchar_t path[MAX_PATH];
	GetCurrentDirectory(MAX_PATH - 1, path);
	convert_from_fs(path, buf, size);
	return buf;
}

extern std::string _config_file;

void DetermineBasePaths(const char *exe)
{
	extern std::array<std::string, NUM_SEARCHPATHS> _searchpaths;

	wchar_t path[MAX_PATH];
#ifdef WITH_PERSONAL_DIR
	if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, path))) {
		std::string tmp(FS2OTTD(path));
		AppendPathSeparator(tmp);
		tmp += PERSONAL_DIR;
		AppendPathSeparator(tmp);
		_searchpaths[SP_PERSONAL_DIR] = tmp;

		tmp += "content_download";
		AppendPathSeparator(tmp);
		_searchpaths[SP_AUTODOWNLOAD_PERSONAL_DIR] = tmp;
	} else {
		_searchpaths[SP_PERSONAL_DIR].clear();
	}

	if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_COMMON_DOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, path))) {
		std::string tmp(FS2OTTD(path));
		AppendPathSeparator(tmp);
		tmp += PERSONAL_DIR;
		AppendPathSeparator(tmp);
		_searchpaths[SP_SHARED_DIR] = tmp;
	} else {
		_searchpaths[SP_SHARED_DIR].clear();
	}
#else
	_searchpaths[SP_PERSONAL_DIR].clear();
	_searchpaths[SP_SHARED_DIR].clear();
#endif

	if (_config_file.empty()) {
		char cwd[MAX_PATH];
		getcwd(cwd, lengthof(cwd));
		std::string cwd_s(cwd);
		AppendPathSeparator(cwd_s);
		_searchpaths[SP_WORKING_DIR] = cwd_s;
	} else {
		/* Use the folder of the config file as working directory. */
		wchar_t config_dir[MAX_PATH];
		wcsncpy(path, convert_to_fs(_config_file, path, lengthof(path)), lengthof(path));
		if (!GetFullPathName(path, lengthof(config_dir), config_dir, nullptr)) {
			Debug(misc, 0, "GetFullPathName failed ({})", GetLastError());
			_searchpaths[SP_WORKING_DIR].clear();
		} else {
			std::string tmp(FS2OTTD(config_dir));
			auto pos = tmp.find_last_of(PATHSEPCHAR);
			if (pos != std::string::npos) tmp.erase(pos + 1);

			_searchpaths[SP_WORKING_DIR] = tmp;
		}
	}

	if (!GetModuleFileName(nullptr, path, lengthof(path))) {
		Debug(misc, 0, "GetModuleFileName failed ({})", GetLastError());
		_searchpaths[SP_BINARY_DIR].clear();
	} else {
		wchar_t exec_dir[MAX_PATH];
		wcsncpy(path, convert_to_fs(exe, path, lengthof(path)), lengthof(path));
		if (!GetFullPathName(path, lengthof(exec_dir), exec_dir, nullptr)) {
			Debug(misc, 0, "GetFullPathName failed ({})", GetLastError());
			_searchpaths[SP_BINARY_DIR].clear();
		} else {
			std::string tmp(FS2OTTD(exec_dir));
			auto pos = tmp.find_last_of(PATHSEPCHAR);
			if (pos != std::string::npos) tmp.erase(pos + 1);

			_searchpaths[SP_BINARY_DIR] = tmp;
		}
	}

	_searchpaths[SP_INSTALLATION_DIR].clear();
	_searchpaths[SP_APPLICATION_BUNDLE_DIR].clear();
}


std::optional<std::string> GetClipboardContents()
{
	if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return std::nullopt;

	OpenClipboard(nullptr);
	HGLOBAL cbuf = GetClipboardData(CF_UNICODETEXT);

	std::string result = FS2OTTD(static_cast<LPCWSTR>(GlobalLock(cbuf)));
	GlobalUnlock(cbuf);
	CloseClipboard();

	if (result.empty()) return std::nullopt;
	return result;
}


/**
 * Convert to OpenTTD's encoding from a wide string.
 * OpenTTD internal encoding is UTF8.
 * @param name valid string that will be converted (local, or wide)
 * @return converted string; if failed string is of zero-length
 * @see the current code-page comes from video\win32_v.cpp, event-notification
 * WM_INPUTLANGCHANGE
 */
std::string FS2OTTD(const std::wstring &name)
{
	int name_len = (name.length() >= INT_MAX) ? INT_MAX : (int)name.length();
	int len = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), name_len, nullptr, 0, nullptr, nullptr);
	if (len <= 0) return std::string();
	std::string utf8_buf(len, '\0'); // len includes terminating null
	WideCharToMultiByte(CP_UTF8, 0, name.c_str(), name_len, utf8_buf.data(), len, nullptr, nullptr);
	return utf8_buf;
}

/**
 * Convert from OpenTTD's encoding to a wide string.
 * OpenTTD internal encoding is UTF8.
 * @param name valid string that will be converted (UTF8)
 * @param console_cp convert to the console encoding instead of the normal system encoding.
 * @return converted string; if failed string is of zero-length
 */
std::wstring OTTD2FS(const std::string &name)
{
	int name_len = (name.length() >= INT_MAX) ? INT_MAX : (int)name.length();
	int len = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name_len, nullptr, 0);
	if (len <= 0) return std::wstring();
	std::wstring system_buf(len, L'\0'); // len includes terminating null
	MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name_len, system_buf.data(), len);
	return system_buf;
}


/**
 * Convert to OpenTTD's encoding from that of the environment in
 * UNICODE. OpenTTD encoding is UTF8, local is wide.
 * @param name pointer to a valid string that will be converted
 * @param utf8_buf pointer to a valid buffer that will receive the converted string
 * @param buflen length in characters of the receiving buffer
 * @return pointer to utf8_buf. If conversion fails the string is of zero-length
 */
char *convert_from_fs(const wchar_t *name, char *utf8_buf, size_t buflen)
{
	/* Convert UTF-16 string to UTF-8. */
	int len = WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8_buf, (int)buflen, nullptr, nullptr);
	if (len == 0) utf8_buf[0] = '\0';

	return utf8_buf;
}


/**
 * Convert from OpenTTD's encoding to that of the environment in
 * UNICODE. OpenTTD encoding is UTF8, local is wide.
 * @param name pointer to a valid string that will be converted
 * @param system_buf pointer to a valid wide-char buffer that will receive the
 * converted string
 * @param buflen length in wide characters of the receiving buffer
 * @param console_cp convert to the console encoding instead of the normal system encoding.
 * @return pointer to system_buf. If conversion fails the string is of zero-length
 */
wchar_t *convert_to_fs(const std::string_view name, wchar_t *system_buf, size_t buflen)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, name.data(), (int)name.size(), system_buf, (int)buflen);
	system_buf[len] = '\0';

	return system_buf;
}

/** Determine the current user's locale. */
const char *GetCurrentLocale(const char *)
{
	const LANGID userUiLang = GetUserDefaultUILanguage();
	const LCID userUiLocale = MAKELCID(userUiLang, SORT_DEFAULT);

	char lang[9], country[9];
	if (GetLocaleInfoA(userUiLocale, LOCALE_SISO639LANGNAME, lang, lengthof(lang)) == 0 ||
	    GetLocaleInfoA(userUiLocale, LOCALE_SISO3166CTRYNAME, country, lengthof(country)) == 0) {
		/* Unable to retrieve the locale. */
		return nullptr;
	}
	/* Format it as 'en_us'. */
	static char retbuf[6] = {lang[0], lang[1], '_', country[0], country[1], 0};
	return retbuf;
}


static WCHAR _cur_iso_locale[16] = L"";

void Win32SetCurrentLocaleName(std::string iso_code)
{
	/* Convert the iso code into the format that windows expects. */
	if (iso_code == "zh_TW") {
		iso_code = "zh-Hant";
	} else if (iso_code == "zh_CN") {
		iso_code = "zh-Hans";
	} else {
		/* Windows expects a '-' between language and country code, but we use a '_'. */
		for (char &c : iso_code) {
			if (c == '_') c = '-';
		}
	}

	MultiByteToWideChar(CP_UTF8, 0, iso_code.c_str(), -1, _cur_iso_locale, lengthof(_cur_iso_locale));
}

int OTTDStringCompare(std::string_view s1, std::string_view s2)
{
	typedef int (WINAPI *PFNCOMPARESTRINGEX)(LPCWSTR, DWORD, LPCWCH, int, LPCWCH, int, LPVOID, LPVOID, LPARAM);
	static PFNCOMPARESTRINGEX _CompareStringEx = nullptr;
	static bool first_time = true;

#ifndef SORT_DIGITSASNUMBERS
#	define SORT_DIGITSASNUMBERS 0x00000008  // use digits as numbers sort method
#endif
#ifndef LINGUISTIC_IGNORECASE
#	define LINGUISTIC_IGNORECASE 0x00000010 // linguistically appropriate 'ignore case'
#endif

	if (first_time) {
		static DllLoader _kernel32(L"Kernel32.dll");
		_CompareStringEx = _kernel32.GetProcAddress("CompareStringEx");
		first_time = false;
	}

	if (_CompareStringEx != nullptr) {
		/* CompareStringEx takes UTF-16 strings, even in ANSI-builds. */
		int len_s1 = MultiByteToWideChar(CP_UTF8, 0, s1.data(), (int)s1.size(), nullptr, 0);
		int len_s2 = MultiByteToWideChar(CP_UTF8, 0, s2.data(), (int)s2.size(), nullptr, 0);

		if (len_s1 != 0 && len_s2 != 0) {
			std::wstring str_s1(len_s1, L'\0'); // len includes terminating null
			std::wstring str_s2(len_s2, L'\0');

			MultiByteToWideChar(CP_UTF8, 0, s1.data(), (int)s1.size(), str_s1.data(), len_s1);
			MultiByteToWideChar(CP_UTF8, 0, s2.data(), (int)s2.size(), str_s2.data(), len_s2);

			int result = _CompareStringEx(_cur_iso_locale, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS, str_s1.c_str(), -1, str_s2.c_str(), -1, nullptr, nullptr, 0);
			if (result != 0) return result;
		}
	}

	wchar_t s1_buf[512], s2_buf[512];
	convert_to_fs(s1, s1_buf, lengthof(s1_buf));
	convert_to_fs(s2, s2_buf, lengthof(s2_buf));

	return CompareString(MAKELCID(_current_language->winlangid, SORT_DEFAULT), NORM_IGNORECASE, s1_buf, -1, s2_buf, -1);
}

/**
 * Search if a string is contained in another string using the current locale.
 *
 * @param str String to search in.
 * @param value String to search for.
 * @param case_insensitive Search case-insensitive.
 * @return 1 if value was found, 0 if it was not found, or -1 if not supported by the OS.
 */
int Win32StringContains(const std::string_view str, const std::string_view value, bool case_insensitive)
{
	typedef int (WINAPI *PFNFINDNLSSTRINGEX)(LPCWSTR, DWORD, LPCWSTR, int, LPCWSTR, int, LPINT, LPNLSVERSIONINFO, LPVOID, LPARAM);
	static PFNFINDNLSSTRINGEX _FindNLSStringEx = nullptr;
	static bool first_time = true;

	if (first_time) {
		static DllLoader _kernel32(L"Kernel32.dll");
		_FindNLSStringEx = _kernel32.GetProcAddress("FindNLSStringEx");
		first_time = false;
	}

	if (_FindNLSStringEx != nullptr) {
		int len_str = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
		int len_value = MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0);

		if (len_str != 0 && len_value != 0) {
			std::wstring str_str(len_str, L'\0'); // len includes terminating null
			std::wstring str_value(len_value, L'\0');

			MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), str_str.data(), len_str);
			MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), str_value.data(), len_value);

			return _FindNLSStringEx(_cur_iso_locale, FIND_FROMSTART | (case_insensitive ? LINGUISTIC_IGNORECASE : 0), str_str.data(), -1, str_value.data(), -1, nullptr, nullptr, nullptr, 0) >= 0 ? 1 : 0;
		}
	}

	return -1; // Failure indication.
}

#ifdef _MSC_VER
/* Based on code from MSDN: https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx */
const DWORD MS_VC_EXCEPTION = 0x406D1388;

PACK_N(struct THREADNAME_INFO {
	DWORD dwType;     ///< Must be 0x1000.
	LPCSTR szName;    ///< Pointer to name (in user addr space).
	DWORD dwThreadID; ///< Thread ID (-1=caller thread).
	DWORD dwFlags;    ///< Reserved for future use, must be zero.
}, 8);

/**
 * Signal thread name to any attached debuggers.
 */
void SetCurrentThreadName(const char *threadName)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = -1;
	info.dwFlags = 0;

#pragma warning(push)
#pragma warning(disable: 6320 6322)
	__try {
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
#pragma warning(pop)
}
#else
void SetCurrentThreadName(const char *) {}
#endif
