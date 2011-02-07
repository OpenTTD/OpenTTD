/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unix.cpp Implementation of Unix specific file handling. */

#include "../../stdafx.h"
#include "../../textbuf_gui.h"
#include "../../openttd.h"
#include "../../crashlog.h"
#include "../../core/random_func.hpp"


#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#ifdef __APPLE__
	#include <sys/mount.h>
#elif (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) || defined(__GLIBC__)
	#define HAS_STATVFS
#endif

#ifdef HAS_STATVFS
#include <sys/statvfs.h>
#endif


#ifdef __MORPHOS__
#include <exec/types.h>
ULONG __stack = (1024*1024)*2; // maybe not that much is needed actually ;)

/* The system supplied definition of SIG_IGN does not match */
#undef SIG_IGN
#define SIG_IGN (void (*)(int))1
#endif /* __MORPHOS__ */

#ifdef __AMIGA__
#warning add stack symbol to avoid that user needs to set stack manually (tokai)
// ULONG __stack =
#endif

#if defined(__APPLE__)
	#if defined(WITH_SDL)
		/* the mac implementation needs this file included in the same file as main() */
		#include <SDL.h>
	#endif
#endif

bool FiosIsRoot(const char *path)
{
#if !defined(__MORPHOS__) && !defined(__AMIGAOS__)
	return path[1] == '\0';
#else
	/* On MorphOS or AmigaOS paths look like: "Volume:directory/subdirectory" */
	const char *s = strchr(path, ':');
	return s != NULL && s[1] == '\0';
#endif
}

void FiosGetDrives()
{
	return;
}

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
	if (tot != NULL) *tot = free;
	return true;
}

bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb)
{
	char filename[MAX_PATH];
	int res;
#if defined(__MORPHOS__) || defined(__AMIGAOS__)
	/* On MorphOS or AmigaOS paths look like: "Volume:directory/subdirectory" */
	if (FiosIsRoot(path)) {
		res = snprintf(filename, lengthof(filename), "%s:%s", path, ent->d_name);
	} else // XXX - only next line!
#else
	assert(path[strlen(path) - 1] == PATHSEPCHAR);
	if (strlen(path) > 2) assert(path[strlen(path) - 2] != PATHSEPCHAR);
#endif
	res = snprintf(filename, lengthof(filename), "%s%s", path, ent->d_name);

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
	if (locale != NULL) locale = strchr(locale, '.');

	return (locale == NULL) ? "" : locale + 1;
#endif
}

/**
 * Convert between locales, which from and which to is set in the calling
 * functions OTTD2FS() and FS2OTTD().
 */
static const char *convert_tofrom_fs(iconv_t convd, const char *name)
{
	static char buf[1024];
	/* There are different implementations of iconv. The older ones,
	 * e.g. SUSv2, pass a const pointer, whereas the newer ones, e.g.
	 * IEEE 1003.1 (2004), pass a non-const pointer. */
#ifdef HAVE_NON_CONST_ICONV
	char *inbuf = const_cast<char*>(name);
#else
	const char *inbuf = name;
#endif

	char *outbuf  = buf;
	size_t outlen = sizeof(buf) - 1;
	size_t inlen  = strlen(name);

	strecpy(outbuf, name, outbuf + outlen);

	iconv(convd, NULL, NULL, NULL, NULL);
	if (iconv(convd, &inbuf, &inlen, &outbuf, &outlen) == (size_t)(-1)) {
		DEBUG(misc, 0, "[iconv] error converting '%s'. Errno %d", name, errno);
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
const char *OTTD2FS(const char *name)
{
	static iconv_t convd = (iconv_t)(-1);

	if (convd == (iconv_t)(-1)) {
		const char *env = GetLocalCode();
		convd = iconv_open(env, INTERNALCODE);
		if (convd == (iconv_t)(-1)) {
			DEBUG(misc, 0, "[iconv] conversion from codeset '%s' to '%s' unsupported", INTERNALCODE, env);
			return name;
		}
	}

	return convert_tofrom_fs(convd, name);
}

/**
 * Convert to OpenTTD's encoding from that of the local environment
 * @param name pointer to a valid string that will be converted
 * @return pointer to a new stringbuffer that contains the converted string
 */
const char *FS2OTTD(const char *name)
{
	static iconv_t convd = (iconv_t)(-1);

	if (convd == (iconv_t)(-1)) {
		const char *env = GetLocalCode();
		convd = iconv_open(INTERNALCODE, env);
		if (convd == (iconv_t)(-1)) {
			DEBUG(misc, 0, "[iconv] conversion from codeset '%s' to '%s' unsupported", env, INTERNALCODE);
			return name;
		}
	}

	return convert_tofrom_fs(convd, name);
}

#else
const char *FS2OTTD(const char *name) {return name;}
const char *OTTD2FS(const char *name) {return name;}
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

#ifdef WITH_COCOA
void cocoaSetupAutoreleasePool();
void cocoaReleaseAutoreleasePool();
#endif

int CDECL main(int argc, char *argv[])
{
	int ret;

#ifdef WITH_COCOA
	cocoaSetupAutoreleasePool();
	/* This is passed if we are launched by double-clicking */
	if (argc >= 2 && strncmp(argv[1], "-psn", 4) == 0) {
		argv[1] = NULL;
		argc = 1;
	}
#endif
	CrashLog::InitialiseCrashLog();

	SetRandomSeed(time(NULL));

	signal(SIGPIPE, SIG_IGN);

	ret = ttd_main(argc, argv);

#ifdef WITH_COCOA
	cocoaReleaseAutoreleasePool();
#endif

	return ret;
}

#ifndef WITH_COCOA
bool GetClipboardContents(char *buffer, size_t buff_len)
{
	return false;
}
#endif


/* multi os compatible sleep function */

#ifdef __AMIGA__
/* usleep() implementation */
#	include <devices/timer.h>
#	include <dos/dos.h>

	extern struct Device      *TimerBase    = NULL;
	extern struct MsgPort     *TimerPort    = NULL;
	extern struct timerequest *TimerRequest = NULL;
#endif /* __AMIGA__ */

void CSleep(int milliseconds)
{
	#if defined(PSP)
		sceKernelDelayThread(milliseconds * 1000);
	#elif defined(__BEOS__)
		snooze(milliseconds * 1000);
	#elif defined(__AMIGA__)
	{
		ULONG signals;
		ULONG TimerSigBit = 1 << TimerPort->mp_SigBit;

		/* send IORequest */
		TimerRequest->tr_node.io_Command = TR_ADDREQUEST;
		TimerRequest->tr_time.tv_secs    = (milliseconds * 1000) / 1000000;
		TimerRequest->tr_time.tv_micro   = (milliseconds * 1000) % 1000000;
		SendIO((struct IORequest *)TimerRequest);

		if (!((signals = Wait(TimerSigBit | SIGBREAKF_CTRL_C)) & TimerSigBit) ) {
			AbortIO((struct IORequest *)TimerRequest);
		}
		WaitIO((struct IORequest *)TimerRequest);
	}
	#else
		usleep(milliseconds * 1000);
	#endif
}
