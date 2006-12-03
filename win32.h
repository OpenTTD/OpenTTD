/* $Id$ */

#ifndef WIN32_H
#define WIN32_H

bool MyShowCursor(bool show);

typedef void (*Function)(int);
bool LoadLibraryList(Function proc[], const char *dll);

char *convert_from_fs(const wchar_t *name, char *utf8_buf, size_t buflen);
wchar_t *convert_to_fs(const char *name, wchar_t *utf16_buf, size_t buflen);

#if defined(UNICODE)
# define MB_TO_WIDE(str) OTTD2FS(str)
# define MB_TO_WIDE_BUFFER(str, buffer, buflen) convert_to_fs(str, buffer, buflen)
# define WIDE_TO_MB(str) FS2OTTD(str)
# define WIDE_TO_MB_BUFFER(str, buffer, buflen) convert_from_fs(str, buffer, buflen)
#else
# define MB_TO_WIDE(str) (str)
# define MB_TO_WIDE_BUFFER(str, buffer, buflen) (str)
# define WIDE_TO_MB(str) (str)
# define WIDE_TO_MB_BUFFER(str, buffer, buflen) (str)
#endif

#endif /* WIN32_H */
