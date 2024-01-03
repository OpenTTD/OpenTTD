/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file random_access_file.cpp Actual implementation of the RandomAccessFile class. */

#include "stdafx.h"
#include "random_access_file_type.h"

#include "debug.h"
#include "error_func.h"
#include "fileio_func.h"
#include "string_func.h"

#include "safeguards.h"

/**
 * Create the RandomAccesFile.
 * @param filename Name of the file at the disk.
 * @param subdir   The sub directory to search this file in.
 */
RandomAccessFile::RandomAccessFile(const std::string &filename, Subdirectory subdir) : filename(filename)
{
	this->file_handle = FioFOpenFile(filename, "rb", subdir);
	if (this->file_handle == nullptr) UserError("Cannot open file '{}'", filename);

	/* When files are in a tar-file, the begin of the file might not be at 0. */
	long pos = ftell(this->file_handle);
	if (pos < 0) UserError("Cannot read file '{}'", filename);

	/* Store the filename without path and extension */
	auto t = filename.rfind(PATHSEPCHAR);
	std::string name_without_path = filename.substr(t != std::string::npos ? t + 1 : 0);
	this->simplified_filename = name_without_path.substr(0, name_without_path.rfind('.'));
	strtolower(this->simplified_filename);

	this->SeekTo((size_t)pos, SEEK_SET);
}

/**
 * Close the file's file handle.
 */
RandomAccessFile::~RandomAccessFile()
{
	fclose(this->file_handle);
}

/**
 * Get the filename of the opened file with the path from the SubDirectory and the extension.
 * @return Name of the file.
 */
const std::string &RandomAccessFile::GetFilename() const
{
	return this->filename;
}

/**
 * Get the simplified filename of the opened file. The simplified filename is the name of the
 * file without the SubDirectory or extension in lower case.
 * @return Name of the file.
 */
const std::string &RandomAccessFile::GetSimplifiedFilename() const
{
	return this->simplified_filename;
}

/**
 * Get position in the file.
 * @return Position in the file.
 */
size_t RandomAccessFile::GetPos() const
{
	return this->pos + (this->buffer - this->buffer_end);
}

/**
 * Seek in the current file.
 * @param pos New position.
 * @param mode Type of seek (\c SEEK_CUR means \a pos is relative to current position, \c SEEK_SET means \a pos is absolute).
 */
void RandomAccessFile::SeekTo(size_t pos, int mode)
{
	if (mode == SEEK_CUR) pos += this->GetPos();

	this->pos = pos;
	if (fseek(this->file_handle, this->pos, SEEK_SET) < 0) {
		Debug(misc, 0, "Seeking in {} failed", this->filename);
	}

	/* Reset the buffer, so the next ReadByte will read bytes from the file. */
	this->buffer = this->buffer_end = this->buffer_start;
}

/**
 * Read a byte from the file.
 * @return Read byte.
 */
byte RandomAccessFile::ReadByte()
{
	if (this->buffer == this->buffer_end) {
		this->buffer = this->buffer_start;
		size_t size = fread(this->buffer, 1, RandomAccessFile::BUFFER_SIZE, this->file_handle);
		this->pos += size;
		this->buffer_end = this->buffer_start + size;

		if (size == 0) return 0;
	}
	return *this->buffer++;
}

/**
 * Read a word (16 bits) from the file (in low endian format).
 * @return Read word.
 */
uint16_t RandomAccessFile::ReadWord()
{
	byte b = this->ReadByte();
	return (this->ReadByte() << 8) | b;
}

/**
 * Read a double word (32 bits) from the file (in low endian format).
 * @return Read word.
 */
uint32_t RandomAccessFile::ReadDword()
{
	uint b = this->ReadWord();
	return (this->ReadWord() << 16) | b;
}

/**
 * Read a block.
 * @param ptr  Destination buffer.
 * @param size Number of bytes to read.
 */
void RandomAccessFile::ReadBlock(void *ptr, size_t size)
{
	this->SeekTo(this->GetPos(), SEEK_SET);
	this->pos += fread(ptr, 1, size, this->file_handle);
}

/**
 * Skip \a n bytes ahead in the file.
 * @param n Number of bytes to skip reading.
 */
void RandomAccessFile::SkipBytes(size_t n)
{
	assert(this->buffer_end >= this->buffer);
	size_t remaining = this->buffer_end - this->buffer;
	if (n <= remaining) {
		this->buffer += n;
	} else {
		this->SeekTo(n, SEEK_CUR);
	}
}
