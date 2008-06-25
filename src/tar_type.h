/* $Id$ */

#ifndef TAR_TYPE_H
#define TAR_TYPE_H

/** @file tar_type.h Structs, typedefs and macros used for TAR file handling. */

#include <map>
#include <string>

/** The define of a TarList. */
struct TarListEntry {
	const char *filename;
};

struct TarFileListEntry {
	TarListEntry *tar;
	size_t size;
	size_t position;
};

typedef std::map<std::string, TarListEntry *> TarList;
typedef std::map<std::string, TarFileListEntry> TarFileList;
extern TarList _tar_list;
extern TarFileList _tar_filelist;

#define FOR_ALL_TARS(tar) for (tar = _tar_filelist.begin(); tar != _tar_filelist.end(); tar++)

typedef bool FioTarFileListCallback(const char *filename, int size, void *userdata);
FILE *FioTarFileList(const char *tar, const char *mode, size_t *filesize, FioTarFileListCallback *callback, void *userdata);

#endif /* TAR_TYPE_H */
