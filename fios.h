/* $Id$ */

#ifndef FIOS_H
#define FIOS_H

/* Implementation of opendir/readdir/closedir for Windows */
#if defined(WIN32)
#include <windows.h>
typedef struct DIR DIR;

typedef struct dirent { // XXX - only d_name implemented
	char *d_name; /* name of found file */
	/* little hack which will point to parent DIR struct which will
	 * save us a call to GetFileAttributes if we want information
	 * about the file (for example in function fio_bla */
	DIR *dir;
} dirent;

struct DIR {
	HANDLE hFind;
	/* the dirent returned by readdir.
	 * note: having only one global instance is not possible because
	 * multiple independent opendir/readdir sequences must be supported. */
	dirent ent;
	WIN32_FIND_DATA fd;
	/* since opendir calls FindFirstFile, we need a means of telling the
	 * first call to readdir that we already have a file.
	 * that's the case iff this is true */
	bool at_first_entry;
};

DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);

#endif /* defined(WIN32) */

#endif /* FIOS_H */
