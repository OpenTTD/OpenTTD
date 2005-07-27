/* $Id$ */

#ifndef WIN32_H
#define WIN32_H

bool MyShowCursor(bool show);

typedef void (*Function)(int);
bool LoadLibraryList(Function proc[], const char* dll);

#endif
