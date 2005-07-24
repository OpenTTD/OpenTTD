/* $Id$ */

#ifndef WIN32_H
#define WIN32_H

typedef void (*Function)(int);
bool LoadLibraryList(Function proc[], const char* dll);

#endif
