/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unix.cpp Implementation of Unix specific file handling. */

#include "../../stdafx.h"
#include "../../textbuf_gui.h"
#include "../../debug.h"
#include "../../string_func.h"
#include "../../fios.h"
#include "../../thread.h"


#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#ifdef WITH_SDL2
#include <SDL.h>
#endif

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif

#ifdef __APPLE__
#	include <sys/mount.h>
#elif (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) || defined(__GLIBC__)
#	define HAS_STATVFS
#endif

#if defined(OPENBSD) || defined(__NetBSD__) || defined(__FreeBSD__)
#	define HAS_SYSCTL
#endif

#ifdef HAS_STATVFS
#include <sys/statvfs.h>
#endif

#ifdef HAS_SYSCTL
#include <sys/sysctl.h>
#endif

#if defined(__APPLE__)
#	include "../macosx/macos.h"
#endif

#include "../../safeguards.h"

bool FiosIsRoot(const std::string &path)
{
	return path == PATHSEP;
}

void FiosGetDrives(FileList &)
{
	return;
}

std::optional<uint64_t> FiosGetDiskFreeSpace(const std::string &path)
{
#ifdef __APPLE__
	struct statfs s;

	if (statfs(path.c_str(), &s) == 0) return static_cast<uint64_t>(s.f_bsize) * s.f_bavail;
#elif defined(HAS_STATVFS)
	struct statvfs s;

	if (statvfs(path.c_str(), &s) == 0) return static_cast<uint64_t>(s.f_frsize) * s.f_bavail;
#endif
	return std::nullopt;
}

bool FiosIsValidFile(const std::string &path, const struct dirent *ent, struct stat *sb)
{
	assert(path.back() == PATHSEPCHAR);
	if (path.size() > 2) assert(path[path.size() - 2] != PATHSEPCHAR);
	std::string filename = fmt::format("{}{}", path, ent->d_name);

	return stat(filename.c_str(), sb) == 0;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return ent->d_name[0] == '.';
}

#ifdef WITH_ICONV

#include <iconv.h>
#include "../../debug.h"
#include "../../string_func.h"

const char *GetCurrentLocale(const char *param);

#define INTERNALCODE "UTF-8"

/**
 * Try and try to decipher the current locale from environmental
 * variables. MacOSX is hardcoded, other OS's are dynamic. If no suitable
 * locale can be found, don't do any conversion ""
 */
static const char *GetLocalCode()
{
#if defined(__APPLE__)
	return "UTF-8-MAC";
#else
	/* Strip locale (eg en_US.UTF-8) to only have UTF-8 */
	const char *locale = GetCurrentLocale("LC_CTYPE");
	if (locale != nullptr) locale = strchr(locale, '.');

	return (locale == nullptr) ? "" : locale + 1;
#endif
}

/**
 * Convert between locales, which from and which to is set in the calling
 * functions OTTD2FS() and FS2OTTD().
 */
static std::string convert_tofrom_fs(iconv_t convd, const std::string &name)
{
	/* There are different implementations of iconv. The older ones,
	 * e.g. SUSv2, pass a const pointer, whereas the newer ones, e.g.
	 * IEEE 1003.1 (2004), pass a non-const pointer. */
#ifdef HAVE_NON_CONST_ICONV
	char *inbuf = const_cast<char*>(name.data());
#else
	const char *inbuf = name.data();
#endif

	/* If the output is UTF-32, then 1 ASCII character becomes 4 bytes. */
	size_t inlen = name.size();
	std::string buf(inlen * 4, '\0');

	size_t outlen = buf.size();
	char *outbuf = buf.data();
	iconv(convd, nullptr, nullptr, nullptr, nullptr);
	if (iconv(convd, &inbuf, &inlen, &outbuf, &outlen) == (size_t)(-1)) {
		Debug(misc, 0, "[iconv] error converting '{}'. Errno {}", name, errno);
		return name;
	}

	buf.resize(outbuf - buf.data());
	return buf;
}

/**
 * Convert from OpenTTD's encoding to that of the local environment
 * @param name pointer to a valid string that will be converted
 * @return pointer to a new stringbuffer that contains the converted string
 */
std::string OTTD2FS(const std::string &name)
{
	static iconv_t convd = (iconv_t)(-1);
	if (convd == (iconv_t)(-1)) {
		const char *env = GetLocalCode();
		convd = iconv_open(env, INTERNALCODE);
		if (convd == (iconv_t)(-1)) {
			Debug(misc, 0, "[iconv] conversion from codeset '{}' to '{}' unsupported", INTERNALCODE, env);
			return name;
		}
	}

	return convert_tofrom_fs(convd, name);
}

/**
 * Convert to OpenTTD's encoding from that of the local environment
 * @param name valid string that will be converted
 * @return pointer to a new stringbuffer that contains the converted string
 */
std::string FS2OTTD(const std::string &name)
{
	static iconv_t convd = (iconv_t)(-1);
	if (convd == (iconv_t)(-1)) {
		const char *env = GetLocalCode();
		convd = iconv_open(INTERNALCODE, env);
		if (convd == (iconv_t)(-1)) {
			Debug(misc, 0, "[iconv] conversion from codeset '{}' to '{}' unsupported", env, INTERNALCODE);
			return name;
		}
	}

	return convert_tofrom_fs(convd, name);
}

#endif /* WITH_ICONV */

void ShowInfoI(const std::string &str)
{
	fmt::print(stderr, "{}\n", str);
}

#if !defined(__APPLE__)
void ShowOSErrorBox(const char *buf, bool)
{
	/* All unix systems, except OSX. Only use escape codes on a TTY. */
	if (isatty(fileno(stderr))) {
		fmt::print(stderr, "\033[1;31mError: {}\033[0;39m\n", buf);
	} else {
		fmt::print(stderr, "Error: {}\n", buf);
	}
}
#endif

#ifndef WITH_COCOA
std::optional<std::string> GetClipboardContents()
{
#ifdef WITH_SDL2
	if (SDL_HasClipboardText() == SDL_FALSE) return std::nullopt;

	char *clip = SDL_GetClipboardText();
	if (clip != nullptr) {
		std::string result = clip;
		SDL_free(clip);
		return result;
	}
#endif

	return std::nullopt;
}
#endif


#if defined(__EMSCRIPTEN__)
void OSOpenBrowser(const std::string &url)
{
	/* Implementation in pre.js */
	EM_ASM({ if (window["openttd_open_url"]) window.openttd_open_url($0, $1) }, url.c_str(), url.size());
}
#elif !defined( __APPLE__)
void OSOpenBrowser(const std::string &url)
{
	pid_t child_pid = fork();
	if (child_pid != 0) return;

	const char *args[3];
	args[0] = "xdg-open";
	args[1] = url.c_str();
	args[2] = nullptr;
	execvp(args[0], const_cast<char * const *>(args));
	Debug(misc, 0, "Failed to open url: {}", url);
	exit(0);
}
#endif /* __APPLE__ */

void SetCurrentThreadName([[maybe_unused]] const char *threadName)
{
#if defined(__GLIBC__)
	if (threadName) pthread_setname_np(pthread_self(), threadName);
#endif /* defined(__GLIBC__) */
#if defined(__APPLE__)
	MacOSSetThreadName(threadName);
#endif /* defined(__APPLE__) */
}
