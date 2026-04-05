/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fileio_type.h Types for standard in/out file operations. */

#ifndef FILEIO_TYPE_H
#define FILEIO_TYPE_H

#include "core/enum_type.hpp"
#include "misc/autorelease.hpp"

/** The different abstract types of files that the system knows about. */
enum class AbstractFileType : uint8_t {
	None, ///< nothing to do
	Savegame, ///< old or new savegame
	Scenario, ///< old or new scenario
	Heightmap, ///< heightmap file
	TownData, ///< town data file

	Invalid = 7, ///< Invalid or unknown file type.
};

/** Kinds of files in each #AbstractFileType. */
enum class DetailedFileType : uint8_t {
	/* Save game and scenario files. */
	OldGameFile, ///< Old save game or scenario file.
	GameFile, ///< Save game or scenario file.

	/* Heightmap files. */
	HeightmapBmp, ///< BMP file.
	HeightmapPng, ///< PNG file.

	/* Town data files. */
	TownDataJson, ///< JSON file.

	/* fios 'files' */
	FiosDrive, ///< A drive (letter) entry.
	FiosParent, ///< A parent directory entry.
	FiosDirectory, ///< A directory entry.
	FiosDirect, ///< Direct filename.

	End, ///< End marker.

	Invalid = 255, ///< Unknown or invalid file.
};

/** Operation performed on the file. */
enum class SaveLoadOperation : uint8_t {
	Check, ///< Load file for checking and/or preview.
	Load, ///< File is being loaded.
	Save, ///< File is being saved.

	Invalid, ///< Unknown file operation.
};

/**
 * Elements of a file system that are recognized.
 */
struct FiosType {
	AbstractFileType abstract; ///< Abstract file type.
	DetailedFileType detailed; ///< Detailed file type.

	constexpr bool operator==(const FiosType &) const noexcept = default;
};

constexpr FiosType FIOS_TYPE_DRIVE{AbstractFileType::None, DetailedFileType::FiosDrive};
constexpr FiosType FIOS_TYPE_PARENT{AbstractFileType::None, DetailedFileType::FiosParent};
constexpr FiosType FIOS_TYPE_DIR{AbstractFileType::None, DetailedFileType::FiosDirectory};
constexpr FiosType FIOS_TYPE_DIRECT{AbstractFileType::None, DetailedFileType::FiosDirect};

constexpr FiosType FIOS_TYPE_FILE{AbstractFileType::Savegame, DetailedFileType::GameFile};
constexpr FiosType FIOS_TYPE_OLDFILE{AbstractFileType::Savegame, DetailedFileType::OldGameFile};
constexpr FiosType FIOS_TYPE_SCENARIO{AbstractFileType::Scenario, DetailedFileType::GameFile};
constexpr FiosType FIOS_TYPE_OLD_SCENARIO{AbstractFileType::Scenario, DetailedFileType::OldGameFile};
constexpr FiosType FIOS_TYPE_PNG{AbstractFileType::Heightmap, DetailedFileType::HeightmapPng};
constexpr FiosType FIOS_TYPE_BMP{AbstractFileType::Heightmap, DetailedFileType::HeightmapBmp};
constexpr FiosType FIOS_TYPE_JSON{AbstractFileType::TownData, DetailedFileType::TownDataJson};

constexpr FiosType FIOS_TYPE_INVALID{AbstractFileType::Invalid, DetailedFileType::Invalid};

/**
 * The different kinds of subdirectories OpenTTD uses
 */
enum class Subdirectory : uint8_t {
	Base, ///< Base directory for all subdirectories.
	Save, ///< Base directory for all savegames.
	Autosave, ///< Subdirectory of save for autosaves.
	Scenario, ///< Base directory for all scenarios.
	Heightmap, ///< Subdirectory of scenario for heightmaps.
	OldGm, ///< Old subdirectory for the music.
	OldData, ///< Old subdirectory for the data.
	Baseset, ///< Subdirectory for all base data (base sets, intro game).
	NewGrf, ///< Subdirectory for all NewGRFs.
	Lang, ///< Subdirectory for all translation files.
	Ai, ///< Subdirectory for all %AI files.
	AiLibrary, ///< Subdirectory for all %AI libraries.
	Gs, ///< Subdirectory for all game scripts.
	GsLibrary, ///< Subdirectory for all GS libraries.
	Screenshot, ///< Subdirectory for all screenshots.
	SocialIntegration, ///< Subdirectory for all social integration plugins.
	Docs, ///< Subdirectory for documentation.
	End, ///< End marker.
	None, ///< A path without any base directory.
};

/**
 * Types of searchpaths OpenTTD might use
 */
enum class Searchpath : uint8_t {
	Begin, ///< The lowest valid value.
	WorkingDir = Searchpath::Begin, ///< Search in the working directory.
#ifdef USE_XDG
	PersonalDirXdg, ///< Search in the personal directory from the XDG specification.
#endif
	PersonalDir, ///< Search in the personal directory.
	SharedDir, ///< Search in the shared directory, like 'Shared Files' under Windows.
	BinaryDir, ///< Search in the directory where the binary resides.
	InstallationDir, ///< Search in the installation directory.
	ApplicationBundleDir, ///< Search within the application bundle.
	AutodownloadDir, ///< Search within the autodownload directory.
	AutodownloadPersonalDir,///< Search within the autodownload directory located in the personal directory.
	AutodownloadPersonalDirXdg, ///< Search within the autodownload directory located in the personal directory (XDG variant).
	End, ///< End marker.
};

DECLARE_INCREMENT_DECREMENT_OPERATORS(Searchpath)

class FileHandle {
public:
	/**
	 * Open an RAII file handle if possible.
	 * The canonical RAII-way is for FileHandle to open the file and throw an exception on failure, but we don't want that.
	 * @param filename UTF-8 encoded filename to open.
	 * @param mode Mode to open file.
	 * @return FileHandle, or std::nullopt on failure.
	 */
	static std::optional<FileHandle> Open(const std::string &filename, std::string_view mode);

	/**
	 * Open an RAII file handle if possible.
	 * The canonical RAII-way is for FileHandle to open the file and throw an exception on failure, but we don't want that.
	 * @param filename UTF-8 encoded filename to open.
	 * @param mode Mode to open file.
	 * @return FileHandle, or std::nullopt on failure.
	 */
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
