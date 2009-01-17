/* $Id$ */

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
	GM_DIR,        ///< Subdirectory for all music
	DATA_DIR,      ///< Subdirectory for all data (GRFs, sample.cat, intro game)
	LANG_DIR,      ///< Subdirectory for all translation files
	AI_DIR,        ///< Subdirectory for all AI files
	AI_LIBRARY_DIR,///< Subdirectory for all AI libraries
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

DECLARE_POSTFIX_INCREMENT(Searchpath);

#endif /* FILEIO_TYPE_H */
