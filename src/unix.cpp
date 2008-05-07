/* $Id$ */

/** @file unix.cpp Implementation of Unix specific file handling. */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "textbuf_gui.h"
#include "functions.h"
#include "core/random_func.hpp"

#include "table/strings.h"

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#if (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) || defined(__GLIBC__)
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
		/*the mac implementation needs this file included in the same file as main() */
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

bool FiosGetDiskFreeSpace(const char *path, uint32 *tot)
{
	uint32 free = 0;

#ifdef HAS_STATVFS
# ifdef __APPLE__
	/* OSX 10.3 lacks statvfs so don't try to use it even though later versions of OSX has it. */
	if (MacOSVersionIsAtLeast(10, 4, 0))
# endif
	{
		struct statvfs s;

		if (statvfs(path, &s) != 0) return false;
		free = (uint64)s.f_frsize * s.f_bavail >> 20;
	}
#endif
	if (tot != NULL) *tot = free;
	return true;
}

bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb)
{
	char filename[MAX_PATH];

#if defined(__MORPHOS__) || defined(__AMIGAOS__)
	/* On MorphOS or AmigaOS paths look like: "Volume:directory/subdirectory" */
	if (FiosIsRoot(path)) {
		snprintf(filename, lengthof(filename), "%s:%s", path, ent->d_name);
	} else // XXX - only next line!
#else
	assert(path[strlen(path) - 1] == PATHSEPCHAR);
	if (strlen(path) > 2) assert(path[strlen(path) - 2] != PATHSEPCHAR);
#endif
	snprintf(filename, lengthof(filename), "%s%s", path, ent->d_name);

	return stat(filename, sb) == 0;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return ent->d_name[0] == '.';
}

#ifdef WITH_ICONV

#include <iconv.h>
#include <errno.h>
#include "debug.h"
#include "string_func.h"

const char *GetCurrentLocale(const char *param);

#define INTERNALCODE "UTF-8"

/** Try and try to decipher the current locale from environmental
 * variables. MacOSX is hardcoded, other OS's are dynamic. If no suitable
 * locale can be found, don't do any conversion "" */
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

/** FYI: This is not thread-safe.
 * convert between locales, which from and which to is set in the calling
 * functions OTTD2FS() and FS2OTTD(). You should NOT use this function directly
 * NOTE: iconv was added in OSX 10.3. 10.2.x will still have the invalid char
 * issues. There aren't any easy fix for this */
static const char *convert_tofrom_fs(iconv_t convd, const char *name)
{
	static char buf[1024];
	/* Work around buggy iconv implementation where inbuf is wrongly typed as
	 * non-const. Correct implementation is at
	 * http://www.opengroup.org/onlinepubs/007908799/xsh/iconv.html */
#ifdef HAVE_BROKEN_ICONV
	char *inbuf = (char*)name;
#else
	const char *inbuf = name;
#endif

	char *outbuf  = buf;
	size_t outlen = sizeof(buf) - 1;
	size_t inlen  = strlen(name);

	ttd_strlcpy(outbuf, name, sizeof(buf));

	iconv(convd, NULL, NULL, NULL, NULL);
	if (iconv(convd, &inbuf, &inlen, &outbuf, &outlen) == (size_t)(-1)) {
		DEBUG(misc, 0, "[iconv] error converting '%s'. Errno %d", name, errno);
	}

	*outbuf = '\0';
	/* FIX: invalid characters will abort conversion, but they shouldn't occur? */
	return buf;
}

/** Convert from OpenTTD's encoding to that of the local environment
 * @param name pointer to a valid string that will be converted
 * @return pointer to a new stringbuffer that contains the converted string */
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

/** Convert to OpenTTD's encoding from that of the local environment
 * @param name pointer to a valid string that will be converted
 * @return pointer to a new stringbuffer that contains the converted string */
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

void ShowOSErrorBox(const char *buf)
{
#if defined(__APPLE__)
	/* this creates an NSAlertPanel with the contents of 'buf'
	 * this is the native and nicest way to do this on OSX */
	ShowMacDialog( buf, "See readme for more info\nMost likely you are missing files from the original TTD", "Quit" );
#else
	/* all systems, but OSX */
	fprintf(stderr, "\033[1;31mError: %s\033[0;39m\n", buf);
#endif
}

#ifdef WITH_COCOA
void cocoaSetupAutoreleasePool();
void cocoaReleaseAutoreleasePool();
#endif

int CDECL main(int argc, char* argv[])
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

	SetRandomSeed(time(NULL));

	signal(SIGPIPE, SIG_IGN);

	ret = ttd_main(argc, argv);

#ifdef WITH_COCOA
	cocoaReleaseAutoreleasePool();
#endif

	return ret;
}

bool InsertTextBufferClipboard(Textbuf *tb)
{
	return false;
}


/* multi os compatible sleep function */

#ifdef __AMIGA__
/* usleep() implementation */
#	include <devices/timer.h>
#	include <dos/dos.h>

	extern struct Device      *TimerBase    = NULL;
	extern struct MsgPort     *TimerPort    = NULL;
	extern struct timerequest *TimerRequest = NULL;
#endif // __AMIGA__

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
