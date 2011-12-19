/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fileio_type.h Types for Standard In/Out file operations */

#ifndef FILEIO_TYPE_H
#define FILEIO_TYPE_H

#include "core/enum_type.hpp"

/**
 * The different kinds of subdirectories OpenTTD uses
 */
enum Subdirectory {
	BASE_DIR,      ///< Base directory for all subdirectories
	SAVE_DIR,      ///< Base directory for all savegames
	AUTOSAVE_DIR,  ///< Subdirectory of save for autosaves
	SCENARIO_DIR,  ///< Base directory for all scenarios
	HEIGHTMAP_DIR, ///< Subdirectory of scenario for heightmaps
	OLD_GM_DIR,    ///< Old subdirectory for the music
	OLD_DATA_DIR,  ///< Old subdirectory for the data.
	BASESET_DIR,   ///< Subdirectory for all base data (base sets, intro game)
	NEWGRF_DIR,    ///< Subdirectory for all NewGRFs
	LANG_DIR,      ///< Subdirectory for all translation files
	AI_DIR,        ///< Subdirectory for all AI files
	AI_LIBRARY_DIR,///< Subdirectory for all AI libraries
	GAME_DIR,      ///< Subdirectory for all game scripts
	GAME_LIBRARY_DIR, ///< Subdirectory for all GS libraries
	NUM_SUBDIRS,   ///< Number of subdirectories
	NO_DIRECTORY,  ///< A path without any base directory
};

/**
 * Types of searchpaths OpenTTD might use
 */
enum Searchpath {
	SP_FIRST_DIR,
	SP_WORKING_DIR = SP_FIRST_DIR, ///< Search in the working directory
	SP_PERSONAL_DIR,               ///< Search in the personal directory
	SP_SHARED_DIR,                 ///< Search in the shared directory, like 'Shared Files' under Windows
	SP_BINARY_DIR,                 ///< Search in the directory where the binary resides
	SP_INSTALLATION_DIR,           ///< Search in the installation directory
	SP_APPLICATION_BUNDLE_DIR,     ///< Search within the application bundle
	SP_AUTODOWNLOAD_DIR,           ///< Search within the autodownload directory
	NUM_SEARCHPATHS
};

DECLARE_POSTFIX_INCREMENT(Searchpath)

#endif /* FILEIO_TYPE_H */
