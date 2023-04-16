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
#include "fileio_type.h"

#include <map>
#include <fstream>
#include <regex>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

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

#ifndef NO_THREADS
#include <pthread.h>
#endif

#if defined(__APPLE__)
#	include "../macosx/macos.h"
#endif

#include "../../safeguards.h"

bool FiosIsRoot(const char *path)
{
	return path[1] == '\0';
}

#if defined(__EMSCRIPTEN__)

void FiosGetDrives(FileList &file_list)
{
	return; // nothing
}

#elif !defined(__APPLE__)

static std::string getenv_str(const char *name)
{
	char *val = getenv(name);
	if (val == nullptr) return std::string();
	else return std::string(val);
}

static std::vector<FiosItem> FindSpecialFolders()
{
	/* This algorithm is modeled after xdg_user_dir_lookup_with_fallback()
	 * in xdg-user-dir-lookup.c from the xdg-user-dirs package. */

	/* First, find the user-dirs.dirs config file */
	std::string homedir = getenv_str("HOME");
	if (homedir.empty()) return {};
	std::string config_home = getenv_str("XDG_CONFIG_HOME");
	if (config_home.empty()) config_home = homedir + "/.config";
	std::string config_file_name = config_home + "/user-dirs.dirs";

	/* Prepare a map for directory names */
	std::map<std::string, std::string> directory_map;
	directory_map["XDG_DESKTOP_DIR"] = homedir + "/Desktop"; // required compatibility default

	/* Read the config file */
	std::ifstream config_file(config_file_name);
	if (config_file.is_open()) {
		/* The lines that matter have the format:
		 *   XDG_<name>_DIR = "<path>"
		 * where <path> must be enclosed in double quotes,
		 * and may either start with $HOME, or be absolute.
		 * Within the double quotes, backslash can be used
		 * for escaping double quotes and backslashes. */
		std::regex pattern("^\\s*(XDG_[A-Z]+_DIR)\\s*=\\s*\"(\\$HOME|)(/.*)");
		while (!config_file.eof()) {
			std::string line;
			std::getline(config_file, line);
			std::smatch match;
			/* Match against the desired line format */
			if (std::regex_match(line, match, pattern) && match.size() == 4) {
				std::string directory_path = match[3];
				/* Unescape the path, and cut at first unescaped double quote */
				size_t srcpos = 0, dstpos = 0;
				while (srcpos < directory_path.size()) {
					if (directory_path[srcpos] == '"') break;
					if (directory_path[srcpos] == '\\') ++srcpos;
					directory_path[dstpos] = directory_path[srcpos];
					++srcpos;
					++dstpos;
				}
				directory_path.resize(dstpos);
				/* Adjust for homedir-relative, and store the found path */
				if (match[2] == "$HOME") directory_path = homedir + directory_path;
				directory_map[match[1]] = directory_path;
			}
		}
		config_file.close();
	}

	/* The directories we're interested in returning */
	const char *DIRNAMES[] = {
		"XDG_DESKTOP_DIR", "XDG_DOCUMENTS_DIR", "XDG_DOWNLOAD_DIR", "XDG_PICTURES_DIR"
	};

	/* Look them up in first environment, then the map from the config file */
	std::vector<FiosItem> result;
	for (std::string dirname : DIRNAMES) {
		std::string dirpath = getenv_str(dirname.c_str());
		if (dirpath.empty()) dirpath = directory_map[dirname];
		if (dirpath.empty()) continue;
		/* Check if the dir actually exists, before adding it */
		DIR *dir = opendir(dirpath.c_str());
		if (dir != nullptr) {
			std::string dir_basename = basename(dirpath.c_str());
			dirpath = dirpath + "/"; // Fios code wants them to end with PATHSEP
			result.emplace_back(FIOS_TYPE_DRIVE, dirpath, dir_basename);
			closedir(dir);
		}
	}

	return result;
}

void FiosGetDrives(FileList &file_list)
{
	static std::vector<FiosItem> cache;
	static bool cache_inited = false;

	if (!cache_inited) {
		cache = FindSpecialFolders();
		cache_inited = true;
	}

	file_list.insert(file_list.end(), cache.begin(), cache.end());
}
#endif

bool FiosGetDiskFreeSpace(const char *path, uint64 *tot)
{
	uint64 free = 0;

#ifdef __APPLE__
	struct statfs s;

	if (statfs(path, &s) != 0) return false;
	free = (uint64)s.f_bsize * s.f_bavail;
#elif defined(HAS_STATVFS)
	struct statvfs s;

	if (statvfs(path, &s) != 0) return false;
	free = (uint64)s.f_frsize * s.f_bavail;
#endif
	if (tot != nullptr) *tot = free;
	return true;
}

bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb)
{
	char filename[MAX_PATH];
	int res;
	assert(path[strlen(path) - 1] == PATHSEPCHAR);
	if (strlen(path) > 2) assert(path[strlen(path) - 2] != PATHSEPCHAR);
	res = seprintf(filename, lastof(filename), "%s%s", path, ent->d_name);

	/* Could we fully concatenate the path and filename? */
	if (res >= (int)lengthof(filename) || res < 0) return false;

	return stat(filename, sb) == 0;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return ent->d_name[0] == '.';
}

#ifdef WITH_ICONV

#include <iconv.h>
#include <errno.h>
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
static const char *convert_tofrom_fs(iconv_t convd, const char *name, char *outbuf, size_t outlen)
{
	/* There are different implementations of iconv. The older ones,
	 * e.g. SUSv2, pass a const pointer, whereas the newer ones, e.g.
	 * IEEE 1003.1 (2004), pass a non-const pointer. */
#ifdef HAVE_NON_CONST_ICONV
	char *inbuf = const_cast<char*>(name);
#else
	const char *inbuf = name;
#endif

	size_t inlen  = strlen(name);
	char *buf = outbuf;

	strecpy(outbuf, name, outbuf + outlen);

	iconv(convd, nullptr, nullptr, nullptr, nullptr);
	if (iconv(convd, &inbuf, &inlen, &outbuf, &outlen) == (size_t)(-1)) {
		Debug(misc, 0, "[iconv] error converting '{}'. Errno {}", name, errno);
	}

	*outbuf = '\0';
	/* FIX: invalid characters will abort conversion, but they shouldn't occur? */
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
	char buf[1024] = {};

	if (convd == (iconv_t)(-1)) {
		const char *env = GetLocalCode();
		convd = iconv_open(env, INTERNALCODE);
		if (convd == (iconv_t)(-1)) {
			Debug(misc, 0, "[iconv] conversion from codeset '{}' to '{}' unsupported", INTERNALCODE, env);
			return name;
		}
	}

	return convert_tofrom_fs(convd, name.c_str(), buf, lengthof(buf));
}

/**
 * Convert to OpenTTD's encoding from that of the local environment
 * @param name valid string that will be converted
 * @return pointer to a new stringbuffer that contains the converted string
 */
std::string FS2OTTD(const std::string &name)
{
	static iconv_t convd = (iconv_t)(-1);
	char buf[1024] = {};

	if (convd == (iconv_t)(-1)) {
		const char *env = GetLocalCode();
		convd = iconv_open(INTERNALCODE, env);
		if (convd == (iconv_t)(-1)) {
			Debug(misc, 0, "[iconv] conversion from codeset '{}' to '{}' unsupported", env, INTERNALCODE);
			return name;
		}
	}

	return convert_tofrom_fs(convd, name.c_str(), buf, lengthof(buf));
}

#endif /* WITH_ICONV */

void ShowInfo(const char *str)
{
	fprintf(stderr, "%s\n", str);
}

#if !defined(__APPLE__)
void ShowOSErrorBox(const char *buf, bool system)
{
	/* All unix systems, except OSX. Only use escape codes on a TTY. */
	if (isatty(fileno(stderr))) {
		fprintf(stderr, "\033[1;31mError: %s\033[0;39m\n", buf);
	} else {
		fprintf(stderr, "Error: %s\n", buf);
	}
}
#endif

#ifndef WITH_COCOA
bool GetClipboardContents(char *buffer, const char *last)
{
#ifdef WITH_SDL2
	if (SDL_HasClipboardText() == SDL_FALSE) {
		return false;
	}

	char *clip = SDL_GetClipboardText();
	if (clip != nullptr) {
		strecpy(buffer, clip, last);
		SDL_free(clip);
		return true;
	}
#endif

	return false;
}
#endif


#if defined(__EMSCRIPTEN__)
void OSOpenBrowser(const char *url)
{
	/* Implementation in pre.js */
	EM_ASM({ if(window["openttd_open_url"]) window.openttd_open_url($0, $1) }, url, strlen(url));
}
#elif !defined( __APPLE__)
void OSOpenBrowser(const char *url)
{
	pid_t child_pid = fork();
	if (child_pid != 0) return;

	const char *args[3];
	args[0] = "xdg-open";
	args[1] = url;
	args[2] = nullptr;
	execvp(args[0], const_cast<char * const *>(args));
	Debug(misc, 0, "Failed to open url: {}", url);
	exit(0);
}
#endif /* __APPLE__ */

void SetCurrentThreadName(const char *threadName) {
#if !defined(NO_THREADS) && defined(__GLIBC__)
#if __GLIBC_PREREQ(2, 12)
	if (threadName) pthread_setname_np(pthread_self(), threadName);
#endif /* __GLIBC_PREREQ(2, 12) */
#endif /* !defined(NO_THREADS) && defined(__GLIBC__) */
#if defined(__APPLE__)
	MacOSSetThreadName(threadName);
#endif /* defined(__APPLE__) */
}
