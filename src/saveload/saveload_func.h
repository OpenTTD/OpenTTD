/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file saveload_func.h Functions related to saving and loading games. */

#ifndef SAVELOAD_FUNC_H
#define SAVELOAD_FUNC_H

#include "saveload_type.h"
#include "../fileio_type.h"
#include "../fios.h"

/** Deals with the type of the savegame, independent of extension */
struct FileToSaveLoad {
	SaveLoadOperation file_op;       ///< File operation to perform.
	FiosType ftype;                  ///< File type.
	std::string name;                ///< Name of the file.
	EncodedString title;             ///< Internal name of the game.

	void SetMode(const FiosType &ft, SaveLoadOperation fop = SaveLoadOperation::Load);
	void Set(const FiosItem &item);
};

extern FileToSaveLoad _file_to_saveload;

std::string GenerateDefaultSaveName();
EncodedString GetSaveLoadErrorType();
EncodedString GetSaveLoadErrorMessage();
SaveLoadResult SaveOrLoad(std::string_view filename, SaveLoadOperation fop, DetailedFileType dft, Subdirectory sb, bool threaded = true);
void WaitTillSaved();
void ProcessAsyncSaveFinish();
void DoExitSave();

void DoAutoOrNetsave(FiosNumberedSaveName &counter);

SaveLoadResult SaveWithFilter(std::shared_ptr<struct SaveFilter> writer, bool threaded);
SaveLoadResult LoadWithFilter(std::shared_ptr<struct LoadFilter> reader);

bool SaveloadCrashWithMissingNewGRFs();

extern std::string _savegame_format;
extern bool _do_autosave;

#endif /* SAVELOAD_FUNC_H */
