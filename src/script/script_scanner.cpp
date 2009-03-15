/* $Id$ */

/** @file script_scanner.cpp Allows scanning for scripts. */

#include "../stdafx.h"
#include "../string_func.h"
#include "../fileio_func.h"
#include <sys/stat.h>

#include <squirrel.h>
#include "../script/squirrel.hpp"
#include "script_info.hpp"
#include "script_scanner.hpp"

void ScriptScanner::ScanDir(const char *dirname, const char *info_file_name)
{
	extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);
	extern bool FiosIsHiddenFile(const struct dirent *ent);

	char d_name[MAX_PATH];
	char temp_script[1024];
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;

	dir = ttd_opendir(dirname);
	/* Dir not found, so do nothing */
	if (dir == NULL) return;

	/* Walk all dirs trying to find a dir in which 'main.nut' exists */
	while ((dirent = readdir(dir)) != NULL) {
		ttd_strlcpy(d_name, FS2OTTD(dirent->d_name), sizeof(d_name));

		/* Valid file, not '.' or '..', not hidden */
		if (!FiosIsValidFile(dirname, dirent, &sb)) continue;
		if (strcmp(d_name, ".") == 0 || strcmp(d_name, "..") == 0) continue;
		if (FiosIsHiddenFile(dirent) && strncasecmp(d_name, PERSONAL_DIR, strlen(d_name)) != 0) continue;

		/* Create the full length dirname */
		ttd_strlcpy(temp_script, dirname, sizeof(temp_script));
		ttd_strlcat(temp_script, d_name,  sizeof(temp_script));

		if (S_ISREG(sb.st_mode)) {
			/* Only .tar files are allowed */
			char *ext = strrchr(d_name, '.');
			if (ext == NULL || strcasecmp(ext, ".tar") != 0) continue;

			/* We always expect a directory in the TAR */
			const char *first_dir = FioTarFirstDir(temp_script);
			if (first_dir == NULL) continue;

			ttd_strlcat(temp_script, PATHSEP, sizeof(temp_script));
			ttd_strlcat(temp_script, first_dir, sizeof(temp_script));
			FioTarAddLink(temp_script, first_dir);
		} else if (!S_ISDIR(sb.st_mode)) {
			/* Skip any other type of file */
			continue;
		}

		/* Add an additional / where needed */
		if (temp_script[strlen(temp_script) - 1] != PATHSEPCHAR) ttd_strlcat(temp_script, PATHSEP, sizeof(temp_script));

		char info_script[MAX_PATH];
		ttd_strlcpy(info_script, temp_script, sizeof(info_script));
		ttd_strlcpy(main_script, temp_script, sizeof(main_script));

		/* Every script should contain an 'info.nut' and 'main.nut' file; else it is not a valid script */
		ttd_strlcat(info_script, info_file_name, sizeof(info_script));
		ttd_strlcat(main_script, "main.nut", sizeof(main_script));
		if (!FioCheckFileExists(info_script, NO_DIRECTORY) || !FioCheckFileExists(main_script, NO_DIRECTORY)) continue;

		/* We don't care if one of the other scripts failed to load. */
		this->engine->ResetCrashed();
		this->engine->LoadScript(info_script);
	}
	closedir(dir);
}

void ScriptScanner::ScanScriptDir(const char *info_file_name, Subdirectory search_dir)
{
	char buf[MAX_PATH];
	Searchpath sp;

	extern void ScanForTarFiles();
	ScanForTarFiles();

	FOR_ALL_SEARCHPATHS(sp) {
		FioAppendDirectory(buf, MAX_PATH, sp, search_dir);
		if (FileExists(buf)) this->ScanDir(buf, info_file_name);
	}
}

ScriptScanner::ScriptScanner()
{
	this->engine = new Squirrel();
	this->main_script[0] = '\0';

	/* Mark this class as global pointer */
	this->engine->SetGlobalPointer(this);
}

ScriptScanner::~ScriptScanner()
{
	delete this->engine;
}
