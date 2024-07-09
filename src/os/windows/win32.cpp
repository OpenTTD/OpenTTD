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
#include "../../library_loader.h"

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

bool FiosIsRoot(const std::string &file)
{
	return file.size() == 3; // C:\...
}

void FiosGetDrives(FileList &file_list)
{
	wchar_t drives[256];
	const wchar_t *s;

	GetLogicalDriveStrings(static_cast<DWORD>(std::size(drives)), drives);
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

bool FiosIsHiddenFile(const std::filesystem::path &path)
{
	UINT sem = SetErrorMode(SEM_FAILCRITICALERRORS); // Disable 'no-disk' message box.

	DWORD attributes = GetFileAttributes(path.c_str());

	SetErrorMode(sem); // Restore previous setting.

	return (attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
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

#if defined(_MSC_VER)
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

/**
 * Replace linefeeds with carriage-return and linefeed.
 * @param msg string with LF linefeeds.
 * @return String with Lf linefeeds converted to CrLf linefeeds.
 */
static std::string ConvertLfToCrLf(std::string_view msg)
{
	std::string output;

	size_t last = 0;
	size_t next = 0;
	while ((next = msg.find('\n', last)) != std::string_view::npos) {
		output += msg.substr(last, next - last);
		output += "\r\n";
		last = next + 1;
	}
	output += msg.substr(last);

	return output;
}

/** Callback function to handle the window */
static INT_PTR CALLBACK HelpDialogFunc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG: {
			std::wstring &msg = *reinterpret_cast<std::wstring *>(lParam);
			SetDlgItemText(wnd, 11, msg.c_str());
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
		std::wstring native_str = OTTD2FS(ConvertLfToCrLf(str));
		if (native_str.size() > 2048) {
			/* The minimum length of the help message is 2048. Other messages sent via
			 * ShowInfo are much shorter, or so long they need this way of displaying
			 * them anyway. */
			DialogBoxParam(GetModuleHandle(nullptr), MAKEINTRESOURCE(101), nullptr, HelpDialogFunc, reinterpret_cast<LPARAM>(&native_str));
		} else {
			MessageBox(GetActiveWindow(), native_str.c_str(), L"OpenTTD", MB_ICONINFORMATION | MB_OK);
		}
		MyShowCursor(old);
	}
}

char *getcwd(char *buf, size_t size)
{
	wchar_t path[MAX_PATH];
	GetCurrentDirectory(MAX_PATH - 1, path);
	convert_from_fs(path, {buf, size});
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
		convert_to_fs(_config_file, path);
		if (!GetFullPathName(path, static_cast<DWORD>(std::size(config_dir)), config_dir, nullptr)) {
			Debug(misc, 0, "GetFullPathName failed ({})", GetLastError());
			_searchpaths[SP_WORKING_DIR].clear();
		} else {
			std::string tmp(FS2OTTD(config_dir));
			auto pos = tmp.find_last_of(PATHSEPCHAR);
			if (pos != std::string::npos) tmp.erase(pos + 1);

			_searchpaths[SP_WORKING_DIR] = tmp;
		}
	}

	if (!GetModuleFileName(nullptr, path, static_cast<DWORD>(std::size(path)))) {
		Debug(misc, 0, "GetModuleFileName failed ({})", GetLastError());
		_searchpaths[SP_BINARY_DIR].clear();
	} else {
		wchar_t exec_dir[MAX_PATH];
		convert_to_fs(exe, path);
		if (!GetFullPathName(path, static_cast<DWORD>(std::size(exec_dir)), exec_dir, nullptr)) {
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
 * @param src wide string that will be converted
 * @param dst_buf span of valid char buffer that will receive the converted string
 * @return pointer to dst_buf. If conversion fails the string is of zero-length
 */
char *convert_from_fs(const std::wstring_view src, std::span<char> dst_buf)
{
	/* Convert UTF-16 string to UTF-8. */
	int len = WideCharToMultiByte(CP_UTF8, 0, src.data(), static_cast<int>(src.size()), dst_buf.data(), static_cast<int>(dst_buf.size() - 1U), nullptr, nullptr);
	dst_buf[len] = '\0';

	return dst_buf.data();
}


/**
 * Convert from OpenTTD's encoding to that of the environment in
 * UNICODE. OpenTTD encoding is UTF8, local is wide.
 * @param src string that will be converted
 * @param dst_buf span of valid wide-char buffer that will receive the converted string
 * @return pointer to dst_buf. If conversion fails the string is of zero-length
 */
wchar_t *convert_to_fs(const std::string_view src, std::span<wchar_t> dst_buf)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, src.data(), static_cast<int>(src.size()), dst_buf.data(), static_cast<int>(dst_buf.size() - 1U));
	dst_buf[len] = '\0';

	return dst_buf.data();
}

/** Determine the current user's locale. */
const char *GetCurrentLocale(const char *)
{
	const LANGID userUiLang = GetUserDefaultUILanguage();
	const LCID userUiLocale = MAKELCID(userUiLang, SORT_DEFAULT);

	char lang[9], country[9];
	if (GetLocaleInfoA(userUiLocale, LOCALE_SISO639LANGNAME, lang, static_cast<int>(std::size(lang))) == 0 ||
	    GetLocaleInfoA(userUiLocale, LOCALE_SISO3166CTRYNAME, country, static_cast<int>(std::size(country))) == 0) {
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

	MultiByteToWideChar(CP_UTF8, 0, iso_code.c_str(), -1, _cur_iso_locale, static_cast<int>(std::size(_cur_iso_locale)));
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
		static LibraryLoader _kernel32("Kernel32.dll");
		_CompareStringEx = _kernel32.GetFunction("CompareStringEx");
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
	convert_to_fs(s1, s1_buf);
	convert_to_fs(s2, s2_buf);

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
		static LibraryLoader _kernel32("Kernel32.dll");
		_FindNLSStringEx = _kernel32.GetFunction("FindNLSStringEx");
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
