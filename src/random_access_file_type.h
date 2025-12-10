/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

 /** @file random_access_file_type.h Class related to random access to files. */

#ifndef RANDOM_ACCESS_FILE_TYPE_H
#define RANDOM_ACCESS_FILE_TYPE_H

#include "fileio_type.h"

/**
 * A file from which bytes, words and double words are read in (potentially) a random order.
 *
 * This is mostly intended to be used for things that can be read from GRFs when needed, so
 * the graphics but also the sounds. This also ties into the spritecache as it uses these
 * files to load the sprites from when needed.
 */
class RandomAccessFile {
	/** The number of bytes to allocate for the buffer. */
	static constexpr int BUFFER_SIZE = 512;

	std::string filename;            ///< Full name of the file; relative path to subdir plus the extension of the file.
	std::string simplified_filename; ///< Simplified lowercase name of the file; only the name, no path or extension.

	std::optional<FileHandle> file_handle; ///< File handle of the open file.
	size_t pos;                      ///< Position in the file of the end of the read buffer.
	size_t start_pos; ///< Start position of file. May be non-zero if file is within a tar file.
	size_t end_pos; ///< End position of file.

	uint8_t *buffer;                    ///< Current position within the local buffer.
	uint8_t *buffer_end;                ///< Last valid byte of buffer.
	uint8_t buffer_start[BUFFER_SIZE];  ///< Local buffer when read from file.

public:
	RandomAccessFile(std::string_view filename, Subdirectory subdir);
	RandomAccessFile(const RandomAccessFile&) = delete;
	void operator=(const RandomAccessFile&) = delete;

	virtual ~RandomAccessFile() = default;

	const std::string &GetFilename() const;
	const std::string &GetSimplifiedFilename() const;

	size_t GetPos() const;
	size_t GetStartPos() const { return this->start_pos; }
	size_t GetEndPos() const { return this->end_pos; }
	void SeekTo(size_t pos, int mode);
	bool AtEndOfFile() const;

	uint8_t ReadByte();
	uint16_t ReadWord();
	uint32_t ReadDword();

	void ReadBlock(void *ptr, size_t size);
	void SkipBytes(size_t n);
};

#endif /* RANDOM_ACCESS_FILE_TYPE_H */
