/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fileio_type.h Types for Standard In/Out file operations */

#ifndef FILEIO_TYPE_H
#define FILEIO_TYPE_H

#include "core/enum_type.hpp"
#include "misc/autorelease.hpp"

/** The different abstract types of files that the system knows about. */
enum AbstractFileType : uint8_t {
	FT_NONE,      ///< nothing to do
	FT_SAVEGAME,  ///< old or new savegame
	FT_SCENARIO,  ///< old or new scenario
	FT_HEIGHTMAP, ///< heightmap file
	FT_TOWN_DATA, ///< town data file

	FT_INVALID = 7, ///< Invalid or unknown file type.
};

/** Kinds of files in each #AbstractFileType. */
enum DetailedFileType : uint8_t {
	/* Save game and scenario files. */
	DFT_OLD_GAME_FILE, ///< Old save game or scenario file.
	DFT_GAME_FILE,     ///< Save game or scenario file.

	/* Heightmap files. */
	DFT_HEIGHTMAP_BMP, ///< BMP file.
	DFT_HEIGHTMAP_PNG, ///< PNG file.

	/* Town data files. */
	DFT_TOWN_DATA_JSON,  ///< JSON file.

	/* fios 'files' */
	DFT_FIOS_DRIVE,  ///< A drive (letter) entry.
	DFT_FIOS_PARENT, ///< A parent directory entry.
	DFT_FIOS_DIR,    ///< A directory entry.
	DFT_FIOS_DIRECT, ///< Direct filename.

	DFT_END,         ///< End of this enum. Supports a compile time size check against _fios_colours in fios_gui.cpp

	DFT_INVALID = 255, ///< Unknown or invalid file.
};

/** Operation performed on the file. */
enum SaveLoadOperation : uint8_t {
	SLO_CHECK,   ///< Load file for checking and/or preview.
	SLO_LOAD,    ///< File is being loaded.
	SLO_SAVE,    ///< File is being saved.

	SLO_INVALID, ///< Unknown file operation.
};

/**
 * Elements of a file system that are recognized.
 */
struct FiosType {
	AbstractFileType abstract; ///< Abstract file type.
	DetailedFileType detailed; ///< Detailed file type.

	constexpr bool operator==(const FiosType &) const noexcept = default;
};

constexpr FiosType FIOS_TYPE_DRIVE{FT_NONE, DFT_FIOS_DRIVE};
constexpr FiosType FIOS_TYPE_PARENT{FT_NONE, DFT_FIOS_PARENT};
constexpr FiosType FIOS_TYPE_DIR{FT_NONE, DFT_FIOS_DIR};
constexpr FiosType FIOS_TYPE_DIRECT{FT_NONE, DFT_FIOS_DIRECT};

constexpr FiosType FIOS_TYPE_FILE{FT_SAVEGAME, DFT_GAME_FILE};
constexpr FiosType FIOS_TYPE_OLDFILE{FT_SAVEGAME, DFT_OLD_GAME_FILE};
constexpr FiosType FIOS_TYPE_SCENARIO{FT_SCENARIO, DFT_GAME_FILE};
constexpr FiosType FIOS_TYPE_OLD_SCENARIO{FT_SCENARIO, DFT_OLD_GAME_FILE};
constexpr FiosType FIOS_TYPE_PNG{FT_HEIGHTMAP, DFT_HEIGHTMAP_PNG};
constexpr FiosType FIOS_TYPE_BMP{FT_HEIGHTMAP, DFT_HEIGHTMAP_BMP};
constexpr FiosType FIOS_TYPE_JSON{FT_TOWN_DATA, DFT_TOWN_DATA_JSON};

constexpr FiosType FIOS_TYPE_INVALID{FT_INVALID, DFT_INVALID};

/**
 * The different kinds of subdirectories OpenTTD uses
 */
enum Subdirectory : uint8_t {
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
	AI_DIR,        ///< Subdirectory for all %AI files
	AI_LIBRARY_DIR,///< Subdirectory for all %AI libraries
	GAME_DIR,      ///< Subdirectory for all game scripts
	GAME_LIBRARY_DIR, ///< Subdirectory for all GS libraries
	SCREENSHOT_DIR,   ///< Subdirectory for all screenshots
	SOCIAL_INTEGRATION_DIR, ///< Subdirectory for all social integration plugins
	DOCS_DIR,      ///< Subdirectory for documentation
	NUM_SUBDIRS,   ///< Number of subdirectories
	NO_DIRECTORY,  ///< A path without any base directory
};

/**
 * Types of searchpaths OpenTTD might use
 */
enum Searchpath : uint8_t {
	SP_FIRST_DIR,
	SP_WORKING_DIR = SP_FIRST_DIR, ///< Search in the working directory
#ifdef USE_XDG
	SP_PERSONAL_DIR_XDG,           ///< Search in the personal directory from the XDG specification
#endif
	SP_PERSONAL_DIR,               ///< Search in the personal directory
	SP_SHARED_DIR,                 ///< Search in the shared directory, like 'Shared Files' under Windows
	SP_BINARY_DIR,                 ///< Search in the directory where the binary resides
	SP_INSTALLATION_DIR,           ///< Search in the installation directory
	SP_APPLICATION_BUNDLE_DIR,     ///< Search within the application bundle
	SP_AUTODOWNLOAD_DIR,           ///< Search within the autodownload directory
	SP_AUTODOWNLOAD_PERSONAL_DIR,  ///< Search within the autodownload directory located in the personal directory
	SP_AUTODOWNLOAD_PERSONAL_DIR_XDG, ///< Search within the autodownload directory located in the personal directory (XDG variant)
	NUM_SEARCHPATHS
};

DECLARE_INCREMENT_DECREMENT_OPERATORS(Searchpath)

class FileHandle {
public:
	static std::optional<FileHandle> Open(const std::string &filename, std::string_view mode);
	static std::optional<FileHandle> Open(std::string_view filename, std::string_view mode) { return FileHandle::Open(std::string{filename}, mode); }

	inline void Close() { this->f.reset(); }

	inline operator FILE *()
	{
		assert(this->f != nullptr);
		return this->f.get();
	}

private:
	AutoRelease<FILE, fclose> f;

	FileHandle(FILE *f) : f(f) { assert(this->f != nullptr); }
};

/* Ensure has_value() is used consistently. */
template <> constexpr std::optional<FileHandle>::operator bool() const noexcept = delete;

#endif /* FILEIO_TYPE_H */
