/* $Id$ */

#ifndef WIN32_H
#define WIN32_H

bool MyShowCursor(bool show);

typedef void (*Function)(int);
bool LoadLibraryList(Function proc[], const char *dll);

#if defined(UNICODE)
# define MB_TO_WIDE(x) OTTD2FS(x)
# define WIDE_TO_MB(x) FS2OTTD(x)
#else
# define MB_TO_WIDE(x) (x)
# define WIDE_TO_MB(x) (x)
#endif

#endif /* WIN32_H */
