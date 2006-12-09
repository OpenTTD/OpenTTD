/* $Id$ */

#ifndef WIN32_H
#define WIN32_H

#include <windows.h>
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

/* Override SHGetFolderPath with our custom implementation */
#if defined(SHGetFolderPath)
#undef SHGetFolderPath
#endif
#define SHGetFolderPath OTTDSHGetFolderPath

HRESULT OTTDSHGetFolderPath(HWND, int, HANDLE, DWORD, LPTSTR);

#if defined(__MINGW32__)
#define SHGFP_TYPE_CURRENT 0
#endif /* __MINGW32__ */

#endif /* WIN32_H */
