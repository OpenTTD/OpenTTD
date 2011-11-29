/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_scanner.cpp Allows scanning for scripts. */

#include "../stdafx.h"
#include "../string_func.h"
#include "../fileio_func.h"
#include <sys/stat.h>

#include "../script/squirrel.hpp"
#include "script_scanner.hpp"

bool ScriptScanner::AddFile(const char *filename, size_t basepath_length, const char *tar_filename)
{
	free(this->main_script);
	this->main_script = strdup(filename);
	if (this->main_script == NULL) return false;

	free(this->tar_file);
	if (tar_filename != NULL) {
		this->tar_file = strdup(tar_filename);
		if (this->tar_file == NULL) return false;
	} else {
		this->tar_file = NULL;
	}

	const char *end = this->main_script + strlen(this->main_script) + 1;
	char *p = strrchr(this->main_script, PATHSEPCHAR);
	if (p == NULL) {
		p = this->main_script;
	} else {
		/* Skip over the path separator character. We don't need that. */
		p++;
	}

	strecpy(p, "main.nut", end);

	if (!FioCheckFileExists(filename, this->subdir) || !FioCheckFileExists(this->main_script, this->subdir)) return false;

	/* We don't care if one of the other scripts failed to load. */
	this->engine->ResetCrashed();
	this->engine->LoadScript(filename);
	return true;
}

ScriptScanner::ScriptScanner()
{
	this->engine = new Squirrel("Scanner");

	/* Mark this class as global pointer */
	this->engine->SetGlobalPointer(this);
	this->main_script = NULL;
	this->tar_file = NULL;
}

ScriptScanner::~ScriptScanner()
{
	free(this->main_script);
	delete this->engine;
}
